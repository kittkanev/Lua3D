#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>

// A simple C function to be called from Lua
static int l_c_hello(lua_State *L) {
    printf("Hello from C function called by Lua!\n");
    return 0; // Number of return values
}

int main() {
    printf("Initializing Lua Engine...\n");

    // Create a new Lua state
    lua_State *L = luaL_newstate();

    // Open all standard Lua libraries
    luaL_openlibs(L);

    // Register the C function to Lua
    lua_pushcfunction(L, l_c_hello);
    lua_setglobal(L, "c_hello");

    // Load and run a Lua script
    const char* lua_script =
        "print('Hello from Lua script!')\n"
        "c_hello()\n";

    if (luaL_dostring(L, lua_script) != LUA_OK) {
        fprintf(stderr, "Lua Error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); // pop error message from the stack
    }

    // Close the Lua state
    lua_close(L);
    printf("Engine shut down.\n");

    return 0;
}
