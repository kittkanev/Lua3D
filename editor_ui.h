#pragma once
#include "imgui_internal.h"
#include "viewport_target.h"
#include "editor_docs.h"
#include <array>
#include <fstream>

struct ScriptDocument {
    bool Open=false,Allocated=false;
    char Name[64]={};
    char Code[65536]={};
};
struct StudioState {
    bool ShowExplorer=true,ShowProperties=true,ShowTools=true,ShowOutput=true,ShowDocs=false;
    bool FullscreenCode=false,WasFullscreen=false,SceneActive=true,SceneHovered=false;
    bool SculptEnabled=false,HasSculptUndo=false,BrushHit=false,ResetLayout=false;
    int ActiveScript=0,RequestDocument=-2,RequestTool=-1,BrushMode=0;
    float BrushRadius=.4f,BrushStrength=.1f;
    float Snap=.5f;
    int TransformMode=1;
    bool LocalTransform=false,TransformSnap=false,TransformDragging=false;
    ImVec2 SceneMin={0,0},SceneSize={1,1};
    Vec3 BrushWorld={0,0,0};
    CharacterConfig Character;
    std::array<ScriptDocument,8> Scripts;
};
static StudioState g_Studio;
static bool g_ShowUV=false;
static GameObject g_SculptUndo("Sculpt Undo");
static GLuint g_LogoTexture=0;
#include "editor_project.h"
#include "game_package.h"

