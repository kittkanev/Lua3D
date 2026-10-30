#define main editor_main
#include "../main.cpp"
#undef main
#include <cassert>

static void CheckLua(const char* script) {
    if (luaL_dostring(g_LuaState, script) != LUA_OK) {
        std::cerr << LuaErrorText(g_LuaState) << '\n';
        std::abort();
    }
    assert(lua_gettop(g_LuaState) == 0);
}
int main() {
    InitLuaState();
    CheckLua(R"(
        local a = add_object('original', 'sphere')
        assert(a == 0)
        assert(not set_position(4294967296, 9, 9, 9))
        assert(not remove_object(4294967296))
        assert(not pcall(set_position, a, 1, 0/0, 3))
        local x,y,z = get_position(a); assert(x == 0 and y == 0 and z == 0)
        assert(set_material(a, 2, -1))
        local m,r = get_material(a); assert(m == 1 and r == 0)
        local b = duplicate_object(a, 'copy'); assert(b == 1)
        assert(set_name(b, 'renamed') and find_object('renamed') == b)
        assert(duplicate_object(-1) == -1)
        assert(get_object_count() == 2)
    )");
    ExecuteLuaScript("function on_update(dt) error('stale') end");
    ExecuteLuaScript("return 1, 2, 3");
    CheckLua("assert(on_update == nil)");
    ExecuteLuaScript("error(setmetatable({}, {__tostring=function() error('nested') end}))");
    assert(lua_gettop(g_LuaState) == 0);
    ExecuteLuaScript("function on_update(dt) error({}) end");
    UpdateLuaScript(0.01f);
    assert(!g_SimulationRunning && lua_gettop(g_LuaState) == 0);
    assert(ExportGLB("tests/test-scene.glb"));
    tinygltf::Model model; tinygltf::TinyGLTF reader; std::string err, warn;
    assert(reader.LoadBinaryFromFile(&model, &err, &warn, "tests/test-scene.glb"));
    assert(model.nodes.size() == 2);
    std::remove("tests/test-scene.glb");
    ShutdownLuaState();
    std::cout << "PASS: Lua API, error recovery, callback lifecycle, GLB round trip\n";
}
