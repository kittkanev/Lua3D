#include <SDL.h>
#include <GL/gl.h>
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
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// tinygltf requires json
#include <tiny_gltf.h>

static const float PI = 3.14159265358979323846f;

enum class MeshType : int {
    Cube = 0,
    Pyramid = 1,
    Sphere = 2,
    Cylinder = 3,
    Plane = 4
};

static const char* GetMeshTypeName(MeshType type) {
    switch (type) {
        case MeshType::Cube:     return "Cube";
        case MeshType::Pyramid:  return "Pyramid";
        case MeshType::Sphere:   return "Sphere";
        case MeshType::Cylinder: return "Cylinder";
        case MeshType::Plane:    return "Plane";
        default:                 return "Cube";
    }
}

static MeshType ParseMeshType(const std::string& typeStr) {
    std::string lower = typeStr;
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    }
    if (lower == "pyramid")  return MeshType::Pyramid;
    if (lower == "sphere")   return MeshType::Sphere;
    if (lower == "cylinder") return MeshType::Cylinder;
    if (lower == "plane")    return MeshType::Plane;
    return MeshType::Cube;
}

// Enhanced GameObject structure
struct GameObject {
    std::string Name;
    MeshType Type = MeshType::Cube;
    float Position[3] = {0.0f, 0.0f, 0.0f};
    float Rotation[3] = {0.0f, 0.0f, 0.0f};
    float Scale[3]    = {1.0f, 1.0f, 1.0f};
    float Color[3]    = {0.80f, 0.45f, 0.20f};
    float Metallic    = 0.1f;
    float Roughness   = 0.5f;
    bool Visible      = true;
    bool Wireframe    = false;

    GameObject(const std::string& name, MeshType type = MeshType::Cube)
        : Name(name), Type(type) {}
};

// Global state
std::vector<GameObject> g_Objects;
int g_SelectedObjectIndex = -1;
lua_State* g_LuaState = nullptr;
bool g_SimulationRunning = true;
bool g_ShowGrid = true;
bool g_ShowAxes = true;
bool g_GlobalWireframe = false;
bool g_AutoScrollConsole = true;
char g_OutlinerFilter[128] = "";

char g_ScriptBuffer[1024 * 16] =
    "-- Lua 3D Engine: Interactive Demo Script\n"
    "print('Initializing procedural scene...')\n"
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

std::string g_ConsoleOutput = "";

// Forward declarations
static bool ExportGLB(const std::string& filename);
static void InitLuaState();
static void ShutdownLuaState();

static void AppendConsole(const std::string& msg) {
    g_ConsoleOutput += msg;
    if (g_ConsoleOutput.size() > 64 * 1024) {
        g_ConsoleOutput.erase(0, g_ConsoleOutput.size() - 48 * 1024);
    }
}

// Fixed l_print: uses luaL_tolstring so booleans, nil, numbers, and tables print properly
static int l_print(lua_State *L) {
    int n = lua_gettop(L);
    std::string line;
    for (int i = 1; i <= n; i++) {
        const char *s = luaL_tolstring(L, i, nullptr);
        if (s) {
            line += s;
        }
        lua_pop(L, 1); // pop string pushed by luaL_tolstring
        if (i < n) line += "\t";
    }
    line += "\n";
    AppendConsole(line);
    return 0;
}

// Lua: add_object(name, [type]) -> index (0-based)
static int l_add_object(lua_State *L) {
    const char* name = luaL_checkstring(L, 1);
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
    int index = (int)luaL_checkinteger(L, 1);
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
    AppendConsole("Error: Object index out of bounds in remove_object\n");
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
    const char* name = luaL_checkstring(L, 1);
    for (size_t i = 0; i < g_Objects.size(); ++i) {
        if (g_Objects[i].Name == name) {
            lua_pushinteger(L, (lua_Integer)i);
            return 1;
        }
    }
    lua_pushinteger(L, -1);
    return 1;
}

// Lua: set_position(index, x, y, z) - 0-based index
static int l_set_position(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Position[0] = (float)luaL_checknumber(L, 2);
        g_Objects[index].Position[1] = (float)luaL_checknumber(L, 3);
        g_Objects[index].Position[2] = (float)luaL_checknumber(L, 4);
    } else {
        AppendConsole("Error: Object index out of bounds in set_position\n");
    }
    return 0;
}

