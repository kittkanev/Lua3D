#include <SDL.h>
#include <SDL_opengl.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// tinygltf requires json
#include <tiny_gltf.h>

#include "scene_tools.h"
#include "file_output.h"
#include "asset_io.h"
#include "engine_version.h"
#include "editor_transform.h"
#include "extensions/ImGuizmo/ImGuizmo.h"
#include <limits>
#include <exception>
#ifdef _WIN32
#include <windows.h>
#endif
static const size_t MAX_CONSOLE_LINES = 500;

// Global state
std::vector<GameObject> g_Objects;
int g_SelectedObjectIndex = -1;
lua_State* g_LuaState = nullptr;
bool g_SimulationRunning = true;
bool g_ShowGrid = true;
bool g_ShowAxes = true;
bool g_GlobalWireframe = false;
bool g_AutoScrollConsole = true;
bool g_ConsoleScrollToBottom = false;
char g_OutlinerFilter[128] = "";

std::vector<std::string> g_ConsoleLines;
std::string g_ConsoleOutput = "";

bool g_PhysicsRunning=false;
PhysicsState g_Physics;
std::vector<GameObject> g_PlaySnapshot;
int g_SkyPreset=0;
float g_SkyTop[3]={.12f,.31f,.58f},g_SkyBottom[3]={.64f,.8f,.94f};
static void ApplySkyPreset(int preset) {
    static const float top[][3]={{.12f,.31f,.58f},{.19f,.08f,.3f},{.005f,.012f,.04f}};
    static const float bottom[][3]={{.64f,.8f,.94f},{1,.38f,.16f},{.06f,.11f,.22f}};
    if(preset<0||preset>2)return;
    g_SkyPreset=preset;std::copy(top[preset],top[preset]+3,g_SkyTop);std::copy(bottom[preset],bottom[preset]+3,g_SkyBottom);
}
static void AppendConsole(const std::string& msg);
#include "studio_sounds.h"
#include "editor_history.h"
static float g_PlayTop[3],g_PlayBottom[3];
static int g_PlaySky=0;
static PhysicsState g_PlayPhysics;

static void StartSimulation() {
    if(!g_PhysicsRunning){g_PlaySnapshot=g_Objects;g_PlaySoundSnapshot=g_Sounds;std::copy(g_SkyTop,g_SkyTop+3,g_PlayTop);std::copy(g_SkyBottom,g_SkyBottom+3,g_PlayBottom);g_PlaySky=g_SkyPreset;g_PlayPhysics=g_Physics;}
    for(const auto& sound:g_Sounds)if(sound.Autoplay)PlayStudioSound(sound);
    g_Physics.Accumulator=0;g_PhysicsRunning=true;g_SimulationRunning=true;
}
static void StopSimulation() {
    if(g_PhysicsRunning){std::copy(g_PlayTop,g_PlayTop+3,g_SkyTop);std::copy(g_PlayBottom,g_PlayBottom+3,g_SkyBottom);g_SkyPreset=g_PlaySky;g_Physics=g_PlayPhysics;g_Objects=g_PlaySnapshot;for(const auto& sound:g_Sounds)studio_audio_unload(sound.Id);g_Sounds=g_PlaySoundSnapshot;g_SelectedSound=-1;}
    g_PhysicsRunning=false;g_SimulationRunning=false;g_Physics.Accumulator=0;g_SelectedObjectIndex=-1;
    studio_audio_stop_all();
}

char g_ScriptBuffer[1024 * 16] =
    "-- Lunoryx: Interactive Demo Script\n"
    "print('Initializing procedural scene...', true, nil)\n"
    "clear_scene()\n\n"
    "local sun = add_object('Sun', 'sphere')\n"
    "set_position(sun, 0, 1.5, 0)\n"
    "set_scale(sun, 1.4, 1.4, 1.4)\n"
    "set_color(sun, 1.0, 0.8, 0.2)\n\n"
    "local planet = add_object('Orbiting Planet', 'cube')\n"
    "set_scale(planet, 0.7, 0.7, 0.7)\n"
    "set_color(planet, 0.2, 0.6, 1.0)\n\n"
    "local moon = add_object('Moon Pyramid', 'pyramid')\n"
    "set_scale(moon, 0.4, 0.4, 0.4)\n"
    "set_color(moon, 0.9, 0.3, 0.5)\n\n"
    "local angle = 0\n"
    "function on_update(dt)\n"
    "    angle = angle + dt * 1.5\n"
    "    local px = math.cos(angle) * 4.0\n"
    "    local pz = math.sin(angle) * 4.0\n"
    "    set_position(planet, px, 1.5, pz)\n"
    "    set_rotation(planet, angle * 30, angle * 60, 0)\n\n"
    "    local mx = px + math.cos(angle * 3.0) * 1.3\n"
    "    local mz = pz + math.sin(angle * 3.0) * 1.3\n"
    "    set_position(moon, mx, 1.5 + math.sin(angle * 2.0) * 0.4, mz)\n"
    "    set_rotation(moon, 0, -angle * 120, 0)\n"
    "end\n";

// Forward declarations
static bool ExportGLB(const std::string& filename);
static void InitLuaState();
static void ShutdownLuaState();

// Bounded console output (fixes Bug #7: prevents unbounded growth & splits lines for ImGuiListClipper)
static void AppendConsole(const std::string& msg) {
    size_t start = 0;
    while (start < msg.size()) {
        size_t pos = msg.find('\n', start);
        std::string line = (pos == std::string::npos)
            ? msg.substr(start)
            : msg.substr(start, pos - start);
        if (!line.empty() || pos != std::string::npos) {
            g_ConsoleLines.push_back(line);
            if (g_ConsoleLines.size() > MAX_CONSOLE_LINES) {
                g_ConsoleLines.erase(g_ConsoleLines.begin(),
                                     g_ConsoleLines.begin() + (g_ConsoleLines.size() - MAX_CONSOLE_LINES));
            }
        }
        if (pos == std::string::npos) break;
        start = pos + 1;
    }

    g_ConsoleOutput += msg;
    if (g_ConsoleOutput.size() > 64 * 1024) {
        g_ConsoleOutput.erase(0, g_ConsoleOutput.size() - 48 * 1024);
    }
    g_ConsoleScrollToBottom = true;
}

#include "editor_preferences.h"
static StudioHistory g_ObjectHistory;

static void ClearConsole() {
    g_ConsoleLines.clear();
    g_ConsoleOutput.clear();
}

// The conversion may call a user-defined __tostring and must run protected.
static int LuaToString(lua_State* L) {
    luaL_tolstring(L, 1, nullptr);
    return 1;
}

