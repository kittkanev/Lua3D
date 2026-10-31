# Lunoryx 0.5.0-beta.1 — a playable game in one executable

The original Studio theme, panels and code tabs are retained. Tools → Character → Game / Build adds startup-script selection, a follow camera and Build standalone Game.exe. No script is selected automatically for existing projects.

- Studio Play and the standalone player share project loading, Lua, physics, audio and rendering. Each Play gets a fresh Lua state; Stop restores objects, sounds, sky, gravity and game settings.
- Native single-executable packer embeds the project and imported resources. The player reads assets from memory, checks the container version, checksum, manifest and resource ranges. Building does not overwrite the editable source project or require Python/a compiler.
- UTF-8 paths, spaces and Cyrillic names work in project I/O and native command-line packaging.
- New Lua APIs: `input_key(name)`, `follow_camera(index,distance,pitch)`, `game_score(value?)`, `hud_text(text)` and `restart_game()`.
- Gem Trail sample: a UV-mapped textured floor, platforms, a character, collectible gems, score, HUD and embedded pickup audio. Create it in the existing Character panel, then Play or Build. WASD moves, Space jumps, R restarts, Escape closes the standalone player.
- Embedded WAV/MP3/FLAC/OGG decoding uses memory data sources; existing external resource paths remain supported.
- Reduce idle-frame work: sample document history after input operations and cache decoded GPU textures. Preserve memory limits for history, including embedded resources.

Validation: the test packer builds in a path containing spaces and Cyrillic, deletes the source project, verifies relocation, rejects a damaged checksum and a valid-checksum manifest with invalid ranges. A deterministic input test uses the real controller, fixed-step physics and Lua to collect all three gems, verifies nonzero mixed audio and restarts the level. Linux and Windows/Mesa run this scenario; Windows uses a checksum-pinned test-only graphics driver, which is not included in the released application. Editor-state restoration and all earlier geometry, UV/GLB, Lua, MCP, audio and sanitizer checks remain required.

Attached `GemTrail.exe` is the tested sample game. `game-linux.png` / `game-windows.png` show the completed level; `modeling.png` and `uv.png` show the editor. GitHub identifies the tested source commit.

Limits: AABB collision physics; base-color texture and simple lit material preview; static GLB import; up to 512 MiB packed assets and a 32 MiB project document. Only resources imported into the project are packed; arbitrary Lua file/process access is not virtualized. This beta precedes the 1.0 release candidate.
