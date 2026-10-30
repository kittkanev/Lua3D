Lua 3D Engine

A lightweight 3D game engine written in C++ with integrated Lua 5.4 scripting, real-time OpenGL primitive rendering, an Unreal-style Dear ImGui editor, and binary GLB export.

Requirements

Linux (Ubuntu / Debian)

To build and run the engine on Linux, install the required development packages:

sudo apt-get update
sudo apt-get install -y build-essential liblua5.4-dev libsdl2-dev libgl1-mesa-dev libtinygltf-dev nlohmann-json3-dev libstb-dev


Windows Cross-Compilation (MinGW-w64 on Linux)

To cross-compile engine.exe for Windows (Makefile.win), you need:

g++-mingw-w64-x86-64 (x86_64-w64-mingw32-g++)

Local Lua 5.4 sources compiled for MinGW in ./lua-5.4.6/src

Local SDL2 MinGW development libraries in ./SDL2-2.30.0/x86_64-w64-mingw32

Header dependencies in ./win_deps (tiny_gltf.h, stb_image.h, stb_image_write.h, nlohmann/json.hpp)

make -f Makefile.win


Building and Running on Linux

Compile the engine using incremental object builds and automatic header dependency tracking (-O2 -MMD -MP):

make
./engine


To clean build artifacts (.o, .d, binary, and exported .glb files):

make clean


Environment Variables

TEST_TIMEOUT: Optional timeout in seconds for automated/headless testing. When set to a positive integer (e.g., TEST_TIMEOUT=5 ./engine), the engine automatically exits cleanly after the specified number of seconds.

Features

Real-Time 3D Viewport: Orbit (RMB drag), Pan (MMB drag), and Zoom (Mouse Wheel) with 5 shaded 3D primitives (Cube, Pyramid, Sphere, Cylinder, Plane), wireframe mode, coordinate axes gizmos, and selection bounding boxes.

Embedded Lua 5.4 Scripting & Live on_update(dt) Loop: Persistent Lua state with stack-safe execution, full print() type serialization (nil, booleans, tables), bounds-checked transform/material APIs, and per-frame on_update(dt) simulation callbacks.

Binary GLB 2.0 Export: Exports visible scene objects with accurate degree-to-radians quaternion rotations, primitive geometry buffers, and PBR material properties (Base Color, Metallic, Roughness) into valid binary .glb files.

Unreal-Style ImGui Interface: Transparent central dockspace, World Outliner with search filter, Details inspector, interactive Content Browser (mesh spawners, PBR material presets, and Lua script templates), and virtualized console output (ImGuiListClipper).
