#include <SDL2/SDL.h>
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

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// tinygltf requires json
#include <tiny_gltf.h>

// Basic GameObject structure
struct GameObject {
    std::string Name;
    float Position[3] = {0.0f, 0.0f, 0.0f};
    float Rotation[3] = {0.0f, 0.0f, 0.0f};
    float Scale[3] = {1.0f, 1.0f, 1.0f};

    GameObject(const std::string& name) : Name(name) {}
};

// Global state
std::vector<GameObject> g_Objects;
int g_SelectedObjectIndex = -1;
char g_ScriptBuffer[1024 * 16] = "print('Hello from Lua!')\nadd_object('Scripted Cube')\nset_position(2, 10.0, 0.0, 0.0)\n--export_glb('scene.glb')\n";
std::string g_ConsoleOutput = "";

// Forward declarations
static bool ExportGLB(const std::string& filename);

// A simple C++ function to be called from Lua to print to the custom console
static int l_print(lua_State *L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        const char *s = lua_tostring(L, i);
        if (s) {
            g_ConsoleOutput += s;
        }
        if (i < n) g_ConsoleOutput += "\t";
    }
    g_ConsoleOutput += "\n";
    return 0;
}

// A simple C++ function to be called from Lua to add an object
static int l_add_object(lua_State *L) {
    const char* name = luaL_checkstring(L, 1);
    g_Objects.emplace_back(name);
    g_ConsoleOutput += std::string("Added object via script: ") + name + "\n";
    return 0;
}

// Lua: set_position(index, x, y, z) - 0 based index
static int l_set_position(lua_State *L) {
    int index = luaL_checkinteger(L, 1);
    if (index >= 0 && index < (int)g_Objects.size()) {
        g_Objects[index].Position[0] = luaL_checknumber(L, 2);
        g_Objects[index].Position[1] = luaL_checknumber(L, 3);
        g_Objects[index].Position[2] = luaL_checknumber(L, 4);
    }
    return 0;
}

// Lua: export_glb(filename)
static int l_export_glb(lua_State *L) {
    const char* filename = luaL_checkstring(L, 1);
    if (ExportGLB(filename)) {
        g_ConsoleOutput += std::string("Successfully exported to ") + filename + "\n";
    } else {
        g_ConsoleOutput += std::string("Failed to export GLB to ") + filename + "\n";
    }
    return 0;
}


static bool ExportGLB(const std::string& filename) {
    tinygltf::Model model;
    tinygltf::Scene scene;

    // Add nodes for all game objects
    for (size_t i = 0; i < g_Objects.size(); i++) {
        const auto& obj = g_Objects[i];
        tinygltf::Node node;
        node.name = obj.Name;

        node.translation.push_back(obj.Position[0]);
        node.translation.push_back(obj.Position[1]);
        node.translation.push_back(obj.Position[2]);

        node.scale.push_back(obj.Scale[0]);
        node.scale.push_back(obj.Scale[1]);
        node.scale.push_back(obj.Scale[2]);

        // Convert euler to quaternion for tinygltf
        // Very basic conversion (Y-up standard assumed, order XYZ for simplicity)
        float cr = cos(obj.Rotation[0] * 0.5f);
        float sr = sin(obj.Rotation[0] * 0.5f);
        float cp = cos(obj.Rotation[1] * 0.5f);
        float sp = sin(obj.Rotation[1] * 0.5f);
        float cy = cos(obj.Rotation[2] * 0.5f);
        float sy = sin(obj.Rotation[2] * 0.5f);

        node.rotation.push_back(sr * cp * cy - cr * sp * sy); // x
        node.rotation.push_back(cr * sp * cy + sr * cp * sy); // y
        node.rotation.push_back(cr * cp * sy - sr * sp * cy); // z
        node.rotation.push_back(cr * cp * cy + sr * sp * sy); // w

        model.nodes.push_back(node);
        scene.nodes.push_back(i); // add root nodes
    }

    model.scenes.push_back(scene);
    model.defaultScene = 0;

    // Asset info
    tinygltf::Asset asset;
    asset.version = "2.0";
    asset.generator = "3D Lua Engine Editor";
    model.asset = asset;

    tinygltf::TinyGLTF gltf;
    // Write out the glTF as a .glb (binary file)
    bool ret = gltf.WriteGltfSceneToFile(&model, filename, true, true, true, false);
    return ret;
}

