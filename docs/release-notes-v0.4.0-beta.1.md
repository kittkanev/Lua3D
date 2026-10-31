# Lunoryx 0.4.0-beta.1 — UV, materials and static GLB import

The original Studio panels and theme are retained. New UV and material controls extend the existing workspace.

- xatlas automatic chart generation and packing, pinned to f700c7790aaa030e794b52ba7791a05c085faf0c. Mark edge seams in Mesh Edit, then open UV. Apply modifiers before editing UVs.
- Select UV islands by clicking, Ctrl to add; drag to move or use offset, rotation and scale. The canvas previews the assigned texture.
- Material slots, face assignment, PNG/JPEG base-color textures, metallic and roughness controls. Lighting uses the compatibility OpenGL pipeline for a simple material preview; this is not an offline PBR renderer.
- Import static GLB through File or drag/drop: scene/node transforms, triangle geometry, authored normals, UV, vertex colors and base-color materials. Embedded images stay inside the editable project. Import errors leave the scene intact; unsupported required extensions are rejected and ignored optional features are reported.
- Export evaluated meshes, normals, UV, per-face materials and embedded PNG textures to a self-contained GLB. Schema 2 persists the new data and remains compatible with schema-1 projects.
- Texture dimensions are bounded to 4096 x 4096. Editable project files retain the 32 MiB document budget. Sparse accessors, skinning, morph animation and non-triangle primitives are outside this static importer; the import report describes omitted features.

Validation: texture → GLB → import → project save/reload round trip, finite normalized UVs, authored-normal storage, independent glTF validation, existing modeling tests, Lua/MCP/audio, Windows execution and sanitizers. `uv.png` is captured from the running editor and shows the actual packed mesh and checker material. GitHub records the tested source SHA for this release.

This remains a beta. Game packaging and the standalone runtime are the next milestone.
