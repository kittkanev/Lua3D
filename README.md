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


To clean build artifacts (.o, .d, binary, and test executables):

make clean


Environment Variables

TEST_TIMEOUT: Optional timeout in seconds for automated/headless testing. When set to a positive integer (e.g., TEST_TIMEOUT=5 ./engine), the engine automatically exits cleanly after the specified number of seconds.

Features

Real-Time 3D Viewport: Orbit (RMB drag), Pan (MMB drag), and Zoom (Mouse Wheel) with 5 shaded 3D primitives (Cube, Pyramid, Sphere, Cylinder, Plane), wireframe mode, coordinate axes gizmos, and selection bounding boxes.

Embedded Lua 5.4 Scripting & Live on_update(dt) Loop: Persistent Lua state with stack-safe execution, full print() type serialization (nil, booleans, tables), bounds-checked transform/material APIs, and per-frame on_update(dt) simulation callbacks.

Binary GLB 2.0 Export: Exports visible scene objects with accurate degree-to-radians quaternion rotations, primitive geometry buffers, and PBR material properties (Base Color, Metallic, Roughness) into valid binary .glb files.

Unreal-Style ImGui Interface: Transparent central dockspace, World Outliner with search filter, Details inspector, interactive Content Browser (mesh spawners, PBR material presets, and Lua script templates), and virtualized console output (ImGuiListClipper).


## Additional Lua API

Object indices are zero-based. Removing an object shifts all following indices;
use `find_object(name)` again after removal rather than caching shifted indices.

- `duplicate_object(index [, name])`: copy mesh, transform, color, material and visibility; returns the new index, or `-1` for an invalid index. The default name appends ` Copy`. The position is preserved.
- `set_name(index, name)`: rename an object; returns success.
- `set_material(index, metallic, roughness)`: set PBR factors, clamped to `[0, 1]`; returns success.
- `get_material(index)`: return metallic and roughness, or no values for an invalid index.

Transform, color and material setters reject non-finite or out-of-float-range
numbers before changing the object. Running a new script clears the previous
`on_update` callback; the new script may install its own callback. Pause/Play
remains under the user's control.

## Tests

`make test` runs standalone C++ scene and geometry regression tests (no SDL/Lua
installation required). `make test-integration` additionally tests the real Lua
API, stack recovery, callback lifecycle and binary GLB round trip with the normal
build dependencies installed. GitHub Actions also builds and smoke-tests the
editor under Xvfb. `make clean` preserves exported `.glb` assets.

The checked-in Windows `engine.exe` is a legacy build; rebuild with
`make -f Makefile.win` to include source changes.
