#include "lunoryx.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void near(float a, float b) { assert(fabsf(a - b) < .002f); }
static void primitive_tests(void) {
  for (int k = 0; k <= LX_TERRAIN; k++) {
    Mesh m = {0};
    assert(mesh_primitive(&m, (Kind)k));
    assert(m.nv && m.ni && m.ni % 3 == 0);
    for (size_t i = 0; i < m.ni; i++)
      assert(m.i[i] < m.nv);
    if (k == LX_CUBE)
      near((float)mesh_volume(&m), 1);
    if (k == LX_SPHERE)
      assert(mesh_volume(&m) > .49 && mesh_volume(&m) < .53);
    if (k == LX_CYLINDER)
      assert(mesh_volume(&m) > .75 && mesh_volume(&m) < .8);
    mesh_free(&m);
  }
}
static void csg_tests(void) {
  Mesh a = {0}, b = {0}, out = {0};
  assert(mesh_primitive(&a, LX_CUBE) && mesh_primitive(&b, LX_CUBE));
  for (size_t i = 0; i < b.nv; i++)
    b.v[i].p.x += .5f;
  const float volumes[] = {1.5f, .5f, .5f};
  for (int op = 0; op < 3; op++) {
    assert(mesh_csg(&out, &a, &b, op));
    near((float)mesh_volume(&out), volumes[op]);
    mesh_free(&out);
  }
  for (size_t i = 0; i < b.nv; i++)
    b.v[i].p.x += 3;
  assert(mesh_csg(&out, &a, &b, 2));
  assert(out.ni == 0);
  mesh_free(&out);
  assert(mesh_csg(&out, &a, &b, 0));
  near((float)mesh_volume(&out), 2);
  mesh_free(&out);
  assert(mesh_csg(&out, &a, &a, 0));
  near((float)mesh_volume(&out), 1);
  mesh_free(&out);
  assert(mesh_subdivide(&a));
  size_t n = a.nv;
  assert(mesh_sculpt(&a, v3(0, .5f, 0), 1, .1f, 0));
  assert(a.nv == n);
  mesh_free(&a);
  mesh_free(&b);
  mesh_free(&out);
}
static void project_tests(void) {
  Scene s;
  scene_init(&s);
  Object *o = scene_add(&s, LX_CUBE, "Cube \"utf8\" Тест");
  assert(o);
  uint32_t id = o->id;
  o->position = v3(3, 4, 5);
  o->rotation = v3(12, 34, 56);
  o->scale = v3(2, 3, 4);
  o->selected = true;
  Object *b = scene_add(&s, LX_SPHERE, "Sphere");
  assert(b);
  b->position = v3(7, 8, 9);
  b->selected = true;
  V3 before = point(object_world(&s, scene_find(&s, id)), v3(.1f, .2f, .3f));
  uint32_t group = scene_group(&s);
  assert(group);
  V3 after = point(object_world(&s, scene_find(&s, id)), v3(.1f, .2f, .3f));
  near(before.x, after.x);
  near(before.y, after.y);
  near(before.z, after.z);
  assert(!scene_reparent(&s, group, id));
  assert(scene_separate(&s, group));
  assert(scene_find(&s, id));
  assert(scene_character(&s, 2, .8f));
  assert(scene_gui(&s, 0));
  Gui *g = scene_gui(&s, 3);
  assert(g);
  snprintf(g->on_click, 64, "on_click");
  Script *script = scene_script(&s, "Test.lua");
  assert(script);
  snprintf(script->code, LX_CODE_SIZE, "print('hello')");
  script->breakpoints[0] = 1;
  script->break_count = 1;
  char error[256];
  assert(scene_validate(&s, error));
  char *text = scene_json(&s);
  assert(text);
  Scene copy;
  scene_init(&copy);
  assert(scene_parse(&copy, text, error));
  char *same = scene_json(&copy);
  assert(same && !strcmp(same, text));
  free(same);
  assert(scene_save(&s, "tests/test-project.lunoryx", error));
  assert(scene_save(&s, "tests/test-project.lunoryx", error));
  assert(scene_load(&copy, "tests/test-project.lunoryx", error));
  uint32_t old = copy.objects[0].id;
  assert(!scene_parse(&copy, "{\"schema\":2,\"objects\":[{}],\"scripts\":[]}",
                      error));
  assert(copy.objects[0].id == old);
  assert(!scene_parse(&copy, "{\"schema\":99}", error));
  History h = {0};
  assert(history_capture(&h, &s));
  scene_find(&s, id)->position.x = 99;
  assert(history_capture(&h, &s));
  assert(history_undo(&h, &s));
  assert(scene_find(&s, id)->position.x != 99);
  assert(history_redo(&h, &s));
  near(scene_find(&s, id)->position.x, 99);
  assert(scene_export_glb(&s, "tests/test-scene.glb", error));
  assert(scene_export_glb(&s, "tests/test-scene.glb", error));
  Scene imported;
  scene_init(&imported);
  assert(scene_import(&imported, "tests/test-scene.glb", error));
  assert(imported.count >= 3);
  scene_free(&imported);
  history_free(&h);
  free(text);
  scene_free(&s);
  scene_free(&copy);
}
static void ownership_tests(void) {
  Scene s;
  scene_init(&s);
  for (int i = 0; i < 32; i++)
    assert(scene_add(&s, LX_CUBE, "Part"));
  uint32_t id = s.objects[0].id;
  assert(scene_duplicate(&s, id));
  assert(s.count == 33);
  scene_free(&s);
  scene_init(&s);
  uint32_t root = scene_add(&s, LX_GROUP, "Root")->id;
  uint32_t keep = scene_add(&s, LX_CUBE, "Keep")->id;
  uint32_t grandchild = scene_add(&s, LX_CUBE, "Grandchild")->id;
  uint32_t sibling = scene_add(&s, LX_CUBE, "Sibling")->id;
  uint32_t child = scene_add(&s, LX_GROUP, "Child")->id;
  scene_find(&s, child)->parent = root;
  scene_find(&s, grandchild)->parent = child;
  scene_find(&s, sibling)->parent = root;
  assert(scene_delete(&s, root));
  assert(s.count == 1 && s.objects[0].id == keep);
  scene_free(&s);
}
static void migration_tests(void) {
  Scene s;
  scene_init(&s);
  char error[256];
  assert(scene_load(&s, "tests/fixtures/v02-project.json", error));
  assert(s.count == 2 && s.script_count == 1);
  Object *o = &s.objects[0];
  assert(o->kind == LX_PYRAMID && o->mesh.ni == 18);
  near((float)mesh_volume(&o->mesh), 1.0f / 3);
  assert(o->property_count == 3 && o->properties[0].number == 87 &&
         o->properties[1].number == 1 &&
         !strcmp(o->properties[2].text, "legacy"));
  scene_free(&s);
}
static void physics_tests(void) {
  Scene s;
  scene_init(&s);
  Object *floor = scene_add(&s, LX_CUBE, "Floor");
  assert(floor);
  floor->position = v3(0, -.5f, 0);
  floor->scale = v3(20, 1, 20);
  Object *box = scene_add(&s, LX_CUBE, "Falling");
  assert(box);
  uint32_t id = box->id;
  box->position = v3(0, 4, 0);
  box->anchored = false;
  for (int i = 0; i < 600; i++)
    scene_step(&s, 1.0f / 120, i / 120.0f, v3(0, 0, 0), false);
  box = scene_find(&s, id);
  assert(box->position.y > .48f && box->position.y < .53f);
  assert(box->grounded);
  Ray r = {v3(0, 10, 0), v3(0, -1, 0)};
  uint32_t hit_id = 0;
  V3 hit;
  float distance = 100;
  assert(scene_raycast(&s, r, &hit_id, &hit, &distance));
  assert(hit_id == id);
  scene_free(&s);
}
int main(void) {
  near(wrap_angle(100000), -80);
  near(wrap_angle(360000), 0);
  ownership_tests();
  migration_tests();
  primitive_tests();
  csg_tests();
  project_tests();
  physics_tests();
  puts("PASS: C geometry, BSP booleans, project roundtrips, history, "
       "import/export, physics and picking");
  return 0;
}
