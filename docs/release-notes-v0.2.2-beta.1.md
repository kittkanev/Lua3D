# Lunoryx 0.2.2-beta.1 — editor reliability

The original Studio theme, docking and panels are retained.

- Undo/Redo now covers sound references, sound settings and environment as well as objects and sculpted geometry. Completed mouse gestures are grouped together; script text retains its own editor Undo.
- Stopping simulation restores sky colors, gravity, objects and sounds.
- Unsaved changes are marked in the menu bar. Closing, clearing or opening another project offers Save / Discard / Cancel. Save As creates a separate project; Ctrl+S saves the opened document.
- Audio paths are written relative to the project and resolved from the project directory. Missing audio is shown with a replacement action.
- Project I/O accepts UTF-8 paths on Windows and validates the complete document before replacing the scene.
- Publication reads the version from the tested source and attaches real editor captures and this report. Existing releases are retained.

Validation: geometry, Lua/GLB, MCP, audio codec and operation-history regression tests; Linux editor capture; Windows execution and sanitizers are release gates in CI. Exact source SHA and CI run are recorded by GitHub on this release.

Visual evidence: `studio-smoke.png` and `gizmo-smoke.png` attached to this release are captured from the tested executable. Compare the original docked panels and operation controls.

This is a beta, not 1.0. Advanced modeling, UV and standalone game packaging are still under development. Assets must accompany editable projects; the editor executable itself remains standalone.
