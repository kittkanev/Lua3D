#pragma once
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
struct StudioMcpTool {
  const char *Name;
  const char *Description;
  const char *Schema;
};
static const StudioMcpTool kStudioMcpTools[] = {
    {"mesh_operation","Edit a mesh by stable object ID. Actions: modifier, apply, boolean, polygonize, extrude, weld, delete, normals. JSON options use the same modeling commands as Lua.",R"({"type":"object","properties":{"id":{"type":"integer"},"action":{"type":"string"},"options":{"type":"object"}},"required":["id","action"]})"},
    {"scene_list", "Read the v0.2 scene with stable IDs and legacy Lua indices",
     R"({"type":"object","properties":{}})"},
    {"add_object", "Add an object using the original engine",
     R"({"type":"object","properties":{"name":{"type":"string"},"type":{"type":"string","enum":["cube","sphere","pyramid","cylinder","plane"]}},"required":["name","type"]})"},
    {"set_transform",
     "Set local position, rotation in degrees or scale by stable object ID",
     R"({"type":"object","properties":{"id":{"type":"integer"},"position":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},"rotation":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},"scale":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3}},"required":["id"]})"},
    {"remove_object", "Remove an object by its stable ID",
     R"({"type":"object","properties":{"id":{"type":"integer"}},"required":["id"]})"},
    {"project_save", "Save a v0.2-compatible .lunoryx project including sounds",
     R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})"},
    {"project_load", "Load a project without running its scripts",
     R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})"},
    {"export_glb", "Export visible objects using the existing GLB exporter",
     R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})"},
    {"run_lua", "Run trusted Lua with the original engine API",
     R"({"type":"object","properties":{"code":{"type":"string"}},"required":["code"]})"},
    {"add_sound", "Add a sound asset reference without playing it",
     R"({"type":"object","properties":{"path":{"type":"string"},"name":{"type":"string"}},"required":["path"]})"},
    {"undo_objects", "Undo scene object edits",
     R"({"type":"object","properties":{}})"},
    {"redo_objects", "Redo scene object edits",
     R"({"type":"object","properties":{}})"}};
