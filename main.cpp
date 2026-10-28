#include <SDL2/SDL.h>
#include <GL/gl.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <iostream>

// A simple C++ function to be called from Lua
static int l_c_hello(lua_State *L) {
    std::cout << "Hello from C++ function called by Lua!" << std::endl;
    return 0; // Number of return values
}

int main(int argc, char* argv[]) {
    std::cout << "Initializing 3D Lua Engine..." << std::endl;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Set OpenGL version and profile (e.g., OpenGL 3.1)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Create a window with an OpenGL context
    SDL_Window* window = SDL_CreateWindow(
        "3D Lua Engine",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
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
    while (!quit && iterations < 3) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // User requests quit
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }

        // Clear screen
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Update window
        SDL_GL_SwapWindow(window);

        SDL_Delay(500); // Wait 500 ms per frame so loop doesn't spin wildly, loop will exit after 1.5 seconds.
        iterations++;
    }

    // Clean up
    lua_close(L);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Engine shut down." << std::endl;

    return 0;
}