int main(int argc, char* argv[]) {
    // Add default objects
    g_Objects.emplace_back("Camera");
    g_Objects.emplace_back("Directional Light");

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Set OpenGL version and profile
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "3D Lua Engine Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create OpenGL context
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (glContext == nullptr) {
        std::cerr << "OpenGL context could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize Lua state
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // Register custom print function
    lua_pushcfunction(L, l_print);
    lua_setglobal(L, "print");

    lua_pushcfunction(L, l_add_object);
    lua_setglobal(L, "add_object");

    lua_pushcfunction(L, l_set_position);
    lua_setglobal(L, "set_position");

    lua_pushcfunction(L, l_export_glb);
    lua_setglobal(L, "export_glb");

    // Main loop flag
    bool quit = false;

    // Event handler
    SDL_Event e;

    const char* env_timeout = std::getenv("TEST_TIMEOUT");
    Uint32 start_time = SDL_GetTicks();
    Uint32 timeout_ms = 0;
    if (env_timeout != nullptr) {
        timeout_ms = std::atoi(env_timeout) * 1000;
    }

    // Main game loop
    while (!quit) {
        if (timeout_ms > 0 && SDL_GetTicks() - start_time > timeout_ms) {
            quit = true;
        }

        // Handle events
        while (SDL_PollEvent(&e) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT)
                quit = true;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE && e.window.windowID == SDL_GetWindowID(window))
                quit = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // --- Create Editor Layout ---

        // Setup Dockspace
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(2);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Export GLB")) {
                    if (ExportGLB("export.glb")) {
                        g_ConsoleOutput += "> Exported UI scene to export.glb\n";
                    } else {
                        g_ConsoleOutput += "> Failed to export GLB\n";
                    }
                }
                if (ImGui::MenuItem("Exit")) quit = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGui::End(); // End DockSpace

        // 1. Viewport Panel
        ImGui::Begin("Viewport");
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        ImGui::Text("3D Scene Viewport (%.0fx%.0f)", viewportPanelSize.x, viewportPanelSize.y);
        ImGui::End();

        // 2. World Outliner Panel
        ImGui::Begin("World Outliner");
        if (ImGui::Button("Add Object")) {
            g_Objects.emplace_back("New Object");
        }
        ImGui::Separator();

        for (int i = 0; i < (int)g_Objects.size(); i++) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (g_SelectedObjectIndex == i) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", g_Objects[i].Name.c_str());
            if (ImGui::IsItemClicked()) {
                g_SelectedObjectIndex = i;
            }
        }
        ImGui::End();

        // 3. Details Panel
        ImGui::Begin("Details");
        if (g_SelectedObjectIndex >= 0 && g_SelectedObjectIndex < (int)g_Objects.size()) {
            GameObject& obj = g_Objects[g_SelectedObjectIndex];

            char nameBuf[128];
            strncpy(nameBuf, obj.Name.c_str(), sizeof(nameBuf));
            nameBuf[sizeof(nameBuf) - 1] = 0;
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                obj.Name = nameBuf;
            }

            ImGui::Separator();
            ImGui::DragFloat3("Position", obj.Position, 0.1f);
            ImGui::DragFloat3("Rotation", obj.Rotation, 0.1f);
            ImGui::DragFloat3("Scale", obj.Scale, 0.1f);
        } else {
            ImGui::Text("Select an object in World Outliner.");
        }
        ImGui::End();

        // 4. Content Browser Panel
        ImGui::Begin("Content Browser");
        ImGui::Text("Folders");
        ImGui::SameLine();
        ImGui::Text("| Files");
        ImGui::Separator();
        ImGui::Text("Models/");
        ImGui::Text("Textures/");
        ImGui::Text("Scripts/");
        ImGui::End();

        // 5. Script Editor & Output
        ImGui::Begin("Script Editor");
        ImGui::InputTextMultiline("##source", g_ScriptBuffer, IM_ARRAYSIZE(g_ScriptBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16), ImGuiInputTextFlags_AllowTabInput);
        if (ImGui::Button("Run Script")) {
            g_ConsoleOutput += "> Running script...\n";
            if (luaL_dostring(L, g_ScriptBuffer) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                g_ConsoleOutput += std::string("Error: ") + err + "\n";
                lua_pop(L, 1);
            }
        }
        ImGui::Separator();
        ImGui::Text("Console Output:");
        ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::TextUnformatted(g_ConsoleOutput.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::End();

        // --------------------------------------------------

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }

    // Clean up
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    lua_close(L);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
