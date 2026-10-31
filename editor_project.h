#pragma once
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
static char g_ProjectPath[512]="project.lunoryx";

static nlohmann::json SaveVector(const float v[3]) { return {v[0],v[1],v[2]}; }
static void ReadVector(const nlohmann::json& j,float v[3],bool color=false) {
    if(!j.is_array()||j.size()!=3)throw std::runtime_error("Expected a three-component vector");
    for(int k=0;k<3;++k) {
        if(!j[k].is_number())throw std::runtime_error("Vector components must be numbers");
        double value=j[k].get<double>();
        if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max()||(color&&(value<0||value>1)))throw std::runtime_error("Invalid vector value");
        v[k]=float(value);
    }
}
static float ReadRange(const nlohmann::json& value,float low,float high) {
    if(!value.is_number())throw std::runtime_error("Expected a number");
    double number=value.get<double>();if(!std::isfinite(number)||number<low||number>high)throw std::runtime_error("Number outside allowed range");return float(number);
}
static bool SaveStudioProject(const std::string& path) {
    std::string temporary=path+".tmp";
    try {
        if(path.empty()||path.find('\0')!=std::string::npos)throw std::runtime_error("Invalid project path");
        if(g_Objects.size()>10000)throw std::runtime_error("Project object limit is 10000");
        size_t totalVertices=0;
        auto finiteRange=[](float value,float low,float high){return std::isfinite(value)&&value>=low&&value<=high;};
        for(const auto& o:g_Objects) {
            if(o.Name.size()>255||o.Name.find('\0')!=std::string::npos)throw std::runtime_error("Invalid object name");
            for(int k=0;k<3;++k)if(!std::isfinite(o.Position[k])||!std::isfinite(o.Rotation[k])||!std::isfinite(o.Scale[k])||!finiteRange(o.Color[k],0,1)||!finiteRange(o.Velocity[k],-10000,10000))throw std::runtime_error("Invalid transform, color or velocity; stop simulation before saving");
            if(!finiteRange(o.Metallic,0,1)||!finiteRange(o.Roughness,0,1)||!finiteRange(o.Mass,.01f,10000)||!finiteRange(o.Restitution,0,1)||!finiteRange(o.Friction,0,20)||!finiteRange(o.MoveSpeed,0,100)||!finiteRange(o.JumpSpeed,0,100))throw std::runtime_error("Invalid material or physics property");
            if(o.Vertices.size()/3>MAX_EDIT_VERTICES||o.Vertices.size()%3||o.Indices.size()>MAX_EDIT_INDICES||o.Indices.size()%3||(o.Vertices.empty()!=o.Indices.empty())||(!o.VertexColors.empty()&&o.VertexColors.size()!=o.Vertices.size()))throw std::runtime_error("Invalid mesh size");
            totalVertices+=o.Vertices.size()/3;if(totalVertices>1000000)throw std::runtime_error("Project mesh budget exceeded");
            for(float v:o.Vertices)if(!std::isfinite(v))throw std::runtime_error("Invalid vertex");
            for(float v:o.VertexColors)if(!finiteRange(v,0,1))throw std::runtime_error("Invalid vertex color");
            for(uint32_t i:o.Indices)if(i>=o.Vertices.size()/3)throw std::runtime_error("Invalid mesh index");
            if(o.Properties.size()>256)throw std::runtime_error("Too many custom properties");
        }
        for(int k=0;k<3;++k)if(!finiteRange(g_SkyTop[k],0,1)||!finiteRange(g_SkyBottom[k],0,1))throw std::runtime_error("Invalid sky color");
        if(g_SkyPreset<0||g_SkyPreset>2||!finiteRange(g_Physics.Gravity,-30,0))throw std::runtime_error("Invalid environment settings");
        nlohmann::json root={{"schema",1},{"objects",nlohmann::json::array()},{"scripts",nlohmann::json::array()},
            {"sky_top",SaveVector(g_SkyTop)},{"sky_bottom",SaveVector(g_SkyBottom)},{"sky_preset",g_SkyPreset},{"gravity",g_Physics.Gravity},{"ground",g_Physics.Ground}};
        for(const auto& o:g_Objects) {
            nlohmann::json j={{"name",o.Name},{"type",int(o.Type)},{"position",SaveVector(o.Position)},{"rotation",SaveVector(o.Rotation)},{"scale",SaveVector(o.Scale)},
                {"color",SaveVector(o.Color)},{"metallic",o.Metallic},{"roughness",o.Roughness},{"visible",o.Visible},{"wireframe",o.Wireframe},
                {"dynamic",o.Dynamic},{"character",o.Character},{"mass",o.Mass},{"bounce",o.Restitution},{"friction",o.Friction},{"speed",o.MoveSpeed},{"jump",o.JumpSpeed},{"velocity",SaveVector(o.Velocity)},
                {"vertices",o.Vertices},{"indices",o.Indices},{"vertex_colors",o.VertexColors},{"properties",nlohmann::json::object()}};
            for(const auto& property:o.Properties) {
                const auto& v=property.second;
                j["properties"][property.first]={{"kind",int(v.Type)},{"number",v.NumberValue},{"boolean",v.BoolValue},{"text",v.TextValue}};
            }
            root["objects"].push_back(std::move(j));
        }
        root["sounds"]=nlohmann::json::array();
        if(g_Sounds.size()>64)throw std::runtime_error("At most 64 sounds per project");
        for(const auto& sound:g_Sounds){
            if(sound.Name.size()>255||sound.Path.size()>511||sound.Name.find('\0')!=std::string::npos||sound.Path.find('\0')!=std::string::npos||!finiteRange(sound.Volume,0,2)||!finiteRange(sound.Pitch,.1f,4))throw std::runtime_error("Invalid sound settings");
            for(float p:sound.Position)if(!std::isfinite(p))throw std::runtime_error("Invalid sound position");
            root["sounds"].push_back({{"name",sound.Name},{"path",sound.Path},{"position",SaveVector(sound.Position)},{"volume",sound.Volume},{"pitch",sound.Pitch},{"loop",sound.Loop},{"spatial",sound.Spatial},{"autoplay",sound.Autoplay}});
        }
        for(const auto& script:g_Studio.Scripts)if(script.Allocated)root["scripts"].push_back({{"name",script.Name},{"code",script.Code},{"open",script.Open}});
        const std::string document=root.dump(2);
        if(document.size()>32*1024*1024)throw std::runtime_error("Project exceeds 32 MiB limit");
        std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file.write(document.data(),std::streamsize(document.size()));file.close();
        if(!file)throw std::runtime_error("Could not write project temporary file");
#ifdef _WIN32
        if(!MoveFileExA(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not replace project file");
#else
        if(std::rename(temporary.c_str(),path.c_str())!=0)throw std::runtime_error("Could not replace project file");
#endif
        AppendConsole("Saved project: "+path+"\n");return true;
    } catch(const std::exception& error) {
        std::remove(temporary.c_str());AppendConsole(std::string("Project save failed: ")+error.what()+"\n");return false;
    }
}
static bool LoadStudioProject(const std::string& path) {
    try {
        if(path.empty()||path.find('\0')!=std::string::npos)throw std::runtime_error("Invalid project path");
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        auto size=file.tellg();if(!file||size<0||size>32*1024*1024)throw std::runtime_error("Project missing or larger than 32 MiB");file.seekg(0);
        nlohmann::json root;file>>root;
        if(root.at("schema").get<int>()!=1)throw std::runtime_error("Unsupported project schema");
        const auto& saved=root.at("objects");if(!saved.is_array()||saved.size()>10000)throw std::runtime_error("Invalid object count");
        std::vector<GameObject> objects;objects.reserve(saved.size());size_t totalVertices=0;
        for(const auto& j:saved) {
            std::string name=j.at("name").get<std::string>();if(name.size()>255||name.find('\0')!=std::string::npos)throw std::runtime_error("Object name too long");
            int type=j.at("type").get<int>();if(type<0||type>4)throw std::runtime_error("Unknown primitive");
            GameObject o(name,MeshType(type));
            ReadVector(j.at("position"),o.Position);ReadVector(j.at("rotation"),o.Rotation);ReadVector(j.at("scale"),o.Scale);ReadVector(j.at("color"),o.Color,true);ReadVector(j.at("velocity"),o.Velocity);
            for(float v:o.Velocity)if(std::abs(v)>10000)throw std::runtime_error("Velocity outside allowed range");
            o.Metallic=ReadRange(j.at("metallic"),0,1);o.Roughness=ReadRange(j.at("roughness"),0,1);
            o.Visible=j.at("visible").get<bool>();o.Wireframe=j.at("wireframe").get<bool>();o.Dynamic=j.at("dynamic").get<bool>();o.Character=j.at("character").get<bool>();
            o.Mass=ReadRange(j.at("mass"),.01f,10000);o.Restitution=ReadRange(j.at("bounce"),0,1);o.Friction=ReadRange(j.at("friction"),0,20);o.MoveSpeed=ReadRange(j.at("speed"),0,100);o.JumpSpeed=ReadRange(j.at("jump"),0,100);
            if(!j.at("vertices").is_array()||j.at("vertices").size()>MAX_EDIT_VERTICES*3||!j.at("indices").is_array()||j.at("indices").size()>MAX_EDIT_INDICES)throw std::runtime_error("Mesh exceeds limits");
            for(const auto& index:j.at("indices"))if(!index.is_number_integer()||index.get<double>()<0||index.get<double>()>4294967295.0)throw std::runtime_error("Invalid mesh index type");
            o.Vertices=j.at("vertices").get<std::vector<float>>();o.Indices=j.at("indices").get<std::vector<uint32_t>>();o.VertexColors=j.at("vertex_colors").get<std::vector<float>>();
            if(o.Vertices.size()%3||o.Indices.size()%3||(!o.Vertices.empty()&&o.Indices.empty())||(o.Vertices.empty()&&!o.Indices.empty())||(!o.VertexColors.empty()&&o.VertexColors.size()!=o.Vertices.size()))throw std::runtime_error("Malformed mesh");
            totalVertices+=o.Vertices.size()/3;if(totalVertices>1000000)throw std::runtime_error("Project mesh budget exceeded");
            for(float v:o.Vertices)if(!std::isfinite(v))throw std::runtime_error("Invalid vertex");
            for(float v:o.VertexColors)if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Invalid vertex color");
            for(uint32_t i:o.Indices)if(i>=o.Vertices.size()/3)throw std::runtime_error("Mesh index outside vertex range");
            const auto& properties=j.at("properties");if(!properties.is_object()||properties.size()>256)throw std::runtime_error("Invalid properties");
            for(auto it=properties.begin();it!=properties.end();++it) {
                ObjectProperty v;int kind=it.value().at("kind").get<int>();if(kind<0||kind>2)throw std::runtime_error("Invalid property kind");
                v.Type=ObjectProperty::Kind(kind);v.NumberValue=it.value().at("number").get<double>();v.BoolValue=it.value().at("boolean").get<bool>();v.TextValue=it.value().at("text").get<std::string>();
                if(!SetProperty(o,it.key(),v))throw std::runtime_error("Invalid property value");
            }
            objects.push_back(std::move(o));
        }
        auto scripts=std::unique_ptr<std::array<ScriptDocument,8>>(new std::array<ScriptDocument,8>());
        const auto& docs=root.at("scripts");if(!docs.is_array()||docs.size()>8)throw std::runtime_error("Invalid script count");
        for(size_t i=0;i<docs.size();++i){auto name=docs[i].at("name").get<std::string>(),code=docs[i].at("code").get<std::string>();if(name.size()>63||code.size()>65535||name.find('\0')!=std::string::npos||code.find('\0')!=std::string::npos)throw std::runtime_error("Invalid script document");auto& doc=(*scripts)[i];doc.Allocated=true;doc.Open=docs[i].value("open",true);std::snprintf(doc.Name,sizeof(doc.Name),"%s",name.c_str());std::snprintf(doc.Code,sizeof(doc.Code),"%s",code.c_str());}
        float top[3],bottom[3];ReadVector(root.at("sky_top"),top,true);ReadVector(root.at("sky_bottom"),bottom,true);
        int preset=root.at("sky_preset").get<int>();if(preset<0||preset>2)throw std::runtime_error("Unknown sky preset");
        float gravity=ReadRange(root.at("gravity"),-30,0);bool ground=root.at("ground").get<bool>();
        std::vector<StudioSound> sounds;
        if(root.contains("sounds")){
            const auto& savedSounds=root.at("sounds");if(!savedSounds.is_array()||savedSounds.size()>64)throw std::runtime_error("Invalid sound count");
            for(const auto& j:savedSounds){StudioSound sound;sound.Name=j.at("name").get<std::string>();sound.Path=j.at("path").get<std::string>();
                if(sound.Name.size()>255||sound.Path.size()>511||sound.Name.find('\0')!=std::string::npos||sound.Path.find('\0')!=std::string::npos)throw std::runtime_error("Invalid audio path or name");
                ReadVector(j.at("position"),sound.Position);sound.Volume=ReadRange(j.at("volume"),0,2);sound.Pitch=ReadRange(j.at("pitch"),.1f,4);sound.Loop=j.at("loop").get<bool>();sound.Spatial=j.at("spatial").get<bool>();sound.Autoplay=j.at("autoplay").get<bool>();sounds.push_back(sound);}
        }
        // Commit only after the complete document has passed validation. Never execute loaded scripts.
        for(const auto& sound:g_Sounds){studio_audio_unload(sound.Id);}
        g_Sounds.swap(sounds);g_SelectedSound=-1;
        g_Objects.swap(objects);g_Studio.Scripts.swap(*scripts);std::copy(top,top+3,g_SkyTop);std::copy(bottom,bottom+3,g_SkyBottom);g_SkyPreset=preset;
        g_Physics.Gravity=gravity;g_Physics.Ground=ground;g_Physics.Accumulator=0;g_PhysicsRunning=false;g_SimulationRunning=false;g_PlaySnapshot.clear();g_SelectedObjectIndex=-1;
        g_Studio.RequestDocument=-2;g_Studio.ActiveScript=0;g_Studio.HasSculptUndo=false;g_ObjectHistory.Clear();g_ObjectHistory.Capture(g_Objects);
        if(g_LuaState)ClearUpdateCallback(g_LuaState);
        AppendConsole("Loaded project without executing scripts: "+path+"\n");return true;
    } catch(const std::exception& error){AppendConsole(std::string("Project load failed: ")+error.what()+"\n");return false;}
}
