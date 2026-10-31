#pragma once
#include <sys/stat.h>
namespace GamePackage {
static const size_t MaxPayload = 512 * 1024 * 1024;
inline void Number(std::vector<unsigned char> &out, uint64_t n, int count) {
  for (int i = 0; i < count; ++i)
    out.push_back(static_cast<unsigned char>(n >> (i * 8)));
}
inline uint64_t NumberAt(const std::vector<unsigned char> &data, size_t offset,
                         int count) {
  if (offset > data.size() || size_t(count) > data.size() - offset)
    throw std::runtime_error("Truncated game container");
  uint64_t value = 0;
  for (int i = 0; i < count; ++i)
    value |= uint64_t(data[offset + i]) << (i * 8);
  return value;
}
inline uint32_t CRC(const unsigned char *data, size_t size) {
  uint32_t crc = ~uint32_t(0);
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int k = 0; k < 8; ++k)
      crc = (crc >> 1) ^ (0xedb88320u & uint32_t(-int(crc & 1)));
  }
  return ~crc;
}
inline std::string ExecutablePath() {
#ifdef _WIN32
  wchar_t path[32768];
  DWORD length = GetModuleFileNameW(nullptr, path, 32768);
  if (!length || length >= 32768)
    throw std::runtime_error("Cannot find runtime executable");
  int n = WideCharToMultiByte(CP_UTF8, 0, path, int(length), nullptr, 0,
                              nullptr, nullptr);
  std::string result(size_t(n), ' ');
  WideCharToMultiByte(CP_UTF8, 0, path, int(length), &result[0], n, nullptr,
                      nullptr);
  return result;
#else
  char path[4096];
  ssize_t n = readlink("/proc/self/exe", path, sizeof(path));
  if (n <= 0 || size_t(n) >= sizeof(path))
    throw std::runtime_error("Cannot find runtime executable");
  return std::string(path, size_t(n));
#endif
}
inline bool Read(const std::string &executable,
                 std::map<std::string, std::vector<unsigned char>> &assets) {
  auto bytes = ReadAssetFile(executable, MaxPayload + 128 * 1024 * 1024);
  if (bytes.size() < 32 ||
      std::memcmp(bytes.data() + bytes.size() - 32, "LNXPCK01", 8))
    return false;
  size_t footer = bytes.size() - 32;
  uint64_t offset = NumberAt(bytes, footer + 8, 8),
           length = NumberAt(bytes, footer + 16, 8);
  uint32_t checksum = uint32_t(NumberAt(bytes, footer + 24, 4));
  if (NumberAt(bytes, footer + 28, 4) != 1 || length < 8 ||
      length > MaxPayload || offset > footer || length != footer - offset)
    throw std::runtime_error("Unsupported or truncated game package");
  if (CRC(bytes.data() + size_t(offset), size_t(length)) != checksum)
    throw std::runtime_error("Game package checksum mismatch");
  uint64_t manifestSize = NumberAt(bytes, size_t(offset), 8);
  if (manifestSize > 4 * 1024 * 1024 || manifestSize > length - 8)
    throw std::runtime_error("Invalid game manifest length");
  auto manifest = nlohmann::json::parse(bytes.begin() + size_t(offset) + 8,
                                        bytes.begin() + size_t(offset) + 8 +
                                            size_t(manifestSize));
  if (!manifest.is_array() || manifest.size() > 256)
    throw std::runtime_error("Invalid game manifest");
  size_t base = size_t(offset) + 8 + size_t(manifestSize),
         available = footer - base;
  std::map<std::string, std::vector<unsigned char>> parsed;
  for (const auto &entry : manifest) {
    std::string name = entry.at("name").get<std::string>();
    if (name.empty() || name.size() > 511 ||
        name.find('\0') != std::string::npos || AbsolutePath(name) ||
        NormalizePath(name) != name || name == ".." ||
        name.find("../") != std::string::npos)
      throw std::runtime_error("Invalid embedded resource path");
    if (!entry.at("offset").is_number_unsigned() ||
        !entry.at("size").is_number_unsigned())
      throw std::runtime_error("Invalid embedded range");
    uint64_t start = entry.at("offset").get<uint64_t>(),
             size = entry.at("size").get<uint64_t>();
    if (start > available || size > available - start)
      throw std::runtime_error("Embedded resource exceeds payload");
    auto key = "/__lunoryx_bundle__/" + name;
    if (parsed.count(key))
      throw std::runtime_error("Duplicate embedded resource");
    parsed[key] = std::vector<unsigned char>(
        bytes.begin() + base + size_t(start),
        bytes.begin() + base + size_t(start) + size_t(size));
  }
  if (!parsed.count("/__lunoryx_bundle__/project.lunoryx"))
    throw std::runtime_error("Game project is missing");
  assets = std::move(parsed);
  return true;
}
inline void
Write(const std::string &runtime, const std::string &destination,
      const std::map<std::string, std::vector<unsigned char>> &assets) {
  auto binary = ReadAssetFile(runtime, 128 * 1024 * 1024);
  if (binary.size() > 32 &&
      !std::memcmp(binary.data() + binary.size() - 32, "LNXPCK01", 8))
    throw std::runtime_error(
        "Choose the editor executable as a runtime template");
  nlohmann::json manifest = nlohmann::json::array();
  size_t total = 0;
  for (const auto &resource : assets) {
    if (resource.second.size() > MaxPayload - total)
      throw std::runtime_error("Game assets exceed 512 MiB budget");
    manifest.push_back({{"name", resource.first},
                        {"offset", uint64_t(total)},
                        {"size", uint64_t(resource.second.size())}});
    total += resource.second.size();
  }
  std::string metadata = manifest.dump();
  std::vector<unsigned char> payload;
  Number(payload, metadata.size(), 8);
  payload.insert(payload.end(), metadata.begin(), metadata.end());
  for (const auto &resource : assets)
    payload.insert(payload.end(), resource.second.begin(),
                   resource.second.end());
  if (payload.size() > MaxPayload)
    throw std::runtime_error("Game package exceeds budget");
  uint64_t offset = binary.size();
  uint32_t checksum = CRC(payload.data(), payload.size());
  binary.insert(binary.end(), payload.begin(), payload.end());
  const char *magic = "LNXPCK01";
  binary.insert(binary.end(), magic, magic + 8);
  Number(binary, offset, 8);
  Number(binary, payload.size(), 8);
  Number(binary, checksum, 4);
  Number(binary, 1, 4);
  std::string temporary = destination + ".tmp";
  WriteAssetFile(temporary, binary.data(), binary.size());
#ifndef _WIN32
  if (chmod(temporary.c_str(), 0755))
    throw std::runtime_error("Cannot make game executable");
#endif
  ReplaceAssetFile(temporary, destination);
}
} // namespace GamePackage
static bool BuildStudioGame(const std::string &destination,
                            const std::string &runtime = "") {
  try {
    if (g_PhysicsRunning)
      throw std::runtime_error("Stop simulation before building");
    auto project = StudioProjectJson(
        g_OpenedProject.empty() ? std::string(g_ProjectPath) : g_OpenedProject);
    std::map<std::string, std::vector<unsigned char>> assets;
    for (size_t i = 0; i < g_Sounds.size(); ++i) {
      if (!g_Sounds[i].Data.empty())
        continue;
      std::string name = "assets/sound" + std::to_string(i);
      assets[name] = ReadAssetFile(g_Sounds[i].Path);
      project["sounds"][i]["path"] = name;
    }
    for (size_t i = 0; i < g_Objects.size(); ++i)
      for (size_t m = 0; m < g_Objects[i].Materials.size(); ++m) {
        const auto &material = g_Objects[i].Materials[m];
        if (!material.Texture.empty()) {
          std::string name =
              "assets/texture" + std::to_string(i) + "_" + std::to_string(m);
          assets[name] = ReadAssetFile(material.Texture, 16 * 1024 * 1024);
          project["objects"][i]["materials"][m]["texture"] = name;
        }
      }
    auto text = project.dump();
    if (text.size() > 32 * 1024 * 1024)
      throw std::runtime_error("Game project exceeds 32 MiB budget");
    assets["project.lunoryx"] =
        std::vector<unsigned char>(text.begin(), text.end());
    GamePackage::Write(runtime.empty() ? GamePackage::ExecutablePath()
                                       : runtime,
                       destination, assets);
    AppendConsole("Built standalone game: " + destination + "\n");
    return true;
  } catch (const std::exception &e) {
    AppendConsole(std::string("Game build failed: ") + e.what() + "\n");
    return false;
  }
}