static int l_print(lua_State* L) {
    const int count = lua_gettop(L);
    bool conversionFailed = false;
    {
        std::string line;
        for (int i = 1; i <= count; ++i) {
            lua_pushcfunction(L, LuaToString);
            lua_pushvalue(L, i);
            if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
                conversionFailed = true;
                break;
            }
            size_t length = 0;
            const char* value = lua_tolstring(L, -1, &length);
            line.append(value, length);
            lua_pop(L, 1);
            if (i < count) line += "\t";
        }
        if (!conversionFailed) AppendConsole(line + "\n");
    }
    // Destroy C++ objects before Lua propagates the error via longjmp.
    if (conversionFailed) return lua_error(L);
    return 0;
}

// Used only at the C++ host boundary, where Lua errors are handled by pcall.
class LuaStackGuard {
public:
    explicit LuaStackGuard(lua_State* state) : state_(state), top_(lua_gettop(state)) {}
    ~LuaStackGuard() { lua_settop(state_, top_); }
    LuaStackGuard(const LuaStackGuard&) = delete;
    LuaStackGuard& operator=(const LuaStackGuard&) = delete;
private:
    lua_State* state_;
    int top_;
};

// Hooks bound accidental infinite loops in trusted editor scripts.
static void LuaInstructionLimit(lua_State* L, lua_Debug*) {
    luaL_error(L, "Lua instruction budget exceeded");
}
class LuaExecutionGuard {
public:
    explicit LuaExecutionGuard(lua_State* L)
        : state_(L), previous_(lua_gethook(L)), mask_(lua_gethookmask(L)), count_(lua_gethookcount(L)) {
        lua_sethook(L, LuaInstructionLimit, LUA_MASKCOUNT, 1000000);
    }
    ~LuaExecutionGuard() { lua_sethook(state_, previous_, mask_, count_); }
    LuaExecutionGuard(const LuaExecutionGuard&) = delete;
    LuaExecutionGuard& operator=(const LuaExecutionGuard&) = delete;
private:
    lua_State* state_;
    lua_Hook previous_;
    int mask_;
    int count_;
};

// C++ exceptions must not unwind through the Lua C runtime.
template<int (*Function)(lua_State*)>
static int LuaBinding(lua_State* L) {
    char message[512] = {};
    try {
        return Function(L);
    } catch (const std::exception& error) {
        std::snprintf(message, sizeof(message), "Native API error: %s", error.what());
    } catch (...) {
        std::snprintf(message, sizeof(message), "Native API error: unknown exception");
    }
    // The exception object has been destroyed before Lua may longjmp.
    lua_pushstring(L, message);
    return lua_error(L);
}

static void ClearUpdateCallback(lua_State* L) {
    lua_pushglobaltable(L);
    lua_pushliteral(L, "on_update");
    lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

static void PushUpdateCallback(lua_State* L) {
    lua_pushglobaltable(L);
    lua_pushliteral(L, "on_update");
    lua_rawget(L, -2);
    lua_remove(L, -2);
}

static float CheckFiniteNumber(lua_State* L, int argument) {
    const lua_Number value = luaL_checknumber(L, argument);
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
        luaL_argerror(L, argument, "expected a finite float");
    return static_cast<float>(value);
}

// Never invoke an error object's __tostring metamethod outside a protected call.
static std::string LuaErrorText(lua_State* L) {
    const char* message = lua_tostring(L, -1);
    return message ? message : luaL_typename(L, -1);
}

static const char* CheckObjectName(lua_State* L,int argument) {
    size_t length=0;const char* name=luaL_checklstring(L,argument,&length);
    if(length>255||std::memchr(name,'\0',length))luaL_argerror(L,argument,"name must be at most 255 bytes, without embedded NUL");
    return name;
}

// Lua: add_object(name, [type]) -> index (0-based)
static int l_add_object(lua_State *L) {
    const char* name = CheckObjectName(L, 1);
    MeshType type = MeshType::Cube;
    if (lua_gettop(L) >= 2 && lua_isstring(L, 2)) {
        type = ParseMeshType(lua_tostring(L, 2));
    }
    g_Objects.emplace_back(name, type);
    int idx = (int)g_Objects.size() - 1;
    AppendConsole("Added " + std::string(GetMeshTypeName(type)) + " '" + name + "' (index " + std::to_string(idx) + ")\n");
    lua_pushinteger(L, idx);
    return 1;
}

// Lua: remove_object(index) -> bool
static int l_remove_object(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects.erase(g_Objects.begin() + index);
        if (g_SelectedObjectIndex == index) {
            g_SelectedObjectIndex = -1;
        } else if (g_SelectedObjectIndex > index) {
            g_SelectedObjectIndex--;
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    AppendConsole("Error: Object index " + std::to_string(index) + " out of bounds in remove_object\n");
    lua_pushboolean(L, 0);
    return 1;
}

// Lua: clear_scene()
static int l_clear_scene(lua_State *L) {
    (void)L;
    g_Objects.clear();
    g_SelectedObjectIndex = -1;
    return 0;
}

// Lua: find_object(name) -> index or -1
static int l_find_object(lua_State *L) {
    const char* name = CheckObjectName(L, 1);
    for (size_t i = 0; i < g_Objects.size(); ++i) {
        if (g_Objects[i].Name == name) {
            lua_pushinteger(L, (lua_Integer)i);
            return 1;
        }
    }
    lua_pushinteger(L, -1);
    return 1;
}

// Lua: set_position(index, x, y, z) - 0-based index (Fixes Bug #6: reports out-of-bounds index)
static int l_set_position(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    float x = CheckFiniteNumber(L, 2);
    float y = CheckFiniteNumber(L, 3);
    float z = CheckFiniteNumber(L, 4);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Position[0] = x;
        g_Objects[index].Position[1] = y;
        g_Objects[index].Position[2] = z;
        lua_pushboolean(L, 1);
    } else {
        AppendConsole("Error: Object index " + std::to_string(index) +
                      " out of bounds in set_position (object count: " +
                      std::to_string(g_Objects.size()) + ")\n");
        lua_pushboolean(L, 0);
    }
    return 1;
}

// Lua: get_position(index) -> x, y, z
static int l_get_position(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        lua_pushnumber(L, g_Objects[index].Position[0]);
        lua_pushnumber(L, g_Objects[index].Position[1]);
        lua_pushnumber(L, g_Objects[index].Position[2]);
        return 3;
    }
    AppendConsole("Error: Object index " + std::to_string(index) + " out of bounds in get_position\n");
    return 0;
}

// Lua: set_rotation(index, rx_deg, ry_deg, rz_deg)
static int l_set_rotation(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    float rx = CheckFiniteNumber(L, 2);
    float ry = CheckFiniteNumber(L, 3);
    float rz = CheckFiniteNumber(L, 4);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Rotation[0] = rx;
        g_Objects[index].Rotation[1] = ry;
        g_Objects[index].Rotation[2] = rz;
        lua_pushboolean(L, 1);
    } else {
        AppendConsole("Error: Object index " + std::to_string(index) + " out of bounds in set_rotation\n");
        lua_pushboolean(L, 0);
    }
    return 1;
}