// Lua: get_position(index) -> x, y, z
static int l_get_position(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        lua_pushnumber(L, g_Objects[index].Position[0]);
        lua_pushnumber(L, g_Objects[index].Position[1]);
        lua_pushnumber(L, g_Objects[index].Position[2]);
        return 3;
    }
    return 0;
}

// Lua: set_rotation(index, rx, ry, rz)
static int l_set_rotation(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Rotation[0] = (float)luaL_checknumber(L, 2);
        g_Objects[index].Rotation[1] = (float)luaL_checknumber(L, 3);
        g_Objects[index].Rotation[2] = (float)luaL_checknumber(L, 4);
    } else {
        AppendConsole("Error: Object index out of bounds in set_rotation\n");
    }
    return 0;
}

// Lua: get_rotation(index) -> rx, ry, rz
static int l_get_rotation(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        lua_pushnumber(L, g_Objects[index].Rotation[0]);
        lua_pushnumber(L, g_Objects[index].Rotation[1]);
        lua_pushnumber(L, g_Objects[index].Rotation[2]);
        return 3;
    }
    return 0;
}

// Lua: set_scale(index, sx, sy, sz)
static int l_set_scale(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Scale[0] = (float)luaL_checknumber(L, 2);
        g_Objects[index].Scale[1] = (float)luaL_checknumber(L, 3);
        g_Objects[index].Scale[2] = (float)luaL_checknumber(L, 4);
    } else {
        AppendConsole("Error: Object index out of bounds in set_scale\n");
    }
    return 0;
}

// Lua: get_scale(index) -> sx, sy, sz
static int l_get_scale(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        lua_pushnumber(L, g_Objects[index].Scale[0]);
        lua_pushnumber(L, g_Objects[index].Scale[1]);
        lua_pushnumber(L, g_Objects[index].Scale[2]);
        return 3;
    }
    return 0;
}

// Lua: set_color(index, r, g, b)
static int l_set_color(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Color[0] = std::max(0.0f, std::min(1.0f, (float)luaL_checknumber(L, 2)));
        g_Objects[index].Color[1] = std::max(0.0f, std::min(1.0f, (float)luaL_checknumber(L, 3)));
        g_Objects[index].Color[2] = std::max(0.0f, std::min(1.0f, (float)luaL_checknumber(L, 4)));
    } else {
        AppendConsole("Error: Object index out of bounds in set_color\n");
    }
    return 0;
}

// Lua: get_color(index) -> r, g, b
static int l_get_color(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        lua_pushnumber(L, g_Objects[index].Color[0]);
        lua_pushnumber(L, g_Objects[index].Color[1]);
        lua_pushnumber(L, g_Objects[index].Color[2]);
        return 3;
    }
    return 0;
}

// Lua: set_visible(index, visible)
static int l_set_visible(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Visible = lua_toboolean(L, 2) != 0;
    }
    return 0;
}

// Lua: set_wireframe(index, wireframe)
static int l_set_wireframe(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Wireframe = lua_toboolean(L, 2) != 0;
    }
    return 0;
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
    const char* filename = luaL_checkstring(L, 1);
    bool res = ExportGLB(filename);
    if (res) {
        AppendConsole(std::string("Successfully exported scene to ") + filename + "\n");
    } else {
        AppendConsole(std::string("Failed to export scene to ") + filename + "\n");
    }
    lua_pushboolean(L, res ? 1 : 0);
    return 1;
}

static void InitLuaState() {
    ShutdownLuaState();
    g_LuaState = luaL_newstate();
    luaL_openlibs(g_LuaState);

    lua_register(g_LuaState, "print",            l_print);
    lua_register(g_LuaState, "add_object",       l_add_object);
    lua_register(g_LuaState, "remove_object",    l_remove_object);
    lua_register(g_LuaState, "clear_scene",      l_clear_scene);
    lua_register(g_LuaState, "find_object",      l_find_object);
    lua_register(g_LuaState, "set_position",     l_set_position);
    lua_register(g_LuaState, "get_position",     l_get_position);
    lua_register(g_LuaState, "set_rotation",     l_set_rotation);
    lua_register(g_LuaState, "get_rotation",     l_get_rotation);
    lua_register(g_LuaState, "set_scale",        l_set_scale);
    lua_register(g_LuaState, "get_scale",        l_get_scale);
    lua_register(g_LuaState, "set_color",        l_set_color);
    lua_register(g_LuaState, "get_color",        l_get_color);
    lua_register(g_LuaState, "set_visible",      l_set_visible);
    lua_register(g_LuaState, "set_wireframe",    l_set_wireframe);
    lua_register(g_LuaState, "get_object_count", l_get_object_count);
    lua_register(g_LuaState, "get_time",         l_get_time);
    lua_register(g_LuaState, "export_glb",       l_export_glb);
}

