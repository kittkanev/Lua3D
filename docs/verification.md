# Verification

The release pipeline must pass Linux C builds, core/runtime/editor tests, a real
OpenGL/Mesa window smoke test, Khronos GLB validation, ASan/UBSan/leak checks,
MinGW compilation and execution of the Windows test binaries on Windows.

- `make test`: primitive winding/volumes, BSP overlap/disjoint/identical solids,
  subdivision/sculpt, hierarchy cycles and transforms, complete project roundtrip,
  rejected-load preservation, repeated saves/GLB writes, undo/redo, GLB import,
  falling rigid body and ray picking.
- `make test-integration`: stable Lua IDs, finite arguments, sandbox, instruction
  exhaustion, breakpoint/step/local-watch, update callbacks, syntax and plugin tools.
- `make test-editor`: actual editor input handlers and manipulators, axis-constrained
  drag, locked objects, undo/redo, GUI/script creation and full Play/Stop restoration.
- `python tools/test_mcp.py ./engine`: protocol handshake, discovery, tools,
  resources, invalid input rollback, nested-pcall instruction exhaustion,
  load/save/export, undo/redo and clean EOF.
- `tools/test_audio.py`: generate WAV/MP3/FLAC/Vorbis/AIFF/AU fixtures, decode and mix
  nonzero samples with miniaudio's no-device engine. This verifies decoding/mixing,
  not the user's speakers or OS audio routing.
- `tools/test_player.py`: pack scene/assets, overwrite a game build, run it, and
  confirm the player scene is restored from the appended archive.
- `tools/verify_windows.py`: PE x64 GUI subsystem and no non-system runtime DLLs.
- `node tools/validate_glb.cjs tests/test-scene.glb`: independent Khronos validation.

Graphical smoke captures are CI artifacts. Hardware-specific GPU/audio behavior
and hostile-input isolation are not certified by these checks. The release remains
an alpha even when all tests pass.
