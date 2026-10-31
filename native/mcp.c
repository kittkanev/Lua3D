/* MCP stdio transport. stdout is reserved exclusively for JSON-RPC messages. */
#include "../engine_version.h"
#include "cJSON.h"
#include "lunoryx.h"
#include "runtime.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
typedef struct {
  const char *name, *description, *schema;
} Tool;
static const Tool tools[] = {
    {"scene_list", "Read object IDs, types and local transforms",
     "{\"type\":\"object\",\"properties\":{}}"},
    {"scene_create",
     "Create a mesh primitive, character, terrain, attachment or sound",
     "{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},"
     "\"kind\":{\"type\":\"string\",\"enum\":[\"cube\",\"sphere\",\"cylinder\","
     "\"plane\",\"wedge\",\"terrain\",\"group\",\"attachment\",\"effect\","
     "\"character\",\"sound\"]}},\"required\":[\"name\",\"kind\"]}"},
    {"scene_transform",
     "Set position, rotation (degrees), or positive scale using stable ID",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"},"
     "\"position\":{\"type\":\"array\",\"items\":{\"type\":\"number\"},"
     "\"minItems\":3,\"maxItems\":3},\"rotation\":{\"type\":\"array\","
     "\"items\":{\"type\":\"number\"},\"minItems\":3,\"maxItems\":3},\"scale\":"
     "{\"type\":\"array\",\"items\":{\"type\":\"number\"},\"minItems\":3,"
     "\"maxItems\":3}},\"required\":[\"id\"]}"},
    {"scene_delete", "Delete an object and descendants",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\"}},"
     "\"required\":[\"id\"]}"},
    {"scene_boolean",
     "Apply actual BSP solid union, difference or intersection in ID order",
     "{\"type\":\"object\",\"properties\":{\"ids\":{\"type\":\"array\","
     "\"items\":{\"type\":\"integer\"},\"minItems\":2},\"operation\":{\"type\":"
     "\"string\",\"enum\":[\"union\",\"difference\",\"intersection\"]}},"
     "\"required\":[\"ids\",\"operation\"]}"},
    {"project_save", "Atomically save the current .lunoryx project",
     "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},"
     "\"required\":[\"path\"]}"},
    {"project_load",
     "Load and validate a project without executing its scripts",
     "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},"
     "\"required\":[\"path\"]}"},
    {"asset_import", "Import .obj, .gltf or .glb transactionally",
     "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},"
     "\"required\":[\"path\"]}"},
    {"asset_export", "Export the visible scene to GLB",
     "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},"
     "\"required\":[\"path\"]}"},
    {"script_execute",
     "Execute sandboxed Lua with a 1M instruction / 64MiB limit",
     "{\"type\":\"object\",\"properties\":{\"code\":{\"type\":\"string\"}},"
     "\"required\":[\"code\"]}"},
    {"simulation_step",
     "Advance fixed 120Hz physics, up to 10 seconds per call",
     "{\"type\":\"object\",\"properties\":{\"seconds\":{\"type\":\"number\","
     "\"minimum\":0,\"maximum\":10}},\"required\":[\"seconds\"]}"},
    {"scene_undo", "Undo the last successful mutation",
     "{\"type\":\"object\",\"properties\":{}}"},
    {"scene_redo", "Redo an undone mutation",
     "{\"type\":\"object\",\"properties\":{}}"}};