static void SelectObject(int index,bool toggle=false) {
    if(index<0||size_t(index)>=g_Objects.size())return;
    if(!toggle)for(auto& o:g_Objects)o.Selected=false;
    g_Objects[size_t(index)].Selected=toggle?!g_Objects[size_t(index)].Selected:true;
    g_SelectedObjectIndex=g_Objects[size_t(index)].Selected?index:-1;
}
static void NewScript() {
    for(size_t i=0;i<g_Studio.Scripts.size();++i)if(!g_Studio.Scripts[i].Allocated) {
        auto& script=g_Studio.Scripts[i];script.Open=true;script.Allocated=true;script.Code[0]='\0';
        std::snprintf(script.Name,sizeof(script.Name),"Script %d.lua",int(i)+1);
        g_Studio.ActiveScript=int(i);g_Studio.RequestDocument=int(i);return;
    }
    AppendConsole("All eight script slots are in use. Close a tab to create another.\n");
}
static void InitializeStudio() {
    auto& main=g_Studio.Scripts[0];main.Open=true;main.Allocated=true;
    std::snprintf(main.Name,sizeof(main.Name),"Main.lua");
    std::snprintf(main.Code,sizeof(main.Code),"%s",g_ScriptBuffer);
}
static bool SceneContains(float x,float y) {
    return g_Studio.SceneActive&&x>=g_Studio.SceneMin.x&&y>=g_Studio.SceneMin.y&&x<g_Studio.SceneMin.x+g_Studio.SceneSize.x&&y<g_Studio.SceneMin.y+g_Studio.SceneSize.y;
}
static void BuildStudioLayout(ImGuiID dockspace,const ImGuiViewport* viewport) {
    if(ImGui::DockBuilderGetNode(dockspace)&&!g_Studio.ResetLayout)return;
    g_Studio.ResetLayout=false;
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace,ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace,viewport->WorkSize);
    ImGuiID center=dockspace,left,right,bottom,properties;
    ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,.24f,&right,&center);
    ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,.23f,&left,&center);
    ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.24f,&bottom,&center);
    ImGui::DockBuilderSplitNode(right,ImGuiDir_Down,.58f,&properties,&right);
    ImGui::DockBuilderDockWindow("Workspace",center);
    ImGui::DockBuilderDockWindow("Explorer",right);
    ImGui::DockBuilderDockWindow("Properties",properties);
    ImGui::DockBuilderDockWindow("Tools",left);
    ImGui::DockBuilderDockWindow("Output",bottom);
    ImGui::DockBuilderDockWindow("API Docs",bottom);
    ImGui::DockBuilderFinish(dockspace);
}
static void StudioMenu() {
    if(ImGui::BeginMenu("Edit")) {
        ImGui::BeginDisabled(g_PhysicsRunning);
        if(ImGui::MenuItem("Undo scene edit","Ctrl+Z")){if(g_ObjectHistory.Undo(g_Objects)){g_SelectedObjectIndex=-1;g_SimulationRunning=false;}}
        if(ImGui::MenuItem("Redo scene edit","Ctrl+Y")){if(g_ObjectHistory.Redo(g_Objects)){g_SelectedObjectIndex=-1;g_SimulationRunning=false;}}
        ImGui::EndDisabled();ImGui::EndMenu();
    }
    if(ImGui::MenuItem("UV"))g_ShowUV=true;
    if(ImGui::MenuItem("Audio"))g_ShowAudio=true;
    if(ImGui::MenuItem("Settings"))g_Preferences.ShowSettings=true;
    if(ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("Explorer",nullptr,&g_Studio.ShowExplorer);
        ImGui::MenuItem("Properties",nullptr,&g_Studio.ShowProperties);
        ImGui::MenuItem("Tools",nullptr,&g_Studio.ShowTools);
        ImGui::MenuItem("Output",nullptr,&g_Studio.ShowOutput);
        ImGui::MenuItem("API Docs",nullptr,&g_Studio.ShowDocs);
        if(ImGui::MenuItem("Reset Layout"))g_Studio.ResetLayout=true;
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Scripts")) {
        if(ImGui::MenuItem("New Script"))NewScript();
        if(ImGui::MenuItem("Run Active Script","F5")&&g_Studio.Scripts[g_Studio.ActiveScript].Open)ExecuteLuaScript(g_Studio.Scripts[g_Studio.ActiveScript].Code);
        if(ImGui::MenuItem("Full Screen Editor","F11",&g_Studio.FullscreenCode)&&g_Studio.FullscreenCode)g_Studio.RequestDocument=g_Studio.ActiveScript;
        if(ImGui::MenuItem("Delete Active Script")){g_Studio.Scripts[g_Studio.ActiveScript].Allocated=false;g_Studio.Scripts[g_Studio.ActiveScript].Open=false;}
        ImGui::EndMenu();
    }
    if(ImGui::Button(g_PhysicsRunning?"Stop Simulation":"Play Simulation")) {
        if(g_PhysicsRunning)StopSimulation();else StartSimulation();
    }
    ImGui::SameLine();
}
static Vec3 CameraVector(Vec3 v,float pitch,float yaw) {
    const float toRad=-PI/180;
    float a=pitch*toRad,c=std::cos(a),s=std::sin(a),y=c*v.y-s*v.z;
    v.z=s*v.y+c*v.z;v.y=y;
    a=yaw*toRad;c=std::cos(a);s=std::sin(a);float x=c*v.x+s*v.z;
    v.z=-s*v.x+c*v.z;v.x=x;return v;
}
#include "editor_modeling.h"
#include "editor_uv.h"
#include "editor_game.h"
#include "editor_gizmo.h"

