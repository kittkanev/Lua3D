#pragma once
static int ModelCommand(std::vector<GameObject>& objects,int index,const std::string& action,const nlohmann::json& args){
    if(index<0||size_t(index)>=objects.size())throw std::runtime_error("Object index outside scene");
    GameObject changed=objects[size_t(index)];
    if(action=="modifier"){
        StudioModifier mod;std::string type=args.value("type",std::string("mirror"));
        if(type=="mirror")mod.Type=StudioModifier::Mirror;else if(type=="array")mod.Type=StudioModifier::Array;else if(type=="subdivision")mod.Type=StudioModifier::Subdivision;else if(type=="bevel")mod.Type=StudioModifier::Bevel;else throw std::runtime_error("Unknown modifier");
        mod.Axis=args.value("axis",0);mod.Count=args.value("count",2);mod.Levels=args.value("levels",1);mod.Amount=args.value("amount",.1f);
        if(args.contains("offset")){const auto& v=args.at("offset");if(!v.is_array()||v.size()!=3)throw std::runtime_error("Expected three-component offset");for(int k=0;k<3;++k){mod.Offset[k]=v[k].get<float>();if(!std::isfinite(mod.Offset[k]))throw std::runtime_error("Invalid modifier offset");}}
        Model::AddModifier(changed,mod);
    }else if(action=="apply")Model::ApplyModifiers(changed);
    else if(action=="boolean"){
        int second=args.at("other").get<int>();if(second<0||size_t(second)>=objects.size()||second==index)throw std::runtime_error("Invalid boolean operand");std::string operation=args.value("operation",std::string("union"));int op=operation=="union"?0:operation=="difference"?1:operation=="intersection"?2:-1;if(op<0)throw std::runtime_error("Unknown boolean operation");auto result=Model::Boolean(changed,objects[size_t(second)],op);objects.push_back(std::move(result));return int(objects.size())-1;
    }else{
        auto m=Model::Editable(changed);std::set<uint32_t> selected;if(args.contains("selection")){if(!args["selection"].is_array()||args["selection"].size()>MAX_EDIT_VERTICES)throw std::runtime_error("Invalid element selection");for(const auto& v:args["selection"]){if(!v.is_number_integer()||v.get<int64_t>()<0||v.get<uint64_t>()>MAX_EDIT_INDICES)throw std::runtime_error("Invalid element index");selected.insert(v.get<uint32_t>());}}
        if(action=="extrude")Model::Extrude(m,selected,args.value("distance",.25f));
        else if(action=="weld")Model::Weld(m,selected,args.value("distance",.001f));
        else if(action=="delete")Model::DeleteElements(m,selected,args.value("mode",2));
        else if(action=="polygonize")Model::Polygonize(m);
        else if(action=="normals")Model::Triangulate(m);
        else throw std::runtime_error("Unknown mesh operation");
        Model::SetEditable(changed,std::move(m));
    }
    objects[size_t(index)]=std::move(changed);return index;
}
static int l_mesh_operation(lua_State* L){lua_Integer i=luaL_checkinteger(L,1);const char* action=luaL_checkstring(L,2);size_t n=0;const char* json=luaL_optlstring(L,3,"{}",&n);if(n>1024*1024||i<0||uint64_t(i)>=g_Objects.size()){lua_pushinteger(L,-1);return 1;}int result=ModelCommand(g_Objects,int(i),action,nlohmann::json::parse(json,json+n));lua_pushinteger(L,result);return 1;}
