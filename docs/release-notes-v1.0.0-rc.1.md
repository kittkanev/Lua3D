# Lunoryx Studio 1.0.0-rc.1

Release candidate for the first stable Studio. The original theme, docking, Explorer, Properties, Tools and code tabs remain in place.

The complete workflow is available: scene creation → polygon editing/modifiers/CSG → UV and textures → Lua and simulation → project reload → one standalone Game.exe with embedded resources.

Candidate fixes:
- Preserve face material assignments through subdivision, extrusion, bevel, deletion and welding; compact unused vertices after destructive edits.
- Delete selected edges through their incident faces. Invalid modifier settings are rejected without changing the original stack.
- Respect marked UV seams through chart constraints, preserve authored normals and retain seam markings across repeated unwrap.
- Handle reflected transforms in boolean operands.
- Cache GPU textures and avoid allocating a mesh copy on each lit draw. History sampling follows completed input operations; embedded resources count toward its memory budget.
- Refresh the README, first-run instructions and embedded API guide to describe the shipped behavior and explicit limits.

Validation gates: Linux and MinGW builds with -Werror; native Windows tests; ASan/UBSan/LeakSanitizer; CSG volume and closed-topology checks; modifier/material and UV seam regressions; schema-1/schema-2 project compatibility; Lua stack/error recovery; MCP; six audio codecs from files and memory; Khronos GLB validation; Linux and Windows/Mesa gameplay.

The native packaging test deletes the source project and external PNG/WAV, runs from a Cyrillic/spaced directory, completes all pickups using controller/physics/Lua, verifies nonzero sound and restarts the level. Invalid checksums and out-of-range manifests are rejected. Captures and the tested GemTrail.exe are attached; GitHub records the exact tested source SHA.

This is a release candidate. Scope remains: AABB physics, static GLB and a simple lit preview. Rigging, skeletal animation, node systems, offline rendering and full Blender compatibility are outside 1.0. Bevel needs a closed oriented mesh; newly generated surfaces use material slot 0, and boolean results need fresh UV/material setup.