// Lua: get_rotation(index) -> rx, ry, rz
static int l_get_rotation(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        lua_pushnumber(L, g_Objects[index].Rotation[0]);
        lua_pushnumber(L, g_Objects[index].Rotation[1]);
        lua_pushnumber(L, g_Objects[index].Rotation[2]);
        return 3;
    }
    AppendConsole("Error: Object index " + std::to_string(index) + " out of bounds in get_rotation\n");
    return 0;
}

// Lua: set_scale(index, sx, sy, sz)
static int l_set_scale(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    float sx = CheckFiniteNumber(L, 2);
    float sy = CheckFiniteNumber(L, 3);
    float sz = CheckFiniteNumber(L, 4);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Scale[0] = sx;
        g_Objects[index].Scale[1] = sy;
        g_Objects[index].Scale[2] = sz;
        lua_pushboolean(L, 1);
    } else {
        AppendConsole("Error: Object index " + std::to_string(index) + " out of bounds in set_scale\n");
        lua_pushboolean(L, 0);
    }
    return 1;
}

// Lua: get_scale(index) -> sx, sy, sz
static int l_get_scale(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        lua_pushnumber(L, g_Objects[index].Scale[0]);
        lua_pushnumber(L, g_Objects[index].Scale[1]);
        lua_pushnumber(L, g_Objects[index].Scale[2]);
        return 3;
    }
    AppendConsole("Error: Object index " + std::to_string(index) + " out of bounds in get_scale\n");
    return 0;
}

// Lua: set_color(index, r, g, b)
static int l_set_color(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    float r = CheckFiniteNumber(L, 2);
    float g = CheckFiniteNumber(L, 3);
    float b = CheckFiniteNumber(L, 4);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Color[0] = std::max(0.0f, std::min(1.0f, r));
        g_Objects[index].Color[1] = std::max(0.0f, std::min(1.0f, g));
        g_Objects[index].Color[2] = std::max(0.0f, std::min(1.0f, b));
        lua_pushboolean(L, 1);
    } else {
        AppendConsole("Error: Object index " + std::to_string(index) + " out of bounds in set_color\n");
        lua_pushboolean(L, 0);
    }
    return 1;
}

// Lua: get_color(index) -> r, g, b
static int l_get_color(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        lua_pushnumber(L, g_Objects[index].Color[0]);
        lua_pushnumber(L, g_Objects[index].Color[1]);
        lua_pushnumber(L, g_Objects[index].Color[2]);
        return 3;
    }
    AppendConsole("Error: Object index " + std::to_string(index) + " out of bounds in get_color\n");
    return 0;
}

// Lua: set_visible(index, visible)
static int l_set_visible(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Visible = lua_toboolean(L, 2) != 0;
        lua_pushboolean(L, 1);
    } else {
        AppendConsole("Error: Object index " + std::to_string(index) + " out of bounds in set_visible\n");
        lua_pushboolean(L, 0);
    }
    return 1;
}

// Lua: set_wireframe(index, wireframe)
static int l_set_wireframe(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Wireframe = lua_toboolean(L, 2) != 0;
        lua_pushboolean(L, 1);
    } else {
        AppendConsole("Error: Object index " + std::to_string(index) + " out of bounds in set_wireframe\n");
        lua_pushboolean(L, 0);
    }
    return 1;
}

// Lua: get_object_count() -> int
static int l_get_object_count(lua_State *L) {
    lua_pushinteger(L, (lua_Integer)g_Objects.size());
    return 1;
}

// Lua: get_time() -> seconds since SDL init
static int l_get_time(lua_State *L) {
    lua_pushnumber(L, (double)SDL_GetTicks() / 1000.0);
    return 1;
}

// Lua: export_glb(filename)
static int l_export_glb(lua_State *L) {
    size_t filenameLength=0;
    const char* filename = luaL_checklstring(L, 1, &filenameLength);
    bool res = ExportGLB(std::string(filename,filenameLength));
    if (res) {
        AppendConsole(std::string("Successfully exported binary GLB to ") + filename + "\n");
    } else {
        AppendConsole(std::string("Failed to export GLB to ") + filename + "\n");
    }
    lua_pushboolean(L, res ? 1 : 0);
    return 1;
}

// Lua: duplicate_object(index [, name]) -> new index, or -1.
static int l_duplicate_object(lua_State* L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    const char* name = luaL_optstring(L, 2, nullptr);
    if (index < 0 || index >= static_cast<lua_Integer>(g_Objects.size())) {
        lua_pushinteger(L, -1);
        return 1;
    }
    lua_pushinteger(L, DuplicateObject(g_Objects, static_cast<int>(index),
        name ? std::string(name) : g_Objects[index].Name + " Copy"));
    return 1;
}

static int l_set_name(lua_State* L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    const char* name = CheckObjectName(L, 2);
    bool valid = index >= 0 && index < static_cast<lua_Integer>(g_Objects.size());
    if (valid) g_Objects[index].Name = name;
    lua_pushboolean(L, valid);
    return 1;
}

static int l_set_material(lua_State* L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    float metallic = CheckFiniteNumber(L, 2);
    float roughness = CheckFiniteNumber(L, 3);
    bool valid = index >= 0 && index < static_cast<lua_Integer>(g_Objects.size());
    if (valid) {
        g_Objects[index].Metallic = std::max(0.0f, std::min(1.0f, metallic));
        g_Objects[index].Roughness = std::max(0.0f, std::min(1.0f, roughness));
    }
    lua_pushboolean(L, valid);
    return 1;
}

static int l_get_material(lua_State* L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index < 0 || index >= static_cast<lua_Integer>(g_Objects.size())) return 0;
    lua_pushnumber(L, g_Objects[index].Metallic);
    lua_pushnumber(L, g_Objects[index].Roughness);
    return 2;
}

#include "scene_bindings.h"

