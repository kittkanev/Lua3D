# C implementation

`native/lunoryx.h` defines the scene model. Objects have stable IDs, hierarchy,
TRS/pivot, mesh/material, optional rig, physics values, custom properties and sound.
Scripts, GUI nodes and constraints belong to the scene, so Play/Stop and history
can restore the complete authored state.

| Module | Responsibility |
|---|---|
| math.c | Vectors, affine matrices, inversion, Euler/quaternion conversion |
| mesh.c | Procedural geometry, normals, subdivision and sculpt |
| csg.c | BSP polygon splitting and solid union/difference/intersection |
| scene.c | Ownership, cloning, IDs, hierarchy, rigs and modelling commands |
| physics.c | Fixed-step OBB SAT, static triangle collision, impulses and constraints |
| project.c | Validated schema-2 JSON, schema-1 migration, atomic save, bounded history |
| assets.c | OBJ/cgltf import, linear/step rig tracks and binary glTF export |
| runtime.c | Lua allocator, bindings, coroutine debugger, instruction budget, plugins |
| audio.c | miniaudio mixing, Vorbis backend, AIFF/AU PCM decoders |
| renderer.c | GL2.1 viewport, Cook–Torrance material, shaders, images, sky and grid |
| editor.c | Ribbon, documents, inspector, GUI canvas and manipulators |
| settings.c | Validated user preferences |
| mcp.c | Headless stdio JSON-RPC/MCP transport and transactional tools |
| player.c | Standalone scene/asset archive appended to the host executable |
| main.c | SDL lifecycle, input, fixed-step scheduling and player/editor modes |

Third-party implementations are C. No C++ compiler, runtime, ImGui or tinygltf is
needed. Vendor snapshots are checked in, with upstream attribution in
`vendor/SOURCES.md`; Lua and SDL Windows archives are SHA-256 verified by the build script.

Budgets: 4096 objects, 200k vertices per mesh / 1M per project, 64 scripts of 64KiB,
64 bones per rig, 4096 GUI nodes / constraints, 32 custom properties per object,
64 audio voices, 64MiB Lua memory, 1M instructions per host resume, 128MiB project
file, 256MiB standalone asset payload, 64 snapshots / 64MiB history.

CSG expects closed, consistently wound solids and uses an epsilon of 1e-5. Splits
interpolate vertex attributes. A work/depth/geometry limit rejects pathological
inputs. It is not a robust CAD kernel. Skinned mesh edits are not topology-aware.
Physics is translational rigid-body approximation; dynamic concave shapes use OBBs.

The editor owns the scene on the main thread. miniaudio owns its internal audio
threads and streams. The MCP process owns a separate scene and writes protocol
messages only to stdout. Lua output is captured into a bounded buffer.

GLB export is an asset interchange path, not a project or game container. Project
serialization includes mesh weights, rig bind data/tracks, Lua, GUI, constraints,
sound settings and resource paths. Standalone games gather those resource paths,
embed the files, then extract only sanitized generated asset names to a private
player cache. Loaded projects never execute code until an explicit Run/Play.