static void ShutdownLuaState() {
    if (g_LuaState) {
        lua_close(g_LuaState);
        g_LuaState = nullptr;
    }
}

static void ExecuteLuaScript(const char* script) {
    InitLuaState();
    AppendConsole("> Running Lua Script...\n");
    if (luaL_dostring(g_LuaState, script) != LUA_OK) {
        const char *err = lua_tostring(g_LuaState, -1);
        AppendConsole(std::string("Lua Error: ") + (err ? err : "Unknown error") + "\n");
        lua_pop(g_LuaState, 1);
    }
}

static void UpdateLuaScript(float dt) {
    if (!g_LuaState || !g_SimulationRunning) return;
    lua_getglobal(g_LuaState, "on_update");
    if (lua_isfunction(g_LuaState, -1)) {
        lua_pushnumber(g_LuaState, dt);
        if (lua_pcall(g_LuaState, 1, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(g_LuaState, -1);
            AppendConsole(std::string("Lua on_update Error: ") + (err ? err : "Unknown error") + "\n");
            lua_pop(g_LuaState, 1);
            g_SimulationRunning = false; // pause on runtime error to prevent log spam
        }
    } else {
        lua_pop(g_LuaState, 1);
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

static void RenderGrid() {
    glBegin(GL_LINES);
    for (int i = -10; i <= 10; ++i) {
        if (i == 0) {
            glColor3f(0.45f, 0.45f, 0.48f);
        } else {
            glColor3f(0.26f, 0.26f, 0.28f);
        }
        glVertex3f((float)i, 0.0f, -10.0f);
        glVertex3f((float)i, 0.0f, 10.0f);
        glVertex3f(-10.0f, 0.0f, (float)i);
        glVertex3f(10.0f, 0.0f, (float)i);
    }
    glEnd();
}

// Convert OpenGL Euler angles (degrees, applied as Rx * Ry * Rz) to glTF quaternion [x, y, z, w]
static std::vector<double> EulerToQuaternion(float rxDeg, float ryDeg, float rzDeg) {
    double hx = (rxDeg * PI / 180.0) * 0.5;
    double hy = (ryDeg * PI / 180.0) * 0.5;
    double hz = (rzDeg * PI / 180.0) * 0.5;

    double cx = std::cos(hx), sx = std::sin(hx);
    double cy = std::cos(hy), sy = std::sin(hy);
    double cz = std::cos(hz), sz = std::sin(hz);

    // Quaternion product Q = Qx * Qy * Qz
    double qw = cx * cy * cz - sx * sy * sz;
    double qx = sx * cy * cz + cx * sy * sz;
    double qy = cx * sy * cz - sx * cy * sz;
    double qz = cx * cy * sz + sx * sy * cz;

    return {qx, qy, qz, qw};
}

// Generate vertex positions and triangle indices for any MeshType for GLB export
static void GeneratePrimitiveMeshData(MeshType type, std::vector<float>& outPositions, std::vector<uint16_t>& outIndices) {
    outPositions.clear();
    outIndices.clear();

    if (type == MeshType::Cube) {
        outPositions = {
            -0.5f, -0.5f,  0.5f, // 0: Front-Bottom-Left
             0.5f, -0.5f,  0.5f, // 1: Front-Bottom-Right
             0.5f,  0.5f,  0.5f, // 2: Front-Top-Right
            -0.5f,  0.5f,  0.5f, // 3: Front-Top-Left
            -0.5f, -0.5f, -0.5f, // 4: Back-Bottom-Left
            -0.5f,  0.5f, -0.5f, // 5: Back-Top-Left
             0.5f,  0.5f, -0.5f, // 6: Back-Top-Right
             0.5f, -0.5f, -0.5f  // 7: Back-Bottom-Right
        };
        // Proper outward CCW winding for all 6 faces (fixes the Left face winding bug)
        outIndices = {
            0, 1, 2,  0, 2, 3, // Front  (+Z)
            7, 4, 5,  7, 5, 6, // Back   (-Z)
            3, 2, 6,  3, 6, 5, // Top    (+Y)
            4, 0, 3,  4, 3, 5, // Left   (-X)
            1, 7, 6,  1, 6, 2, // Right  (+X)
            4, 7, 1,  4, 1, 0  // Bottom (-Y)
        };
    } else if (type == MeshType::Pyramid) {
        outPositions = {
             0.0f,  0.5f,  0.0f, // 0: Apex
            -0.5f, -0.5f,  0.5f, // 1: Front-Left
             0.5f, -0.5f,  0.5f, // 2: Front-Right
             0.5f, -0.5f, -0.5f, // 3: Back-Right
            -0.5f, -0.5f, -0.5f  // 4: Back-Left
        };
        outIndices = {
            0, 1, 2, // Front
            0, 2, 3, // Right
            0, 3, 4, // Back
            0, 4, 1, // Left
            4, 3, 2,  4, 2, 1 // Base
        };
    } else if (type == MeshType::Plane) {
        outPositions = {
            -0.5f, 0.0f,  0.5f,
             0.5f, 0.0f,  0.5f,
             0.5f, 0.0f, -0.5f,
            -0.5f, 0.0f, -0.5f
        };
        outIndices = { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 };
    } else if (type == MeshType::Sphere) {
        const int rings = 12;
        const int sectors = 16;
        const float radius = 0.5f;
        for (int r = 0; r <= rings; ++r) {
            float theta = (float)r / rings * PI;
            for (int s = 0; s <= sectors; ++s) {
                float phi = (float)s / sectors * 2.0f * PI;
                outPositions.push_back(std::sin(theta) * std::cos(phi) * radius);
                outPositions.push_back(std::cos(theta) * radius);
                outPositions.push_back(std::sin(theta) * std::sin(phi) * radius);
            }
        }
        for (int r = 0; r < rings; ++r) {
            for (int s = 0; s < sectors; ++s) {
                uint16_t cur = (uint16_t)(r * (sectors + 1) + s);
                uint16_t next = (uint16_t)((r + 1) * (sectors + 1) + s);
                outIndices.push_back(cur);
                outIndices.push_back(next);
                outIndices.push_back((uint16_t)(cur + 1));

                outIndices.push_back((uint16_t)(cur + 1));
                outIndices.push_back(next);
                outIndices.push_back((uint16_t)(next + 1));
            }
        }
    } else if (type == MeshType::Cylinder) {
        const int segments = 16;
        const float radius = 0.5f;
        const float halfH = 0.5f;
        // 0: Top center, 1: Bottom center
        outPositions.insert(outPositions.end(), {0.0f, halfH, 0.0f, 0.0f, -halfH, 0.0f});
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)i / segments * 2.0f * PI;
            float x = std::cos(angle) * radius;
            float z = std::sin(angle) * radius;
            outPositions.insert(outPositions.end(), {x, halfH, z});
            outPositions.insert(outPositions.end(), {x, -halfH, z});
        }
        for (int i = 0; i < segments; ++i) {
            uint16_t t0 = (uint16_t)(2 + i * 2);
            uint16_t b0 = (uint16_t)(t0 + 1);
            uint16_t t1 = (uint16_t)(t0 + 2);
            uint16_t b1 = (uint16_t)(t0 + 3);
            // Side quad
            outIndices.insert(outIndices.end(), {t0, b0, t1, t1, b0, b1});
            // Top cap
            outIndices.insert(outIndices.end(), {0, t1, t0});
            // Bottom cap
            outIndices.insert(outIndices.end(), {1, b0, b1});
        }
    }
}

// Fixed and enhanced GLB Export using tinygltf (proper CCW winding, Euler->Quaternion, all 5 primitives & PBR materials)
static bool ExportGLB(const std::string& filename) {
    tinygltf::Model model;
    tinygltf::Scene scene;
    scene.name = "Lua3D Scene";

    // Build shared geometry accessors for each of the 5 MeshTypes
    int posAccessorByType[5] = {-1, -1, -1, -1, -1};
    int idxAccessorByType[5] = {-1, -1, -1, -1, -1};

    for (int t = 0; t < 5; ++t) {
        std::vector<float> positions;
        std::vector<uint16_t> indices;
        GeneratePrimitiveMeshData((MeshType)t, positions, indices);

        double minV[3] = { 1e9,  1e9,  1e9};
        double maxV[3] = {-1e9, -1e9, -1e9};
        for (size_t v = 0; v + 2 < positions.size(); v += 3) {
            for (int c = 0; c < 3; ++c) {
                minV[c] = std::min(minV[c], (double)positions[v + c]);
                maxV[c] = std::max(maxV[c], (double)positions[v + c]);
            }
        }

        // Positions Buffer
        tinygltf::Buffer posBuffer;
        posBuffer.data.resize(positions.size() * sizeof(float));
        std::memcpy(posBuffer.data.data(), positions.data(), posBuffer.data.size());
        int posBufIdx = (int)model.buffers.size();
        model.buffers.push_back(posBuffer);

        tinygltf::BufferView posBufferView;
        posBufferView.buffer = posBufIdx;
        posBufferView.byteOffset = 0;
        posBufferView.byteLength = posBuffer.data.size();
        posBufferView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
        int posViewIdx = (int)model.bufferViews.size();
        model.bufferViews.push_back(posBufferView);

        tinygltf::Accessor posAccessor;
        posAccessor.bufferView = posViewIdx;
        posAccessor.byteOffset = 0;
        posAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        posAccessor.count = positions.size() / 3;
        posAccessor.type = TINYGLTF_TYPE_VEC3;
        posAccessor.minValues = {minV[0], minV[1], minV[2]};
        posAccessor.maxValues = {maxV[0], maxV[1], maxV[2]};
        posAccessorByType[t] = (int)model.accessors.size();
        model.accessors.push_back(posAccessor);

        // Indices Buffer
        tinygltf::Buffer idxBuffer;
        idxBuffer.data.resize(indices.size() * sizeof(uint16_t));
        std::memcpy(idxBuffer.data.data(), indices.data(), idxBuffer.data.size());
        int idxBufIdx = (int)model.buffers.size();
        model.buffers.push_back(idxBuffer);

        tinygltf::BufferView idxBufferView;
        idxBufferView.buffer = idxBufIdx;
        idxBufferView.byteOffset = 0;
        idxBufferView.byteLength = idxBuffer.data.size();
        idxBufferView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
        int idxViewIdx = (int)model.bufferViews.size();
        model.bufferViews.push_back(idxBufferView);

        tinygltf::Accessor idxAccessor;
        idxAccessor.bufferView = idxViewIdx;
        idxAccessor.byteOffset = 0;
        idxAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
        idxAccessor.count = indices.size();
        idxAccessor.type = TINYGLTF_TYPE_SCALAR;
        idxAccessorByType[t] = (int)model.accessors.size();
        model.accessors.push_back(idxAccessor);
    }

    // Create Nodes, Materials, and Meshes for each visible GameObject
    for (size_t i = 0; i < g_Objects.size(); ++i) {
        const GameObject& obj = g_Objects[i];
        if (!obj.Visible) continue;

        int typeIdx = (int)obj.Type;
        if (typeIdx < 0 || typeIdx >= 5) typeIdx = 0;

        tinygltf::Material mat;
        mat.name = obj.Name + "_Mat";
        mat.pbrMetallicRoughness.baseColorFactor = {
            obj.Color[0], obj.Color[1], obj.Color[2], 1.0
        };
        mat.pbrMetallicRoughness.metallicFactor = obj.Metallic;
        mat.pbrMetallicRoughness.roughnessFactor = obj.Roughness;
        int matIndex = (int)model.materials.size();
        model.materials.push_back(mat);

        tinygltf::Mesh mesh;
        mesh.name = obj.Name + "_Mesh";
        tinygltf::Primitive primitive;
        primitive.attributes["POSITION"] = posAccessorByType[typeIdx];
        primitive.indices = idxAccessorByType[typeIdx];
        primitive.material = matIndex;
        primitive.mode = TINYGLTF_MODE_TRIANGLES;
        mesh.primitives.push_back(primitive);
        int meshIndex = (int)model.meshes.size();
        model.meshes.push_back(mesh);

        tinygltf::Node node;
        node.name = obj.Name;
        node.mesh = meshIndex;
        node.translation = {obj.Position[0], obj.Position[1], obj.Position[2]};
        node.rotation = EulerToQuaternion(obj.Rotation[0], obj.Rotation[1], obj.Rotation[2]);
        node.scale = {obj.Scale[0], obj.Scale[1], obj.Scale[2]};

        int nodeIndex = (int)model.nodes.size();
        model.nodes.push_back(node);
        scene.nodes.push_back(nodeIndex);
    }

    model.scenes.push_back(scene);
    model.defaultScene = 0;

    tinygltf::TinyGLTF writer;
    return writer.WriteGltfSceneToFile(&model, filename, true, true, true, false);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "Error initializing SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Lua 3D Engine Studio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1360, 800, window_flags);
    if (!window) {
        std::cerr << "Error creating SDL Window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 4.0f;
    style.WindowRounding = 5.0f;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Run initial default script to populate the scene
    ExecuteLuaScript(g_ScriptBuffer);

    bool done = false;
    float cameraDistance = 10.0f;
    float cameraAngleX = 25.0f;
    float cameraAngleY = -35.0f;
    float cameraTarget[3] = {0.0f, 1.0f, 0.0f};

    Uint32 lastTicks = SDL_GetTicks();

    while (!done) {
        Uint32 currentTicks = SDL_GetTicks();
        float dt = (float)(currentTicks - lastTicks) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        lastTicks = currentTicks;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;

            // Mouse orbit / pan / zoom in 3D viewport when cursor is not captured by ImGui windows
            if (!io.WantCaptureMouse) {
                if (event.type == SDL_MOUSEWHEEL) {
                    cameraDistance -= event.wheel.y * 0.8f;
                    cameraDistance = std::max(1.5f, std::min(50.0f, cameraDistance));
                } else if (event.type == SDL_MOUSEMOTION) {
                    if (event.motion.state & SDL_BUTTON_RMASK) {
                        cameraAngleY += event.motion.xrel * 0.4f;
                        cameraAngleX += event.motion.yrel * 0.4f;
                        cameraAngleX = std::max(-89.0f, std::min(89.0f, cameraAngleX));
                    } else if (event.motion.state & SDL_BUTTON_MMASK) {
                        cameraTarget[0] -= event.motion.xrel * 0.02f;
                        cameraTarget[1] += event.motion.yrel * 0.02f;
                    }
                }
            }
        }

        // Tick Lua on_update(dt)
        UpdateLuaScript(dt);

        // Start Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Transparent DockSpace Setup (Fix: PassthruCentralNode + NoBackground so 3D scene is visible!)
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags window_flags_dock = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        window_flags_dock |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags_dock |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Host", nullptr, window_flags_dock);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Export GLB Scene (scene.glb)")) {
                    if (ExportGLB("scene.glb")) {
                        AppendConsole("Exported scene.glb successfully.\n");
                    } else {
                        AppendConsole("Error exporting scene.glb.\n");
                    }
                }
                if (ImGui::MenuItem("Clear Scene")) {
                    g_Objects.clear();
                    g_SelectedObjectIndex = -1;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) done = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Show Grid", nullptr, &g_ShowGrid);
                ImGui::MenuItem("Show World Axes", nullptr, &g_ShowAxes);
                ImGui::MenuItem("Global Wireframe", nullptr, &g_GlobalWireframe);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::Button(g_SimulationRunning ? "Pause Lua (on_update)" : "Play Lua (on_update)")) {
                g_SimulationRunning = !g_SimulationRunning;
            }
            ImGui::Text(" | Objects: %d | FPS: %.1f (%.2f ms)", (int)g_Objects.size(), io.Framerate, 1000.0f / (io.Framerate > 0 ? io.Framerate : 1.0f));
            ImGui::EndMenuBar();
        }
        ImGui::End();

        // 1. World Outliner Window
        ImGui::Begin("World Outliner");
        ImGui::Text("Add Primitive:");
        if (ImGui::Button("+ Cube"))     g_Objects.emplace_back("Cube " + std::to_string(g_Objects.size() + 1), MeshType::Cube);
        ImGui::SameLine();
        if (ImGui::Button("+ Pyramid"))  g_Objects.emplace_back("Pyramid " + std::to_string(g_Objects.size() + 1), MeshType::Pyramid);
        ImGui::SameLine();
        if (ImGui::Button("+ Sphere"))   g_Objects.emplace_back("Sphere " + std::to_string(g_Objects.size() + 1), MeshType::Sphere);
        if (ImGui::Button("+ Cylinder")) g_Objects.emplace_back("Cylinder " + std::to_string(g_Objects.size() + 1), MeshType::Cylinder);
        ImGui::SameLine();
        if (ImGui::Button("+ Plane"))    g_Objects.emplace_back("Plane " + std::to_string(g_Objects.size() + 1), MeshType::Plane);

        ImGui::Separator();
        ImGui::InputTextWithHint("##Filter", "Search objects...", g_OutlinerFilter, sizeof(g_OutlinerFilter));
        ImGui::Separator();

        for (int i = 0; i < (int)g_Objects.size(); i++) {
            if (g_OutlinerFilter[0] != '\0' && g_Objects[i].Name.find(g_OutlinerFilter) == std::string::npos) {
                continue;
            }
            char label[192];
            snprintf(label, sizeof(label), "[%s] %s%s##%d",
                     GetMeshTypeName(g_Objects[i].Type),
                     g_Objects[i].Name.c_str(),
                     g_Objects[i].Visible ? "" : " (Hidden)",
                     i);
            if (ImGui::Selectable(label, g_SelectedObjectIndex == i)) {
                g_SelectedObjectIndex = i;
            }
        }
        ImGui::End();

        // 2. Details Window
        ImGui::Begin("Details");
        if (g_SelectedObjectIndex >= 0 && g_SelectedObjectIndex < (int)g_Objects.size()) {
            GameObject& obj = g_Objects[g_SelectedObjectIndex];
            char nameBuf[128];
            snprintf(nameBuf, sizeof(nameBuf), "%s", obj.Name.c_str()); // Safe null-terminated copy
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                obj.Name = nameBuf;
            }

            const char* meshItems[] = { "Cube", "Pyramid", "Sphere", "Cylinder", "Plane" };
            int currentMesh = (int)obj.Type;
            if (ImGui::Combo("Mesh Type", &currentMesh, meshItems, IM_ARRAYSIZE(meshItems))) {
                obj.Type = (MeshType)currentMesh;
            }

            ImGui::Checkbox("Visible", &obj.Visible);
            ImGui::SameLine();
            ImGui::Checkbox("Wireframe", &obj.Wireframe);

            ImGui::Separator();
            ImGui::Text("Transform");
            ImGui::DragFloat3("Position", obj.Position, 0.1f);
            ImGui::DragFloat3("Rotation", obj.Rotation, 1.0f);
            ImGui::DragFloat3("Scale", obj.Scale, 0.05f, 0.01f, 100.0f);
            if (ImGui::Button("Reset Transform")) {
                obj.Position[0] = obj.Position[1] = obj.Position[2] = 0.0f;
                obj.Rotation[0] = obj.Rotation[1] = obj.Rotation[2] = 0.0f;
                obj.Scale[0] = obj.Scale[1] = obj.Scale[2] = 1.0f;
            }

            ImGui::Separator();
            ImGui::Text("Material & PBR (GLB Export)");
            ImGui::ColorEdit3("Base Color", obj.Color);
            ImGui::SliderFloat("Metallic", &obj.Metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &obj.Roughness, 0.0f, 1.0f);

            ImGui::Separator();
            if (ImGui::Button("Duplicate Object")) {
                GameObject copy = obj;
                copy.Name += " Copy";
                copy.Position[0] += 1.2f;
                g_Objects.push_back(copy);
                g_SelectedObjectIndex = (int)g_Objects.size() - 1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Focus Camera")) {
                cameraTarget[0] = obj.Position[0];
                cameraTarget[1] = obj.Position[1];
                cameraTarget[2] = obj.Position[2];
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete Object")) {
                g_Objects.erase(g_Objects.begin() + g_SelectedObjectIndex);
                g_SelectedObjectIndex = -1;
            }
        } else {
            ImGui::TextDisabled("Select an object in World Outliner to inspect or edit.");
        }
        ImGui::End();

        // 3. Camera & Viewport Controls Window
        ImGui::Begin("Camera & Viewport");
        ImGui::TextDisabled("Tip: RMB Drag = Orbit, MMB Drag = Pan, Scroll = Zoom");
        ImGui::SliderFloat("Distance", &cameraDistance, 1.5f, 50.0f);
        ImGui::SliderFloat("Pitch (X)", &cameraAngleX, -89.0f, 89.0f);
        ImGui::SliderFloat("Yaw (Y)", &cameraAngleY, -180.0f, 180.0f);
        ImGui::DragFloat3("Target", cameraTarget, 0.1f);

        if (ImGui::Button("Iso View"))   { cameraAngleX = 25.0f; cameraAngleY = -35.0f; }
        ImGui::SameLine();
        if (ImGui::Button("Top View"))   { cameraAngleX = 89.0f; cameraAngleY = 0.0f; }
        ImGui::SameLine();
        if (ImGui::Button("Front View")) { cameraAngleX = 0.0f;  cameraAngleY = 0.0f; }
        ImGui::SameLine();
        if (ImGui::Button("Reset Origin")) {
            cameraTarget[0] = 0.0f; cameraTarget[1] = 0.0f; cameraTarget[2] = 0.0f;
            cameraDistance = 10.0f;
        }
        ImGui::End();

        // 4. Script Editor & Console Window
        ImGui::Begin("Script Editor");
        if (ImGui::Button("Run Lua Script")) {
            ExecuteLuaScript(g_ScriptBuffer);
        }
        ImGui::SameLine();
        if (ImGui::Button("Preset: Solar System")) {
            snprintf(g_ScriptBuffer, sizeof(g_ScriptBuffer), "%s",
                "-- Animated Solar System Preset\n"
                "clear_scene()\n"
                "local sun = add_object('Sun', 'sphere')\n"
                "set_scale(sun, 1.6, 1.6, 1.6)\n"
                "set_color(sun, 1.0, 0.78, 0.15)\n\n"
                "local earth = add_object('Earth', 'sphere')\n"
                "set_scale(earth, 0.65, 0.65, 0.65)\n"
                "set_color(earth, 0.2, 0.55, 0.95)\n\n"
                "local mars = add_object('Mars', 'sphere')\n"
                "set_scale(mars, 0.5, 0.5, 0.5)\n"
                "set_color(mars, 0.9, 0.35, 0.2)\n\n"
                "local t = 0\n"
                "function on_update(dt)\n"
                "    t = t + dt\n"
                "    set_position(earth, math.cos(t * 1.4) * 3.5, 0, math.sin(t * 1.4) * 3.5)\n"
                "    set_position(mars, math.cos(t * 0.8) * 5.8, 0, math.sin(t * 0.8) * 5.8)\n"
                "end\n");
            ExecuteLuaScript(g_ScriptBuffer);
        }
        ImGui::SameLine();
        if (ImGui::Button("Preset: Spiral Tower")) {
            snprintf(g_ScriptBuffer, sizeof(g_ScriptBuffer), "%s",
                "-- Procedural Spiral Tower Preset\n"
                "clear_scene()\n"
                "for i = 0, 29 do\n"
                "    local a = i * 0.35\n"
                "    local idx = add_object('Step_' .. i, 'cube')\n"
                "    set_position(idx, math.cos(a) * 2.5, i * 0.25, math.sin(a) * 2.5)\n"
                "    set_rotation(idx, 0, -a * 57.2958, 0)\n"
                "    set_scale(idx, 1.2, 0.2, 0.5)\n"
                "    set_color(idx, 0.3 + (i / 30) * 0.7, 0.5, 1.0 - (i / 30) * 0.6)\n"
                "end\n"
                "print('Generated 30 spiral steps!')\n");
            ExecuteLuaScript(g_ScriptBuffer);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Console")) {
            g_ConsoleOutput.clear();
        }

        ImGui::InputTextMultiline("##Script", g_ScriptBuffer, sizeof(g_ScriptBuffer), ImVec2(-1, 210));
        ImGui::Separator();
        ImGui::Text("Console Output:");
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &g_AutoScrollConsole);
        ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), true);
        ImGui::TextUnformatted(g_ConsoleOutput.c_str());
        if (g_AutoScrollConsole && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::End();

        // Rendering
        ImGui::Render();

        int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
        int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
        if (fb_width <= 0 || fb_height <= 0) {
            SDL_GL_SwapWindow(window);
            continue;
        }

        glViewport(0, 0, fb_width, fb_height);
        glClearColor(0.11f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // Setup 3D Projection Matrix
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)fb_width / (float)fb_height;
        float fov = 45.0f * PI / 180.0f;
        float zNear = 0.1f;
        float zFar = 200.0f;
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
            RenderGrid();
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

            switch (obj.Type) {
                case MeshType::Cube:     RenderCube(obj.Color);     break;
                case MeshType::Pyramid:  RenderPyramid(obj.Color);  break;
                case MeshType::Sphere:   RenderSphere(obj.Color);   break;
                case MeshType::Cylinder: RenderCylinder(obj.Color); break;
                case MeshType::Plane:    RenderPlane(obj.Color);    break;
            }

            if (useWireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }

            if (i == g_SelectedObjectIndex) {
                RenderSelectionBox();
                RenderAxesGizmo(0.8f);
            }

            glPopMatrix();
        }

        // Render ImGui overlay
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ShutdownLuaState();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
