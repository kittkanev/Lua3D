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
        assert(not export_glb('tests/invalid' .. string.char(0) .. '.glb'))
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
    assert(ExportGLB("tests/test-scene.glb")); // writable overwrite on every supported CRT
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
        std::vector<float> vertices; std::vector<uint32_t> indices;
        ObjectMesh(g_Objects[size_t(type)], vertices, indices);
        const auto& primitive = model.meshes.at(model.nodes.at(type).mesh).primitives.at(0);
        const auto& p = model.accessors.at(primitive.attributes.at("POSITION"));
        const auto& i = model.accessors.at(primitive.indices);
        const auto& pv = model.bufferViews.at(p.bufferView);
        const auto& iv = model.bufferViews.at(i.bufferView);
        assert(p.count == vertices.size()/3 && i.count == indices.size());
        assert(std::memcmp(model.buffers[0].data.data()+pv.byteOffset, vertices.data(), vertices.size()*sizeof(float)) == 0);
        assert(std::memcmp(model.buffers[0].data.data()+iv.byteOffset, indices.data(), indices.size()*sizeof(uint32_t)) == 0);
    }
    CheckLua(R"(
        local clay=add_object('Clay','sphere')
        assert(make_editable(clay,1))
        assert(not make_editable(clay,-1))
        assert(not pcall(sculpt,clay,0,0,0,1,0/0))
        assert(not sculpt(clay,0,0,0,0,1))
        assert(sculpt(clay,0,0.5,0,1,0.1,0))
        set_position(clay,2,0,0)
        local player=add_character('Player',2,0.65,0.45)
        assert(player>=0 and add_character('Invalid',0)==-1)
        assert(set_character(player,true,5,7))
        assert(not pcall(set_character,player,true,math.huge,7))
        assert(set_physics(player,true,3,0))
        assert(not pcall(set_physics,player,true,-1,0))
        assert(set_velocity(player,1,2,3))
        assert(not pcall(set_velocity,player,0,0,math.huge))
        local x,y,z=get_velocity(player);assert(x==1 and y==2 and z==3)
        assert(set_property(player,'Health',100))
        assert(set_property(player,'Name','Hero'))
        assert(set_property(player,'Enabled',true))
        assert(not set_property(player,'',1))
        assert(not pcall(set_property,player,'Bad',{}))
        assert(not pcall(set_property,player,'Health',0/0))
        assert(get_property(player,'Health')==100)
        assert(get_property(player,'Name')=='Hero')
        assert(get_property(player,'Enabled')==true)
        assert(set_property(player,'Name',nil) and get_property(player,'Name')==nil)
        assert(set_skybox('night') and not set_skybox('invalid'))
        assert(merge_objects({clay,99999},'bad')==-1)
        local joined=merge_objects({clay,player},'Joined');assert(joined>=0)
        assert(export_glb('tests/test-scene.glb'))
    )");
    assert(reader.LoadBinaryFromFile(&model,&err,&warn,"tests/test-scene.glb"));
    const auto& joined=model.meshes.back().primitives[0];
    assert(joined.attributes.count("COLOR_0")==1);
    int index=int(g_Objects.size())-1;
    auto original=g_Objects[size_t(index)].Position[1];g_Objects[size_t(index)].Dynamic=true;
    StartSimulation();AdvancePhysics(g_Objects,g_Physics,.1f);StopSimulation();
    assert(g_Objects[size_t(index)].Position[1]==original&&!g_PhysicsRunning);
    if (!std::getenv("KEEP_TEST_GLB")) std::remove("tests/test-scene.glb");
    InitializeStudio();
    std::snprintf(g_Studio.Scripts[0].Code,sizeof(g_Studio.Scripts[0].Code),"print('loaded only on explicit Run')");
    ObjectProperty property;property.NumberValue=42;assert(SetProperty(g_Objects[0],"Health",property));
    g_Studio.Scripts[1].Allocated=true;g_Studio.Scripts[1].Open=false;std::snprintf(g_Studio.Scripts[1].Name,sizeof(g_Studio.Scripts[1].Name),"Closed.lua");
    auto savedName=g_Objects[0].Name;size_t savedCount=g_Objects.size();
    StudioSound clip;clip.Name="Saved sound";clip.Path="tests/missing.wav";clip.Volume=.4f;clip.Loop=true;g_Sounds.push_back(clip);
    assert(SaveStudioProject("tests/test-project.lunoryx"));
    auto validPosition=g_Objects[0].Position[0];g_Objects[0].Position[0]=std::numeric_limits<float>::quiet_NaN();
    assert(!SaveStudioProject("tests/test-project.lunoryx"));g_Objects[0].Position[0]=validPosition;
    assert(SaveStudioProject("tests/test-project.lunoryx")); // replacing an existing save works on Windows too
    g_Objects[0].Name="changed";
    assert(LoadStudioProject("tests/test-project.lunoryx"));
    assert(g_Objects.size()==savedCount&&g_Objects[0].Name==savedName);
    assert(g_Sounds.size()==1&&g_Sounds[0].Name=="Saved sound"&&g_Sounds[0].Loop&&std::abs(g_Sounds[0].Volume-.4f)<1e-6f);
    assert(g_Objects[0].Properties.at("Health").NumberValue==42);
    assert(std::strstr(g_Studio.Scripts[0].Code,"explicit Run"));
    assert(g_Studio.Scripts[1].Allocated&&!g_Studio.Scripts[1].Open);
    CheckLua("assert(rawget(_G, 'on_update')==nil)");
    assert(!SaveStudioProject("tests/nonexistent-directory/project.lunoryx"));
    assert(!LoadStudioProject("tests/missing-project.lunoryx"));
    {std::ofstream broken("tests/broken-project.lunoryx");broken<<"{bad json";}
    assert(!LoadStudioProject("tests/broken-project.lunoryx")&&g_Objects[0].Name==savedName);
    nlohmann::json corrupt;{std::ifstream f("tests/test-project.lunoryx");f>>corrupt;}
    auto legacy=corrupt;legacy["schema"]=1;legacy.erase("sounds");{std::ofstream f("tests/legacy-project.lunoryx");f<<legacy;}assert(LoadStudioProject("tests/legacy-project.lunoryx")&&g_Sounds.empty());std::remove("tests/legacy-project.lunoryx");
    corrupt["objects"].back()["indices"][0]=4294967295u;
    {std::ofstream broken("tests/broken-project.lunoryx");broken<<corrupt;}
    assert(!LoadStudioProject("tests/broken-project.lunoryx")&&g_Objects.size()==savedCount);
    std::remove("tests/test-project.lunoryx");std::remove("tests/broken-project.lunoryx");
    g_PreferencesPath="tests/test-settings.json";g_Preferences.Fov=73;g_Preferences.OrbitSensitivity=.7f;g_Preferences.MoveSnap=2;g_Preferences.SnapEnabled=true;assert(SaveEditorPreferences());g_Preferences.Fov=45;LoadEditorPreferences();assert(g_Preferences.Fov==73&&std::abs(g_Preferences.OrbitSensitivity-.7f)<1e-6f&&g_Preferences.MoveSnap==2&&g_Preferences.SnapEnabled);
    {std::ofstream invalid("tests/test-settings.json");invalid<<"{\"fov\":0}";}LoadEditorPreferences();assert(g_Preferences.Fov==73);std::remove("tests/test-settings.json");g_PreferencesPath.clear();g_Preferences=EditorPreferences();
    ShutdownLuaState();
    for (int run = 0; run < 50; ++run) {
        InitLuaState();
        ExecuteLuaScript("local x = {}; for i=1,100 do x[i]=string.rep('x',100) end");
        assert(lua_gettop(g_LuaState) == 0);
        ShutdownLuaState();
    }
    g_Objects.clear();g_Objects.emplace_back("Stacked");StudioModifier mirror;Model::AddModifier(g_Objects[0],mirror);uint64_t stable=g_Objects[0].Id;
    assert(SaveStudioProject("tests/modeling.lunoryx"));g_Objects.clear();assert(LoadStudioProject("tests/modeling.lunoryx"));assert(g_Objects[0].Id==stable&&g_Objects[0].Modifiers.size()==1&&g_Objects[0].BaseMesh.Faces.size()==12);
    InitLuaState();CheckLua("assert(mesh_operation(0,'apply')==0)");ShutdownLuaState();assert(g_Objects[0].Modifiers.empty());
    g_Objects.clear();g_Objects.emplace_back("Textured round trip");auto uvMesh=Model::FromObject(g_Objects[0]);Model::Unwrap(uvMesh);assert(uvMesh.UV.size()==uvMesh.Positions.size()/3*2);for(float uv:uvMesh.UV)assert(std::isfinite(uv)&&uv>=0&&uv<=1);Model::ToObject(g_Objects[0],uvMesh);
    unsigned char pixels[12]={255,0,0,0,255,0,0,0,255,255,255,255};assert(stbi_write_png("tests/texture.png",2,2,3,pixels,6));StudioMaterial texture;texture.Texture=AbsoluteAssetPath("tests/texture.png");g_Objects[0].Materials.push_back(texture);g_Objects[0].Color[0]=g_Objects[0].Color[1]=g_Objects[0].Color[2]=1;
    assert(ExportGLB("tests/textured.glb"));std::string importReport;auto imported=ReadGLBObjects("tests/textured.glb",importReport);assert(imported.size()==1&&!imported[0].UV.empty()&&!imported[0].Materials[0].Image.empty());assert(imported[0].Normals.size()==imported[0].Vertices.size());g_Objects=imported;
    assert(SaveStudioProject("tests/textured.lunoryx"));assert(LoadStudioProject("tests/textured.lunoryx")&&!g_Objects[0].Materials[0].Image.empty());
    std::remove("tests/texture.png");
    // Completed operations cover audio and environment, and Play restores both.
    g_PhysicsRunning=false;g_ObjectHistory.Clear();g_Sounds.clear();g_ObjectHistory.Capture(g_Objects);
    float originalSky=g_SkyTop[0];g_SkyTop[0]=.37f;StudioSound tone;tone.Path="missing.wav";g_Sounds.push_back(tone);g_ObjectHistory.Capture(g_Objects);
    assert(g_ObjectHistory.Undo(g_Objects)&&g_Sounds.empty()&&g_SkyTop[0]==originalSky);
    assert(g_ObjectHistory.Redo(g_Objects)&&g_Sounds.size()==1&&g_SkyTop[0]==.37f);
    StartSimulation();g_SkyTop[0]=.91f;g_Physics.Gravity=-1;g_Sounds.clear();StopSimulation();
    assert(g_SkyTop[0]==.37f&&g_Sounds.size()==1&&g_Physics.Gravity!=-1);
    assert(RelativeAssetPath("/tmp/game/assets/tone.wav","/tmp/game/scene.lunoryx")=="assets/tone.wav");
    assert(RelativeAssetPath("/tmp/shared/tone.wav","/tmp/game/scene.lunoryx")=="../shared/tone.wav");
    assert(SaveStudioProject("tests/history.lunoryx")&&!ProjectDirty());
    g_Studio.Scripts[0].Allocated=true;std::strcpy(g_Studio.Scripts[0].Code,"-- changed");assert(ProjectDirty());
    assert(!LoadStudioProject("tests/does-not-exist.lunoryx")&&ProjectDirty());
    std::cout << "PASS: Lua bounds, stack recovery, hostile metamethods, GLB buffer round trip\n";
}