static void InitLuaState() {
    ShutdownLuaState();
    g_LuaState = luaL_newstate();
    if (!g_LuaState) throw std::bad_alloc();
    luaL_openlibs(g_LuaState);
    lua_register(g_LuaState, "make_editable", LuaBinding<l_make_editable>);
    lua_register(g_LuaState, "sculpt", LuaBinding<l_sculpt>);
    lua_register(g_LuaState, "merge_objects", LuaBinding<l_merge_objects>);
    lua_register(g_LuaState, "set_physics", LuaBinding<l_set_physics>);
    lua_register(g_LuaState, "set_velocity", LuaBinding<l_set_velocity>);
    lua_register(g_LuaState, "get_velocity", LuaBinding<l_get_velocity>);
    lua_register(g_LuaState, "set_character", LuaBinding<l_set_character>);
    lua_register(g_LuaState, "add_character", LuaBinding<l_add_character>);
    lua_register(g_LuaState, "set_property", LuaBinding<l_set_property>);
    lua_register(g_LuaState, "get_property", LuaBinding<l_get_property>);
    lua_register(g_LuaState, "set_skybox", LuaBinding<l_set_skybox>);
    lua_register(g_LuaState, "duplicate_object", LuaBinding<l_duplicate_object>);
    lua_register(g_LuaState, "set_name", LuaBinding<l_set_name>);
    lua_register(g_LuaState, "set_material", LuaBinding<l_set_material>);
    lua_register(g_LuaState, "get_material", LuaBinding<l_get_material>);

    lua_register(g_LuaState, "print",            LuaBinding<l_print>);
    lua_register(g_LuaState, "add_object",       LuaBinding<l_add_object>);
    lua_register(g_LuaState, "remove_object",    LuaBinding<l_remove_object>);
    lua_register(g_LuaState, "clear_scene",      LuaBinding<l_clear_scene>);
    lua_register(g_LuaState, "find_object",      LuaBinding<l_find_object>);
    lua_register(g_LuaState, "set_position",     LuaBinding<l_set_position>);
    lua_register(g_LuaState, "get_position",     LuaBinding<l_get_position>);
    lua_register(g_LuaState, "set_rotation",     LuaBinding<l_set_rotation>);
    lua_register(g_LuaState, "get_rotation",     LuaBinding<l_get_rotation>);
    lua_register(g_LuaState, "set_scale",        LuaBinding<l_set_scale>);
    lua_register(g_LuaState, "get_scale",        LuaBinding<l_get_scale>);
    lua_register(g_LuaState, "set_color",        LuaBinding<l_set_color>);
    lua_register(g_LuaState, "get_color",        LuaBinding<l_get_color>);
    lua_register(g_LuaState, "set_visible",      LuaBinding<l_set_visible>);
    lua_register(g_LuaState, "set_wireframe",    LuaBinding<l_set_wireframe>);
    lua_register(g_LuaState, "get_object_count", LuaBinding<l_get_object_count>);
    lua_register(g_LuaState, "get_time",         LuaBinding<l_get_time>);
    lua_register(g_LuaState, "export_glb",       LuaBinding<l_export_glb>);
    RegisterSoundBindings(g_LuaState);
}

static void ShutdownLuaState() {
    if (g_LuaState) {
        lua_close(g_LuaState);
        g_LuaState = nullptr;
    }
}

// Fixes Bug #4 (stack leak on return values) and Bug #5 (crash on non-string Lua error like error({}))
static void ExecuteLuaScript(const char* script) {
    if (!g_LuaState) {
        InitLuaState();
    }
    AppendConsole("> Running Lua Script...\n");
    LuaStackGuard guard(g_LuaState);
    ClearUpdateCallback(g_LuaState);
    LuaExecutionGuard execution(g_LuaState);
    if (luaL_dostring(g_LuaState, script) != LUA_OK) {
        ClearUpdateCallback(g_LuaState);
        const std::string err = LuaErrorText(g_LuaState);
        AppendConsole(std::string("Lua Error: ") + err + "\n");
    }
}

static void UpdateLuaScript(float dt) {
    if (!g_LuaState || !g_SimulationRunning) return;
    LuaStackGuard guard(g_LuaState);
    PushUpdateCallback(g_LuaState);
    if (lua_isfunction(g_LuaState, -1)) {
        LuaExecutionGuard execution(g_LuaState);
        lua_pushnumber(g_LuaState, dt);
        if (lua_pcall(g_LuaState, 1, 0, 0) != LUA_OK) {
            const std::string err = LuaErrorText(g_LuaState);
            AppendConsole(std::string("Lua on_update Error: ") + err + "\n");
            g_SimulationRunning = false; // pause on runtime error to prevent log spam
        }
    }
}

// -----------------------------------------------------------------------------
// 3D Primitive Rendering (with directional face shading based on object color)
// -----------------------------------------------------------------------------
static void SetShadedColor(const float baseColor[3], float shadeFactor) {
    glColor3f(
        std::min(1.0f, baseColor[0] * shadeFactor),
        std::min(1.0f, baseColor[1] * shadeFactor),
        std::min(1.0f, baseColor[2] * shadeFactor)
    );
}

