#include "runtime.h"
#include <lauxlib.h>
#include <lualib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static void *allocator(void *ud, void *ptr, size_t old, size_t n) {
  Runtime *r = ud;
  if (!ptr)
    old = 0;
  if (!n) {
    free(ptr);
    r->memory -= old;
    return NULL;
  }
  if (n > old && r->memory + n - old > 64 * 1024 * 1024)
    return NULL;
  void *p = realloc(ptr, n);
  if (p)
    r->memory = r->memory - old + n;
  return p;
}
static Runtime *host(lua_State *L) { return *(Runtime **)lua_getextraspace(L); }
void runtime_log(Runtime *r, const char *text) {
  size_t n = strlen(text), len = strlen(r->output);
  if (n >= sizeof(r->output))
    text += n - (sizeof(r->output) - 1), n = sizeof(r->output) - 1;
  if (len + n >= sizeof(r->output)) {
    size_t drop = len + n - sizeof(r->output) + 1;
    memmove(r->output, r->output + drop, len - drop + 1);
    len -= drop;
  }
  memcpy(r->output + len, text, n + 1);
}
static int print_value(lua_State *L) {
  luaL_tolstring(L, 1, NULL);
  return 1;
}
static int l_print(lua_State *L) {
  Runtime *r = host(L);
  int n = lua_gettop(L);
  for (int i = 1; i <= n; i++) {
    lua_pushcfunction(L, print_value);
    lua_pushvalue(L, i);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK)
      return lua_error(L);
    const char *v = lua_tostring(L, -1);
    runtime_log(r, v ? v : "");
    lua_pop(L, 1);
    if (i < n)
      runtime_log(r, "\t");
  }
  runtime_log(r, "\n");
  return 0;
}
static float finite_number(lua_State *L, int a) {
  double n = luaL_checknumber(L, a);
  if (!isfinite(n) || fabs(n) > 1e9)
    luaL_argerror(L, a, "finite number within +/-1e9 required");
  return (float)n;
}
static const char *string_arg(lua_State *L, int a, size_t limit) {
  size_t n;
  const char *s = luaL_checklstring(L, a, &n);
  if (n >= limit || strlen(s) != n)
    luaL_argerror(L, a, "string is too long or contains NUL");
  return s;
}
static Object *obj(lua_State *L) {
  lua_Integer id = luaL_checkinteger(L, 1);
  return id > 0 && id < UINT32_MAX ? scene_find(host(L)->scene, (uint32_t)id)
                                   : NULL;
}
static Kind kind(const char *s) {
  const char *names[] = {"cube",  "sphere",     "cylinder", "plane",
                         "wedge", "terrain",    "mesh",     "group",
                         "union", "attachment", "effect",   "character",
                         "sound", "pyramid"};
  for (int i = 0; i < 14; i++)
    if (!strcmp(s, names[i]))
      return (Kind)i;
  return LX_CUBE;
}
static int l_add(lua_State *L) {
  const char *name = string_arg(L, 1, 128),
             *type = luaL_optstring(L, 2, "cube");
  Object *o = scene_add(host(L)->scene, kind(type), name);
  lua_pushinteger(L, o ? o->id : 0);
  return 1;
}
static int l_remove(lua_State *L) {
  Object *o = obj(L);
  lua_pushboolean(L, o && scene_delete(host(L)->scene, o->id));
  return 1;
}
static int l_clear(lua_State *L) {
  Scene *s = host(L)->scene;
  while (s->count)
    scene_delete(s, s->objects[s->count - 1].id);
  return 0;
}
static int l_count(lua_State *L) {
  lua_pushinteger(L, (lua_Integer)host(L)->scene->count);
  return 1;
}
static int l_find(lua_State *L) {
  Scene *s = host(L)->scene;
  const char *name = string_arg(L, 1, 128);
  for (size_t i = 0; i < s->count; i++)
    if (!strcmp(name, s->objects[i].name)) {
      lua_pushinteger(L, s->objects[i].id);
      return 1;
    }
  lua_pushnil(L);
  return 1;
}
static int l_set_vector(lua_State *L) {
  Object *o = obj(L);
  int which = (int)lua_tointeger(L, lua_upvalueindex(1));
  V3 v = v3(finite_number(L, 2), finite_number(L, 3), finite_number(L, 4));
  bool ok = o != NULL;
  if (ok) {
    if (which == 0)
      o->position = v;
    else if (which == 1)
      o->rotation = v3(wrap_angle(v.x), wrap_angle(v.y), wrap_angle(v.z));
    else if (which == 2) {
      ok = v.x > 1e-5f && v.y > 1e-5f && v.z > 1e-5f;
      if (ok)
        o->scale = v;
    } else if (which == 3) {
      o->material.color[0] = fmaxf(0, fminf(v.x, 1));
      o->material.color[1] = fmaxf(0, fminf(v.y, 1));
      o->material.color[2] = fmaxf(0, fminf(v.z, 1));
    } else
      o->velocity = v;
  }
  lua_pushboolean(L, ok);
  return 1;
}
static int l_get_vector(lua_State *L) {
  Object *o = obj(L);
  if (!o) {
    lua_pushnil(L);
    return 1;
  }
  int which = (int)lua_tointeger(L, lua_upvalueindex(1));
  V3 v = which == 0   ? o->position
         : which == 1 ? o->rotation
         : which == 2 ? o->scale
         : which == 3 ? v3(o->material.color[0], o->material.color[1],
                           o->material.color[2])
                      : o->velocity;
  lua_pushnumber(L, v.x);
  lua_pushnumber(L, v.y);
  lua_pushnumber(L, v.z);
  return 3;
}
static int l_material(lua_State *L) {
  Object *o = obj(L);
  float m = finite_number(L, 2), r = finite_number(L, 3);
  if (o) {
    o->material.metallic = fmaxf(0, fminf(m, 1));
    o->material.roughness = fmaxf(0, fminf(r, 1));
  }
  lua_pushboolean(L, o != NULL);
  return 1;
}
static int l_get_material(lua_State *L) {
  Object *o = obj(L);
  if (!o) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushnumber(L, o->material.metallic);
  lua_pushnumber(L, o->material.roughness);
  return 2;
}
static int l_name(lua_State *L) {
  Object *o = obj(L);
  const char *name = string_arg(L, 2, 128);
  if (o)
    snprintf(o->name, sizeof(o->name), "%s", name);
  lua_pushboolean(L, o != NULL);
  return 1;
}
static int l_duplicate(lua_State *L) {
  Object *o = obj(L);
  Scene *s = host(L)->scene;
  uint32_t next = s->next_id;
  bool ok = o && scene_duplicate(s, o->id);
  if (ok && lua_gettop(L) > 1)
    snprintf(scene_find(s, next)->name, 128, "%s", string_arg(L, 2, 128));
  lua_pushinteger(L, ok ? next : 0);
  return 1;
}
static int l_flag(lua_State *L) {
  Object *o = obj(L);
  bool value = lua_toboolean(L, 2) != 0;
  int which = (int)lua_tointeger(L, lua_upvalueindex(1));
  if (o) {
    if (which == 0)
      o->visible = value;
    else if (which == 1)
      o->anchored = !value;
    else
      o->anchored = value;
  }
  lua_pushboolean(L, o != NULL);
  return 1;
}
static int l_property(lua_State *L) {
  Object *o = obj(L);
  const char *name = string_arg(L, 2, 64);
  if (!o) {
    lua_pushboolean(L, 0);
    return 1;
  }
  int i;
  for (i = 0; i < o->property_count; i++)
    if (!strcmp(o->properties[i].name, name))
      break;
  if (i == 32)
    return luaL_error(L, "property limit");
  Property p = {0};
  snprintf(p.name, sizeof(p.name), "%s", name);
  if (lua_type(L, 3) == LUA_TBOOLEAN) {
    p.type = 1;
    p.number = lua_toboolean(L, 3);
  } else if (lua_type(L, 3) == LUA_TNUMBER) {
    p.type = 0;
    p.number = finite_number(L, 3);
  } else {
    p.type = 2;
    snprintf(p.text, sizeof(p.text), "%s", string_arg(L, 3, 256));
  }
  o->properties[i] = p;
  if (i == o->property_count)
    o->property_count++;
  lua_pushboolean(L, 1);
  return 1;
}
static int l_get_property(lua_State *L) {
  Object *o = obj(L);
  const char *name = string_arg(L, 2, 64);
  if (o)
    for (int i = 0; i < o->property_count; i++)
      if (!strcmp(o->properties[i].name, name)) {
        Property *p = &o->properties[i];
        if (p->type == 0)
          lua_pushnumber(L, p->number);
        else if (p->type == 1)
          lua_pushboolean(L, p->number != 0);
        else
          lua_pushstring(L, p->text);
        return 1;
      }
  lua_pushnil(L);
  return 1;
}
static int l_parent(lua_State *L) {
  Object *o = obj(L);
  lua_Integer p = luaL_checkinteger(L, 2);
  lua_pushboolean(L, o && p >= 0 && p < UINT32_MAX &&
                         scene_reparent(host(L)->scene, o->id, (uint32_t)p));
  return 1;
}
static int l_texture(lua_State *L) {
  Object *o = obj(L);
  const char *p = string_arg(L, 2, 512);
  if (o)
    snprintf(o->material.texture, 512, "%s", p);
  lua_pushboolean(L, o != NULL);
  return 1;
}
static int l_character(lua_State *L) {
  Scene *s = host(L)->scene;
  uint32_t id = s->next_id;
  lua_pushinteger(L, scene_character(s, (float)luaL_optnumber(L, 1, 2),
                                     (float)luaL_optnumber(L, 2, .8f))
                         ? id
                         : 0);
  return 1;
}
static int l_export(lua_State *L) {
  char error[256];
  lua_pushboolean(
      L, scene_export_glb(host(L)->scene, string_arg(L, 1, 512), error));
  return 1;
}
static int l_sky(lua_State *L) {
  const char *p = string_arg(L, 1, 512);
  Scene *s = host(L)->scene;
  snprintf(s->sky[0], 512, "%s", p);
  s->sky_mode = 1;
  return 0;
}
static int l_set_sound(lua_State *L) {
  Object *o = obj(L);
  const char *path = string_arg(L, 2, 512);
  if (o) {
    snprintf(o->sound, 512, "%s", path);
    o->kind = LX_SOUND;
  }
  lua_pushboolean(L, o != NULL);
  return 1;
}
static int l_add_sound(lua_State *L) {
  const char *path = string_arg(L, 1, 512),
             *name = luaL_optstring(L, 2, "Sound");
  Object *o = scene_add(host(L)->scene, LX_SOUND, name);
  if (o)
    snprintf(o->sound, 512, "%s", path);
  lua_pushinteger(L, o ? o->id : 0);
  return 1;
}
static int l_sound(lua_State *L) {
  Object *o = obj(L);
  Runtime *r = host(L);
  if (o && r->sound)
    r->sound(r->sound_userdata, o->id,
             (int)lua_tointeger(L, lua_upvalueindex(1)));
  return 0;
}
static int l_gui(lua_State *L) {
  Gui *g = scene_gui(host(L)->scene, (int)luaL_optinteger(L, 1, 0));
  lua_pushinteger(L, g ? g->id : 0);
  return 1;
}
static int l_gui_text(lua_State *L) {
  Scene *s = host(L)->scene;
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  const char *text = string_arg(L, 2, 256);
  for (size_t i = 0; i < s->gui_count; i++)
    if (s->gui[i].id == id)
      snprintf(s->gui[i].text, 256, "%s", text);
  return 0;
}
static int l_plugin(lua_State *L) {
  Runtime *r = host(L);
  const char *name = string_arg(L, 1, 64);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  if (r->tool_count >= 32)
    return luaL_error(L, "plugin tool limit");
  int i = r->tool_count++;
  snprintf(r->tool_names[i], 64, "%s", name);
  lua_pushvalue(L, 2);
  r->tool_refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}
