#pragma once
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
inline std::wstring WidePath(const std::string &s) {
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                              int(s.size()), nullptr, 0);
  if (!n)
    throw std::runtime_error("Invalid UTF-8 path");
  std::wstring w(size_t(n), L' ');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()),
                      &w[0], n);
  return w;
}
#else
#include <unistd.h>
#endif
inline FILE *AssetOpen(const std::string &path, const char *mode) {
  if (path.empty() || path.find('\0') != std::string::npos)
    return nullptr;
#ifdef _WIN32
  std::wstring m;
  for (const char *c = mode; *c; ++c)
    m += wchar_t(*c);
  return _wfopen(WidePath(path).c_str(), m.c_str());
#else
  return std::fopen(path.c_str(), mode);
#endif
}
inline std::string NormalizePath(std::string p) {
  std::replace(p.begin(), p.end(), '\\', '/');
  std::string prefix;
  size_t start = 0;
  if (p.size() > 1 && p[1] == ':') {
    prefix = p.substr(0, 2);
    start = 2;
  }
  bool rooted = start < p.size() && p[start] == '/';
  if (rooted) {
    prefix += '/';
    ++start;
  }
  std::vector<std::string> parts;
  while (start < p.size()) {
    size_t end = p.find('/', start);
    if (end == std::string::npos)
      end = p.size();
    auto part = p.substr(start, end - start);
    start = end + 1;
    if (part.empty() || part == ".")
      continue;
    if (part == ".." && !parts.empty() && parts.back() != "..")
      parts.pop_back();
    else if (part != ".." || !rooted)
      parts.push_back(part);
  }
  std::string out = prefix;
  for (const auto &part : parts) {
    if (!out.empty() && out.back() != '/')
      out += '/';
    out += part;
  }
  return out.empty() ? "." : out;
}
inline bool AbsolutePath(const std::string &p) {
  return !p.empty() &&
         (p[0] == '/' || p[0] == '\\' || (p.size() > 1 && p[1] == ':'));
}
inline std::string AbsoluteAssetPath(const std::string &p) {
  if (AbsolutePath(p))
    return NormalizePath(p);
#ifdef _WIN32
  wchar_t cwd[32768];
  if (!_wgetcwd(cwd, 32768))
    throw std::runtime_error("Cannot read working directory");
  int n =
      WideCharToMultiByte(CP_UTF8, 0, cwd, -1, nullptr, 0, nullptr, nullptr);
  std::string base(size_t(n), ' ');
  WideCharToMultiByte(CP_UTF8, 0, cwd, -1, &base[0], n, nullptr, nullptr);
  base.resize(size_t(n - 1));
#else
  char cwd[4096];
  if (!getcwd(cwd, sizeof(cwd)))
    throw std::runtime_error("Cannot read working directory");
  std::string base = cwd;
#endif
  return NormalizePath(base + "/" + p);
}
inline std::string PathDirectory(const std::string &p) {
  size_t i = p.find_last_of("/\\");
  return i == std::string::npos ? "." : p.substr(0, i);
}
inline std::string RelativeAssetPath(const std::string &asset,
                                     const std::string &project) {
  std::string a = AbsoluteAssetPath(asset),
              b = PathDirectory(AbsoluteAssetPath(project));
  if (a.size() > 1 && b.size() > 1 && a[1] == ':' &&
      a.substr(0, 2) != b.substr(0, 2))
    return a;
  size_t common = 0;
  while (common < a.size() && common < b.size() && a[common] == b[common])
    ++common;
  if (common == b.size() && common < a.size() && a[common] == '/')
    return a.substr(common + 1);
  while (common && a[common - 1] != '/')
    --common;
  std::string result;
  for (size_t k = common; k < b.size(); ++k)
    if (b[k] == '/')
      result += "../";
  if (common < b.size())
    result += "../";
  return result + a.substr(common);
}
inline std::vector<unsigned char>
ReadAssetFile(const std::string &path, size_t limit = 256 * 1024 * 1024) {
  FILE *f = AssetOpen(path, "rb");
  if (!f)
    throw std::runtime_error("Cannot open resource: " + path);
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    throw std::runtime_error("Cannot seek resource");
  }
  long size = std::ftell(f);
  if (size < 0 || size_t(size) > limit) {
    std::fclose(f);
    throw std::runtime_error("Resource exceeds size limit");
  }
  std::rewind(f);
  std::vector<unsigned char> data(size_t(size), 0);
  bool ok =
      data.empty() || std::fread(data.data(), 1, data.size(), f) == data.size();
  std::fclose(f);
  if (!ok)
    throw std::runtime_error("Cannot read resource");
  return data;
}
inline bool AssetExists(const std::string &path) {
  FILE *f = AssetOpen(path, "rb");
  if (!f)
    return false;
  std::fclose(f);
  return true;
}
inline void WriteAssetFile(const std::string &path, const void *data,
                           size_t n) {
  FILE *f = AssetOpen(path, "wb");
  if (!f)
    throw std::runtime_error("Cannot write file: " + path);
  bool ok = std::fwrite(data, 1, n, f) == n;
  if (std::fclose(f) != 0)
    ok = false;
  if (!ok)
    throw std::runtime_error("Cannot finish writing file: " + path);
}
inline void ReplaceAssetFile(const std::string &from, const std::string &to) {
#ifdef _WIN32
  if (!MoveFileExW(WidePath(from).c_str(), WidePath(to).c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error("Cannot replace file");
#else
  if (std::rename(from.c_str(), to.c_str()))
    throw std::runtime_error("Cannot replace file");
#endif
}