static void RenderCube(const float color[3]) {
    glBegin(GL_QUADS);
    // Top face (brightest)
    SetShadedColor(color, 1.10f);
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    // Front face
    SetShadedColor(color, 0.95f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    // Right face
    SetShadedColor(color, 0.82f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    // Left face
    SetShadedColor(color, 0.72f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f, -0.5f);
    // Back face
    SetShadedColor(color, 0.65f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    // Bottom face (darkest)
    SetShadedColor(color, 0.50f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glEnd();
}

static void RenderPyramid(const float color[3]) {
    glBegin(GL_TRIANGLES);
    // Front
    SetShadedColor(color, 1.00f);
    glVertex3f( 0.0f,  0.5f,  0.0f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    // Right
    SetShadedColor(color, 0.85f);
    glVertex3f( 0.0f,  0.5f,  0.0f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    // Back
    SetShadedColor(color, 0.70f);
    glVertex3f( 0.0f,  0.5f,  0.0f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    // Left
    SetShadedColor(color, 0.78f);
    glVertex3f( 0.0f,  0.5f,  0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glEnd();

    // Base quad
    glBegin(GL_QUADS);
    SetShadedColor(color, 0.50f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glEnd();
}

static void RenderSphere(const float color[3], int rings = 14, int sectors = 20) {
    const float radius = 0.5f;
    for (int r = 0; r < rings; ++r) {
        float theta1 = (float)r / rings * PI;
        float theta2 = (float)(r + 1) / rings * PI;
        glBegin(GL_QUAD_STRIP);
        for (int s = 0; s <= sectors; ++s) {
            float phi = (float)s / sectors * 2.0f * PI;
            float x1 = std::sin(theta1) * std::cos(phi);
            float y1 = std::cos(theta1);
            float z1 = std::sin(theta1) * std::sin(phi);

            float x2 = std::sin(theta2) * std::cos(phi);
            float y2 = std::cos(theta2);
            float z2 = std::sin(theta2) * std::sin(phi);

            float shade = 0.55f + 0.50f * (0.5f * (y1 + 1.0f));
            SetShadedColor(color, shade);
            glVertex3f(x1 * radius, y1 * radius, z1 * radius);
            glVertex3f(x2 * radius, y2 * radius, z2 * radius);
        }
        glEnd();
    }
}

static void RenderCylinder(const float color[3], int segments = 20) {
    const float radius = 0.5f;
    const float halfH  = 0.5f;

    // Side surface
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / segments * 2.0f * PI;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;
        float shade = 0.65f + 0.35f * (0.5f * (std::cos(angle) + 1.0f));
        SetShadedColor(color, shade);
        glVertex3f(x,  halfH, z);
        glVertex3f(x, -halfH, z);
    }
    glEnd();

    // Top cap
    glBegin(GL_TRIANGLE_FAN);
    SetShadedColor(color, 1.08f);
    glVertex3f(0.0f, halfH, 0.0f);
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / segments * 2.0f * PI;
        glVertex3f(std::cos(angle) * radius, halfH, -std::sin(angle) * radius);
    }
    glEnd();

    // Bottom cap
    glBegin(GL_TRIANGLE_FAN);
    SetShadedColor(color, 0.50f);
    glVertex3f(0.0f, -halfH, 0.0f);
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / segments * 2.0f * PI;
        glVertex3f(std::cos(angle) * radius, -halfH, std::sin(angle) * radius);
    }
    glEnd();
}

static void RenderPlane(const float color[3]) {
    glBegin(GL_QUADS);
    SetShadedColor(color, 1.0f);
    glVertex3f(-0.5f, 0.0f, -0.5f);
    glVertex3f(-0.5f, 0.0f,  0.5f);
    glVertex3f( 0.5f, 0.0f,  0.5f);
    glVertex3f( 0.5f, 0.0f, -0.5f);
    glEnd();
}

static void RenderSelectionBox() {
    const float s = 0.53f;
    glLineWidth(2.0f);
    glColor3f(1.0f, 0.85f, 0.1f);
    glBegin(GL_LINES);
    // Bottom rectangle
    glVertex3f(-s, -s, -s); glVertex3f( s, -s, -s);
    glVertex3f( s, -s, -s); glVertex3f( s, -s,  s);
    glVertex3f( s, -s,  s); glVertex3f(-s, -s,  s);
    glVertex3f(-s, -s,  s); glVertex3f(-s, -s, -s);
    // Top rectangle
    glVertex3f(-s,  s, -s); glVertex3f( s,  s, -s);
    glVertex3f( s,  s, -s); glVertex3f( s,  s,  s);
    glVertex3f( s,  s,  s); glVertex3f(-s,  s,  s);
    glVertex3f(-s,  s,  s); glVertex3f(-s,  s, -s);
    // Vertical pillars
    glVertex3f(-s, -s, -s); glVertex3f(-s,  s, -s);
    glVertex3f( s, -s, -s); glVertex3f( s,  s, -s);
    glVertex3f( s, -s,  s); glVertex3f( s,  s,  s);
    glVertex3f(-s, -s,  s); glVertex3f(-s,  s,  s);
    glEnd();
    glLineWidth(1.0f);
}

static void RenderAxesGizmo(float length = 1.2f) {
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    // X Axis (Red)
    glColor3f(0.95f, 0.25f, 0.25f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(length, 0.0f, 0.0f);
    // Y Axis (Green)
    glColor3f(0.25f, 0.95f, 0.25f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, length, 0.0f);
    // Z Axis (Blue)
    glColor3f(0.25f, 0.55f, 0.95f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, length);
    glEnd();
    glLineWidth(1.0f);
}

static void RenderGrid(float distance,const float target[3]) {
    StudioGridPlan plan=StudioPlanGrid(distance,target[0],target[2],g_Preferences.GridStep,g_Preferences.AdaptiveGrid);
    glBegin(GL_LINES);
    for(int level=0;level<12;++level){float step=plan.Step*std::pow(10.0f,float(level)),extent=step*plan.Lines;
        float cx=std::floor(target[0]/step)*step,cz=std::floor(target[2]/step)*step;
        for(int i=-plan.Lines;i<=plan.Lines;++i){float x=cx+i*step,z=cz+i*step;float shade=i%10==0?.34f:.23f;glColor3f(shade,shade,shade+.025f);
            glVertex3f(x,0,cz-extent);glVertex3f(x,0,cz+extent);glVertex3f(cx-extent,0,z);glVertex3f(cx+extent,0,z);}
        if(extent>g_Preferences.FarClip*2)break;
    }glEnd();
}

static int ExportAccessor(tinygltf::Model& model,const void* data,size_t bytes,size_t count,int component,int type,int target,const std::vector<double>& lo={},const std::vector<double>& hi={}) {
    auto& binary=model.buffers[0].data;
    while(binary.size()%4)binary.push_back(0);
    tinygltf::BufferView view;view.buffer=0;view.byteOffset=binary.size();view.byteLength=bytes;view.target=target;
    size_t offset=binary.size();binary.resize(offset+bytes);std::memcpy(binary.data()+offset,data,bytes);
    int viewIndex=int(model.bufferViews.size());model.bufferViews.push_back(view);
    tinygltf::Accessor accessor;accessor.bufferView=viewIndex;accessor.componentType=component;accessor.type=type;accessor.count=count;
    accessor.minValues=lo;accessor.maxValues=hi;
    model.accessors.push_back(accessor);return int(model.accessors.size())-1;
}

static bool ExportGLB(const std::string& filename) {
    if(filename.empty()||filename.find('\0')!=std::string::npos)return false;
    tinygltf::Model model;model.asset.version="2.0";model.asset.generator=LUNORYX_NAME " " LUNORYX_VERSION;
    model.buffers.emplace_back();tinygltf::Scene scene;scene.name=LUNORYX_NAME " Scene";
    for(const auto& obj:g_Objects) {
        if(!obj.Visible)continue;
        std::vector<float> p;std::vector<uint32_t> indices;ObjectMesh(obj,p,indices);
        if(p.empty()||indices.empty())return false;
        std::vector<double> lo={p[0],p[1],p[2]},hi=lo;
        for(size_t i=0;i<p.size();i+=3)for(int k=0;k<3;++k){if(!std::isfinite(p[i+k]))return false;lo[k]=std::min(lo[k],double(p[i+k]));hi[k]=std::max(hi[k],double(p[i+k]));}
        int positions=ExportAccessor(model,p.data(),p.size()*sizeof(float),p.size()/3,TINYGLTF_COMPONENT_TYPE_FLOAT,TINYGLTF_TYPE_VEC3,TINYGLTF_TARGET_ARRAY_BUFFER,lo,hi);
        int elements=ExportAccessor(model,indices.data(),indices.size()*sizeof(uint32_t),indices.size(),TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT,TINYGLTF_TYPE_SCALAR,TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);
        tinygltf::Material material;material.name=obj.Name+" Material";
        material.pbrMetallicRoughness.baseColorFactor={obj.Color[0],obj.Color[1],obj.Color[2],1};
        material.pbrMetallicRoughness.metallicFactor=obj.Metallic;material.pbrMetallicRoughness.roughnessFactor=obj.Roughness;
        material.doubleSided=obj.Type==MeshType::Plane;
        int materialIndex=int(model.materials.size());model.materials.push_back(material);
        tinygltf::Primitive primitive;primitive.attributes["POSITION"]=positions;primitive.indices=elements;primitive.material=materialIndex;primitive.mode=TINYGLTF_MODE_TRIANGLES;
        if(!obj.VertexColors.empty())primitive.attributes["COLOR_0"]=ExportAccessor(model,obj.VertexColors.data(),obj.VertexColors.size()*sizeof(float),obj.VertexColors.size()/3,TINYGLTF_COMPONENT_TYPE_FLOAT,TINYGLTF_TYPE_VEC3,TINYGLTF_TARGET_ARRAY_BUFFER);
        tinygltf::Mesh mesh;mesh.name=obj.Name;mesh.primitives.push_back(primitive);
        tinygltf::Node node;node.name=obj.Name;node.mesh=int(model.meshes.size());
        node.translation={obj.Position[0],obj.Position[1],obj.Position[2]};node.scale={obj.Scale[0],obj.Scale[1],obj.Scale[2]};
        node.rotation=EulerDegreesToQuaternion(obj.Rotation[0],obj.Rotation[1],obj.Rotation[2]);
        model.meshes.push_back(mesh);scene.nodes.push_back(int(model.nodes.size()));model.nodes.push_back(node);
    }
    if(model.buffers[0].data.empty())model.buffers.clear();
    model.scenes.push_back(scene);model.defaultScene=0;
    std::unique_ptr<std::FILE,int(*)(std::FILE*)> file(OpenBinaryOutput(filename),std::fclose);
    if(!file)return false;
    bool result=false;
    {
        BinaryOutputBuffer buffer(file.get());std::ostream output(&buffer);
        tinygltf::TinyGLTF writer;result=writer.WriteGltfSceneToStream(&model,output,false,true);
        output.flush();result=result&&output.good();
    }
    return std::fclose(file.release())==0&&result;
}

#include "editor_audio.h"
#include "editor_ui.h"
#include "studio_mcp.h"
#include "tests/gizmo_smoke.h"
#include "assets/logo_data.h"

int main(int argc, char* argv[]) {
    if(argc>1&&std::strcmp(argv[1],"--mcp")==0)return RunStudioMcp();
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Lunoryx - SDL initialization failed",SDL_GetError(),nullptr);
        return 1;
    }

    const char* glsl_version = "#version 120";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow(LUNORYX_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1360, 800, window_flags);
    if (!window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Lunoryx - Window creation failed",SDL_GetError(),nullptr);
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Lunoryx - OpenGL initialization failed",SDL_GetError(),window);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename=nullptr; // Built-in layout; no external configuration is required.

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 4.0f;
    style.TabRounding=5;style.WindowPadding=ImVec2(10,8);
    style.Colors[ImGuiCol_WindowBg]=ImVec4(.055f,.072f,.105f,1);
    style.Colors[ImGuiCol_Header]=ImVec4(.10f,.28f,.34f,1);
    style.Colors[ImGuiCol_HeaderHovered]=ImVec4(.13f,.4f,.46f,1);
    style.Colors[ImGuiCol_Button]=ImVec4(.10f,.24f,.3f,1);
    style.Colors[ImGuiCol_ButtonHovered]=ImVec4(.13f,.38f,.44f,1);
    style.Colors[ImGuiCol_CheckMark]=ImVec4(.4f,.9f,.9f,1);
    style.WindowRounding = 5.0f;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    if(!g_ViewportTarget.Initialize()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Lunoryx","OpenGL framebuffer support is required. Please update your GPU driver.",window);
        g_ViewportTarget.Destroy();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplSDL2_Shutdown();ImGui::DestroyContext();SDL_GL_DeleteContext(gl_context);SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
    int logoWidth=0,logoHeight=0,channels=0;
    unsigned char* logo=stbi_load_from_memory(kLogoPng,int(sizeof(kLogoPng)),&logoWidth,&logoHeight,&channels,4);
    if(logo) {
        SDL_Surface* icon=SDL_CreateRGBSurfaceWithFormatFrom(logo,logoWidth,logoHeight,32,logoWidth*4,SDL_PIXELFORMAT_RGBA32);
        if(icon){SDL_SetWindowIcon(window,icon);SDL_FreeSurface(icon);}
        glGenTextures(1,&g_LogoTexture);glBindTexture(GL_TEXTURE_2D,g_LogoTexture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,logoWidth,logoHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,logo);glBindTexture(GL_TEXTURE_2D,0);stbi_image_free(logo);
    }
    char* preferenceDirectory=SDL_GetPrefPath("Lunoryx","Studio-v02");if(preferenceDirectory){g_PreferencesPath=std::string(preferenceDirectory)+"settings.json";SDL_free(preferenceDirectory);}LoadEditorPreferences();SDL_GL_SetSwapInterval(g_Preferences.VSync?1:0);if(g_Preferences.Theme==1)ImGui::StyleColorsLight();
    InitializeStudio();
    g_Studio.Snap=g_Preferences.MoveSnap;g_Studio.TransformSnap=g_Preferences.SnapEnabled;g_Studio.LocalTransform=g_Preferences.LocalTransform;
    InitLuaState();
    ExecuteLuaScript(g_ScriptBuffer);
    g_SimulationRunning=false;g_ObjectHistory.Capture(g_Objects);g_SavedDocumentHash=CompleteDocumentHash();

    const Uint32 start_time = SDL_GetTicks();
    const uint64_t timeout_ms = ParseTestTimeoutMs(std::getenv("TEST_TIMEOUT"));

    const char* capturePath=std::getenv("LUNORYX_CAPTURE_PATH");
    int captureFrame=0,uiSmokeFrame=0;
    const bool uiSmoke=std::getenv("LUNORYX_UI_SMOKE")!=nullptr;
    bool done = false;
    float cameraDistance = 10.0f;
    float cameraAngleX = 25.0f;
    float cameraAngleY = -35.0f;
    float cameraTarget[3] = {0.0f, 1.0f, 0.0f};

    GizmoSmokeTest gizmoSmoke;gizmoSmoke.Enabled=std::getenv("LUNORYX_GIZMO_SMOKE")!=nullptr;gizmoSmoke.Initialize();
    Uint32 lastTicks = SDL_GetTicks();

    while (!done) {
        Uint32 currentTicks = SDL_GetTicks();
        if (timeout_ms > 0 && (uint64_t)(currentTicks - start_time) >= timeout_ms) {
            break;
        }

        if(uiSmoke) {
            ++uiSmokeFrame;
            if(uiSmokeFrame==3||uiSmokeFrame==6){SDL_Event key={};key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_F11;SDL_PushEvent(&key);}
            if(uiSmokeFrame==9){NewScript();g_Studio.ShowDocs=true;}
            if(uiSmokeFrame==12){g_Studio.RequestDocument=-2;g_Studio.ResetLayout=true;}
            if(uiSmokeFrame==15){CreateCharacter(g_Objects,"Smoke Player",g_Studio.Character);StartSimulation();}
            if(uiSmokeFrame==18)StopSimulation();
        }
        float dt = (float)(currentTicks - lastTicks) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        lastTicks = currentTicks;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                RequestProjectAction(1);
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                RequestProjectAction(1);

            if(event.type==SDL_DROPFILE){std::string path=event.drop.file?event.drop.file:"";SDL_free(event.drop.file);std::string extension=path.substr(path.find_last_of('.')==std::string::npos?path.size():path.find_last_of('.'));for(char& c:extension)if(c>='A'&&c<='Z')c=char(c-'A'+'a');if(extension==".lunoryx")RequestProjectAction(2,path);else ImportStudioSound(path);}
            if(event.type==SDL_KEYDOWN&&!event.key.repeat) {
                if(event.key.keysym.sym==SDLK_s&&(event.key.keysym.mod&KMOD_CTRL))SaveCurrentProject();
                if(event.key.keysym.sym==SDLK_F11){g_Studio.FullscreenCode=!g_Studio.FullscreenCode;if(g_Studio.FullscreenCode)g_Studio.RequestDocument=g_Studio.ActiveScript;}
                if(event.key.keysym.sym==SDLK_F5&&g_Studio.Scripts[g_Studio.ActiveScript].Open)ExecuteLuaScript(g_Studio.Scripts[g_Studio.ActiveScript].Code);
                if(event.key.keysym.sym==SDLK_F6){if(g_PhysicsRunning)StopSimulation();else StartSimulation();}
            }
            int mouseX=0,mouseY=0;SDL_GetMouseState(&mouseX,&mouseY);
            if (SceneContains(float(mouseX),float(mouseY))) {
                if(event.type==SDL_KEYDOWN&&!event.key.repeat&&!io.WantTextInput&&!g_PhysicsRunning){
                    if(event.key.keysym.mod&KMOD_CTRL){if(event.key.keysym.sym==SDLK_z&&g_ObjectHistory.Undo(g_Objects)){g_SelectedObjectIndex=-1;g_SimulationRunning=false;}if(event.key.keysym.sym==SDLK_y&&g_ObjectHistory.Redo(g_Objects)){g_SelectedObjectIndex=-1;g_SimulationRunning=false;}}
                    else if(!(SDL_GetMouseState(nullptr,nullptr)&SDL_BUTTON_RMASK)) {SDL_Keycode key=event.key.keysym.sym;if(key==SDLK_q)g_Studio.TransformMode=0;if(key==SDLK_w)g_Studio.TransformMode=1;if(key==SDLK_e)g_Studio.TransformMode=2;if(key==SDLK_r)g_Studio.TransformMode=3;if(key==SDLK_q||key==SDLK_w||key==SDLK_e||key==SDLK_r)g_Studio.SculptEnabled=false;}
                }

                if (event.type == SDL_MOUSEWHEEL) {
                    cameraDistance *= std::pow(.85f,event.wheel.y*g_Preferences.ZoomSensitivity/.8f);
                    cameraDistance = std::max(.2f, std::min(g_Preferences.FarClip*.25f, cameraDistance));
                } else if (event.type == SDL_MOUSEMOTION) {
                    if (event.motion.state & SDL_BUTTON_RMASK) {
                        cameraAngleY = WrapYaw(cameraAngleY + event.motion.xrel * g_Preferences.OrbitSensitivity);
                        cameraAngleX += event.motion.yrel * g_Preferences.OrbitSensitivity;
                        cameraAngleX = std::max(-89.0f, std::min(89.0f, cameraAngleX));
                    } else if (event.motion.state & SDL_BUTTON_MMASK) {
                        cameraTarget[0] -= event.motion.xrel * g_Preferences.PanSensitivity*cameraDistance/10;
                        cameraTarget[1] += event.motion.yrel * g_Preferences.PanSensitivity*cameraDistance/10;
                    }
                }
            }
        }

        UpdateLuaScript(dt);
        if(g_PhysicsRunning) {
            if(g_Studio.SceneActive&&!io.WantTextInput) {
                const Uint8* keys=SDL_GetKeyboardState(nullptr);
                for(auto& object:g_Objects)if(object.Character&&object.Dynamic) {
                    float x=float(keys[SDL_SCANCODE_D])-float(keys[SDL_SCANCODE_A]);
                    float z=float(keys[SDL_SCANCODE_S])-float(keys[SDL_SCANCODE_W]);
                    float length=std::sqrt(x*x+z*z);if(length>0){x/=length;z/=length;}
                    float angle=-cameraAngleY*PI/180,c=std::cos(angle),sn=std::sin(angle);
                    object.Velocity[0]=(x*c+z*sn)*object.MoveSpeed;object.Velocity[2]=(-x*sn+z*c)*object.MoveSpeed;
                    if(keys[SDL_SCANCODE_SPACE]&&object.Grounded)object.Velocity[1]=object.JumpSpeed;
                }
            }
            AdvancePhysics(g_Objects,g_Physics,dt);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        gizmoSmoke.Input(cameraDistance,cameraAngleX,cameraAngleY,cameraTarget);
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        // Transparent DockSpace Setup (PassthruCentralNode + NoBackground so 3D scene is visible)
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags window_flags_dock = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        window_flags_dock |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags_dock |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, window_flags_dock);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        BuildStudioLayout(dockspace_id,viewport);
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                ImGui::InputText("Project path",g_ProjectPath,sizeof(g_ProjectPath));
                if(ImGui::MenuItem("Save Project","Ctrl+S"))SaveCurrentProject();
                if(ImGui::MenuItem("Save Project As..."))g_OpenSaveAs=true;
                if(ImGui::MenuItem("Load Project"))RequestProjectAction(2,g_ProjectPath);
                if (ImGui::MenuItem("Export GLB")) {
                    if (ExportGLB("export.glb")) {
                        AppendConsole("> Exported binary scene to export.glb\n");
                    } else {
                        AppendConsole("> Failed to export GLB\n");
                    }
                }
                if (ImGui::MenuItem("Clear Scene")) {
                    RequestProjectAction(3);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) RequestProjectAction(1);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Show Grid", nullptr, &g_ShowGrid);
                ImGui::MenuItem("Show World Axes", nullptr, &g_ShowAxes);
                ImGui::MenuItem("Global Wireframe", nullptr, &g_GlobalWireframe);
                ImGui::EndMenu();
            }
            StudioMenu();
            ImGui::Separator();
            if (ImGui::Button(g_SimulationRunning ? "Pause Lua (on_update)" : "Play Lua (on_update)")) {
                g_SimulationRunning = !g_SimulationRunning;
            }
            ImGui::Text(" | Objects: %d | FPS: %.1f", (int)g_Objects.size(), io.Framerate);
            if(ProjectDirty())ImGui::TextUnformatted(" * Unsaved");
            ImGui::EndMenuBar();
        }
        ImGui::End();

        RenderProjectDialogs();if(g_ExitRequested)done=true;
        RenderStudio(cameraDistance,cameraAngleX,cameraAngleY,cameraTarget,dt);
        gizmoSmoke.Check();if(gizmoSmoke.Done)done=true;
        RenderStudioAudio();
        Vec3 listener=CameraVector({0,0,cameraDistance},cameraAngleX,cameraAngleY);float listenerPosition[3]={listener.x+cameraTarget[0],listener.y+cameraTarget[1],listener.z+cameraTarget[2]};studio_audio_listener(listenerPosition,g_Preferences.MasterVolume);
        if(!g_PhysicsRunning&&!ImGui::IsAnyItemActive()&&!ImGuizmo::IsUsing()&&!ImGui::IsMouseDown(ImGuiMouseButton_Left))g_ObjectHistory.Capture(g_Objects);


        // Rendering
        ImGui::Render();

        int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
        int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
        if (fb_width <= 0 || fb_height <= 0) {
            SDL_GL_SwapWindow(window);
            continue;
        }

        glViewport(0, 0, fb_width, fb_height);
        glClearColor(0.035f, 0.045f, 0.065f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        if(g_Studio.SceneActive) {
        float pixelScaleX=io.DisplayFramebufferScale.x,pixelScaleY=io.DisplayFramebufferScale.y;
        int sceneWidth=std::max(1,int(g_Studio.SceneSize.x*pixelScaleX)),sceneHeight=std::max(1,int(g_Studio.SceneSize.y*pixelScaleY));
        g_ViewportTarget.Resize(sceneWidth,sceneHeight);
        g_ViewportTarget.BindFramebuffer(GL_FRAMEBUFFER,g_ViewportTarget.Framebuffer);
        glViewport(0,0,sceneWidth,sceneHeight);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        RenderSky();
        // Setup 3D Projection Matrix
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = float(sceneWidth) / float(sceneHeight);
        float fov = g_Preferences.Fov * PI / 180.0f;
        float zNear = 0.1f;
        float zFar = g_Preferences.FarClip;
        float f = 1.0f / std::tan(fov / 2.0f);
        float proj[16] = {
            f / aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, (zFar + zNear) / (zNear - zFar), -1,
            0, 0, (2 * zFar * zNear) / (zNear - zFar), 0
        };
        glLoadMatrixf(proj);

        // Setup View Matrix (Camera Orbit + Pan Target)
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -cameraDistance);
        glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f);
        glRotatef(cameraAngleY, 0.0f, 1.0f, 0.0f);
        glTranslatef(-cameraTarget[0], -cameraTarget[1], -cameraTarget[2]);

        if (g_ShowGrid) {
            RenderGrid(cameraDistance,cameraTarget);
        }
        if (g_ShowAxes) {
            RenderAxesGizmo(1.5f);
        }

        // Render GameObjects
        for (int i = 0; i < (int)g_Objects.size(); ++i) {
            const GameObject& obj = g_Objects[i];
            if (!obj.Visible) continue;

            glPushMatrix();
            glTranslatef(obj.Position[0], obj.Position[1], obj.Position[2]);
            glRotatef(obj.Rotation[0], 1.0f, 0.0f, 0.0f);
            glRotatef(obj.Rotation[1], 0.0f, 1.0f, 0.0f);
            glRotatef(obj.Rotation[2], 0.0f, 0.0f, 1.0f);
            glScalef(obj.Scale[0], obj.Scale[1], obj.Scale[2]);

            bool useWireframe = g_GlobalWireframe || obj.Wireframe;
            if (useWireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            }

            if(!obj.Vertices.empty())RenderEditableMesh(obj);
            else switch (obj.Type) {
                case MeshType::Cube:     RenderCube(obj.Color);     break;
                case MeshType::Pyramid:  RenderPyramid(obj.Color);  break;
                case MeshType::Sphere:   RenderSphere(obj.Color);   break;
                case MeshType::Cylinder: RenderCylinder(obj.Color); break;
                case MeshType::Plane:    RenderPlane(obj.Color);    break;
            }

            if (useWireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }

            if (obj.Selected || i == g_SelectedObjectIndex) {
                if(obj.Vertices.empty())RenderSelectionBox();
                else {
                    float lo[3]={obj.Vertices[0],obj.Vertices[1],obj.Vertices[2]},hi[3]={lo[0],lo[1],lo[2]};
                    for(size_t vertex=0;vertex<obj.Vertices.size();vertex+=3)for(int k=0;k<3;++k){lo[k]=std::min(lo[k],obj.Vertices[vertex+k]);hi[k]=std::max(hi[k],obj.Vertices[vertex+k]);}
                    glPushMatrix();glTranslatef((lo[0]+hi[0])*.5f,(lo[1]+hi[1])*.5f,(lo[2]+hi[2])*.5f);glScalef(hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2]);RenderSelectionBox();glPopMatrix();
                }
                RenderAxesGizmo(0.8f);
            }

            glPopMatrix();
        }

        if(g_Studio.BrushHit){glDisable(GL_DEPTH_TEST);glPointSize(10);glColor3f(1,.75f,.25f);glBegin(GL_POINTS);glVertex3f(g_Studio.BrushWorld.x,g_Studio.BrushWorld.y,g_Studio.BrushWorld.z);glEnd();glPointSize(1);glEnable(GL_DEPTH_TEST);}
        }
        g_ViewportTarget.BindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,fb_width,fb_height);
        // Render ImGui overlay
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if(capturePath&&++captureFrame==5&&fb_width<=4096&&fb_height<=4096) {
            std::vector<unsigned char> pixels(size_t(fb_width)*fb_height*3);
            glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,fb_width,fb_height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
            stbi_flip_vertically_on_write(1);stbi_write_png(capturePath,fb_width,fb_height,3,pixels.data(),fb_width*3);stbi_flip_vertically_on_write(0);
        }
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    PersistStudioPreferences();
    ShutdownLuaState();
    studio_audio_shutdown();
    if(g_LogoTexture)glDeleteTextures(1,&g_LogoTexture);
    g_ViewportTarget.Destroy();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return gizmoSmoke.Enabled&&!gizmoSmoke.Passed?1:0;
}
