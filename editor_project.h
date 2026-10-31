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
static std::string g_OpenedProject;
static uint64_t g_SavedDocumentHash=0;
static uint64_t CompleteDocumentHash();
static bool ProjectDirty(){return CompleteDocumentHash()!=g_SavedDocumentHash;}


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
static nlohmann::json SaveModelMesh(const StudioMesh& m){return {{"positions",m.Positions},{"indices",m.Indices},{"colors",m.Colors},{"uv",m.UV},{"faces",m.Faces},{"face_materials",m.FaceMaterials},{"seams",m.Seams}};}
static StudioMesh ReadModelMesh(const nlohmann::json& j){StudioMesh m;m.Positions=j.at("positions").get<std::vector<float>>();m.Indices=j.at("indices").get<std::vector<uint32_t>>();m.Colors=j.value("colors",std::vector<float>());m.UV=j.value("uv",std::vector<float>());m.Faces=j.at("faces").get<std::vector<std::vector<uint32_t>>>();m.FaceMaterials=j.value("face_materials",std::vector<uint32_t>());m.Seams=j.value("seams",std::vector<uint64_t>());Model::Check(m);for(float uv:m.UV)if(!std::isfinite(uv))throw std::runtime_error("Invalid UV value");Model::Triangulate(m);return m;}
static void SaveModelFields(nlohmann::json& j,const GameObject& o,const std::string& path){
    j["id"]=o.Id;
    if(!o.Vertices.empty())j["mesh"]=SaveModelMesh(Model::FromObject(o));
    j["modifiers"]=nlohmann::json::array();
    for(const auto& m:o.Modifiers)j["modifiers"].push_back({{"type",int(m.Type)},{"enabled",m.Enabled},{"axis",m.Axis},{"count",m.Count},{"levels",m.Levels},{"amount",m.Amount},{"offset",SaveVector(m.Offset)}});
    if(!o.Modifiers.empty())j["base_mesh"]=SaveModelMesh(o.BaseMesh);
    j["materials"]=nlohmann::json::array();for(const auto& m:o.Materials)j["materials"].push_back({{"name",m.Name},{"texture",m.Texture.empty()?"":RelativeAssetPath(m.Texture,path)},{"color",SaveVector(m.Color)},{"metallic",m.Metallic},{"roughness",m.Roughness}});
}
static void ReadModelFields(const nlohmann::json& j,GameObject& o,const std::string& path){
    if(j.contains("id")){if(!j.at("id").is_number_unsigned()&&!j.at("id").is_number_integer())throw std::runtime_error("Invalid object ID");auto id=j.at("id").get<int64_t>();if(id<=0)throw std::runtime_error("Invalid object ID");o.Id=uint64_t(id);}
    if(j.contains("mesh"))Model::ToObject(o,ReadModelMesh(j.at("mesh")));
    if(j.contains("modifiers")){if(!j["modifiers"].is_array()||j["modifiers"].size()>16)throw std::runtime_error("Too many modifiers");for(const auto& v:j["modifiers"]){StudioModifier m;int type=v.at("type").get<int>();if(type<0||type>3)throw std::runtime_error("Invalid modifier type");m.Type=StudioModifier::Kind(type);m.Enabled=v.at("enabled").get<bool>();m.Axis=int(ReadRange(v.at("axis"),0,2));m.Count=int(ReadRange(v.at("count"),2,64));m.Levels=int(ReadRange(v.at("levels"),1,4));m.Amount=ReadRange(v.at("amount"),.00001f,1000);ReadVector(v.at("offset"),m.Offset);o.Modifiers.push_back(m);}}
    if(!o.Modifiers.empty()){o.BaseMesh=ReadModelMesh(j.at("base_mesh"));Model::Evaluate(o);}
    if(j.contains("materials")){if(!j["materials"].is_array()||j["materials"].size()>64)throw std::runtime_error("Material limit exceeded");for(const auto& v:j["materials"]){StudioMaterial m;m.Name=v.at("name").get<std::string>();m.Texture=v.at("texture").get<std::string>();if(m.Name.size()>255||m.Texture.size()>511||m.Texture.find('\0')!=std::string::npos)throw std::runtime_error("Invalid material resource");ReadVector(v.at("color"),m.Color,true);m.Metallic=ReadRange(v.at("metallic"),0,1);m.Roughness=ReadRange(v.at("roughness"),0,1);if(!m.Texture.empty()&&!AbsolutePath(m.Texture))m.Texture=NormalizePath(PathDirectory(AbsoluteAssetPath(path))+"/"+m.Texture);o.Materials.push_back(m);}}
    if(!o.FaceMaterials.empty()){if(o.FaceMaterials.size()!=o.Faces.size())throw std::runtime_error("Invalid face material count");for(auto slot:o.FaceMaterials)if(slot>=o.Materials.size())throw std::runtime_error("Invalid material slot");}
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
        nlohmann::json root={{"schema",2},{"objects",nlohmann::json::array()},{"scripts",nlohmann::json::array()},
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
            SaveModelFields(j,o,path);
            root["objects"].push_back(std::move(j));
        }
        root["sounds"]=nlohmann::json::array();
        if(g_Sounds.size()>64)throw std::runtime_error("At most 64 sounds per project");
        for(const auto& sound:g_Sounds){
            if(sound.Name.size()>255||sound.Path.size()>511||sound.Name.find('\0')!=std::string::npos||sound.Path.find('\0')!=std::string::npos||!finiteRange(sound.Volume,0,2)||!finiteRange(sound.Pitch,.1f,4))throw std::runtime_error("Invalid sound settings");
            for(float p:sound.Position)if(!std::isfinite(p))throw std::runtime_error("Invalid sound position");
            root["sounds"].push_back({{"name",sound.Name},{"path",RelativeAssetPath(sound.Path,path)},{"position",SaveVector(sound.Position)},{"volume",sound.Volume},{"pitch",sound.Pitch},{"loop",sound.Loop},{"spatial",sound.Spatial},{"autoplay",sound.Autoplay}});
        }
        for(const auto& script:g_Studio.Scripts)if(script.Allocated)root["scripts"].push_back({{"name",script.Name},{"code",script.Code},{"open",script.Open}});
        const std::string document=root.dump(2);
        if(document.size()>32*1024*1024)throw std::runtime_error("Project exceeds 32 MiB limit");
        WriteAssetFile(temporary,document.data(),document.size());
        ReplaceAssetFile(temporary,path);
        g_OpenedProject=AbsoluteAssetPath(path);g_SavedDocumentHash=CompleteDocumentHash();
        AppendConsole("Saved project: "+path+"\n");return true;
    } catch(const std::exception& error) {
        std::remove(temporary.c_str());AppendConsole(std::string("Project save failed: ")+error.what()+"\n");return false;
    }
}
static bool LoadStudioProject(const std::string& path) {
    try {
        if(path.empty()||path.find('\0')!=std::string::npos)throw std::runtime_error("Invalid project path");
        auto data=ReadAssetFile(path,32*1024*1024);
        nlohmann::json root=nlohmann::json::parse(data.begin(),data.end());
        if(root.at("schema").get<int>()!=1&&root.at("schema").get<int>()!=2)throw std::runtime_error("Unsupported project schema");
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
            if(root.at("schema").get<int>()==2)ReadModelFields(j,o,path);
            for(const auto& previous:objects)if(previous.Id==o.Id)throw std::runtime_error("Duplicate stable object ID");
            objects.push_back(std::move(o));
        }
        auto scripts=std::unique_ptr<std::array<ScriptDocument,8>>(new std::array<ScriptDocument,8>());
        const auto& docs=root.at("scripts");if(!docs.is_array()||docs.size()>8)throw std::runtime_error("Invalid script count");
        for(size_t i=0;i<docs.size();++i){auto name=docs[i].at("name").get<std::string>(),code=docs[i].at("code").get<std::string>();if(name.size()>63||code.size()>65535||name.find('\0')!=std::string::npos||code.find('\0')!=std::string::npos)throw std::runtime_error("Invalid script document");auto& doc=(*scripts)[i];doc.Allocated=true;doc.Open=docs[i].value("open",true);std::snprintf(doc.Name,sizeof(doc.Name),"%s",name.c_str());std::snprintf(doc.Code,sizeof(doc.Code),"%s",code.c_str());}
        float top[3],bottom[3];ReadVector(root.at("sky_top"),top,true);ReadVector(root.at("sky_bottom"),bottom,true);
        int preset=root.at("sky_preset").get<int>();if(preset<0||preset>2)throw std::runtime_error("Unknown sky preset");
        float gravity=ReadRange(root.at("gravity"),-30,0);bool ground=root.at("ground").get<bool>();
        for(const auto& o:objects)ReserveObjectId(o.Id);
        std::vector<StudioSound> sounds;
        if(root.contains("sounds")){
            const auto& savedSounds=root.at("sounds");if(!savedSounds.is_array()||savedSounds.size()>64)throw std::runtime_error("Invalid sound count");
            for(const auto& j:savedSounds){StudioSound sound;sound.Name=j.at("name").get<std::string>();sound.Path=j.at("path").get<std::string>();
                if(sound.Name.size()>255||sound.Path.size()>511||sound.Name.find('\0')!=std::string::npos||sound.Path.find('\0')!=std::string::npos)throw std::runtime_error("Invalid audio path or name");
                ReadVector(j.at("position"),sound.Position);sound.Volume=ReadRange(j.at("volume"),0,2);sound.Pitch=ReadRange(j.at("pitch"),.1f,4);sound.Loop=j.at("loop").get<bool>();sound.Spatial=j.at("spatial").get<bool>();sound.Autoplay=j.at("autoplay").get<bool>();if(!AbsolutePath(sound.Path))sound.Path=NormalizePath(PathDirectory(AbsoluteAssetPath(path))+"/"+sound.Path);sounds.push_back(sound);}
        }
        // Commit only after the complete document has passed validation. Never execute loaded scripts.
        for(const auto& sound:g_Sounds){studio_audio_unload(sound.Id);}
        g_Sounds.swap(sounds);g_SelectedSound=-1;
        for(const auto& o:objects)ReserveObjectId(o.Id);
        g_Objects.swap(objects);g_Studio.Scripts.swap(*scripts);std::copy(top,top+3,g_SkyTop);std::copy(bottom,bottom+3,g_SkyBottom);g_SkyPreset=preset;
        g_Physics.Gravity=gravity;g_Physics.Ground=ground;g_Physics.Accumulator=0;g_PhysicsRunning=false;g_SimulationRunning=false;g_PlaySnapshot.clear();g_SelectedObjectIndex=-1;
        g_Studio.RequestDocument=-2;g_Studio.ActiveScript=0;g_Studio.HasSculptUndo=false;g_ObjectHistory.Clear();g_ObjectHistory.Capture(g_Objects);
        if(g_LuaState)ClearUpdateCallback(g_LuaState);
        g_OpenedProject=AbsoluteAssetPath(path);std::snprintf(g_ProjectPath,sizeof(g_ProjectPath),"%s",path.c_str());g_SavedDocumentHash=CompleteDocumentHash();
        AppendConsole("Loaded project without executing scripts: "+path+"\n");return true;
    } catch(const std::exception& error){AppendConsole(std::string("Project load failed: ")+error.what()+"\n");return false;}
}