static cJSON *result_text(const char *text, bool error) {
  cJSON *o = cJSON_CreateObject(),
        *content = cJSON_AddArrayToObject(o, "content"),
        *item = cJSON_CreateObject();
  cJSON_AddStringToObject(item, "type", "text");
  cJSON_AddStringToObject(item, "text", text);
  cJSON_AddItemToArray(content, item);
  cJSON_AddBoolToObject(o, "isError", error);
  return o;
}
static const char *str(const cJSON *o, const char *key) {
  const cJSON *j = cJSON_GetObjectItemCaseSensitive(o, key);
  return cJSON_IsString(j) ? j->valuestring : NULL;
}
static uint32_t id_value(const cJSON *j) {
  return cJSON_IsNumber(j) && isfinite(j->valuedouble) && j->valuedouble > 0 &&
                 j->valuedouble < UINT32_MAX &&
                 floor(j->valuedouble) == j->valuedouble
             ? (uint32_t)j->valuedouble
             : 0;
}
static bool read_vec(const cJSON *o, const char *key, V3 *p) {
  const cJSON *a = cJSON_GetObjectItemCaseSensitive(o, key);
  if (!a)
    return true;
  if (!cJSON_IsArray(a) || cJSON_GetArraySize(a) != 3)
    return false;
  for (int k = 0; k < 3; k++) {
    const cJSON *v = cJSON_GetArrayItem(a, k);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) ||
        fabs(v->valuedouble) > 1e8)
      return false;
    ((float *)p)[k] = (float)v->valuedouble;
  }
  return true;
}
static cJSON *call_tool(Scene *s, Runtime *runtime, History *h,
                        const char *name, const cJSON *args) {
  char error[256] = "Invalid tool arguments";
  bool ok = false;
  const char *path = str(args, "path");
  if (!strcmp(name, "scene_list")) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; i < s->count; i++) {
      const Object *o = &s->objects[i];
      cJSON *v = cJSON_CreateObject();
      cJSON_AddNumberToObject(v, "id", o->id);
      cJSON_AddStringToObject(v, "name", o->name);
      cJSON_AddNumberToObject(v, "kind", o->kind);
      cJSON_AddNumberToObject(v, "parent", o->parent);
      cJSON_AddItemToObject(
          v, "position",
          cJSON_CreateFloatArray((const float *)&o->position, 3));
      cJSON_AddItemToObject(
          v, "rotation",
          cJSON_CreateFloatArray((const float *)&o->rotation, 3));
      cJSON_AddItemToObject(
          v, "scale", cJSON_CreateFloatArray((const float *)&o->scale, 3));
      cJSON_AddItemToArray(list, v);
    }
    char *text = cJSON_PrintUnformatted(list);
    cJSON_Delete(list);
    cJSON *r = result_text(text ? text : "[]", false);
    free(text);
    return r;
  }
  if (!strcmp(name, "scene_undo"))
    return result_text(history_undo(h, s) ? "Undone" : "No undo state", false);
  if (!strcmp(name, "scene_redo"))
    return result_text(history_redo(h, s) ? "Redone" : "No redo state", false);
  Scene backup = {0};
  if (!scene_copy(&backup, s))
    return result_text("Out of memory", true);
  if (!strcmp(name, "scene_create")) {
    const char *n = str(args, "name"), *kind = str(args, "kind");
    const char *names[] = {"cube",  "sphere",     "cylinder", "plane",
                           "wedge", "terrain",    "mesh",     "group",
                           "union", "attachment", "effect",   "character",
                           "sound"};
    int k = -1;
    if (kind)
      for (int i = 0; i < 13; i++)
        if (!strcmp(kind, names[i]))
          k = i;
    if (n && strlen(n) < 128 && k >= 0) {
      ok = k == LX_CHARACTER ? scene_character(s, 2, .8f)
                             : scene_add(s, (Kind)k, n) != NULL;
      if (ok && k == LX_CHARACTER)
        snprintf(s->objects[s->count - 1].name, 128, "%s", n);
    }
  } else if (!strcmp(name, "scene_transform")) {
    Object *o =
        scene_find(s, id_value(cJSON_GetObjectItemCaseSensitive(args, "id")));
    if (o) {
      ok = read_vec(args, "position", &o->position) &&
           read_vec(args, "rotation", &o->rotation) &&
           read_vec(args, "scale", &o->scale);
    }
  } else if (!strcmp(name, "scene_delete"))
    ok =
        scene_delete(s, id_value(cJSON_GetObjectItemCaseSensitive(args, "id")));
  else if (!strcmp(name, "scene_boolean")) {
    const cJSON *ids = cJSON_GetObjectItemCaseSensitive(args, "ids");
    const char *op = str(args, "operation");
    if (cJSON_IsArray(ids) && cJSON_GetArraySize(ids) >= 2 && op) {
      for (size_t i = 0; i < s->count; i++)
        s->objects[i].selected = false;
      const cJSON *j;
      ok = true;
      cJSON_ArrayForEach(j, ids) {
        Object *o = scene_find(s, id_value(j));
        if (!o) {
          ok = false;
          break;
        }
        o->selected = true;
      }
      int operation = !strcmp(op, "union")          ? 0
                      : !strcmp(op, "difference")   ? 1
                      : !strcmp(op, "intersection") ? 2
                                                    : -1;
      if (ok && operation >= 0)
        ok = scene_boolean(s, operation);
      else
        ok = false;
    }
  } else if (!strcmp(name, "project_save") && path)
    ok = scene_save(s, path, error);
  else if (!strcmp(name, "project_load") && path) {
    ok = scene_load(s, path, error);
    if (ok) {
      runtime_free(runtime);
      runtime_init(runtime, s, false);
    }
  } else if (!strcmp(name, "asset_import") && path)
    ok = scene_import(s, path, error);
  else if (!strcmp(name, "asset_export") && path)
    ok = scene_export_glb(s, path, error);
  else if (!strcmp(name, "script_execute")) {
    const char *code = str(args, "code");
    if (code && strlen(code) < LX_CODE_SIZE) {
      runtime->output[0] = 0;
      ok = runtime_command(runtime, code);
      if (!ok)
        snprintf(error, 256, "%.255s", runtime->output);
    }
  } else if (!strcmp(name, "simulation_step")) {
    const cJSON *sec = cJSON_GetObjectItemCaseSensitive(args, "seconds");
    if (cJSON_IsNumber(sec) && isfinite(sec->valuedouble) &&
        sec->valuedouble >= 0 && sec->valuedouble <= 10) {
      int frames = (int)ceil(sec->valuedouble * 120);
      for (int i = 0; i < frames; i++)
        scene_step(s, 1.0f / 120, i / 120.0f, v3(0, 0, 0), false);
      ok = true;
    }
  }
  if (ok)
    ok = scene_validate(s, error);
  if (ok) {
    history_capture(h, s);
    scene_free(&backup);
    return result_text(!strcmp(name, "script_execute") ? runtime->output : "OK",
                       false);
  }
  scene_free(s);
  *s = backup;
  return result_text(error, true);
}
static void send_response(const cJSON *id, cJSON *result, int code,
                          const char *message) {
  cJSON *response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "jsonrpc", "2.0");
  cJSON_AddItemToObject(response, "id",
                        id ? cJSON_Duplicate(id, 1) : cJSON_CreateNull());
  if (code) {
    cJSON *e = cJSON_AddObjectToObject(response, "error");
    cJSON_AddNumberToObject(e, "code", code);
    cJSON_AddStringToObject(e, "message", message);
  } else
    cJSON_AddItemToObject(response, "result", result);
  char *text = cJSON_PrintUnformatted(response);
  if (text) {
    fputs(text, stdout);
    fputc('\n', stdout);
    fflush(stdout);
  }
  free(text);
  cJSON_Delete(response);
}
int mcp_server(void) {
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  Scene scene;
  scene_init(&scene);
  Runtime runtime;
  History history = {0};
  if (!runtime_init(&runtime, &scene, false))
    return 1;
  history_capture(&history, &scene);
  char *line = malloc(1024 * 1024 + 1);
  if (!line) {
    runtime_free(&runtime);
    scene_free(&scene);
    history_free(&history);
    return 1;
  }
  while (fgets(line, 1024 * 1024 + 1, stdin)) {
    size_t n = strlen(line);
    if (n == 1024 * 1024 && line[n - 1] != '\n') {
      int ch;
      while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
      }
      send_response(NULL, NULL, -32700, "Request exceeds 1MiB");
      continue;
    }
    cJSON *req = cJSON_ParseWithOpts(line, NULL, 1);
    if (!cJSON_IsObject(req)) {
      send_response(NULL, NULL, -32700, "Parse error");
      cJSON_Delete(req);
      continue;
    }
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(req, "id"),
                *params = cJSON_GetObjectItemCaseSensitive(req, "params");
    const char *method = str(req, "method"), *version = str(req, "jsonrpc");
    if (!method || !version || strcmp(version, "2.0")) {
      send_response(id, NULL, -32600, "Invalid request");
      cJSON_Delete(req);
      continue;
    }
    if (!id) {
      cJSON_Delete(req);
      continue;
    }
    if (!strcmp(method, "initialize")) {
      cJSON *r = cJSON_CreateObject(),
            *caps = cJSON_AddObjectToObject(r, "capabilities");
      cJSON_AddObjectToObject(caps, "tools");
      cJSON_AddObjectToObject(caps, "resources");
      const char *pv = str(params, "protocolVersion");
      if (!pv || (strcmp(pv, "2024-11-05") && strcmp(pv, "2025-03-26") &&
                  strcmp(pv, "2025-06-18")))
        pv = "2025-06-18";
      cJSON_AddStringToObject(r, "protocolVersion", pv);
      cJSON *info = cJSON_AddObjectToObject(r, "serverInfo");
      cJSON_AddStringToObject(info, "name", "Lunoryx C Engine");
      cJSON_AddStringToObject(info, "version", LUNORYX_VERSION);
      send_response(id, r, 0, NULL);
    } else if (!strcmp(method, "ping"))
      send_response(id, cJSON_CreateObject(), 0, NULL);
    else if (!strcmp(method, "tools/list")) {
      cJSON *r = cJSON_CreateObject(),
            *list = cJSON_AddArrayToObject(r, "tools");
      for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        cJSON *t = cJSON_CreateObject();
        cJSON_AddStringToObject(t, "name", tools[i].name);
        cJSON_AddStringToObject(t, "description", tools[i].description);
        cJSON_AddItemToObject(t, "inputSchema", cJSON_Parse(tools[i].schema));
        cJSON_AddItemToArray(list, t);
      }
      send_response(id, r, 0, NULL);
    } else if (!strcmp(method, "tools/call")) {
      const char *name = str(params, "name");
      bool known = false;
      if (name)
        for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++)
          if (!strcmp(name, tools[i].name))
            known = true;
      if (!known)
        send_response(id, NULL, -32602, "Unknown tool");
      else
        send_response(
            id,
            call_tool(&scene, &runtime, &history, name,
                      cJSON_GetObjectItemCaseSensitive(params, "arguments")),
            0, NULL);
    } else if (!strcmp(method, "resources/list")) {
      cJSON *r = cJSON_CreateObject(),
            *list = cJSON_AddArrayToObject(r, "resources"),
            *item = cJSON_CreateObject();
      cJSON_AddStringToObject(item, "uri", "lunoryx://scene");
      cJSON_AddStringToObject(item, "name", "Current scene");
      cJSON_AddStringToObject(item, "mimeType", "application/json");
      cJSON_AddItemToArray(list, item);
      send_response(id, r, 0, NULL);
    } else if (!strcmp(method, "resources/read")) {
      const char *uri = str(params, "uri");
      if (!uri || strcmp(uri, "lunoryx://scene"))
        send_response(id, NULL, -32602, "Unknown resource");
      else {
        char *text = scene_json(&scene);
        cJSON *r = cJSON_CreateObject(),
              *list = cJSON_AddArrayToObject(r, "contents"),
              *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "uri", uri);
        cJSON_AddStringToObject(item, "mimeType", "application/json");
        cJSON_AddStringToObject(item, "text", text ? text : "{}");
        cJSON_AddItemToArray(list, item);
        free(text);
        send_response(id, r, 0, NULL);
      }
    } else
      send_response(id, NULL, -32601, "Method not found");
    cJSON_Delete(req);
  }
  free(line);
  runtime_free(&runtime);
  history_free(&history);
  scene_free(&scene);
  return 0;
}