static void SceneInteraction(float distance,float pitch,float yaw,const float target[3],float dt) {
    auto& io=ImGui::GetIO();
    if(!g_Studio.SceneHovered||g_Modeling.Edit)return;
    Vec3 camera=CameraVector({0,0,distance},pitch,yaw);
    camera.x+=target[0];camera.y+=target[1];camera.z+=target[2];
    float nx=(io.MousePos.x-g_Studio.SceneMin.x)/g_Studio.SceneSize.x*2-1;
    float ny=1-(io.MousePos.y-g_Studio.SceneMin.y)/g_Studio.SceneSize.y*2;
    float tangent=std::tan(g_Preferences.Fov*PI/360),aspect=g_Studio.SceneSize.x/g_Studio.SceneSize.y;
    Vec3 ray=CameraVector({nx*aspect*tangent,ny*tangent,-1},pitch,yaw);
    float length=float(std::sqrt(Dot(ray,ray)));ray.x/=length;ray.y/=length;ray.z/=length;
    if(g_Studio.SculptEnabled&&g_SelectedObjectIndex>=0&&size_t(g_SelectedObjectIndex)<g_Objects.size()&&!g_PhysicsRunning) {
        auto& object=g_Objects[size_t(g_SelectedObjectIndex)];
        if(object.Vertices.empty()){if(!ImGui::IsMouseDown(0)||!MakeEditable(object))return;}
        if(!object.Modifiers.empty())return;
        Vec3 hit;float nearest=10000;
        if(RaycastObject(object,camera,ray,hit,nearest)) {
            g_Studio.BrushHit=TransformPoint(object,hit,g_Studio.BrushWorld);
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)){g_SculptUndo=object;g_Studio.HasSculptUndo=true;}
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float strength=g_Studio.BrushStrength*std::min(dt,.05f)*12;
                if(io.KeyShift)strength=-strength;
                if(!SculptBrush(object,hit,g_Studio.BrushRadius,strength,g_Studio.BrushMode))AppendConsole("Sculpt rejected: invalid brush or geometry limit.\n");
            }
        }
    } else if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&!ImGuizmo::IsOver()&&!ImGuizmo::IsUsing()) {
        int selected=-1;float nearest=10000;Vec3 hit;
        for(size_t i=0;i<g_Objects.size();++i)if(g_Objects[i].Visible&&RaycastObject(g_Objects[i],camera,ray,hit,nearest))selected=int(i);
        if(selected>=0)SelectObject(selected,io.KeyCtrl);
        else if(!io.KeyCtrl){for(auto& o:g_Objects)o.Selected=false;g_SelectedObjectIndex=-1;}
    }
}
static void RenderWorkspace(float distance,float pitch,float yaw,const float target[3],float dt) {
    const auto* viewport=ImGui::GetMainViewport();
    ImGuiWindowFlags flags=0;
    if(g_Studio.FullscreenCode) {
        ImGui::SetNextWindowDockID(0,ImGuiCond_Always);
        ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);
        flags=ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize;
    } else if(g_Studio.WasFullscreen)g_Studio.ResetLayout=true;
    g_Studio.WasFullscreen=g_Studio.FullscreenCode;
    g_Studio.SceneActive=false;g_Studio.BrushHit=false;
    ImGui::SetNextWindowBgAlpha(1.0f);
    if(ImGui::Begin("Workspace",nullptr,flags)) {
        if(ImGui::Button("+ Script")) { NewScript(); }
        ImGui::SameLine();
        if(ImGui::Button(g_Studio.FullscreenCode?"Restore (F11)":"Full Screen (F11)")){g_Studio.FullscreenCode=!g_Studio.FullscreenCode;if(g_Studio.FullscreenCode)g_Studio.RequestDocument=g_Studio.ActiveScript;}
        ImGui::SameLine();if(ImGui::Button("API Reference")){g_Studio.ShowDocs=true;g_Studio.FullscreenCode=false;}
        if(ImGui::BeginTabBar("StudioDocuments",ImGuiTabBarFlags_Reorderable|ImGuiTabBarFlags_FittingPolicyScroll)) {
            ImGuiTabItemFlags sceneFlags=g_Studio.RequestDocument==-2?ImGuiTabItemFlags_SetSelected:0;
            if(ImGui::BeginTabItem("Scene",nullptr,sceneFlags)) {
                if(g_Studio.RequestDocument==-2)g_Studio.RequestDocument=-1;
                const char* modes[]={"Select (Q)","Move (W)","Rotate (E)","Scale (R)"};
                for(int mode=0;mode<4;++mode){if(mode)ImGui::SameLine();if(ImGui::RadioButton(modes[mode],g_Studio.TransformMode==mode)){g_Studio.TransformMode=mode;g_Studio.SculptEnabled=false;}}
                ImGui::SameLine();ImGui::Checkbox("Local",&g_Studio.LocalTransform);ImGui::SameLine();ImGui::Checkbox("Snap",&g_Studio.TransformSnap);
                ImGui::TextDisabled(g_Studio.SculptEnabled?"SCULPT: LMB brush | Shift: invert | RMB orbit":"LMB select | Ctrl+click multi-select | RMB orbit | MMB pan | Wheel zoom");
                g_Studio.SceneMin=ImGui::GetCursorScreenPos();g_Studio.SceneSize=ImGui::GetContentRegionAvail();
                if(g_Studio.SceneSize.x<1) { g_Studio.SceneSize.x=1; }
                if(g_Studio.SceneSize.y<1) { g_Studio.SceneSize.y=1; }
                g_Studio.SceneActive=true;
                ImGui::Image(ImTextureID(g_ViewportTarget.Color),g_Studio.SceneSize,ImVec2(0,1),ImVec2(1,0));
                g_Studio.SceneHovered=ImGui::IsItemHovered();
                if(!g_Modeling.Edit)RenderTransformGizmo(distance,pitch,yaw,target);
                DrawModelSelection(distance,pitch,yaw,target);
                SceneInteraction(distance,pitch,yaw,target,dt);
                ImGui::EndTabItem();
            }
            for(size_t i=0;i<g_Studio.Scripts.size();++i) {
                auto& doc=g_Studio.Scripts[i];if(!doc.Open)continue;
                ImGui::PushID(int(i));
                ImGuiTabItemFlags tabFlags=g_Studio.RequestDocument==int(i)?ImGuiTabItemFlags_SetSelected:0;
                char tabLabel[96];std::snprintf(tabLabel,sizeof(tabLabel),"%s###Script%d",doc.Name,int(i));
                if(ImGui::BeginTabItem(tabLabel,&doc.Open,tabFlags)) {
                    g_Studio.ActiveScript=int(i);if(g_Studio.RequestDocument==int(i))g_Studio.RequestDocument=-1;
                    if(ImGui::Button("Run (F5)")) { ExecuteLuaScript(doc.Code); }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(200);ImGui::InputText("Name",doc.Name,sizeof(doc.Name));
                    ImGui::InputTextMultiline("##Code",doc.Code,sizeof(doc.Code),ImVec2(-FLT_MIN,-FLT_MIN),ImGuiInputTextFlags_AllowTabInput);
                    ImGui::EndTabItem();
                }
                ImGui::PopID();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
static void RenderExplorer() {
    if(!g_Studio.ShowExplorer)return;
    if(ImGui::Begin("Explorer",&g_Studio.ShowExplorer)) {
        ImGui::InputTextWithHint("##Search","Filter objects...",g_OutlinerFilter,sizeof(g_OutlinerFilter));
        if(ImGui::TreeNodeEx("Workspace",ImGuiTreeNodeFlags_DefaultOpen)) {
            for(size_t i=0;i<g_Objects.size();++i) {
                auto& o=g_Objects[i];if(g_OutlinerFilter[0]&&o.Name.find(g_OutlinerFilter)==std::string::npos)continue;
                ImGui::PushID(int(o.Id));
                char label[256];std::snprintf(label,sizeof(label),"[%s] %s%s",o.Character?"Character":(o.Vertices.empty()?GetMeshTypeName(o.Type):"Mesh"),o.Name.c_str(),o.Visible?"":" (hidden)");
                if(ImGui::Selectable(label,o.Selected))SelectObject(int(i),ImGui::GetIO().KeyCtrl);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if(ImGui::TreeNodeEx("Scripts",ImGuiTreeNodeFlags_DefaultOpen)) {
            for(size_t i=0;i<g_Studio.Scripts.size();++i)if(g_Studio.Scripts[i].Allocated) {
                ImGui::PushID(int(i));if(ImGui::Selectable(g_Studio.Scripts[i].Name,g_Studio.ActiveScript==int(i)&&!g_Studio.SceneActive)){g_Studio.Scripts[i].Open=true;g_Studio.RequestDocument=int(i);}ImGui::PopID();
            }
            if(ImGui::Button("New Script")) { NewScript(); }
            ImGui::TreePop();
        }
        if(ImGui::Selectable("Sounds / Audio"))g_ShowAudio=true;
        if(ImGui::Selectable("Environment / Skybox")){g_Studio.ShowTools=true;g_Studio.RequestTool=3;}
        if(ImGui::Selectable("Documentation"))g_Studio.ShowDocs=true;
        ImGui::TextDisabled("Ctrl+click: multi-select");
    }
    ImGui::End();
}
static void EditVector3(const char* label,float values[3],float speed,float low=-1000000,float high=1000000) {
    float edited[3]={values[0],values[1],values[2]};
    if(ImGui::DragFloat3(label,edited,speed,low,high,"%.3f",ImGuiSliderFlags_AlwaysClamp)) {
        bool valid=true;for(float value:edited)valid&=std::isfinite(value)&&value>=low&&value<=high;
        if(valid)std::copy(edited,edited+3,values);
    }
}
static void EditColor(const char* label,float values[3]) {
    float edited[3]={values[0],values[1],values[2]};
    if(ImGui::ColorEdit3(label,edited)) {
        bool valid=true;for(float value:edited)valid&=std::isfinite(value);
        if(valid)for(int k=0;k<3;++k)values[k]=std::max(0.0f,std::min(1.0f,edited[k]));
    }
}
static void EditScalar(const char* label,float& value,float low,float high) {
    float edited=value;if(ImGui::SliderFloat(label,&edited,low,high,"%.3f",ImGuiSliderFlags_AlwaysClamp)&&std::isfinite(edited))value=std::max(low,std::min(high,edited));
}
static void RenderProperties(float target[3]) {
    if(!g_Studio.ShowProperties)return;
    if(ImGui::Begin("Properties",&g_Studio.ShowProperties)) {
        int index=g_SelectedObjectIndex;
        if(index>=0&&size_t(index)<g_Objects.size()) {
            auto& o=g_Objects[size_t(index)];char name[256];std::snprintf(name,sizeof(name),"%s",o.Name.c_str());
            if(ImGui::InputText("Name",name,sizeof(name)))o.Name=name;
            ImGui::TextDisabled("Index %d | %s",index,o.Character?"Character":(o.Vertices.empty()?"Primitive":"Editable Mesh"));
            ImGui::Checkbox("Visible",&o.Visible);ImGui::SameLine();ImGui::Checkbox("Wireframe",&o.Wireframe);
            EditVector3("Position",o.Position,.05f);EditVector3("Rotation (deg)",o.Rotation,.5f,-36000,36000);
            EditVector3("Scale",o.Scale,.02f,.01f,100);
            EditColor("Base Color",o.Color);EditScalar("Metallic",o.Metallic,0,1);EditScalar("Roughness",o.Roughness,0,1);
            if(ImGui::CollapsingHeader("Physics / Character",ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Dynamic",&o.Dynamic);
                if(ImGui::Checkbox("Character Controller",&o.Character)&&o.Character){o.Dynamic=true;o.Restitution=0;}EditScalar("Mass",o.Mass,.01f,10000);
                EditScalar("Bounce",o.Restitution,0,1);EditScalar("Friction",o.Friction,0,20);
                EditVector3("Velocity",o.Velocity,.1f,-10000,10000);
                if(o.Character){EditScalar("Move Speed",o.MoveSpeed,.1f,30);EditScalar("Jump Speed",o.JumpSpeed,0,20);}
            }
            if(ImGui::CollapsingHeader("Custom Properties",ImGuiTreeNodeFlags_DefaultOpen)) {
                for(auto it=o.Properties.begin();it!=o.Properties.end();) {
                    ImGui::PushID(it->first.c_str());auto& value=it->second;
                    if(value.Type==ObjectProperty::Number) {
                        double number=value.NumberValue;
                        if(ImGui::InputDouble(it->first.c_str(),&number)&&std::isfinite(number)&&std::abs(number)<=std::numeric_limits<float>::max())value.NumberValue=number;
                    }
                    else if(value.Type==ObjectProperty::Boolean)ImGui::Checkbox(it->first.c_str(),&value.BoolValue);
                    else {char text[1025];std::snprintf(text,sizeof(text),"%s",value.TextValue.c_str());if(ImGui::InputText(it->first.c_str(),text,sizeof(text)))value.TextValue=text;}
                    ImGui::SameLine();bool remove=ImGui::SmallButton("X");ImGui::PopID();
                    if(remove)it=o.Properties.erase(it);else ++it;
                }
                static char propertyName[65]="Health";static int type=0;
                ImGui::InputText("Property Name",propertyName,sizeof(propertyName));
                const char* types[]={"Number","Boolean","Text"};ImGui::Combo("Type",&type,types,3);
                if(ImGui::Button("Add Property")){ObjectProperty value;value.Type=ObjectProperty::Kind(type);if(!SetProperty(o,propertyName,value))AppendConsole("Property name must contain 1..64 bytes.\n");}
            }
            RenderMaterialTools(o);
            if(!o.Vertices.empty())ImGui::Text("%d vertices | %d triangles",int(o.Vertices.size()/3),int(o.Indices.size()/3));
            if(ImGui::Button("Focus")) { std::copy(o.Position,o.Position+3,target); }
            ImGui::SameLine();
            if(ImGui::Button("Duplicate")){int copy=DuplicateObject(g_Objects,index,o.Name+" Copy");SelectObject(copy);}
            ImGui::SameLine();if(ImGui::Button("Delete")){g_Objects.erase(g_Objects.begin()+index);g_SelectedObjectIndex=-1;}
        } else ImGui::TextWrapped("Select an object in Explorer or click it in the Scene tab.");
    }
    ImGui::End();
}
static void SpawnPrimitive(MeshType type) {
    g_Objects.emplace_back(GetMeshTypeName(type),type);SelectObject(int(g_Objects.size())-1);
}
static void RenderTools(float& distance,float& pitch,float& yaw,float target[3]) {
    if(!g_Studio.ShowTools)return;
    if(ImGui::Begin("Tools",&g_Studio.ShowTools)&&ImGui::BeginTabBar("ToolTabs")) {
        const char* labels[]={"Build","Sculpt","Character","Environment"};
        for(int tab=0;tab<4;++tab) {
            ImGuiTabItemFlags flags=g_Studio.RequestTool==tab?ImGuiTabItemFlags_SetSelected:0;
            if(!ImGui::BeginTabItem(labels[tab],nullptr,flags))continue;
            if(g_Studio.RequestTool==tab)g_Studio.RequestTool=-1;
            if(tab==0) {
                for(int type=0;type<5;++type){if(ImGui::Button(GetMeshTypeName(MeshType(type)),ImVec2(-1,0)))SpawnPrimitive(MeshType(type));}
                ImGui::Separator();EditScalar("Grid Snap",g_Studio.Snap,.01f,10);
                if(ImGui::Button("Snap Selected"))for(auto& o:g_Objects)if(o.Selected)SnapObject(o,g_Studio.Snap);
                if(ImGui::Button("Join Selected Meshes")) {
                    std::vector<int> selection;for(size_t i=0;i<g_Objects.size();++i)if(g_Objects[i].Selected)selection.push_back(int(i));
                    int merged=MergeObjects(g_Objects,selection,"Joined Mesh");if(merged>=0)SelectObject(merged);else AppendConsole("Join needs at least two valid objects within the mesh limit.\n");
                }
                RenderModelTools();
                ImGui::TextWrapped("Join bakes transforms and colors into one mesh. It is not a boolean union.");
                ImGui::Separator();EditScalar("Distance",distance,1.5f,80);EditScalar("Pitch",pitch,-89,89);EditScalar("Yaw",yaw,-180,180);yaw=WrapYaw(yaw);
                EditVector3("Camera Target",target,.1f);
            } else if(tab==1) {
                ImGui::BeginDisabled(g_PhysicsRunning);
                ImGui::Checkbox("Enable Surface Brush",&g_Studio.SculptEnabled);
                const char* brushes[]={"Inflate / Deflate","Smooth","Flatten Y"};ImGui::Combo("Brush",&g_Studio.BrushMode,brushes,3);
                ImGui::SliderFloat("Radius (local)",&g_Studio.BrushRadius,.02f,5);ImGui::SliderFloat("Strength",&g_Studio.BrushStrength,.01f,1);
                int i=g_SelectedObjectIndex;bool valid=i>=0&&size_t(i)<g_Objects.size();ImGui::BeginDisabled(!valid);
                if(ImGui::Button("Convert / Subdivide")) {
                    g_SculptUndo=g_Objects[size_t(i)];g_Studio.HasSculptUndo=true;
                    if(!MakeEditable(g_Objects[size_t(i)],1))AppendConsole("Subdivision rejected: 200,000 vertex limit.\n");
                }
                if(ImGui::Button("Undo Last Stroke")&&g_Studio.HasSculptUndo&&g_Objects[size_t(i)].Id==g_SculptUndo.Id){g_Objects[size_t(i)]=g_SculptUndo;g_Studio.HasSculptUndo=false;}
                ImGui::EndDisabled();ImGui::EndDisabled();
                ImGui::TextWrapped("Select a mesh, subdivide for detail, then brush its surface with LMB. Shift inverts Inflate. Brush size is object-local.");
            } else if(tab==2) {
                RenderGameSettings();
                auto& c=g_Studio.Character;
                ImGui::SliderFloat("Height",&c.Height,1,4);ImGui::SliderFloat("Body Width",&c.Width,.3f,1.5f);ImGui::SliderFloat("Head Size",&c.HeadSize,.15f,std::min(.8f,c.Height*.4f));
                ImGui::ColorEdit3("Outfit",c.BodyColor);ImGui::ColorEdit3("Skin",c.SkinColor);
                if(ImGui::Button("Create Character")){int i=CreateCharacter(g_Objects,"Player",c);if(i>=0)SelectObject(i);else AppendConsole("Invalid character proportions.\n");}
                ImGui::TextWrapped("Press Play Simulation. In the Scene tab use WASD and Space. Adjust movement in Properties. This is an editable mesh, not a skeletal rig.");
            } else {
                const char* skies[]={"Day","Sunset","Night"};int preset=g_SkyPreset;if(ImGui::Combo("Skybox",&preset,skies,3))ApplySkyPreset(preset);
                EditColor("Sky Top",g_SkyTop);EditColor("Horizon",g_SkyBottom);
                EditScalar("Gravity",g_Physics.Gravity,-30,0);ImGui::Checkbox("Ground at Y=0",&g_Physics.Ground);
                ImGui::Checkbox("Grid",&g_ShowGrid);ImGui::Checkbox("World Axes",&g_ShowAxes);ImGui::Checkbox("All Wireframe",&g_GlobalWireframe);
                ImGui::TextWrapped("Procedural sky presets need no external textures. Physics uses axis-aligned collision boxes.");
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
static void RenderOutput() {
    if(!g_Studio.ShowOutput)return;
    if(ImGui::Begin("Output",&g_Studio.ShowOutput)) {
        if(ImGui::Button("Clear")) { ClearConsole(); }
        ImGui::SameLine();ImGui::Checkbox("Auto-scroll",&g_AutoScrollConsole);
        ImGui::BeginChild("ConsoleScroll",ImVec2(0,0),ImGuiChildFlags_None,ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGuiListClipper clipper;clipper.Begin(int(g_ConsoleLines.size()));
        while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i)ImGui::TextUnformatted(g_ConsoleLines[size_t(i)].c_str());
        if(g_AutoScrollConsole&&g_ConsoleScrollToBottom){ImGui::SetScrollHereY(1);g_ConsoleScrollToBottom=false;}
        ImGui::EndChild();
    }
    ImGui::End();
}
static void RenderDocs() {
    if(!g_Studio.ShowDocs)return;
    if(ImGui::Begin("API Docs",&g_Studio.ShowDocs)) {
        if(g_LogoTexture){ImGui::Image(ImTextureID(g_LogoTexture),ImVec2(64,64));ImGui::SameLine();}
        ImGui::Text("Lunoryx Studio %s",LUNORYX_VERSION);
        static int topic=0;static char search[128]="";
        ImGui::InputTextWithHint("##DocSearch","Search API topics...",search,sizeof(search));
        for(int i=0;i<int(sizeof(kApiTopics)/sizeof(kApiTopics[0]));++i) {
            if(search[0]&&!std::strstr(kApiTopics[i].Title,search)&&!std::strstr(kApiTopics[i].Text,search))continue;
            ImGui::PushID(i);if(ImGui::Selectable(kApiTopics[i].Title,topic==i))topic=i;ImGui::PopID();
        }
        ImGui::Separator();ImGui::BeginChild("DocText");ImGui::TextWrapped("%s",kApiTopics[topic].Text);ImGui::EndChild();
    }
    ImGui::End();
}
static void RenderStudio(float& distance,float& pitch,float& yaw,float target[3],float dt) {
    RenderWorkspace(distance,pitch,yaw,target,dt);
    RenderEditorSettings();
    RenderUVEditor();
    if(!g_Studio.FullscreenCode){RenderExplorer();RenderProperties(target);RenderTools(distance,pitch,yaw,target);RenderOutput();RenderDocs();}
}
static void RenderSky() {
    glDisable(GL_DEPTH_TEST);glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glBegin(GL_QUADS);glColor3fv(g_SkyBottom);glVertex2f(-1,-1);glVertex2f(1,-1);glColor3fv(g_SkyTop);glVertex2f(1,1);glVertex2f(-1,1);glEnd();
    if(g_SkyPreset==2){glPointSize(2);glBegin(GL_POINTS);glColor3f(.8f,.85f,1);for(int i=0;i<80;++i)glVertex2f(float((i*73)%199)/100-1,float((i*47)%97)/100);glEnd();glPointSize(1);}
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glEnable(GL_DEPTH_TEST);
}
static void RenderEditableMesh(const GameObject& object) {
    glBegin(GL_TRIANGLES);
    for(size_t i=0;i<object.Indices.size();i+=3) {
        uint32_t a=object.Indices[i]*3,b=object.Indices[i+1]*3,c=object.Indices[i+2]*3;
        Vec3 u={object.Vertices[b]-object.Vertices[a],object.Vertices[b+1]-object.Vertices[a+1],object.Vertices[b+2]-object.Vertices[a+2]};
        Vec3 v={object.Vertices[c]-object.Vertices[a],object.Vertices[c+1]-object.Vertices[a+1],object.Vertices[c+2]-object.Vertices[a+2]};
        Vec3 normal=Cross(u,v);float length=float(std::sqrt(Dot(normal,normal)));float shade=length>1e-8f?.65f+.3f*std::max(0.0f,normal.y/length):.8f;
        for(int vertex=0;vertex<3;++vertex){uint32_t index=object.Indices[i+vertex]*3;float color[3];for(int k=0;k<3;++k)color[k]=object.Color[k]*(object.VertexColors.empty()?1:object.VertexColors[index+k]);SetShadedColor(color,shade);glVertex3fv(object.Vertices.data()+index);}
    }
    glEnd();
}