static nlohmann::json StudioMcpText(const std::string &text,
                                    bool error = false) {
  return {
      {"content", nlohmann::json::array({{{"type", "text"}, {"text", text}}})},
      {"isError", error}};
}
static nlohmann::json StudioMcpScene() {
  nlohmann::json list = nlohmann::json::array();
  for (size_t i = 0; i < g_Objects.size(); ++i) {
    const auto &o = g_Objects[i];
    list.push_back({{"id", o.Id},
                    {"index", i},
                    {"name", o.Name},
                    {"type", GetMeshTypeName(o.Type)},
                    {"position", SaveVector(o.Position)},
                    {"rotation", SaveVector(o.Rotation)},
                    {"scale", SaveVector(o.Scale)}});
  }
  return list;
}
static nlohmann::json StudioMcpCall(const std::string &tool,
                                    const nlohmann::json &args) {
  try {
    if(tool=="mesh_operation"){
      uint64_t id=args.at("id").get<uint64_t>();int index=-1;for(size_t i=0;i<g_Objects.size();++i)if(g_Objects[i].Id==id)index=int(i);
      int result=ModelCommand(g_Objects,index,args.at("action").get<std::string>(),args.value("options",nlohmann::json::object()));g_ObjectHistory.Capture(g_Objects);return StudioMcpText(nlohmann::json({{"index",result},{"id",g_Objects[size_t(result)].Id}}).dump());
    }
    if (tool == "scene_list")
      return StudioMcpText(StudioMcpScene().dump());
    if (tool == "undo_objects")
      return StudioMcpText(g_ObjectHistory.Undo(g_Objects) ? "Undone"
                                                           : "Nothing to undo");
    if (tool == "redo_objects")
      return StudioMcpText(g_ObjectHistory.Redo(g_Objects) ? "Redone"
                                                           : "Nothing to redo");
    if (tool == "add_object") {
      std::string name = args.at("name").get<std::string>(),
                  type = args.value("type", std::string("cube"));
      if (name.size() > 255 || name.find('\0') != std::string::npos ||
          g_Objects.size() >= 10000)
        throw std::runtime_error("Invalid name or object limit");
      g_Objects.emplace_back(name, ParseMeshType(type));
      g_ObjectHistory.Capture(g_Objects);
      return StudioMcpText(nlohmann::json({{"id", g_Objects.back().Id},
                                           {"index", g_Objects.size() - 1}})
                               .dump());
    }
    if (tool == "set_transform" || tool == "remove_object") {
      const auto &value = args.at("id");
      if (!value.is_number_integer() || value.get<double>() <= 0)
        throw std::runtime_error("Invalid stable ID");
      uint64_t id = value.get<uint64_t>();
      auto it = std::find_if(g_Objects.begin(), g_Objects.end(),
                             [&](const GameObject &o) { return o.Id == id; });
      if (it == g_Objects.end())
        throw std::runtime_error("Object not found");
      if (tool == "remove_object")
        g_Objects.erase(it);
      else {
        GameObject changed = *it;
        for (const char *key : {"position", "rotation", "scale"})
          if (args.contains(key)) {
            float *out = std::strcmp(key, "position") == 0   ? changed.Position
                         : std::strcmp(key, "rotation") == 0 ? changed.Rotation
                                                             : changed.Scale;
            ReadVector(args.at(key), out);
            if (std::strcmp(key, "scale") == 0)
              for (int k = 0; k < 3; ++k)
                if (out[k] < .001f || out[k] > 100000)
                  throw std::runtime_error("Scale outside allowed range");
          }
        *it = changed;
      }
      g_ObjectHistory.Capture(g_Objects);
      return StudioMcpText("OK");
    }
    if (tool == "project_save" || tool == "project_load" ||
        tool == "export_glb") {
      std::string path = args.at("path").get<std::string>();
      if (path.empty() || path.size() > 511 ||
          path.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid path");
      bool ok = tool == "project_save"   ? SaveStudioProject(path)
                : tool == "project_load" ? LoadStudioProject(path)
                                         : ExportGLB(path);
      return StudioMcpText(ok ? "OK" : g_ConsoleOutput, !ok);
    }
    if (tool == "run_lua") {
      std::string code = args.at("code").get<std::string>();
      if (code.size() > 65535 || code.find('\0') != std::string::npos)
        throw std::runtime_error("Code exceeds limit");
      LuaStackGuard stack(g_LuaState);
      LuaExecutionGuard budget(g_LuaState);
      if (luaL_loadbuffer(g_LuaState, code.data(), code.size(), "MCP") !=
              LUA_OK ||
          lua_pcall(g_LuaState, 0, 0, 0) != LUA_OK)
        return StudioMcpText(LuaErrorText(g_LuaState), true);
      g_ObjectHistory.Capture(g_Objects);
      return StudioMcpText(g_ConsoleOutput);
    }
    if (tool == "add_sound") {
      StudioSound sound;
      sound.Path = args.at("path").get<std::string>();
      sound.Name = args.value("name", std::string("Sound"));
      if (sound.Path.empty() || sound.Path.size() > 511 ||
          sound.Name.size() > 255 ||
          sound.Path.find('\0') != std::string::npos ||
          sound.Name.find('\0') != std::string::npos || g_Sounds.size() >= 64)
        throw std::runtime_error("Invalid sound reference");
      g_Sounds.push_back(sound);
      g_ObjectHistory.Capture(g_Objects);
      return StudioMcpText("OK");
    }
    return StudioMcpText("Unknown tool", true);
  } catch (const std::exception &e) {
    return StudioMcpText(e.what(), true);
  }
}
static int RunStudioMcp() {
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  InitializeStudio();
  InitLuaState();
  g_ObjectHistory.Capture(g_Objects);
  auto reply = [](const nlohmann::json &id, const nlohmann::json &result,
                  int code = 0, const std::string &message = "") {
    nlohmann::json response = {{"jsonrpc", "2.0"}, {"id", id}};
    if (code)
      response["error"] = {{"code", code}, {"message", message}};
    else
      response["result"] = result;
    std::cout << response.dump() << '\n' << std::flush;
  };
  std::vector<char> line(1024 * 1024 + 1);
  while (std::fgets(line.data(), int(line.size()), stdin)) {
    size_t length = std::strlen(line.data());
    if (length == line.size() - 1 && line[length - 1] != '\n') {
      int c;
      while ((c = std::fgetc(stdin)) != EOF && c != '\n') {
      }
      reply(nullptr, nullptr, -32700, "Request exceeds 1MiB");
      continue;
    }
    nlohmann::json request;
    try {
      request = nlohmann::json::parse(line.data());
    } catch (...) {
      reply(nullptr, nullptr, -32700, "Invalid JSON");
      continue;
    }
    nlohmann::json id = request.is_object() && request.contains("id")
                            ? request["id"]
                            : nlohmann::json(nullptr);
    try {
      if (!request.is_object() ||
          request.value("jsonrpc", std::string()) != "2.0" ||
          !request.contains("method")) {
        reply(id, nullptr, -32600, "Invalid request");
        continue;
      }
      if (!request.contains("id"))
        continue;
      std::string method = request.at("method").get<std::string>();
      auto params = request.value("params", nlohmann::json::object());
      if (method == "initialize") {
        std::string version =
            params.value("protocolVersion", std::string("2025-06-18"));
        if (version != "2024-11-05" && version != "2025-03-26" &&
            version != "2025-06-18")
          version = "2025-06-18";
        reply(id,
              {{"protocolVersion", version},
               {"capabilities",
                {{"tools", nlohmann::json::object()},
                 {"resources", nlohmann::json::object()}}},
               {"serverInfo",
                {{"name", "Lunoryx Studio"}, {"version", LUNORYX_VERSION}}}});
      } else if (method == "ping")
        reply(id, nlohmann::json::object());
      else if (method == "tools/list") {
        nlohmann::json tools = nlohmann::json::array();
        for (const auto &t : kStudioMcpTools)
          tools.push_back({{"name", t.Name},
                           {"description", t.Description},
                           {"inputSchema", nlohmann::json::parse(t.Schema)}});
        reply(id, {{"tools", tools}});
      } else if (method == "tools/call")
        reply(id, StudioMcpCall(
                      params.at("name").get<std::string>(),
                      params.value("arguments", nlohmann::json::object())));
      else if (method == "resources/list")
        reply(id, {{"resources", nlohmann::json::array(
                                     {{{"uri", "lunoryx://scene"},
                                       {"name", "Scene"},
                                       {"mimeType", "application/json"}}})}});
      else if (method == "resources/read") {
        if (params.at("uri") != "lunoryx://scene")
          reply(id, nullptr, -32602, "Unknown resource");
        else
          reply(id, {{"contents", nlohmann::json::array(
                                      {{{"uri", "lunoryx://scene"},
                                        {"mimeType", "application/json"},
                                        {"text", StudioMcpScene().dump()}}})}});
      } else
        reply(id, nullptr, -32601, "Unknown method");
    } catch (const std::exception &e) {
      reply(id, nullptr, -32602, e.what());
    }
  }
  ShutdownLuaState();
  studio_audio_shutdown();
  return 0;
}
