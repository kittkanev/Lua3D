#pragma once
// Included after the host's Lua helpers and scene state.
static int l_make_editable(lua_State* L) {
    lua_Integer i=luaL_checkinteger(L,1),levels=luaL_optinteger(L,2,0);
    bool ok=i>=0&&i<lua_Integer(g_Objects.size())&&levels>=0&&levels<=4;
    lua_pushboolean(L,ok&&MakeEditable(g_Objects[size_t(i)],int(levels)));return 1;
}
static int l_sculpt(lua_State* L) {
    lua_Integer i=luaL_checkinteger(L,1);
    Vec3 center={CheckFiniteNumber(L,2),CheckFiniteNumber(L,3),CheckFiniteNumber(L,4)};
    float radius=CheckFiniteNumber(L,5),strength=CheckFiniteNumber(L,6);
    lua_Integer mode=luaL_optinteger(L,7,0);
    bool valid=i>=0&&i<lua_Integer(g_Objects.size())&&mode>=0&&mode<=2;
    lua_pushboolean(L,valid&&SculptBrush(g_Objects[size_t(i)],center,radius,strength,int(mode)));return 1;
}
static int l_merge_objects(lua_State* L) {
    luaL_checktype(L,1,LUA_TTABLE);
    const char* name=luaL_optstring(L,2,"Joined Mesh");
    size_t count=lua_rawlen(L,1);
    if(count<2||count>g_Objects.size()){lua_pushinteger(L,-1);return 1;}
    for(size_t k=1;k<=count;++k) {
        lua_rawgeti(L,1,lua_Integer(k));
        bool valid=lua_isinteger(L,-1)&&lua_tointeger(L,-1)>=0&&lua_tointeger(L,-1)<lua_Integer(g_Objects.size());
        lua_pop(L,1);if(!valid){lua_pushinteger(L,-1);return 1;}
    }
    std::vector<int> selection;selection.reserve(count);
    for(size_t k=1;k<=count;++k){lua_rawgeti(L,1,lua_Integer(k));selection.push_back(int(lua_tointeger(L,-1)));lua_pop(L,1);}
    int result=MergeObjects(g_Objects,selection,name);g_SelectedObjectIndex=result;
    lua_pushinteger(L,result);return 1;
}
static int l_set_physics(lua_State* L) {
    lua_Integer i=luaL_checkinteger(L,1);luaL_checktype(L,2,LUA_TBOOLEAN);
    float mass=lua_gettop(L)>=3?CheckFiniteNumber(L,3):1;
    float bounce=lua_gettop(L)>=4?CheckFiniteNumber(L,4):.2f;
    if(mass<.01f||mass>10000||bounce<0||bounce>1)return luaL_error(L,"mass must be 0.01..10000; restitution 0..1");
    bool valid=i>=0&&i<lua_Integer(g_Objects.size());
    if(valid){auto& o=g_Objects[size_t(i)];o.Dynamic=lua_toboolean(L,2)!=0;o.Mass=mass;o.Restitution=bounce;}
    lua_pushboolean(L,valid);return 1;
}
static int l_set_velocity(lua_State* L) {
    lua_Integer i=luaL_checkinteger(L,1);
    float v[3]={CheckFiniteNumber(L,2),CheckFiniteNumber(L,3),CheckFiniteNumber(L,4)};
    for(float x:v)if(std::abs(x)>10000)return luaL_error(L,"velocity component outside -10000..10000");
    bool valid=i>=0&&i<lua_Integer(g_Objects.size());
    if(valid)std::copy(v,v+3,g_Objects[size_t(i)].Velocity);
    lua_pushboolean(L,valid);return 1;
}
static int l_get_velocity(lua_State* L) {
    lua_Integer i=luaL_checkinteger(L,1);if(i<0||i>=lua_Integer(g_Objects.size()))return 0;
    for(float x:g_Objects[size_t(i)].Velocity) { lua_pushnumber(L,x); }
    return 3;
}
static int l_set_character(lua_State* L) {
    lua_Integer i=luaL_checkinteger(L,1);luaL_checktype(L,2,LUA_TBOOLEAN);
    float speed=lua_gettop(L)>=3?CheckFiniteNumber(L,3):4.5f;
    float jump=lua_gettop(L)>=4?CheckFiniteNumber(L,4):6;
    if(speed<.1f||speed>30||jump<0||jump>20)return luaL_error(L,"move speed must be 0.1..30; jump speed 0..20");
    bool valid=i>=0&&i<lua_Integer(g_Objects.size());
    if(valid){auto& o=g_Objects[size_t(i)];o.Character=lua_toboolean(L,2)!=0;o.MoveSpeed=speed;o.JumpSpeed=jump;if(o.Character){o.Dynamic=true;o.Restitution=0;}}
    lua_pushboolean(L,valid);return 1;
}
static int l_add_character(lua_State* L) {
    const char* name=luaL_optstring(L,1,"Player");CharacterConfig config;
    if(lua_gettop(L)>=2)config.Height=CheckFiniteNumber(L,2);
    if(lua_gettop(L)>=3)config.Width=CheckFiniteNumber(L,3);
    if(lua_gettop(L)>=4)config.HeadSize=CheckFiniteNumber(L,4);
    lua_pushinteger(L,CreateCharacter(g_Objects,name,config));return 1;
}
static int l_set_property(lua_State* L) {
    lua_Integer i=luaL_checkinteger(L,1);size_t length=0;
    const char* name=luaL_checklstring(L,2,&length);
    int type=lua_type(L,3);
    if(type!=LUA_TNUMBER&&type!=LUA_TBOOLEAN&&type!=LUA_TSTRING&&type!=LUA_TNIL)return luaL_error(L,"property must be number, boolean, string or nil");
    float number=type==LUA_TNUMBER?CheckFiniteNumber(L,3):0;
    size_t textLength=0;const char* text=type==LUA_TSTRING?lua_tolstring(L,3,&textLength):"";
    if(i<0||i>=lua_Integer(g_Objects.size())||length==0||length>64||textLength>1024){lua_pushboolean(L,0);return 1;}
    ObjectProperty value;
    value.Type=type==LUA_TSTRING?ObjectProperty::Text:(type==LUA_TBOOLEAN?ObjectProperty::Boolean:ObjectProperty::Number);
    value.NumberValue=number;value.BoolValue=lua_toboolean(L,3)!=0;
    if(type==LUA_TSTRING)value.TextValue.assign(text,textLength);
    bool result;
    if(type==LUA_TNIL)result=g_Objects[size_t(i)].Properties.erase(std::string(name,length))>0;
    else result=SetProperty(g_Objects[size_t(i)],std::string(name,length),value);
    lua_pushboolean(L,result);return 1;
}
static int l_get_property(lua_State* L) {
    lua_Integer i=luaL_checkinteger(L,1);size_t length=0;const char* name=luaL_checklstring(L,2,&length);
    if(i<0||i>=lua_Integer(g_Objects.size()))return 0;
    auto found=g_Objects[size_t(i)].Properties.find(std::string(name,length));
    if(found==g_Objects[size_t(i)].Properties.end())return 0;
    const auto& value=found->second;
    if(value.Type==ObjectProperty::Number)lua_pushnumber(L,value.NumberValue);
    else if(value.Type==ObjectProperty::Boolean)lua_pushboolean(L,value.BoolValue);
    else lua_pushlstring(L,value.TextValue.data(),value.TextValue.size());
    return 1;
}
static int l_set_skybox(lua_State* L) {
    const char* preset=luaL_checkstring(L,1);int value=-1;
    if(std::strcmp(preset,"day")==0)value=0;
    if(std::strcmp(preset,"sunset")==0)value=1;
    if(std::strcmp(preset,"night")==0)value=2;
    if(value>=0)ApplySkyPreset(value);
    lua_pushboolean(L,value>=0);return 1;
}
