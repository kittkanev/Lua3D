#include <SDL2/SDL.h>
#include <GL/gl.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <iostream>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

// A simple C++ function to be called from Lua
static int l_c_hello(lua_State *L) {
    std::cout << "Hello from C++ function called by Lua!" << std::endl;
    return 0; // Number of return values
}

int main(int argc, char* argv[]) {
    std::cout << "Initializing 3D Lua Engine with ImGui (Unreal Engine layout)..." << std::endl;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Set OpenGL version and profile (e.g., OpenGL 3.0)
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Create a window with an OpenGL context
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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
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

    // Register the C++ function to Lua
    lua_pushcfunction(L, l_c_hello);
    lua_setglobal(L, "c_hello");

    // Load and run a Lua script
    const char* lua_script =
        "print('Hello from Lua script!')\n"
        "c_hello()\n";

    if (luaL_dostring(L, lua_script) != LUA_OK) {
        std::cerr << "Lua Error: " << lua_tostring(L, -1) << std::endl;
        lua_pop(L, 1);
    }

    // Main loop flag
    bool quit = false;

    // Event handler
    SDL_Event e;

    // Main game loop
    // Note for tests/verification we will close window after brief wait
    int iterations = 0;
    while (!quit && iterations < 10) { // increased for test running
        // Handle events on queue
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

        // --- Create Editor Layout (Unreal Engine Style) ---
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
        ImGui::Begin("DockSpace Demo", nullptr, window_flags);
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(2);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) quit = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGui::End();

        // 1. Viewport Panel
        ImGui::Begin("Viewport");
        ImGui::Text("Render 3D Scene Here");
        // Usually you'd render an OpenGL texture here (e.g. ImGui::Image)
        ImGui::End();

        // 2. World Outliner Panel
        ImGui::Begin("World Outliner");
        if (ImGui::TreeNode("Scene Root")) {
            ImGui::TreeNodeEx("Camera", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
            ImGui::TreeNodeEx("Directional Light", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
            ImGui::TreeNodeEx("Cube Mesh", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
            ImGui::TreePop();
        }
        ImGui::End();

        // 3. Details Panel
        ImGui::Begin("Details");
        ImGui::Text("Select an object in World Outliner to see details.");
        ImGui::Separator();
        float position[3] = {0.0f, 0.0f, 0.0f};
        ImGui::DragFloat3("Position", position);
        float rotation[3] = {0.0f, 0.0f, 0.0f};
        ImGui::DragFloat3("Rotation", rotation);
        float scale[3] = {1.0f, 1.0f, 1.0f};
        ImGui::DragFloat3("Scale", scale);
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

        // --------------------------------------------------

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Darker background for editor
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        // Update window
        SDL_GL_SwapWindow(window);

        SDL_Delay(50); // 50ms
        iterations++;
    }

    // Clean up
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    lua_close(L);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Engine shut down." << std::endl;

    return 0;
}
