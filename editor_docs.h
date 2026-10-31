#pragma once
#include "embedded_licenses.h"
#include "extension_licenses.h"
struct ApiTopic {const char* Title;const char* Text;};
static const ApiTopic kApiTopics[]={
{"Getting started",R"DOC(LUNORYX STUDIO
Scene tab: orbit with RMB, pan with MMB, zoom with the wheel.
Q/W/E/R: Select/Move/Rotate/Scale. Drag gizmo axes or planes to transform objects.
Local and Snap are in the Scene toolbar; Edit offers object undo/redo (Ctrl+Z/Y).
Settings stores camera, grid, snap, theme, VSync and master volume.
Audio imports WAV/MP3/FLAC/OGG/W64/RF64; files can be dragged onto the window.
Click to select. Ctrl+click selects multiple objects. Explorer shows objects and scripts.
Tools > Build: spawn primitives, snap to grid, duplicate, join selected meshes.
Tools > Sculpt: subdivide, choose a brush, then hold LMB on the selected mesh.
Tools > Character: set proportions and colors, then Create Character.
Play Simulation: gravity, AABB collisions, WASD movement and Space to jump.
Stop restores the scene captured at Play. Lua animation has its own Pause button.
Scripts: create tabs with + Script. F5 runs the active script. F11 expands the editor.
Output is inside the app; the Windows executable opens no console window.

File > Save Project / Load Project uses .lunoryx files for geometry, properties,
scripts and environment. Loaded scripts do not run automatically. Run them explicitly.
GLB is a separate geometry/material export and does not contain editor metadata.
)DOC"},
{"Objects and transforms",R"DOC(Indices are ZERO-BASED. remove_object shifts later indices.
Use find_object(name) after changing scene membership. Names need not be unique.

local box = add_object('Box', 'cube')
set_position(box, 0, 3, 0)
set_rotation(box, 0, 45, 0) -- DEGREES
set_scale(box, 1, 2, 1)
set_color(box, 0.2, 0.7, 1.0)
set_material(box, 0.2, 0.6) -- metallic, roughness
local x,y,z = get_position(box)
local copy = duplicate_object(box, 'Copy')
set_name(copy, 'Renamed')
set_visible(copy, false)
set_wireframe(box, true)
remove_object(copy)

Types: cube, pyramid, sphere, cylinder, plane.
get_rotation / get_scale / get_color / get_material read values.
get_object_count(), clear_scene(), get_time() are also available.
Transform and material setters validate ALL numbers before changing the object.
NaN, infinity and values outside float range cause a Lua error.
)DOC"},
{"Simulation and characters",R"DOC(local player = add_character('Player', 2.0, 0.65, 0.45)
-- name, height, torso width, head size; returns index or -1
set_character(player, true, 4.5, 6) -- controller, move speed, jump speed
-- set_character also works on your own sculpted or joined mesh.
set_physics(player, true, 3, 0) -- dynamic, mass, restitution
set_velocity(player, 0, 5, 0)
local vx,vy,vz = get_velocity(player)

Physics runs when Play Simulation is enabled in the toolbar.
Static objects have Dynamic disabled; they still collide with dynamic bodies.
Character movement: WASD, Space jump, while the Scene tab is active.
Stop restores the scene, including properties and mesh geometry.
Collision shapes are world-axis-aligned boxes, not triangle meshes.
Character builder creates an editable colored mesh, not a skinned animation rig.
Mass: 0.01..10000. Restitution: 0..1. Velocity components: -10000..10000.

function on_update(dt)
    -- Lua animation callback; seconds, clamped by the editor frame loop.
end
Running a new script clears the previous on_update. Other Lua globals persist.
Errors pause Lua. Calls have a 1,000,000-instruction budget.
Scripts are TRUSTED: standard libraries can access files and processes.
)DOC"},
{"Sculpt and join",R"DOC(local clay = add_object('Clay', 'sphere')
make_editable(clay, 2) -- convert + two subdivision passes
sculpt(clay, 0, 0.5, 0, 0.8, 0.15, 0)
-- index, local x/y/z, radius, strength, mode
-- mode 0: Inflate (negative strength = deflate)
-- mode 1: Smooth; mode 2: Flatten to brush local Y

local merged = merge_objects({0, 1}, 'Joined')
-- Bake world transforms into one mesh. Source objects are removed.
-- Returns the new zero-based index, or -1 without modifying the scene.

Join is mesh concatenation, not a boolean solid union.
Vertex colors are preserved; the first source supplies metallic/roughness.
Sources' scripts and custom properties are not merged.
Sculpt subdivision limit: 4 passes per operation; 200,000 vertices per mesh.
The selected brush operates in LOCAL coordinates and exports to GLB.
Tools > Sculpt has Undo Last Stroke. Edit > Undo/Redo also snapshots objects and meshes.
Script text and audio are not part of object history.
)DOC"},
{"Editable properties",R"DOC(local i = find_object('Player')
set_property(i, 'Health', 100)
set_property(i, 'Team', 'Blue')
set_property(i, 'CanJump', true)
print(get_property(i, 'Health'))
set_property(i, 'Team', nil) -- remove

Properties are typed: number, boolean or text. Edit them in Properties.
Names: 1..64 bytes. Text values: at most 1024 bytes. Numbers must be finite floats.
Missing properties return no value. Invalid indices return false / no value.
Properties are copied when duplicating an object.
They are editor/runtime metadata and are NOT exported by GLB.
)DOC"},
{"Environment and export",R"DOC(set_skybox('day') -- also 'sunset' or 'night'
-- Procedural sky gradients, with no external texture files.
-- Tools > Environment also allows editing top and horizon colors.

local ok = export_glb('scene.glb')
if not ok then print('Export failed: check the destination folder') end

Export includes visible primitives, sculpted/joined meshes, vertex colors,
transforms in degrees converted to quaternions, and PBR material factors.
It excludes Lua code, callbacks, custom properties, physics and the skybox.
The viewport has simple directional shading; it is not a full PBR renderer.
Paths must point to an existing writable folder. No directory is auto-created.
)DOC"}
,{"Audio and MCP",R"DOC(Audio: import a path in the Audio window or drop a supported audio file onto Studio.
Formats: WAV/WAVE/W64/RF64, MP3, FLAC, OGG/OGA Vorbis. Audio properties and
resource paths are saved in .lunoryx; retain the sound files with your project.
local sound = add_sound('sound.ogg', 'Ambient') -- zero-based sound index
play_sound(sound)
stop_sound(sound)
Audio window: volume, pitch, loop, autoplay, spatial position, preview/stop.

MCP: start Lunoryx.exe --mcp from your MCP client (stdio transport).
The process uses this original engine with a separate headless scene.
Tools: scene_list, add_object, set_transform, remove_object, project_save,
project_load, export_glb, run_lua, add_sound, undo_objects, redo_objects.
Use stable object IDs returned by MCP; legacy Lua continues to use array indices.
Save via project_save, then open the project in Studio. Loaded scripts do not run.
run_lua executes TRUSTED Lua with standard libraries, as in the original editor.
)DOC"}
,{"Additional licenses",kExtensionLicenses}
,{"Third-party licenses",kEmbeddedLicenses}
};