static void stack_trace(Runtime *r, lua_State *L) {
  r->stack[0] = 0;
  lua_Debug ar;
  for (int i = 0; i < 32 && lua_getstack(L, i, &ar); i++) {
    lua_getinfo(L, "nSl", &ar);
    char line[256];
    snprintf(line, sizeof(line), "#%d %.100s:%d  %.80s\n", i, ar.short_src,
             ar.currentline, ar.name ? ar.name : "<chunk>");
    if (strlen(r->stack) + strlen(line) < sizeof(r->stack))
      strcat(r->stack, line);
  }
}
static void hook(lua_State *L, lua_Debug *ar) {
  Runtime *r = host(L);
  if (ar->event == LUA_HOOKCOUNT) {
    r->budget -= 10000;
    if (r->budget <= 0) {
      r->exhausted = true;
      lua_yield(L, 0);
    }
    return;
  }
  if (ar->event != LUA_HOOKLINE)
    return;
  bool stop = r->step;
  int line = ar->currentline;
  if (r->active_script >= 0 &&
      (size_t)r->active_script < r->scene->script_count) {
    Script *s = &r->scene->scripts[r->active_script];
    for (int i = 0; i < s->break_count; i++)
      if (s->breakpoints[i] == line)
        stop = true;
  }
  if (line == r->skip_line) {
    r->skip_line = -1;
    return;
  }
  if (stop) {
    if (r->locals_ref != LUA_NOREF)
      luaL_unref(L, LUA_REGISTRYINDEX, r->locals_ref);
    lua_newtable(L);
    lua_Debug frame;
    if (lua_getstack(L, 0, &frame))
      for (int i = 1; i < 256; i++) {
        const char *name = lua_getlocal(L, &frame, i);
        if (!name)
          break;
        lua_setfield(L, -2, name);
      }
    r->locals_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    r->line = line;
    r->step = false;
    r->state = 2;
    stack_trace(r, L);
    lua_yield(L, 0);
  }
}
static void register_closure(lua_State *L, const char *name, lua_CFunction fn,
                             int up) {
  lua_pushinteger(L, up);
  lua_pushcclosure(L, fn, 1);
  lua_setglobal(L, name);
}
bool runtime_init(Runtime *r, Scene *s, bool trusted) {
  memset(r, 0, sizeof(*r));
  r->scene = s;
  r->thread_ref = LUA_NOREF;
  r->locals_ref = LUA_NOREF;
  r->active_script = -1;
  r->skip_line = -1;
  for (int i = 0; i < LX_MAX_SCRIPTS; i++)
    r->callbacks[i] = LUA_NOREF;
  r->L = lua_newstate(allocator, r);
  if (!r->L)
    return false;
  *(Runtime **)lua_getextraspace(r->L) = r;
  lua_State *L = r->L;
  if (trusted)
    luaL_openlibs(L);
  else {
    const struct {
      const char *name;
      lua_CFunction open;
    } libs[] = {
        {LUA_GNAME, luaopen_base},        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string}, {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},  {LUA_COLIBNAME, luaopen_coroutine}};
    for (size_t i = 0; i < sizeof(libs) / sizeof(libs[0]); i++) {
      luaL_requiref(L, libs[i].name, libs[i].open, 1);
      lua_pop(L, 1);
    }
    lua_pushnil(L);
    lua_setglobal(L, "dofile");
    lua_pushnil(L);
    lua_setglobal(L, "loadfile");
  }
  const luaL_Reg api[] = {{"print", l_print},
                          {"add_object", l_add},
                          {"remove_object", l_remove},
                          {"clear_scene", l_clear},
                          {"get_object_count", l_count},
                          {"find_object", l_find},
                          {"set_material", l_material},
                          {"get_material", l_get_material},
                          {"set_name", l_name},
                          {"duplicate_object", l_duplicate},
                          {"set_property", l_property},
                          {"get_property", l_get_property},
                          {"set_parent", l_parent},
                          {"set_texture", l_texture},
                          {"add_character", l_character},
                          {"add_sound", l_add_sound},
                          {"set_sound", l_set_sound},
                          {"set_skybox", l_sky},
                          {"add_gui", l_gui},
                          {"set_gui_text", l_gui_text},
                          {"register_tool", l_plugin},
                          {NULL, NULL}};
  for (int i = 0; api[i].name; i++) {
    lua_pushcfunction(L, api[i].func);
    lua_setglobal(L, api[i].name);
  }
  if (trusted) {
    lua_pushcfunction(L, l_export);
    lua_setglobal(L, "export_glb");
  }
  const char *set[] = {"set_position", "set_rotation", "set_scale", "set_color",
                       "set_velocity"},
             *get[] = {"get_position", "get_rotation", "get_scale", "get_color",
                       "get_velocity"};
  for (int i = 0; i < 5; i++) {
    register_closure(L, set[i], l_set_vector, i);
    register_closure(L, get[i], l_get_vector, i);
  }
  register_closure(L, "set_visible", l_flag, 0);
  register_closure(L, "set_dynamic", l_flag, 1);
  register_closure(L, "set_anchored", l_flag, 2);
  register_closure(L, "play_sound", l_sound, 0);
  register_closure(L, "stop_sound", l_sound, 1);
  return true;
}
void runtime_stop(Runtime *r) {
  if (r->thread_ref != LUA_NOREF && r->L)
    luaL_unref(r->L, LUA_REGISTRYINDEX, r->thread_ref);
  r->thread_ref = LUA_NOREF;
  if (r->locals_ref != LUA_NOREF && r->L)
    luaL_unref(r->L, LUA_REGISTRYINDEX, r->locals_ref);
  r->locals_ref = LUA_NOREF;
  r->thread = NULL;
  r->state = 0;
  r->line = 0;
}
void runtime_free(Runtime *r) {
  if (r->L)
    lua_close(r->L);
  memset(r, 0, sizeof(*r));
}
static bool resume_thread(Runtime *r, int nargs) {
  int n = 0;
  r->budget = 1000000;
  r->exhausted = false;
  clock_t start = clock();
  int status = lua_resume(r->thread, r->L, nargs, &n);
  if (r->active_script >= 0) {
    r->script_ms[r->active_script] +=
        (double)(clock() - start) * 1000 / CLOCKS_PER_SEC;
    r->calls[r->active_script]++;
  }
  if (r->exhausted) {
    runtime_log(r, "instruction budget exceeded\n");
    runtime_stop(r);
    return false;
  }
  if (status == LUA_YIELD) {
    r->state = 2;
    return true;
  }
  if (status != LUA_OK) {
    const char *err = lua_tostring(r->thread, -1);
    runtime_log(r, err ? err : "Lua error");
    runtime_log(r, "\n");
    stack_trace(r, r->thread);
    runtime_stop(r);
    return false;
  }
  if (r->loading_callback && r->active_script >= 0) {
    r->loading_callback = false;
    lua_pushglobaltable(r->L);
    lua_pushliteral(r->L, "on_update");
    lua_rawget(r->L, -2);
    lua_remove(r->L, -2);
    if (lua_isfunction(r->L, -1)) {
      if (r->callbacks[r->active_script] != LUA_NOREF)
        luaL_unref(r->L, LUA_REGISTRYINDEX, r->callbacks[r->active_script]);
      r->callbacks[r->active_script] = luaL_ref(r->L, LUA_REGISTRYINDEX);
    } else
      lua_pop(r->L, 1);
  }
  runtime_stop(r);
  return true;
}
static void new_thread(Runtime *r) {
  runtime_stop(r);
  r->thread = lua_newthread(r->L);
  r->thread_ref = luaL_ref(r->L, LUA_REGISTRYINDEX);
  *(Runtime **)lua_getextraspace(r->thread) = r;
  lua_sethook(r->thread, hook, LUA_MASKLINE | LUA_MASKCOUNT, 10000);
  r->state = 1;
}
bool runtime_run(Runtime *r, int script) {
  if (script < 0 || (size_t)script >= r->scene->script_count)
    return false;
  new_thread(r);
  r->active_script = script;
  r->loading_callback = true;
  lua_pushglobaltable(r->L);
  lua_pushliteral(r->L, "on_update");
  lua_pushnil(r->L);
  lua_rawset(r->L, -3);
  lua_pop(r->L, 1);
  Script *s = &r->scene->scripts[script];
  if (luaL_loadbuffer(r->thread, s->code, strlen(s->code), s->name) != LUA_OK) {
    runtime_log(r, lua_tostring(r->thread, -1));
    runtime_log(r, "\n");
    runtime_stop(r);
    return false;
  }
  return resume_thread(r, 0);
}
void runtime_continue(Runtime *r, bool step) {
  if (r->state != 2 || !r->thread)
    return;
  r->skip_line = r->line;
  r->step = step;
  r->state = 1;
  resume_thread(r, 0);
}
void runtime_update(Runtime *r, float dt) {
  if (r->state == 2)
    return;
  for (size_t i = 0; i < r->scene->script_count; i++)
    if (r->callbacks[i] != LUA_NOREF && r->scene->scripts[i].enabled) {
      new_thread(r);
      r->active_script = (int)i;
      r->loading_callback = false;
      lua_rawgeti(r->thread, LUA_REGISTRYINDEX, r->callbacks[i]);
      lua_pushnumber(r->thread, dt);
      if (!resume_thread(r, 1) || r->state == 2)
        break;
    }
}
bool runtime_command(Runtime *r, const char *code) {
  if (r->state == 2) {
    runtime_log(r,
                "Stop or continue the debugger before executing a command.\n");
    return false;
  }
  new_thread(r);
  r->active_script = -1;
  if (luaL_loadstring(r->thread, code) != LUA_OK) {
    runtime_log(r, lua_tostring(r->thread, -1));
    runtime_log(r, "\n");
    runtime_stop(r);
    return false;
  }
  return resume_thread(r, 0);
}
bool runtime_analyze(Runtime *r, int script) {
  if (script < 0 || (size_t)script >= r->scene->script_count)
    return false;
  int top = lua_gettop(r->L);
  Script *s = &r->scene->scripts[script];
  bool ok = luaL_loadbuffer(r->L, s->code, strlen(s->code), s->name) == LUA_OK;
  snprintf(r->analysis, sizeof(r->analysis), "%s",
           ok ? "Syntax OK" : lua_tostring(r->L, -1));
  lua_settop(r->L, top);
  return ok;
}
void runtime_watch(
    Runtime *r,
    const char *expression) { /* No evaluation of arbitrary functions in a
                                 paused stack: locals and globals only. */
  lua_State *L = r->state == 2 ? r->thread : r->L;
  int top = lua_gettop(L);
  bool found = false;
  if (r->state == 2 && r->locals_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, r->locals_ref);
    lua_pushstring(L, expression);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    found = !lua_isnil(L, -1);
    if (!found)
      lua_pop(L, 1);
  }
  if (!found) {
    lua_pushglobaltable(L);
    lua_pushstring(L, expression);
    lua_rawget(L, -2);
    lua_remove(L, -2);
  }
  int t = lua_type(L, -1);
  if (t == LUA_TNUMBER || t == LUA_TSTRING)
    snprintf(r->watch, sizeof(r->watch), "%s = %.1800s", expression,
             lua_tostring(L, -1));
  else if (t == LUA_TBOOLEAN)
    snprintf(r->watch, sizeof(r->watch), "%s = %s", expression,
             lua_toboolean(L, -1) ? "true" : "false");
  else
    snprintf(r->watch, sizeof(r->watch), "%s = <%s>", expression,
             lua_typename(L, t));
  lua_settop(L, top);
}
void runtime_tool(Runtime *r, int i) {
  if (i < 0 || i >= r->tool_count || r->state == 2)
    return;
  new_thread(r);
  r->active_script = -1;
  lua_rawgeti(r->thread, LUA_REGISTRYINDEX, r->tool_refs[i]);
  resume_thread(r, 0);
}
