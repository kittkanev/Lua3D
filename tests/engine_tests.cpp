#define main editor_main
#include "../main.cpp"
#undef main
#include <cassert>
#include <fstream>
#include <stdexcept>

static void CheckLua(const char* script) {
    const int top = lua_gettop(g_LuaState);
    if (luaL_dostring(g_LuaState, script) != LUA_OK) {
        std::cerr << LuaErrorText(g_LuaState) << '\n';
        std::abort();
    }
    assert(lua_gettop(g_LuaState) == top);
}
static int ThrowingBinding(lua_State*) { throw std::runtime_error("test exception"); }
int main() {
    InitLuaState();
    lua_register(g_LuaState, "throw_native", LuaBinding<ThrowingBinding>);
    CheckLua("local ok,e=pcall(throw_native); assert(not ok and e:find('test exception'))");
    ExecuteLuaScript("while true do end");
    assert(lua_gettop(g_LuaState) == 0);
    assert(g_ConsoleOutput.find("instruction budget exceeded") != std::string::npos);
    ExecuteLuaScript("function on_update(dt) while true do end end");
    UpdateLuaScript(0.01f);
    assert(!g_SimulationRunning && lua_gettop(g_LuaState) == 0);
    g_SimulationRunning = true;
    CheckLua(R"(
        local a = add_object('original', 'sphere')
        assert(a == 0)
        assert(not set_position(4294967296, 9, 9, 9))
        assert(not remove_object(4294967296))
        for _, setter in ipairs({set_position, set_rotation, set_scale, set_color}) do
            assert(not pcall(setter, a, 1, 0/0, 3))
            assert(not pcall(setter, a, 1, math.huge, 3))
            assert(not pcall(setter, a, 1, 1e300, 3))
            assert(not pcall(setter, a, 1, {}, 3))
        end
        local x,y,z = get_position(a); assert(x == 0 and y == 0 and z == 0)
        x,y,z = get_rotation(a); assert(x == 0 and y == 0 and z == 0)
        x,y,z = get_scale(a); assert(x == 1 and y == 1 and z == 1)
        assert(set_material(a, 2, -1))
        local m,r = get_material(a); assert(m == 1 and r == 0)
        assert(not pcall(set_material, a, 0/0, 0.5))
        m,r = get_material(a); assert(m == 1 and r == 0)
        local b = duplicate_object(a, 'copy'); assert(b == 1)
        assert(set_name(b, 'renamed') and find_object('renamed') == b)
        assert(set_name(b, '') and find_object('') == b)
        assert(duplicate_object(-1) == -1)
        assert(get_object_count() == 2)
        assert(not export_glb('tests/nonexistent-directory/scene.glb'))
        for i=1,100 do
            local value = setmetatable({}, {__tostring=function() error('print failed') end})
            local ok, err = pcall(print, value)
            assert(not ok and string.find(err, 'print failed'))
        end
    )");
    lua_pushinteger(g_LuaState, 123); // host calls preserve an existing stack prefix
    ExecuteLuaScript("function on_update(dt) error('stale') end");
    ExecuteLuaScript("return 1, 2, 3");
    assert(lua_gettop(g_LuaState) == 1 && lua_tointeger(g_LuaState, 1) == 123);
    CheckLua("assert(rawget(_G, 'on_update') == nil)");
    ExecuteLuaScript("error(setmetatable({}, {__tostring=function() error('nested') end}))");
    assert(lua_gettop(g_LuaState) == 1);
    ExecuteLuaScript("setmetatable(_G, {__index=function() error('index trap') end, __newindex=function() error('newindex trap') end})");
    UpdateLuaScript(0.01f); // missing callback must not invoke __index
    ExecuteLuaScript("return 42"); // clearing a missing callback must not invoke __newindex
    assert(lua_gettop(g_LuaState) == 1);
    CheckLua("setmetatable(_G, nil)");
    ExecuteLuaScript("function on_update(dt) error({}) end");
    UpdateLuaScript(0.01f);
    assert(!g_SimulationRunning && lua_gettop(g_LuaState) == 1);
    lua_pop(g_LuaState, 1);

    g_Objects.clear();
    for (int type = 0; type < 5; ++type) {
        g_Objects.emplace_back(GetMeshTypeName(static_cast<MeshType>(type)), static_cast<MeshType>(type));
    }
    g_Objects.emplace_back("hidden");
    g_Objects.back().Visible = false;
    assert(ExportGLB("tests/test-scene.glb"));
    std::ifstream file("tests/test-scene.glb", std::ios::binary);
    char magic[4] = {}; file.read(magic, 4);
    assert(file && std::memcmp(magic, "glTF", 4) == 0);
    file.close();
    tinygltf::Model model; tinygltf::TinyGLTF reader; std::string err, warn;
    assert(reader.LoadBinaryFromFile(&model, &err, &warn, "tests/test-scene.glb"));
    assert(model.nodes.size() == 5 && model.buffers.size() == 1);
    for (const auto& view : model.bufferViews) {
        assert(view.buffer == 0 && view.byteOffset % 4 == 0);
        assert(view.byteOffset + view.byteLength <= model.buffers[0].data.size());
    }
    for (int type = 0; type < 5; ++type) {
        std::vector<float> vertices; std::vector<uint16_t> indices;
        GeneratePrimitiveMeshData(static_cast<MeshType>(type), vertices, indices);
        const auto& primitive = model.meshes.at(model.nodes.at(type).mesh).primitives.at(0);
        const auto& p = model.accessors.at(primitive.attributes.at("POSITION"));
        const auto& i = model.accessors.at(primitive.indices);
        const auto& pv = model.bufferViews.at(p.bufferView);
        const auto& iv = model.bufferViews.at(i.bufferView);
        assert(p.count == vertices.size()/3 && i.count == indices.size());
        assert(std::memcmp(model.buffers[0].data.data()+pv.byteOffset, vertices.data(), vertices.size()*sizeof(float)) == 0);
        assert(std::memcmp(model.buffers[0].data.data()+iv.byteOffset, indices.data(), indices.size()*sizeof(uint16_t)) == 0);
    }
    if (!std::getenv("KEEP_TEST_GLB")) std::remove("tests/test-scene.glb");
    ShutdownLuaState();
    for (int run = 0; run < 50; ++run) {
        InitLuaState();
        ExecuteLuaScript("local x = {}; for i=1,100 do x[i]=string.rep('x',100) end");
        assert(lua_gettop(g_LuaState) == 0);
        ShutdownLuaState();
    }
    std::cout << "PASS: Lua bounds, stack recovery, hostile metamethods, GLB buffer round trip\n";
}
