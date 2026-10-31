# Lunoryx 0.3.0-beta.1 — modeling in the original Studio

The theme, dock layout, code tabs, Explorer and Properties remain in place. Open Tools → Build → Mesh Edit / Modifiers.

- Polygon topology with ear-clipping triangulation; vertex, edge and face selection with Ctrl for multiple elements. Pick the element marker in the viewport. Transform selected elements with the existing gizmo or Element offset.
- Extrude selected faces, delete elements, merge vertices by distance, and recalculate normals.
- Ordered nondestructive Mirror, Array, Loop Subdivision and closed-mesh Bevel modifiers. Edit parameters, move a modifier up, remove it or apply the stack.
- Real Union, Difference and Intersection using statically linked Manifold 3.5.4. Select two objects: the result is created and the original operands are hidden, retained for recovery. Difference uses the first object in Explorer as A.
- Heavy operations use a background job and commit only if the input scene is unchanged. A rejected operation leaves the original geometry intact.
- Project schema 2 stores stable IDs, polygon faces, modifier bases and settings. Schema 1 remains readable. Existing Lua APIs retain zero-based indices.
- Lua `mesh_operation(index, action, options_json)` and MCP `mesh_operation` use common modeling operations. MCP addresses the target through its stable ID; boolean `other` remains a documented scene index in options.

Example Lua:
```lua
local box = add_object('Box','cube')
mesh_operation(box,'modifier','{"type":"bevel","amount":0.08}')
mesh_operation(box,'modifier','{"type":"array","count":3,"offset":[1.5,0,0]}')
```

Validation includes known CSG volumes, closed bevel intersection, polygon reconstruction, extrude, Loop subdivision, modifier-base preservation and schema-2 reload. Windows, Linux, sanitizers, Lua, GLB, MCP and audio remain required CI gates. `modeling.png` is an actual capture of the running executable showing Bevel and Boolean Difference; the release points to the exact tested source commit.

Beta limitations: Mirror copies the whole mesh around the local origin; Bevel requires a closed consistently oriented mesh; topology operations have explicit geometry budgets. Apply modifiers before sculpting. Advanced UV/material workflows and standalone game packaging are the next milestones. This release is not 1.0.