static uint64_t CompleteDocumentHash(){
    uint64_t h=StudioDocumentHash();
    for(const auto& doc:g_Studio.Scripts)if(doc.Allocated){for(const char* c=doc.Name;*c;++c)h=(h^static_cast<unsigned char>(*c))*1099511628211ULL;for(const char* c=doc.Code;*c;++c)h=(h^static_cast<unsigned char>(*c))*1099511628211ULL;}
    return h;
}
static int g_PendingProjectAction=0;
static std::string g_PendingProjectPath;
static bool g_ExitRequested=false,g_OpenSaveAs=false;
static void PerformProjectAction(){int action=g_PendingProjectAction;g_PendingProjectAction=0;if(action==1)g_ExitRequested=true;else if(action==2)LoadStudioProject(g_PendingProjectPath);else if(action==3){g_Objects.clear();g_SelectedObjectIndex=-1;}}
static void RequestProjectAction(int action,const std::string& path=""){if(g_PhysicsRunning)StopSimulation();g_PendingProjectAction=action;g_PendingProjectPath=path;if(!ProjectDirty())PerformProjectAction();}
static bool SaveCurrentProject(){return SaveStudioProject(g_OpenedProject.empty()?std::string(g_ProjectPath):g_OpenedProject);}
static void RenderProjectDialogs(){
    if(g_PendingProjectAction)ImGui::OpenPopup("Unsaved project");
    if(ImGui::BeginPopupModal("Unsaved project",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("Save changes before continuing?");
        if(ImGui::Button("Save")){if(SaveCurrentProject()){PerformProjectAction();ImGui::CloseCurrentPopup();}}
        ImGui::SameLine();if(ImGui::Button("Discard")){PerformProjectAction();ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Cancel")){g_PendingProjectAction=0;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
    if(g_OpenSaveAs){ImGui::OpenPopup("Save Project As");g_OpenSaveAs=false;}
    if(ImGui::BeginPopupModal("Save Project As",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::InputText("Path",g_ProjectPath,sizeof(g_ProjectPath));
        if(ImGui::Button("Save")&&SaveStudioProject(g_ProjectPath))ImGui::CloseCurrentPopup();
        ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
}
