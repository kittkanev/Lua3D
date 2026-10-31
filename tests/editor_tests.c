#define NK_IMPLEMENTATION
#include "app.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
/* Device boundaries are isolated; the actual editor, core and Lua code are
 * linked. */
GLuint renderer_texture(Renderer *r, const char *p) {
  (void)r;
  (void)p;
  return 0;
}
void renderer_clear_cache(Renderer *r) { (void)r; }
void audio_stop_all(Audio *a) { (void)a; }
void audio_command(void *a, uint32_t id, int action) {
  (void)a;
  (void)id;
  (void)action;
}
bool audio_load(Audio *a, const Object *o, char e[256]) {
  (void)a;
  (void)o;
  e[0] = 0;
  return true;
}
bool audio_extension(const char *p) {
  (void)p;
  return false;
}
static void input(App *a, float x, float y, bool pressed) {
  nk_input_begin(a->ctx);
  nk_input_motion(a->ctx, (int)x, (int)y);
  nk_input_button(a->ctx, NK_BUTTON_LEFT, (int)x, (int)y, pressed);
  nk_input_end(a->ctx);
  editor_interaction(a, 1.0f / 60);
}
int main(void) {
  App *a = calloc(1, sizeof(*a));
  assert(a);
  struct nk_context ctx;
  assert(nk_init_default(&ctx, NULL));
  a->ctx = &ctx;
  scene_init(&a->scene);
  settings_defaults(&a->settings);
  assert(runtime_init(&a->runtime, &a->scene, false));
  a->document = -1;
  a->selected_gui = -1;
  a->viewport = nk_rect(200, 100, 800, 600);
  a->target = v3(0, 0, 0);
  a->camera = v3(6, 5, 10);
  a->tool = 1;
  Object *o = scene_add(&a->scene, LX_CUBE, "Cube");
  assert(o);
  uint32_t id = o->id;
  editor_select(a, id, false);
  float x, y;
  assert(editor_project_point(a, v3(1.1f, 0, 0), &x, &y));
  input(a, x, y, true);
  assert(a->dragging && a->drag_axis == 0);
  input(a, x + 75, y + 20, true);
  input(a, x + 75, y + 20, false);
  o = scene_find(&a->scene, id);
  assert(o->position.x > .5f);
  assert(fabsf(o->position.y) < .001f && fabsf(o->position.z) < .001f);
  float moved = o->position.x;
  assert(history_undo(&a->history, &a->scene));
  assert(fabsf(scene_find(&a->scene, id)->position.x) < .001f);
  assert(history_redo(&a->history, &a->scene));
  assert(fabsf(scene_find(&a->scene, id)->position.x - moved) < .001f);
  editor_select(a, id, false);
  editor_action(a, "Script");
  assert(a->document == 0);
  editor_action(a, "Frame");
  editor_action(a, "Button");
  assert(a->scene.gui_count == 2);
  editor_select(a, id, false);
  o = scene_find(&a->scene, id);
  o->locked = true;
  assert(editor_project_point(a, add(o->position, v3(1.1f, 0, 0)), &x, &y));
  input(a, x, y, true);
  input(a, x + 50, y + 20, true);
  input(a, x + 50, y + 20, false);
  assert(fabsf(scene_find(&a->scene, id)->position.x - moved) < .001f);
  editor_play(a);
  assert(a->playing);
  scene_find(&a->scene, id)->position.x = 99;
  editor_stop(a);
  assert(!a->playing);
  assert(fabsf(scene_find(&a->scene, id)->position.x - moved) < .001f);
  runtime_free(&a->runtime);
  scene_free(&a->scene);
  scene_free(&a->snapshot);
  history_free(&a->history);
  free(a->drag_objects);
  nk_free(&ctx);
  free(a);
  puts("PASS: editor mouse-axis drag, locked objects, undo/redo, script/GUI "
       "creation and full Play/Stop snapshot");
  return 0;
}
