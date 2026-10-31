#include "app.h"
#include "licenses.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void format_code(App *a);
static void gui_canvas(App *a);
static bool inside(struct nk_rect r, float x, float y) {
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
void editor_log(App *a, const char *text) {
  snprintf(a->status, sizeof(a->status), "%s", text);
  runtime_log(&a->runtime, text);
  runtime_log(&a->runtime, "\n");
}
void editor_select(App *a, uint32_t id, bool toggle) {
  if (!toggle)
    for (size_t i = 0; i < a->scene.count; i++)
      a->scene.objects[i].selected = false;
  Object *o = scene_find(&a->scene, id);
  if (o) {
    o->selected = toggle ? !o->selected : true;
    a->selected_id = o->selected ? id : 0;
  } else
    a->selected_id = 0;
  a->selected_gui = -1;
}
void editor_document(App *a, int doc) {
  a->document = doc;
  if (doc >= 0) {
    a->active_script = doc;
    if ((size_t)doc < a->scene.script_count)
      a->scene.scripts[doc].open = true;
  }
  if (a->doc_history_count < 128 &&
      (!a->doc_history_count || a->history_doc[a->doc_history_cursor] != doc)) {
    a->doc_history_cursor = a->doc_history_count++;
    a->history_doc[a->doc_history_cursor] = doc;
  }
}
void editor_sound(void *data, uint32_t id, int action) {
  App *a = data;
  Object *o = scene_find(&a->scene, id);
  if (!o)
    return;
  char error[256];
  if (action == 0 && !audio_load(a->audio, o, error)) {
    editor_log(a, error);
    return;
  }
  audio_command(a->audio, id, action);
}
static void reset_runtime(App *a) {
  runtime_free(&a->runtime);
  runtime_init(&a->runtime, &a->scene, a->settings.trusted);
  a->runtime.sound = editor_sound;
  a->runtime.sound_userdata = a;
}
void editor_play(App *a) {
  if (a->playing) {
    a->paused = !a->paused;
    return;
  }
  history_capture(&a->history, &a->scene);
  if (!scene_copy(&a->snapshot, &a->scene)) {
    editor_log(a, "Cannot snapshot scene for Play");
    return;
  }
  reset_runtime(a);
  a->playing = true;
  a->paused = false;
  a->time = 0;
  a->accumulator = 0;
  a->play_script_next = 0;
  for (size_t i = 0; i < a->scene.script_count; i++) {
    a->play_script_next = (int)i + 1;
    if (a->scene.scripts[i].enabled) {
      runtime_run(&a->runtime, (int)i);
      if (a->runtime.state == 2)
        break;
    }
  }
  for (size_t i = 0; i < a->scene.count; i++) {
    Object *o = &a->scene.objects[i];
    if (o->sound[0]) {
      char error[256];
      if (audio_load(a->audio, o, error) && o->autoplay)
        audio_command(a->audio, o->id, 0);
    }
  }
  editor_log(a, "Play started. Stop restores the full editor scene.");
}
void editor_stop(App *a) {
  if (a->playing) {
    scene_free(&a->scene);
    a->scene = a->snapshot;
    memset(&a->snapshot, 0, sizeof(a->snapshot));
  }
  a->playing = a->paused = false;
  a->time = 0;
  a->accumulator = 0;
  audio_stop_all(a->audio);
  reset_runtime(a);
}
static void mark(App *a) { a->dirty = true; }
static bool bool_field(struct nk_context *c, const char *label, bool *value) {
  nk_bool v = *value;
  bool changed = nk_checkbox_label(c, label, &v) != 0;
  *value = v != 0;
  return changed;
}
static void text_field(struct nk_context *c, const char *label, char *value,
                       int cap) {
  nk_layout_row_dynamic(c, 18, 1);
  nk_label(c, label, NK_TEXT_LEFT);
  nk_layout_row_dynamic(c, 26, 1);
  nk_edit_string_zero_terminated(c, NK_EDIT_FIELD, value, cap,
                                 nk_filter_default);
}
static void vector_field(struct nk_context *c, const char *name, V3 *v,
                         float step) {
  nk_layout_row_dynamic(c, 18, 1);
  nk_label(c, name, NK_TEXT_LEFT);
  nk_layout_row_dynamic(c, 25, 3);
  nk_property_float(c, "X", -10000000, &v->x, 10000000, step, step * .1f);
  nk_property_float(c, "Y", -10000000, &v->y, 10000000, step, step * .1f);
  nk_property_float(c, "Z", -10000000, &v->z, 10000000, step, step * .1f);
}
static void color_field(struct nk_context *c, const char *label, float *rgba) {
  nk_layout_row_dynamic(c, 20, 1);
  nk_label(c, label, NK_TEXT_LEFT);
  struct nk_colorf color = {rgba[0], rgba[1], rgba[2], rgba[3]};
  nk_layout_row_dynamic(c, 28, 1);
  if (nk_combo_begin_color(c, nk_rgba_cf(color), nk_vec2(240, 250))) {
    nk_layout_row_dynamic(c, 120, 1);
    color = nk_color_picker(c, color, NK_RGBA);
    nk_layout_row_dynamic(c, 22, 1);
    color.r = nk_propertyf(c, "R", 0, color.r, 1, .01f, .005f);
    color.g = nk_propertyf(c, "G", 0, color.g, 1, .01f, .005f);
    color.b = nk_propertyf(c, "B", 0, color.b, 1, .01f, .005f);
    color.a = nk_propertyf(c, "A", 0, color.a, 1, .01f, .005f);
    nk_combo_end(c);
  }
  rgba[0] = color.r;
  rgba[1] = color.g;
  rgba[2] = color.b;
  rgba[3] = color.a;
}
static void add_part(App *a, Kind kind) {
  editor_document(a, -1);
  for (size_t i = 0; i < a->scene.count; i++)
    a->scene.objects[i].selected = false;
  const char *names[] = {"Cube",  "Sphere",     "Cylinder", "Plane",
                         "Wedge", "Terrain",    "Mesh",     "Model",
                         "Union", "Attachment", "Effect",   "Character",
                         "Sound", "Pyramid"};
  Object *o = scene_add(&a->scene, kind, names[kind]);
  if (o) {
    o->position = a->target;
    if (kind == LX_TERRAIN)
      o->scale = v3(30, 10, 30);
    if (kind == LX_PLANE)
      o->scale = v3(10, 1, 10);
    o->selected = true;
    a->selected_id = o->id;
    mark(a);
  }
}
static void save_project(App *a) {
  char error[256];
  if (a->playing) {
    editor_log(a, "Stop Play before saving editor changes.");
    return;
  }
  if (scene_save(&a->scene, a->path, error)) {
    a->dirty = false;
    editor_log(a, "Project saved");
  } else
    editor_log(a, error);
}
static void import_asset(App *a) {
  char error[256];
  if (audio_extension(a->asset_path)) {
    add_part(a, LX_SOUND);
    Object *o = scene_find(&a->scene, a->selected_id);
    if (o) {
      snprintf(o->sound, 512, "%s", a->asset_path);
      if (audio_load(a->audio, o, error))
        editor_log(a, "Sound loaded");
      else
        editor_log(a, error);
    }
    return;
  }
  if (scene_import(&a->scene, a->asset_path, error)) {
    mark(a);
    editor_log(a, "Mesh imported");
  } else
    editor_log(a, error);
}
void editor_action(App *a, const char *action) {
  Object *o = scene_find(&a->scene, a->selected_id);
  if (!strcmp(action, "Select"))
    a->tool = 0;
  else if (!strcmp(action, "Move"))
    a->tool = 1;
  else if (!strcmp(action, "Scale"))
    a->tool = 2;
  else if (!strcmp(action, "Rotate"))
    a->tool = 3;
  else if (!strcmp(action, "Transform"))
    a->tool = 4;
  else if (!strcmp(action, "Pivot"))
    a->tool = 5;
  else if (!strcmp(action, "Sculpt"))
    a->tool = 6;
  else if (!strcmp(action, "Reset") && o) {
    o->pivot = v3(0, 0, 0);
    o->rotation = v3(0, 0, 0);
    o->scale = v3(1, 1, 1);
  } else if (!strcmp(action, "Geometric")) {
    a->settings.local_space = !a->settings.local_space;
  } else if (!strcmp(action, "Part"))
    add_part(a, LX_CUBE);
  else if (!strcmp(action, "Terrain"))
    add_part(a, LX_TERRAIN);
  else if (!strcmp(action, "Character")) {
    if (scene_character(&a->scene, 2, .8f)) {
      editor_select(a, a->scene.objects[a->scene.count - 1].id, false);
      editor_document(a, -1);
    }
  } else if (!strcmp(action, "GUI")) {
    a->tab = 2;
    a->gui_mode = true;
    editor_document(a, -1);
  } else if (!strcmp(action, "Script")) {
    char name[64];
    snprintf(name, 64, "Script%zu.lua", a->scene.script_count + 1);
    Script *s = scene_script(&a->scene, name);
    if (s) {
      snprintf(
          s->code, LX_CODE_SIZE,
          "-- %s\nfunction on_update(dt)\n    -- update your scene here\nend\n",
          name);
      editor_document(a, (int)a->scene.script_count - 1);
      a->tab = 3;
    }
  } else if (!strcmp(action, "Group")) {
    uint32_t id = scene_group(&a->scene);
    if (id)
      a->selected_id = id;
  } else if (!strcmp(action, "Separate") && o) {
    uint32_t id = o->id;
    if (scene_separate(&a->scene, id))
      a->selected_id = 0;
  } else if (!strcmp(action, "Union") || !strcmp(action, "Intersect") ||
             !strcmp(action, "Negate")) {
    if (!strcmp(action, "Negate") && o) {
      o->negative = !o->negative;
    } else if (scene_boolean(&a->scene, !strcmp(action, "Intersect") ? 2 : 0))
      a->selected_id = a->scene.objects[a->scene.count - 1].id;
    else
      editor_log(a, "Select at least two closed meshes. Boolean operation must "
                    "fit geometry limits.");
  } else if (!strcmp(action, "Difference")) {
    if (scene_boolean(&a->scene, 1))
      a->selected_id = a->scene.objects[a->scene.count - 1].id;
  } else if (!strcmp(action, "Join")) {
    if (scene_join(&a->scene))
      a->selected_id = a->scene.objects[a->scene.count - 1].id;
  } else if (!strcmp(action, "Lock")) {
    for (size_t i = 0; i < a->scene.count; i++)
      if (a->scene.objects[i].selected)
        a->scene.objects[i].locked = !a->scene.objects[i].locked;
  } else if (!strcmp(action, "Anchor")) {
    for (size_t i = 0; i < a->scene.count; i++)
      if (a->scene.objects[i].selected)
        a->scene.objects[i].anchored = !a->scene.objects[i].anchored;
  } else if (!strcmp(action, "Explorer"))
    a->show_explorer = !a->show_explorer;
  else if (!strcmp(action, "Properties"))
    a->show_properties = !a->show_properties;
  else if (!strcmp(action, "Toolbox"))
    a->show_toolbox = !a->show_toolbox;
  else if (!strcmp(action, "Assets"))
    a->show_assets = !a->show_assets;
  else if (!strcmp(action, "Import")) {
    a->show_assets = true;
  } else if (!strcmp(action, "Material") || !strcmp(action, "Color") ||
             !strcmp(action, "Texture")) {
    a->show_properties = true;
  } else if (!strcmp(action, "Effect"))
    add_part(a, LX_EFFECT);
  else if (!strcmp(action, "Attachment")) {
    uint32_t parent = a->selected_id;
    add_part(a, LX_ATTACHMENT);
    Object *n = scene_find(&a->scene, a->selected_id);
    if (n && parent) {
      n->parent = parent;
      n->position = v3(0, 0, 0);
    }
  } else if (!strcmp(action, "Constraint")) {
    if (!scene_constraint(&a->scene, 1))
      editor_log(a, "Select two objects for a distance constraint");
  } else if (!strcmp(action, "Weld")) {
    if (!scene_constraint(&a->scene, 0))
      editor_log(a, "Select two objects for a weld");
  } else if (!strcmp(action, "Align")) {
    scene_align(&a->scene, a->axis);
  } else if (!strcmp(action, "Frame") || !strcmp(action, "Label") ||
             !strcmp(action, "Input") || !strcmp(action, "Button") ||
             !strcmp(action, "Image")) {
    int type = !strcmp(action, "Frame")    ? 0
               : !strcmp(action, "Label")  ? 1
               : !strcmp(action, "Input")  ? 2
               : !strcmp(action, "Button") ? 3
                                           : 4;
    Gui *g = scene_gui(&a->scene, type);
    if (g) {
      a->selected_gui = (int)a->scene.gui_count - 1;
      a->gui_mode = true;
      editor_document(a, -1);
    }
  } else if (!strcmp(action, "Appearance") || !strcmp(action, "Layout") ||
             !strcmp(action, "UI Constraint") ||
             !strcmp(action, "Style Editor")) {
    a->show_properties = true;
    a->gui_mode = true;
  } else if (!strcmp(action, "Back")) {
    if (a->doc_history_cursor > 0)
      a->document = a->history_doc[--a->doc_history_cursor];
  } else if (!strcmp(action, "Forward")) {
    if (a->doc_history_cursor + 1 < a->doc_history_count)
      a->document = a->history_doc[++a->doc_history_cursor];
  } else if (!strcmp(action, "Run")) {
    if (a->runtime.state == 2)
      runtime_continue(&a->runtime, false);
    else
      runtime_run(&a->runtime, a->active_script);
    a->show_output = true;
  } else if (!strcmp(action, "Step"))
    runtime_continue(&a->runtime, true);
  else if (!strcmp(action, "Command") || !strcmp(action, "Output") ||
           !strcmp(action, "Breakpoints") || !strcmp(action, "Call Stack") ||
           !strcmp(action, "Watch") || !strcmp(action, "Analysis") ||
           !strcmp(action, "Script Activity") || !strcmp(action, "Find") ||
           !strcmp(action, "Go to Line")) {
    a->show_output = true;
    const char *pages[] = {"Output", "Command",  "Breakpoints",    "Call Stack",
                           "Watch",  "Analysis", "Script Activity"};
    for (int i = 0; i < 7; i++)
      if (!strcmp(action, pages[i]))
        a->output_page = i;
    if (!strcmp(action, "Analysis"))
      runtime_analyze(&a->runtime, a->active_script);
  } else if (!strcmp(action, "Format"))
    format_code(a);
  else if (!strcmp(action, "Save"))
    save_project(a);
  else if (!strcmp(action, "Load")) {
    char error[256];
    editor_stop(a);
    if (scene_load(&a->scene, a->path, error)) {
      a->selected_id = 0;
      a->selected_gui = -1;
      editor_document(a, -1);
      history_free(&a->history);
      history_capture(&a->history, &a->scene);
      reset_runtime(a);
      a->dirty = false;
      editor_log(a, "Project loaded; scripts were not executed");
    } else
      editor_log(a, error);
  } else if (!strcmp(action, "Undo")) {
    history_capture(&a->history, &a->scene);
    if (history_undo(&a->history, &a->scene))
      a->selected_id = 0;
  } else if (!strcmp(action, "Redo")) {
    if (history_redo(&a->history, &a->scene))
      a->selected_id = 0;
  } else if (!strcmp(action, "Delete")) {
    for (size_t i = a->scene.count; i > 0; i--)
      if (i <= a->scene.count && a->scene.objects[i - 1].selected)
        scene_delete(&a->scene, a->scene.objects[i - 1].id);
    a->selected_id = 0;
  } else if (!strcmp(action, "Duplicate")) {
    size_t n = a->scene.count;
    for (size_t i = 0; i < n; i++)
      if (a->scene.objects[i].selected)
        scene_duplicate(&a->scene, a->scene.objects[i].id);
  } else if (!strcmp(action, "Focus") && o) {
    a->target = point(object_world(&a->scene, o), v3(0, 0, 0));
    a->distance = fmaxf(2, length3(o->scale) * 2);
  } else if (!strcmp(action, "Settings"))
    a->show_settings = !a->show_settings;
  else if (!strcmp(action, "Docs"))
    a->show_docs = !a->show_docs;
  mark(a);
}
static void action_button(App *a, const char *label) {
  if (nk_button_label(a->ctx, label))
    editor_action(a, label);
}
/* Small vector icons stay sharp at any DPI and do not require external files.
 */
static void ribbon_button(App *a, const char *label) {
  struct nk_context *c = a->ctx;
  struct nk_rect r = nk_widget_bounds(c);
  bool pressed = nk_button_label(c, "");
  struct nk_command_buffer *b = nk_window_get_canvas(c);
  float x = r.x + r.w * .5f, y = r.y + 16;
  struct nk_color ink = nk_rgb(47, 53, 67), accent = nk_rgb(94, 111, 201);
  if (!strcmp(label, "Move") || !strcmp(label, "Transform") ||
      !strcmp(label, "Pivot")) {
    nk_stroke_line(b, x - 9, y, x + 9, y, 1.8f, ink);
    nk_stroke_line(b, x, y - 9, x, y + 9, 1.8f, ink);
    nk_fill_triangle(b, x - 12, y, x - 6, y - 4, x - 6, y + 4, ink);
    nk_fill_triangle(b, x + 12, y, x + 6, y - 4, x + 6, y + 4, ink);
    nk_fill_triangle(b, x, y - 12, x - 4, y - 6, x + 4, y - 6, ink);
    nk_fill_triangle(b, x, y + 12, x - 4, y + 6, x + 4, y + 6, ink);
  } else if (!strcmp(label, "Select")) {
    nk_fill_triangle(b, x - 7, y - 10, x + 9, y - 1, x, y + 1, ink);
    nk_stroke_line(b, x, y, x + 5, y + 10, 3, ink);
  } else if (!strcmp(label, "Rotate") || !strcmp(label, "Reset")) {
    nk_stroke_circle(b, nk_rect(x - 10, y - 10, 20, 20), 2, ink);
    nk_fill_triangle(b, x + 11, y - 7, x + 11, y + 1, x + 4, y - 4, accent);
  } else if (!strcmp(label, "Scale")) {
    nk_stroke_rect(b, nk_rect(x - 10, y - 4, 15, 15), 0, 1.8f, ink);
    nk_stroke_line(b, x, y + 1, x + 9, y - 9, 2, ink);
    nk_fill_triangle(b, x + 11, y - 11, x + 3, y - 10, x + 10, y - 3, ink);
  } else if (!strcmp(label, "Part") || !strcmp(label, "Union") ||
             !strcmp(label, "Intersect") || !strcmp(label, "Separate") ||
             !strcmp(label, "Negate") || !strcmp(label, "Join")) {
    nk_fill_rect(b, nk_rect(x - 10, y - 8, 13, 16), 3,
                 !strcmp(label, "Part") ? nk_rgb(178, 130, 236)
                                        : nk_rgb(161, 169, 187));
    nk_stroke_rect(b, nk_rect(x - 10, y - 8, 13, 16), 3, 1.4f, ink);
    if (strcmp(label, "Part"))
      nk_stroke_rect(b, nk_rect(x - 1, y - 3, 13, 16), 3, 1.6f, ink);
  } else if (!strcmp(label, "Terrain")) {
    nk_fill_triangle(b, x - 12, y + 8, x - 5, y - 7, x + 4, y + 8,
                     nk_rgb(52, 183, 160));
    nk_fill_triangle(b, x - 3, y + 8, x + 5, y - 10, x + 13, y + 8,
                     nk_rgb(52, 183, 160));
  } else if (!strcmp(label, "Material") || !strcmp(label, "Color")) {
    nk_stroke_circle(b, nk_rect(x - 11, y - 11, 22, 22), 1.7f, ink);
    nk_fill_circle(b, nk_rect(x - 7, y - 7, 5, 5), nk_rgb(239, 137, 88));
    nk_fill_circle(b, nk_rect(x + 2, y - 6, 5, 5), nk_rgb(101, 144, 217));
    nk_fill_circle(b, nk_rect(x - 1, y + 2, 5, 5), nk_rgb(94, 183, 133));
  } else if (!strcmp(label, "Character")) {
    nk_fill_circle(b, nk_rect(x - 4, y - 12, 8, 8), nk_rgb(231, 146, 35));
    nk_stroke_line(b, x, y - 3, x, y + 7, 3, nk_rgb(231, 146, 35));
    nk_stroke_line(b, x - 10, y - 2, x + 10, y - 2, 3, nk_rgb(231, 146, 35));
    nk_stroke_line(b, x, y + 6, x - 7, y + 13, 3, ink);
    nk_stroke_line(b, x, y + 6, x + 7, y + 13, 3, ink);
  } else {
    bool ui = !strcmp(label, "GUI") || !strcmp(label, "Frame") ||
              !strcmp(label, "Label") || !strcmp(label, "Input") ||
              !strcmp(label, "Image") || !strcmp(label, "Appearance") ||
              !strcmp(label, "Layout") || !strcmp(label, "UI Constraint");
    bool script = !strcmp(label, "Script") || !strcmp(label, "Command") ||
                  !strcmp(label, "Output") || !strcmp(label, "Analysis") ||
                  !strcmp(label, "Watch") || !strcmp(label, "Breakpoints");
    struct nk_color fill = ui       ? nk_rgb(223, 153, 189)
                           : script ? nk_rgb(134, 175, 59)
                                    : nk_rgb(177, 186, 207);
    nk_fill_rect(b, nk_rect(x - 10, y - 10, 20, 20), 2, fill);
    nk_stroke_rect(b, nk_rect(x - 10, y - 10, 20, 20), 2, 1.5f, ink);
    nk_stroke_line(b, x - 6, y - 5, x + 6, y - 5, 1.5f, ink);
    nk_stroke_line(b, x - 6, y, x + 3, y, 1.5f, ink);
    nk_stroke_line(b, x - 6, y + 5, x + 6, y + 5, 1.5f, ink);
  }
  static struct nk_user_font font;
  font = *c->style.font;
  font.height = 12;
  struct nk_rect text = nk_rect(r.x + 2, r.y + r.h - 19, r.w - 4, 18);
  nk_draw_text(b, text, label, (int)strlen(label), &font, nk_rgba(0, 0, 0, 0),
               ink);
  if (inside(r, c->input.mouse.pos.x, c->input.mouse.pos.y))
    nk_tooltip(c, label);
  if (pressed)
    editor_action(a, label);
}
static void ribbon(App *a) {
  struct nk_context *c = a->ctx;
  if (nk_begin(c, "Ribbon", nk_rect(0, 0, (float)a->width, 126),
               NK_WINDOW_NO_SCROLLBAR)) {
    nk_layout_row_begin(c, NK_STATIC, 28, 15);
    nk_layout_row_push(c, 80);
    const char *modes[] = {"Test", "Run", "Edit"};
    static int mode;
    mode = nk_combo(c, modes, 3, mode, 24, nk_vec2(100, 110));
    nk_layout_row_push(c, 60);
    if (nk_button_label(c, a->playing ? "Resume" : "Play"))
      editor_play(a);
    nk_layout_row_push(c, 55);
    if (nk_button_label(c, "Pause") && a->playing)
      a->paused = !a->paused;
    nk_layout_row_push(c, 50);
    if (nk_button_label(c, "Stop"))
      editor_stop(a);
    nk_layout_row_push(c, 60);
    action_button(a, "Save");
    nk_layout_row_push(c, 60);
    action_button(a, "Load");
    nk_layout_row_push(c, 55);
    action_button(a, "Undo");
    nk_layout_row_push(c, 55);
    action_button(a, "Redo");
    const char *tabs[] = {"Home", "Avatar", "UI", "Script", "Model", "Plugins"};
    for (int i = 0; i < 6; i++) {
      nk_layout_row_push(c, 73);
      if (nk_button_label(c, tabs[i])) {
        a->tab = i;
        if (i == 2) {
          a->gui_mode = true;
          editor_document(a, -1);
        } else if (i != 3)
          a->gui_mode = false;
      }
    }
    nk_layout_row_push(c, 80);
    action_button(a, "Settings");
    nk_layout_row_end(c);
    const char *home[] = {"Select",    "Move",      "Scale",      "Rotate",
                          "Transform", "Geometric", "Part",       "Terrain",
                          "Character", "GUI",       "Script",     "Import",
                          "Material",  "Color",     "Group",      "Lock",
                          "Anchor",    "Explorer",  "Properties", "Toolbox",
                          "Assets"};
    const char *model[] = {
        "Select",     "Move",       "Scale",    "Rotate",   "Transform",
        "Geometric",  "Pivot",      "Reset",    "Part",     "Effect",
        "Attachment", "Constraint", "Weld",     "Material", "Color",
        "Texture",    "Group",      "Lock",     "Anchor",   "Align",
        "Union",      "Intersect",  "Separate", "Negate",   "Difference",
        "Join",       "Sculpt"};
    const char *ui[] = {"GUI",           "Frame",       "Label",      "Input",
                        "Button",        "Image",       "Appearance", "Layout",
                        "UI Constraint", "Style Editor"};
    const char *script[] = {"Back",
                            "Forward",
                            "Script",
                            "Format",
                            "Find",
                            "Go to Line",
                            "Command",
                            "Output",
                            "Breakpoints",
                            "Call Stack",
                            "Watch",
                            "Analysis",
                            "Script Activity",
                            "Run",
                            "Step"};
    const char *avatar[] = {"Character", "Attachment", "Constraint", "Weld",
                            "Move",      "Rotate",     "Scale",      "Sculpt",
                            "Material",  "Color"};
    const char **items = home;
    int count = (int)(sizeof(home) / sizeof(home[0]));
    if (a->tab == 4) {
      items = model;
      count = (int)(sizeof(model) / sizeof(model[0]));
    } else if (a->tab == 2) {
      items = ui;
      count = (int)(sizeof(ui) / sizeof(ui[0]));
    } else if (a->tab == 3) {
      items = script;
      count = (int)(sizeof(script) / sizeof(script[0]));
    } else if (a->tab == 1) {
      items = avatar;
      count = (int)(sizeof(avatar) / sizeof(avatar[0]));
    }
    if (a->tab == 5) {
      nk_layout_row_dynamic(c, 34, 4);
      action_button(a, "Docs");
      action_button(a, "Script");
      for (int i = 0; i < a->runtime.tool_count; i++)
        if (nk_button_label(c, a->runtime.tool_names[i]))
          runtime_tool(&a->runtime, i);
    } else {
      nk_layout_row_dynamic(c, 74, 1);
      if (nk_group_begin(c, "Commands", 0)) {
        nk_layout_row_static(c, 55, 72, count);
        for (int i = 0; i < count; i++)
          ribbon_button(a, items[i]);
        nk_group_end(c);
      }
    }
  }
  nk_end(c);
}
static const char *kind_name(Kind k) {
  const char *names[] = {"Cube",  "Sphere",     "Cylinder", "Plane",
                         "Wedge", "Terrain",    "Mesh",     "Model",
                         "Union", "Attachment", "Effect",   "Character",
                         "Sound", "Pyramid"};
  return k >= 0 && k <= LX_PYRAMID ? names[k] : "Object";
}
static void tree_objects(App *a, uint32_t parent, int depth) {
  if (depth > 32)
    return;
  struct nk_context *c = a->ctx;
  for (size_t i = 0; i < a->scene.count; i++) {
    Object *o = &a->scene.objects[i];
    if (o->parent != parent)
      continue;
    if (a->filter[0] && !strstr(o->name, a->filter))
      continue;
    char label[256];
    snprintf(label, sizeof(label), "%*s%s %s%s", depth * 2, "",
             o->operand ? "[src]" : kind_name(o->kind), o->name,
             o->locked ? " [locked]" : "");
    nk_layout_row_dynamic(c, 24, 1);
    if (nk_select_label(c, label, NK_TEXT_LEFT, o->selected))
      editor_select(a, o->id, (SDL_GetModState() & KMOD_CTRL) != 0);
    tree_objects(a, o->id, depth + 1);
  }
}
static void explorer(App *a, float x, float y, float w, float h) {
  struct nk_context *c = a->ctx;
  if (nk_begin(c, "Explorer", nk_rect(x, y, w, h),
               NK_WINDOW_TITLE | NK_WINDOW_BORDER)) {
    nk_layout_row_dynamic(c, 25, 1);
    nk_edit_string_zero_terminated(c, NK_EDIT_FIELD, a->filter,
                                   sizeof(a->filter), nk_filter_default);
    nk_layout_row_dynamic(c, 20, 1);
    nk_label(c, "WORKSPACE", NK_TEXT_LEFT);
    tree_objects(a, 0, 0);
    nk_layout_row_dynamic(c, 20, 1);
    nk_label(c, "SCRIPTS", NK_TEXT_LEFT);
    for (size_t i = 0; i < a->scene.script_count; i++) {
      nk_layout_row_dynamic(c, 24, 1);
      if (nk_select_label(c, a->scene.scripts[i].name, NK_TEXT_LEFT,
                          a->document == (int)i))
        editor_document(a, (int)i);
    }
    nk_layout_row_dynamic(c, 20, 1);
    nk_label(c, "GUI", NK_TEXT_LEFT);
    for (size_t i = 0; i < a->scene.gui_count; i++) {
      nk_layout_row_dynamic(c, 24, 1);
      if (nk_select_label(c, a->scene.gui[i].name, NK_TEXT_LEFT,
                          a->selected_gui == (int)i)) {
        a->selected_gui = (int)i;
        a->gui_mode = true;
        editor_document(a, -1);
      }
    }
    nk_layout_row_dynamic(c, 25, 2);
    action_button(a, "Script");
    action_button(a, "GUI");
  }
  nk_end(c);
}
static void properties(App *a, float x, float y, float w, float h) {
  struct nk_context *c = a->ctx;
  if (nk_begin(c, "Properties", nk_rect(x, y, w, h),
               NK_WINDOW_TITLE | NK_WINDOW_BORDER)) {
    if (a->gui_mode && a->selected_gui >= 0 &&
        (size_t)a->selected_gui < a->scene.gui_count) {
      Gui *g = &a->scene.gui[a->selected_gui];
      text_field(c, "Name", g->name, sizeof(g->name));
      text_field(c, "Text / input value", g->text, sizeof(g->text));
      nk_layout_row_dynamic(c, 24, 1);
      bool_field(c, "Visible", &g->visible);
      int parent = (int)g->parent;
      nk_property_int(c, "Parent GUI ID", 0, &parent, 1000000, 1, 1);
      g->parent = (uint32_t)parent;
      nk_property_float(c, "X (pixels)", -10000, &g->x, 10000, 1, 1);
      nk_property_float(c, "Y (pixels)", -10000, &g->y, 10000, 1, 1);
      nk_property_float(c, "Width", 1, &g->w, 10000, 1, 1);
      nk_property_float(c, "Height", 1, &g->h, 10000, 1, 1);
      nk_property_float(c, "Width scale", 0, &g->scale_x, 1, .01f, .005f);
      nk_property_float(c, "Height scale", 0, &g->scale_y, 1, .01f, .005f);
      nk_property_float(c, "Anchor X", 0, &g->anchor_x, 1, .1f, .01f);
      nk_property_float(c, "Anchor Y", 0, &g->anchor_y, 1, .1f, .01f);
      const char *layouts[] = {"Free layout", "Vertical", "Horizontal"};
      g->layout = nk_combo(c, layouts, 3, g->layout, 24, nk_vec2(220, 120));
      nk_property_float(c, "Padding", 0, &g->padding, 100, 1, 1);
      nk_property_float(c, "Gap", 0, &g->gap, 100, 1, 1);
      nk_property_float(c, "Min width", 1, &g->min_w, 10000, 1, 1);
      nk_property_float(c, "Min height", 1, &g->min_h, 10000, 1, 1);
      nk_property_float(c, "Max width", 1, &g->max_w, 10000, 1, 1);
      nk_property_float(c, "Max height", 1, &g->max_h, 10000, 1, 1);
      nk_property_float(c, "Aspect (0 = free)", 0, &g->aspect, 10, .1f, .01f);
      nk_property_float(c, "Corner radius", 0, &g->rounding, 100, 1, 1);
      nk_property_int(c, "Font size", 8, &g->font_size, 72, 1, 1);
      color_field(c, "Background", g->color);
      color_field(c, "Text colour", g->text_color);
      if (g->type == 4)
        text_field(c, "Image path", g->image, sizeof(g->image));
      if (g->type == 3)
        text_field(c, "Lua click function", g->on_click, sizeof(g->on_click));
      nk_layout_row_dynamic(c, 28, 1);
      if (nk_button_label(c, "Delete GUI element")) {
        uint32_t id = g->id;
        for (size_t i = 0; i < a->scene.gui_count; i++)
          if (a->scene.gui[i].parent == id)
            a->scene.gui[i].parent = 0;
        memmove(g, g + 1,
                (a->scene.gui_count - (size_t)a->selected_gui - 1) *
                    sizeof(Gui));
        a->scene.gui_count--;
        a->selected_gui = -1;
      }
    } else {
      Object *o = scene_find(&a->scene, a->selected_id);
      if (o) {
        text_field(c, "Name", o->name, sizeof(o->name));
        char id[80];
        snprintf(id, sizeof(id), "ID %u  |  %s", o->id, kind_name(o->kind));
        nk_layout_row_dynamic(c, 20, 1);
        nk_label(c, id, NK_TEXT_LEFT);
        nk_layout_row_dynamic(c, 24, 1);
        bool_field(c, "Visible", &o->visible);
        bool_field(c, "Locked", &o->locked);
        bool_field(c, "Anchored", &o->anchored);
        vector_field(c, "Position", &o->position, .1f);
        vector_field(c, "Rotation (degrees)", &o->rotation, 1);
        vector_field(c, "Scale", &o->scale, .1f);
        o->scale.x = fmaxf(o->scale.x, .001f);
        o->scale.y = fmaxf(o->scale.y, .001f);
        o->scale.z = fmaxf(o->scale.z, .001f);
        vector_field(c, "Pivot", &o->pivot, .1f);
        nk_layout_row_dynamic(c, 25, 2);
        action_button(a, "Focus");
        action_button(a, "Duplicate");
        nk_layout_row_dynamic(c, 25, 1);
        action_button(a, "Delete");
        o = scene_find(&a->scene, a->selected_id);
        if (!o) {
          nk_end(c);
          return;
        }
        if (o->kind == LX_SOUND) {
          text_field(c, "Sound asset", o->sound, sizeof(o->sound));
          nk_layout_row_dynamic(c, 24, 1);
          nk_property_float(c, "Volume", 0, &o->volume, 2, .05f, .01f);
          nk_property_float(c, "Pitch / speed", .1f, &o->pitch, 4, .05f, .01f);
          bool_field(c, "Loop", &o->loop);
          bool_field(c, "Spatial 3D sound", &o->spatial);
          bool_field(c, "Play on start", &o->autoplay);
          nk_property_float(c, "Min distance", .1f, &o->min_distance, 10000, 1,
                            .1f);
          nk_property_float(c, "Max distance", o->min_distance,
                            &o->max_distance, 100000, 1, .1f);
          nk_layout_row_dynamic(c, 28, 2);
          if (nk_button_label(c, "Preview")) {
            char error[256];
            if (audio_load(a->audio, o, error))
              audio_command(a->audio, o->id, 0);
            else
              editor_log(a, error);
          }
          if (nk_button_label(c, "Stop sound"))
            audio_command(a->audio, o->id, 1);
        } else if (o->mesh.nv) {
          color_field(c, "Base colour", o->material.color);
          nk_layout_row_dynamic(c, 24, 1);
          nk_property_float(c, "Metallic", 0, &o->material.metallic, 1, .05f,
                            .01f);
          nk_property_float(c, "Roughness", .02f, &o->material.roughness, 1,
                            .05f, .01f);
          nk_property_float(c, "Emission", 0, &o->material.emission, 10, .1f,
                            .01f);
          text_field(c, "Albedo texture", o->material.texture,
                     sizeof(o->material.texture));
          text_field(c, "Normal map", o->material.normal,
                     sizeof(o->material.normal));
          nk_layout_row_dynamic(c, 24, 1);
          nk_label(c, "GLSL fragment shader (empty = PBR)", NK_TEXT_LEFT);
          nk_layout_row_dynamic(c, 120, 1);
          nk_edit_string_zero_terminated(c, NK_EDIT_BOX, o->material.shader,
                                         sizeof(o->material.shader),
                                         nk_filter_default);
          nk_layout_row_dynamic(c, 24, 1);
          nk_property_float(c, "Mass", .01f, &o->mass, 10000, .1f, .01f);
          nk_property_float(c, "Bounce", 0, &o->bounce, 1, .05f, .01f);
          nk_property_float(c, "Friction", 0, &o->friction, 10, .1f, .01f);
          vector_field(c, "Velocity", &o->velocity, .1f);
          if (o->kind == LX_CHARACTER) {
            nk_layout_row_dynamic(c, 24, 1);
            nk_property_float(c, "Move speed", 0, &o->speed, 100, .1f, .1f);
            nk_property_float(c, "Jump speed", 0, &o->jump, 100, .1f, .1f);
          }
          if (o->rig) {
            nk_layout_row_dynamic(c, 20, 1);
            nk_label(c, "SKELETON / REST POSE", NK_TEXT_LEFT);
            for (int k = 0; k < o->rig->count; k++) {
              nk_layout_row_dynamic(c, 22, 1);
              if (nk_select_label(c, o->rig->bones[k].name, NK_TEXT_LEFT,
                                  a->selected_bone == k))
                a->selected_bone = k;
            }
            if (a->selected_bone >= 0 && a->selected_bone < o->rig->count) {
              Bone *b = &o->rig->bones[a->selected_bone];
              vector_field(c, "Bone position", &b->position, .05f);
              vector_field(c, "Bone scale", &b->scale, .05f);
            }
          }
        }
        if (o->kind == LX_EFFECT) {
          nk_layout_row_dynamic(c, 24, 1);
          nk_property_int(c, "Effect style", 0, &o->effect, 3, 1, 1);
          color_field(c, "Particle colour", o->material.color);
        }
        nk_layout_row_dynamic(c, 22, 1);
        nk_label(c, "CUSTOM PROPERTIES", NK_TEXT_LEFT);
        for (int k = 0; k < o->property_count; k++) {
          Property *p = &o->properties[k];
          text_field(c, "Key", p->name, sizeof(p->name));
          const char *types[] = {"Number", "Boolean", "Text"};
          nk_layout_row_dynamic(c, 24, 1);
          p->type = nk_combo(c, types, 3, p->type, 22, nk_vec2(200, 110));
          if (p->type == 2)
            nk_edit_string_zero_terminated(c, NK_EDIT_FIELD, p->text,
                                           sizeof(p->text), nk_filter_default);
          else if (p->type == 1) {
            bool value = p->number != 0;
            bool_field(c, "Value", &value);
            p->number = value;
          } else {
            float v = (float)p->number;
            nk_property_float(c, "Value", -1e9f, &v, 1e9f, 1, .1f);
            p->number = v;
          }
        }
        nk_layout_row_dynamic(c, 26, 1);
        if (nk_button_label(c, "+ Property") && o->property_count < 32) {
          Property *p = &o->properties[o->property_count++];
          memset(p, 0, sizeof(*p));
          snprintf(p->name, 64, "Property%d", o->property_count);
        }
      } else {
        nk_layout_row_dynamic(c, 24, 1);
        nk_label_wrap(
            c, "Select an object or GUI element to edit its properties.");
      }
    }
  }
  nk_end(c);
}
static void toolbox(App *a, float x, float y, float w, float h) {
  struct nk_context *c = a->ctx;
  if (nk_begin(c, "Toolbox", nk_rect(x, y, w, h),
               NK_WINDOW_TITLE | NK_WINDOW_BORDER)) {
    text_field(c, "Project file", a->path, sizeof(a->path));
    nk_layout_row_dynamic(c, 26, 2);
    action_button(a, "Save");
    action_button(a, "Load");
    static char game_path[512] = "MyGame.exe";
    text_field(c, "Standalone game output", game_path, sizeof(game_path));
    nk_layout_row_dynamic(c, 28, 1);
    if (nk_button_label(c, "Build standalone game")) {
      char error[256];
      if (player_build(&a->scene, a->executable_path, game_path, error))
        editor_log(a, "Standalone game built with project and assets inside");
      else
        editor_log(a, error);
    }
    static char export_path[512] = "scene.glb";
    text_field(c, "GLB output", export_path, sizeof(export_path));
    nk_layout_row_dynamic(c, 26, 1);
    if (nk_button_label(c, "Export GLB")) {
      char error[256];
      if (scene_export_glb(&a->scene, export_path, error))
        editor_log(a, "GLB exported");
      else
        editor_log(a, error);
    }
    nk_layout_row_dynamic(c, 26, 2);
    action_button(a, "Select");
    action_button(a, "Move");
    action_button(a, "Rotate");
    action_button(a, "Scale");
    action_button(a, "Transform");
    action_button(a, "Pivot");
    nk_layout_row_dynamic(c, 24, 1);
    bool_field(c, "Local coordinates", &a->settings.local_space);
    bool_field(c, "Snap to steps", &a->settings.snap);
    nk_property_float(c, "Move step", .001f, &a->settings.move_snap, 1000, .1f,
                      .01f);
    nk_property_float(c, "Angle step", .1f, &a->settings.rotate_snap, 180, 1,
                      .1f);
    nk_property_float(c, "Scale step", .001f, &a->settings.scale_snap, 10, .05f,
                      .01f);
    nk_layout_row_dynamic(c, 22, 1);
    nk_label(c, "PRIMITIVES", NK_TEXT_LEFT);
    nk_layout_row_dynamic(c, 27, 2);
    const char *parts[] = {"Cube",  "Sphere", "Cylinder",
                           "Plane", "Wedge",  "Terrain"};
    for (int k = 0; k < 6; k++)
      if (nk_button_label(c, parts[k]))
        add_part(a, (Kind)k);
    nk_layout_row_dynamic(c, 28, 1);
    action_button(a, "Character");
    if (nk_button_label(c, "Sound")) {
      add_part(a, LX_SOUND);
      a->show_assets = true;
    }
    nk_layout_row_dynamic(c, 22, 1);
    nk_label(c, "MODELLING", NK_TEXT_LEFT);
    nk_layout_row_dynamic(c, 27, 2);
    action_button(a, "Union");
    action_button(a, "Difference");
    action_button(a, "Intersect");
    action_button(a, "Separate");
    action_button(a, "Join");
    action_button(a, "Sculpt");
    nk_layout_row_dynamic(c, 24, 1);
    Object *o = scene_find(&a->scene, a->selected_id);
    if (nk_button_label(c, "Subdivide selected") && o) {
      if (!mesh_subdivide(&o->mesh))
        editor_log(a, "Subdivision exceeds geometry budget");
    }
    nk_property_float(c, "Brush radius", .01f, &a->brush_radius, 10, .05f,
                      .01f);
    nk_property_float(c, "Strength", .001f, &a->brush_strength, 1, .01f, .005f);
    const char *brushes[] = {"Inflate / Shift deflate", "Smooth", "Flatten"};
    a->brush_mode =
        nk_combo(c, brushes, 3, a->brush_mode, 24, nk_vec2(220, 120));
    nk_layout_row_dynamic(c, 22, 1);
    nk_label(c, "ALIGN AXIS", NK_TEXT_LEFT);
    const char *axes[] = {"X", "Y", "Z"};
    nk_layout_row_dynamic(c, 26, 2);
    a->axis = nk_combo(c, axes, 3, a->axis, 22, nk_vec2(100, 100));
    action_button(a, "Align");
    nk_layout_row_dynamic(c, 22, 1);
    nk_label(c, "ENVIRONMENT", NK_TEXT_LEFT);
    const char *sky[] = {"Procedural sky", "Panorama / HDRI",
                         "Cubemap 6 faces"};
    nk_layout_row_dynamic(c, 26, 1);
    a->scene.sky_mode =
        nk_combo(c, sky, 3, a->scene.sky_mode, 22, nk_vec2(230, 120));
    if (a->scene.sky_mode) {
      const char *labels[] = {"+X / Right",  "-X / Left",  "+Y / Top",
                              "-Y / Bottom", "+Z / Front", "-Z / Back"};
      for (int k = 0; k < (a->scene.sky_mode == 2 ? 6 : 1); k++)
        text_field(c,
                   a->scene.sky_mode == 1 ? "Panorama path (HDR/PNG/JPG)"
                                          : labels[k],
                   a->scene.sky[k], 512);
      nk_layout_row_dynamic(c, 26, 1);
      if (nk_button_label(c, "Reload images / shaders"))
        renderer_clear_cache(&a->renderer);
    } else {
      float top[4] = {a->scene.sky_top[0], a->scene.sky_top[1],
                      a->scene.sky_top[2], 1},
            bottom[4] = {a->scene.sky_bottom[0], a->scene.sky_bottom[1],
                         a->scene.sky_bottom[2], 1};
      color_field(c, "Sky top", top);
      color_field(c, "Sky horizon", bottom);
      memcpy(a->scene.sky_top, top, 12);
      memcpy(a->scene.sky_bottom, bottom, 12);
    }
    V3 sun = v3(a->scene.sun[0], a->scene.sun[1], a->scene.sun[2]);
    vector_field(c, "Sun direction", &sun, .1f);
    memcpy(a->scene.sun, &sun, 12);
    nk_layout_row_dynamic(c, 25, 1);
    nk_property_float(c, "Gravity", -100, &a->scene.gravity, 100, .1f, .1f);
    nk_layout_row_dynamic(c, 22, 1);
    nk_label(c, "CONSTRAINTS", NK_TEXT_LEFT);
    for (size_t k = 0; k < a->scene.constraint_count; k++) {
      Constraint *p = &a->scene.constraints[k];
      char label[100];
      snprintf(label, sizeof(label), "%u -> %u (%s)", p->a, p->b,
               p->type == 0 ? "Weld" : "Distance");
      nk_layout_row_dynamic(c, 20, 1);
      nk_label(c, label, NK_TEXT_LEFT);
      nk_layout_row_dynamic(c, 24, 1);
      nk_property_float(c, "Length", 0, &p->length, 10000, .1f, .1f);
      nk_property_float(c, "Stiffness", 0, &p->stiffness, 1, .05f, .01f);
    }
    nk_layout_row_dynamic(c, 26, 1);
    action_button(a, "Docs");
  }
  nk_end(c);
}
#include <dirent.h>
#include <sys/stat.h>
static void asset_browser(App *a) {
  static char directory[1024] = ".";
  struct nk_context *c = a->ctx;
  if (nk_begin(c, "Assets", nk_rect(240, 155, 650, 500),
               NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE |
                   NK_WINDOW_SCALABLE | NK_WINDOW_CLOSABLE)) {
    text_field(c, "Folder", directory, sizeof(directory));
    text_field(c, "Selected file", a->asset_path, sizeof(a->asset_path));
    nk_layout_row_dynamic(c, 28, 4);
    if (nk_button_label(c, "Import mesh / sound"))
      import_asset(a);
    if (nk_button_label(c, "Apply texture")) {
      Object *o = scene_find(&a->scene, a->selected_id);
      if (o)
        snprintf(o->material.texture, 512, "%s", a->asset_path);
    }
    if (nk_button_label(c, "Use sky panorama")) {
      snprintf(a->scene.sky[0], 512, "%s", a->asset_path);
      a->scene.sky_mode = 1;
    }
    if (nk_button_label(c, "Load Lua script")) {
      FILE *f = fopen(a->asset_path, "rb");
      if (f) {
        char *code = calloc(1, LX_CODE_SIZE);
        if (code) {
          size_t n = fread(code, 1, LX_CODE_SIZE - 1, f);
          if (feof(f) && !memchr(code, 0, n)) {
            Script *s = scene_script(&a->scene, "Imported.lua");
            if (s) {
              memcpy(s->code, code, n + 1);
              editor_document(a, (int)a->scene.script_count - 1);
            }
          } else
            editor_log(a, "Lua file is too large or contains NUL");
          free(code);
        }
        fclose(f);
      }
    }
    nk_layout_row_dynamic(c, 300, 1);
    if (nk_group_begin(c, "Files", NK_WINDOW_BORDER)) {
      DIR *dir = opendir(directory);
      if (dir) {
        struct dirent *entry;
        int count = 0;
        while ((entry = readdir(dir)) && count++ < 300) {
          if (!strcmp(entry->d_name, "."))
            continue;
          char full[2048];
          snprintf(full, sizeof(full), "%s/%s", directory, entry->d_name);
          struct stat st;
          bool is_dir = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
          char label[1024];
          snprintf(label, sizeof(label), "%s %.900s",
                   is_dir ? "[DIR]" : "     ", entry->d_name);
          nk_layout_row_dynamic(c, 23, 1);
          if (nk_button_label(c, label)) {
            if (is_dir && strlen(full) < sizeof(directory))
              snprintf(directory, sizeof(directory), "%.1023s", full);
            else if (strlen(full) < sizeof(a->asset_path))
              snprintf(a->asset_path, sizeof(a->asset_path), "%.511s", full);
            break;
          }
        }
        closedir(dir);
      } else {
        nk_layout_row_dynamic(c, 24, 1);
        nk_label(c, "Cannot open folder", NK_TEXT_LEFT);
      }
      nk_group_end(c);
    }
    nk_layout_row_dynamic(c, 20, 1);
    nk_label(c, "Meshes: OBJ / glTF / GLB. Images: PNG JPG BMP TGA HDR.",
             NK_TEXT_LEFT);
    nk_label(c, "Audio: WAV W64 RF64 MP3 FLAC OGG OGA AIFF AIF AU SND.",
             NK_TEXT_LEFT);
  } else
    a->show_assets = false;
  nk_end(c);
}
static void format_code(App *a) {
  if (a->active_script < 0 || (size_t)a->active_script >= a->scene.script_count)
    return;
  Script *s = &a->scene.scripts[a->active_script];
  if (strstr(s->code, "[[") || strstr(s->code, "[=")) {
    editor_log(a, "Formatting preserves long string literals: this document "
                  "was left unchanged.");
    return;
  }
  char *out = calloc(1, LX_CODE_SIZE);
  if (!out)
    return;
  const char *p = s->code;
  size_t used = 0;
  int depth = 0;
  while (*p) {
    const char *end = strchr(p, '\n');
    size_t n = end ? (size_t)(end - p) : strlen(p);
    while (n && (*p == ' ' || *p == '\t'))
      p++, n--;
    bool closes = !strncmp(p, "end", 3) || !strncmp(p, "until ", 6) ||
                  !strncmp(p, "else", 4);
    if (closes && depth)
      depth--;
    size_t indent = (size_t)depth * 4;
    if (used + indent + n + 2 >= LX_CODE_SIZE) {
      free(out);
      editor_log(a, "Formatted code would exceed document limit");
      return;
    }
    if (n) {
      memset(out + used, ' ', indent);
      used += indent;
      memcpy(out + used, p, n);
      used += n;
    }
    out[used++] = '\n'; /* Indent only; never rewrite tokens. */
    char line[4096];
    size_t copy = n < sizeof(line) - 1 ? n : sizeof(line) - 1;
    memcpy(line, p, copy);
    line[copy] = 0;
    if ((strstr(line, "function") || strstr(line, " then") ||
         strstr(line, " do") || !strncmp(line, "repeat", 6) ||
         !strncmp(line, "else", 4)) &&
        !strstr(line, " end") && depth < 32)
      depth++;
    p = end ? end + 1 : p + n;
  }
  snprintf(s->code, LX_CODE_SIZE, "%s", out);
  free(out);
}
static void code_workspace(App *a, float x, float y, float w, float h) {
  struct nk_context *c = a->ctx;
  if (nk_begin(c, "Workspace", nk_rect(x, y, w, h),
               NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
    nk_layout_row_begin(c, NK_STATIC, 27, (int)a->scene.script_count + 3);
    nk_layout_row_push(c, 90);
    if (nk_button_label(c, "3D Scene")) {
      a->gui_mode = false;
      editor_document(a, -1);
    }
    nk_layout_row_push(c, 80);
    if (nk_button_label(c, "GUI Canvas")) {
      a->gui_mode = true;
      editor_document(a, -1);
    }
    for (size_t i = 0; i < a->scene.script_count; i++)
      if (a->scene.scripts[i].open) {
        nk_layout_row_push(c, 125);
        if (nk_button_label(c, a->scene.scripts[i].name))
          editor_document(a, (int)i);
      }
    nk_layout_row_push(c, 30);
    if (nk_button_label(c, "+"))
      editor_action(a, "Script");
    nk_layout_row_end(c);
    if (a->document >= 0 && (size_t)a->document < a->scene.script_count) {
      Script *s = &a->scene.scripts[a->document];
      nk_layout_row_dynamic(c, 26, 5);
      if (nk_button_label(c,
                          a->runtime.state == 2 ? "Continue (F5)" : "Run (F5)"))
        editor_action(a, "Run");
      if (nk_button_label(c, "Step (F10)"))
        editor_action(a, "Step");
      if (nk_button_label(c, "Format"))
        format_code(a);
      if (nk_button_label(c, a->fullscreen_code ? "Restore (F11)"
                                                : "Full screen (F11)"))
        a->fullscreen_code = !a->fullscreen_code;
      if (nk_button_label(c, "Close tab")) {
        s->open = false;
        editor_document(a, -1);
      }
      nk_layout_row_dynamic(c, 25, 3);
      nk_edit_string_zero_terminated(c, NK_EDIT_FIELD, s->name, sizeof(s->name),
                                     nk_filter_default);
      nk_edit_string_zero_terminated(c, NK_EDIT_FIELD, a->find, sizeof(a->find),
                                     nk_filter_default);
      bool find = nk_button_label(c, "Find next");
      nk_layout_row_dynamic(c, 25, 3);
      nk_property_int(c, "Line", 1, &a->goto_line, 100000, 1, 1);
      bool go = nk_button_label(c, "Go to line");
      if (nk_button_label(c, "Toggle breakpoint")) {
        int n;
        for (n = 0; n < s->break_count; n++)
          if (s->breakpoints[n] == a->goto_line)
            break;
        if (n < s->break_count) {
          memmove(&s->breakpoints[n], &s->breakpoints[n + 1],
                  (size_t)(s->break_count - n - 1) * sizeof(int));
          s->break_count--;
        } else if (n < 64)
          s->breakpoints[s->break_count++] = a->goto_line;
      }
      nk_layout_row_dynamic(c, h - 151, 1);
      nk_edit_string_zero_terminated(c, NK_EDIT_BOX | NK_EDIT_ALLOW_TAB,
                                     s->code, LX_CODE_SIZE, nk_filter_default);
      if (go || find) {
        int pos = 0;
        if (find && a->find[0]) {
          char *hit =
              strstr(s->code + (size_t)fminf((float)c->text_edit.cursor + 1,
                                             (float)strlen(s->code)),
                     a->find);
          if (!hit)
            hit = strstr(s->code, a->find);
          if (hit)
            pos = (int)(hit - s->code);
        } else {
          int line = 1;
          while (s->code[pos] && line < a->goto_line)
            if (s->code[pos++] == '\n')
              line++;
        }
        c->text_edit.cursor = pos;
        c->text_edit.select_start = pos;
        c->text_edit.select_end = find ? pos + (int)strlen(a->find) : pos;
        int line = 0;
        for (int i = 0; i < pos; i++)
          if (s->code[i] == '\n')
            line++;
        c->text_edit.scrollbar.y = (float)line * 20;
        nk_edit_focus(c, NK_EDIT_BOX);
      }
      {
        nk_end(c);
        return;
      }
    }
    /* Reserve the image rectangle; 3D is rendered immediately after layout. */
    nk_layout_row_dynamic(c, h - 53, 1);
    a->viewport = nk_widget_bounds(c);
    struct nk_image image = nk_image_id((int)a->renderer.color);
    nk_image(c, image);
  }
  nk_end(c);
}
static struct nk_rect gui_rect(const Scene *s, size_t index,
                               struct nk_rect canvas, int depth) {
  const Gui *g = &s->gui[index];
  struct nk_rect parent = canvas;
  const Gui *pg = NULL;
  if (g->parent && depth < 32)
    for (size_t i = 0; i < s->gui_count; i++)
      if (s->gui[i].id == g->parent) {
        parent = gui_rect(s, i, canvas, depth + 1);
        pg = &s->gui[i];
        break;
      }
  float width = fmaxf(g->min_w, fminf(g->max_w, g->w + parent.w * g->scale_x)),
        height = fmaxf(g->min_h, fminf(g->max_h, g->h + parent.h * g->scale_y));
  if (g->aspect > 0)
    height = width / g->aspect;
  float x = parent.x + g->x - g->anchor_x * width,
        y = parent.y + g->y - g->anchor_y * height;
  if (pg && pg->layout) {
    x = parent.x + pg->padding;
    y = parent.y + pg->padding;
    for (size_t i = 0; i < index; i++)
      if (s->gui[i].parent == g->parent && s->gui[i].visible) {
        if (pg->layout == 1)
          y += s->gui[i].h + pg->gap;
        else
          x += s->gui[i].w + pg->gap;
      }
  }
  return nk_rect(x, y, width, height);
}
static void gui_canvas(App *a) {
  static struct nk_user_font fonts[4096];
  struct nk_context *c = a->ctx;
  struct nk_rect canvas = a->viewport;
  struct nk_style_window old = c->style.window;
  c->style.window.fixed_background = nk_style_item_hide();
  c->style.window.background = nk_rgba(0, 0, 0, 0);
  c->style.window.padding = nk_vec2(0, 0);
  if (nk_begin(c, "GUI Canvas Overlay", canvas, NK_WINDOW_NO_SCROLLBAR)) {
    nk_layout_space_begin(c, NK_STATIC, canvas.h, (int)a->scene.gui_count);
    for (size_t i = 0; i < a->scene.gui_count; i++) {
      Gui *g = &a->scene.gui[i];
      fonts[i] = *c->style.font;
      fonts[i].height = (float)g->font_size;
      if (!g->visible)
        continue;
      struct nk_rect r = gui_rect(&a->scene, i, canvas, 0);
      struct nk_command_buffer *buf = nk_window_get_canvas(c);
      struct nk_color bg = nk_rgba_f(g->color[0], g->color[1], g->color[2],
                                     g->color[3]),
                      fg = nk_rgba_f(g->text_color[0], g->text_color[1],
                                     g->text_color[2], g->text_color[3]);
      nk_fill_rect(buf, r, g->rounding, bg);
      if (g->type == 4) {
        GLuint tex = renderer_texture(&a->renderer, g->image);
        if (tex) {
          struct nk_image im = nk_image_id((int)tex);
          nk_draw_image(buf, r, &im, nk_rgb(255, 255, 255));
        }
      } else if (a->playing && (g->type == 2 || g->type == 3)) {
        nk_layout_space_push(c,
                             nk_rect(r.x - canvas.x, r.y - canvas.y, r.w, r.h));
        nk_style_push_font(c, &fonts[i]);
        if (g->type == 2)
          nk_edit_string_zero_terminated(c, NK_EDIT_FIELD, g->text,
                                         sizeof(g->text), nk_filter_default);
        else if (nk_button_label(c, g->text) && g->on_click[0]) {
          bool valid = true;
          for (const char *p = g->on_click; *p; p++)
            if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                  (*p >= '0' && *p <= '9') || *p == '_'))
              valid = false;
          if (valid) {
            char code[96];
            snprintf(code, sizeof(code), "%s()", g->on_click);
            runtime_command(&a->runtime, code);
          }
        }
        nk_style_pop_font(c);
        g = &a->scene.gui[i];
      } else if (g->type != 0)
        nk_draw_text(buf, r, g->text, (int)strlen(g->text), &fonts[i], bg, fg);
      if (!a->playing && a->selected_gui == (int)i) {
        nk_stroke_rect(buf, r, 0, 2, nk_rgb(75, 140, 255));
        nk_fill_rect(buf, nk_rect(r.x + r.w - 8, r.y + r.h - 8, 8, 8), 0,
                     nk_rgb(75, 140, 255));
      }
      if (!a->playing && a->gui_mode &&
          nk_input_is_mouse_pressed(&c->input, NK_BUTTON_LEFT) &&
          inside(r, c->input.mouse.pos.x, c->input.mouse.pos.y))
        a->selected_gui = (int)i;
    }
    nk_layout_space_end(c);
  }
  nk_end(c);
  c->style.window = old;
}
static void output(App *a, float x, float y, float w, float h) {
  struct nk_context *c = a->ctx;
  int page = a->output_page;
  if (nk_begin(c, "Output & Debugger", nk_rect(x, y, w, h),
               NK_WINDOW_TITLE | NK_WINDOW_BORDER)) {
    const char *pages[] = {"Output", "Command",  "Breakpoints", "Call Stack",
                           "Watch",  "Analysis", "Activity"};
    nk_layout_row_dynamic(c, 23, 7);
    for (int i = 0; i < 7; i++)
      if (nk_button_label(c, pages[i]))
        page = a->output_page = i;
    nk_layout_row_dynamic(c, h - 82, 1);
    if (nk_group_begin(c, "Debug content", 0)) {
      if (page == 0) {
        nk_layout_row_dynamic(c, 18, 1);
        const char *p = a->runtime.output;
        while (*p) {
          const char *end = strchr(p, '\n');
          int len = end ? (int)(end - p) : (int)strlen(p);
          nk_text(c, p, len, NK_TEXT_LEFT);
          p = end ? end + 1 : p + len;
        }
      } else if (page == 1) {
        nk_layout_row_dynamic(c, 26, 2);
        nk_edit_string_zero_terminated(c, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                                       a->command, sizeof(a->command),
                                       nk_filter_default);
        if (nk_button_label(c, "Execute"))
          runtime_command(&a->runtime, a->command);
      } else if (page == 2) {
        nk_layout_row_dynamic(c, 20, 1);
        for (size_t i = 0; i < a->scene.script_count; i++)
          for (int k = 0; k < a->scene.scripts[i].break_count; k++) {
            char text[128];
            snprintf(text, sizeof(text), "%s:%d", a->scene.scripts[i].name,
                     a->scene.scripts[i].breakpoints[k]);
            if (nk_button_label(c, text)) {
              editor_document(a, (int)i);
              a->goto_line = a->scene.scripts[i].breakpoints[k];
            }
          }
      } else if (page == 3) {
        nk_layout_row_dynamic(c, 100, 1);
        nk_label_wrap(c, a->runtime.stack);
      } else if (page == 4) {
        nk_layout_row_dynamic(c, 25, 2);
        nk_edit_string_zero_terminated(c, NK_EDIT_FIELD, a->watch_expr,
                                       sizeof(a->watch_expr),
                                       nk_filter_default);
        if (nk_button_label(c, "Inspect local / global"))
          runtime_watch(&a->runtime, a->watch_expr);
        nk_layout_row_dynamic(c, 40, 1);
        nk_label_wrap(c, a->runtime.watch);
      } else if (page == 5) {
        nk_layout_row_dynamic(c, 25, 1);
        if (nk_button_label(c, "Analyze syntax"))
          runtime_analyze(&a->runtime, a->active_script);
        nk_layout_row_dynamic(c, 70, 1);
        nk_label_wrap(c, a->runtime.analysis);
      } else {
        nk_layout_row_dynamic(c, 20, 1);
        for (size_t i = 0; i < a->scene.script_count; i++) {
          char text[180];
          snprintf(text, sizeof(text), "%s | calls %u | CPU %.3f ms",
                   a->scene.scripts[i].name, a->runtime.calls[i],
                   a->runtime.script_ms[i]);
          nk_label(c, text, NK_TEXT_LEFT);
        }
      }
      nk_group_end(c);
    }
  }
  nk_end(c);
}
static void settings_window(App *a) {
  struct nk_context *c = a->ctx;
  Settings *s = &a->settings;
  if (nk_begin(c, "Settings", nk_rect(350, 160, 650, 590),
               NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_CLOSABLE |
                   NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
    nk_layout_row_dynamic(c, 25, 1);
    nk_label(c, "CAMERA & WORLD", NK_TEXT_LEFT);
    nk_property_float(c, "Fly speed", .1f, &s->camera_speed, 1000, .5f, .1f);
    nk_property_float(c, "Orbit sensitivity", .01f, &s->orbit_speed, 5, .05f,
                      .01f);
    nk_property_float(c, "Field of view", 10, &s->fov, 150, 1, .1f);
    nk_property_float(c, "Near clip", .001f, &s->near_clip, 10, .01f, .001f);
    nk_property_float(c, "Far clip", 100, &s->far_clip, 10000000, 1000, 100);
    bool_field(c, "Invert vertical orbit", &s->invert_y);
    bool_field(c, "Infinite world grid", &s->grid);
    bool_field(c, "Adaptive grid divisions", &s->adaptive_grid);
    bool_field(c, "World axes", &s->axes);
    nk_property_float(c, "Minimum grid cell", .01f, &s->grid_size, 10000, .1f,
                      .01f);
    bool_field(c, "Transform snapping", &s->snap);
    bool_field(c, "Local transform axes", &s->local_space);
    nk_property_float(c, "Move snap", .001f, &s->move_snap, 1000, .1f, .01f);
    nk_property_float(c, "Rotation snap", .1f, &s->rotate_snap, 180, 1, .1f);
    nk_property_float(c, "Scale snap", .001f, &s->scale_snap, 10, .05f, .005f);
    nk_label(c, "AUDIO & SIMULATION", NK_TEXT_LEFT);
    nk_property_float(c, "Master volume", 0, &s->master_volume, 1, .05f, .01f);
    nk_property_float(c, "Physics rate Hz", 30, &s->physics_hz, 240, 10, 1);
    nk_label(c, "EDITOR", NK_TEXT_LEFT);
    bool old = s->vsync;
    bool_field(c, "VSync", &s->vsync);
    if (old != s->vsync)
      SDL_GL_SetSwapInterval(s->vsync ? 1 : 0);
    const char *themes[] = {"Light", "Dark"};
    s->theme = nk_combo(c, themes, 2, s->theme, 25, nk_vec2(200, 100));
    nk_property_float(c, "UI scale (restart)", .75f, &s->ui_scale, 2, .05f,
                      .01f);
    bool_field(c, "Auto-save", &s->autosave);
    nk_property_int(c, "Auto-save interval (s)", 30, &s->autosave_seconds, 3600,
                    30, 1);
    bool_field(c, "Trusted Lua (restart Play to apply)", &s->trusted);
    text_field(c, "Default project directory", s->project_dir,
               sizeof(s->project_dir));
    nk_layout_row_dynamic(c, 28, 3);
    if (nk_button_label(c, "Save settings")) {
      if (settings_save(s, a->settings_path))
        editor_log(a, "Settings saved");
      else
        editor_log(a, "Cannot save settings");
    }
    if (nk_button_label(c, "Reset defaults"))
      settings_defaults(s);
    if (nk_button_label(c, "Close"))
      a->show_settings = false;
  } else
    a->show_settings = false;
  nk_end(c);
}
static const char help[] =
    "LUNORYX C STUDIO\n\n"
    "Home / Model: Q select, W move, E rotate, R scale. Left-drag an axis to "
    "constrain movement. Drag a selected object to move in the camera plane. "
    "Ctrl-click adds to selection. F focuses the selection. RMB orbit, MMB "
    "pan, wheel zoom. RMB + WASD/QE flies through the world; Shift "
    "accelerates. Grid follows the camera, with fixed world coordinates and "
    "major divisions every ten cells.\n\n"
    "Model: Group retains a hierarchy; Join combines triangles. Union, "
    "Difference and Intersect split closed triangle solids using BSP. Mark an "
    "operand Negate, then Union to cut it out. Separate restores retained "
    "operands. Use closed, consistently wound meshes. Sculpt supports "
    "inflate/deflate (Shift), smooth, flatten and subdivision. Undo/redo "
    "includes mesh edits. Pivot changes the transform origin.\n\n"
    "UI: Frame, Label, Input, Button and Image have pixel offsets, "
    "proportional sizes, anchors, min/max size, aspect ratio, parent layouts, "
    "colours and rounded corners. Drag the selected element on the canvas; "
    "drag its lower-right handle to resize. In Play, inputs accept text and "
    "buttons call the named Lua global function.\n\n"
    "Script: F5 runs or continues; F10 steps a suspended Lua line; F9 toggles "
    "a breakpoint at the chosen line. Open Output for command execution, "
    "breakpoints, call stack, local/global watch, syntax analysis and CPU "
    "activity. F11 expands code. All scripts are preserved when tabs are "
    "closed. Plugins use register_tool(name, function() ... end).\n\n"
    "Lua API uses stable positive IDs. add_object(name, "
    "'cube'|'sphere'|'cylinder'|'plane'|'wedge'|'terrain'|'sound') returns an "
    "ID. find_object(name), remove_object(id), clear_scene(), "
    "get_object_count(), duplicate_object(id), set_name(id,name), "
    "set_parent(id,parentID). set_position/get_position, "
    "set_rotation/get_rotation, set_scale/get_scale, set_color/get_color, "
    "set_velocity/get_velocity. Setters accept id,x,y,z; getters return x,y,z. "
    "set_material(id,metallic,roughness), set_visible(id,bool), "
    "set_dynamic(id,bool), set_anchored(id,bool), set_texture(id,path), "
    "set_property(id,key,value), get_property(id,key), "
    "add_character(height,width), set_skybox(path), add_gui(type), "
    "set_gui_text(id,text), play_sound(id), stop_sound(id). function "
    "on_update(dt) is called during Play. Loaded projects never execute "
    "scripts automatically.\n\n"
    "Audio: import WAV/WAVE/W64/RF64, MP3, FLAC, OGG/OGA (Vorbis), "
    "AIFF/AIF/AIFC (PCM/sowt/float32), AU/SND (PCM/float). Sound objects "
    "support preview, autoplay, looping, pitch, volume and positional distance "
    "attenuation.\n\n"
    "MCP: launch Lunoryx.exe --mcp for local stdio JSON-RPC. The server owns "
    "an independent headless scene. Tools: "
    "scene_list/create/transform/delete/boolean, project_save/load, "
    "asset_import/export, script_execute, simulation_step, scene_undo/redo. "
    "Resource: lunoryx://scene. No socket or external service is required.\n\n"
    "Projects: Save/Load uses .lunoryx. Schema 1 from v0.2 is migrated. Undo "
    "keeps up to 64 snapshots / 64MiB. Assets use filesystem paths: keep them "
    "with your project. Meshes, rig data, GUI, scripts and constraints are "
    "stored in the project. Asset browser accepts OBJ/glTF/GLB; GLB export "
    "writes visible geometry and PBR factors.\n\n"
    "Settings persist in your user settings folder. Auto-save writes a "
    "separate .autosave.lunoryx file. Lua uses an instruction budget and "
    "memory cap; trusted mode exposes standard file/process libraries.\n\n"
    "This is an early test version. Blender and Roblox project formats are not "
    "supported. Physics uses oriented boxes and triangle narrow-phase for "
    "static mesh surfaces, without a production rigid-body solver. Imported "
    "animation currently plays the first linear/step clip; cubic animation "
    "channels are skipped. HDR panoramas are displayed with image-loader tone "
    "mapping.\n";
static void docs_window(App *a) {
  struct nk_context *c = a->ctx;
  if (nk_begin(c, "Documentation", nk_rect(270, 150, 800, 600),
               NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_CLOSABLE |
                   NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
    static int selected_license = 0;
    const char *choices[1 + sizeof(lx_licenses) / sizeof(lx_licenses[0])];
    choices[0] = "User guide / API";
    for (size_t i = 0; i < sizeof(lx_licenses) / sizeof(lx_licenses[0]); i++)
      choices[i + 1] = lx_licenses[i].name;
    nk_layout_row_dynamic(c, 26, 1);
    selected_license = nk_combo(
        c, choices, (int)(1 + sizeof(lx_licenses) / sizeof(lx_licenses[0])),
        selected_license, 24, nk_vec2(400, 300));
    const char *p =
        selected_license ? lx_licenses[selected_license - 1].text : help;
    nk_layout_row_dynamic(c, 18, 1);
    while (*p) {
      const char *end = strchr(p, '\n');
      size_t len = end ? (size_t)(end - p) : strlen(p);
      if (len) {
        float height = (float)(len / 90 + 2) * 19;
        nk_layout_row_dynamic(c, height, 1);
        nk_text_wrap(c, p, (int)len);
      }
      p = end ? end + 1 : p + len;
    }
  } else
    a->show_docs = false;
  nk_end(c);
}
static void theme(App *a) {
  static int last = -1;
  if (last == a->settings.theme)
    return;
  last = a->settings.theme;
  struct nk_color colors[NK_COLOR_COUNT];
  if (last == 1) {
    nk_style_default(a->ctx);
    return;
  }
  colors[NK_COLOR_TEXT] = nk_rgb(40, 43, 53);
  colors[NK_COLOR_WINDOW] = nk_rgb(249, 249, 252);
  colors[NK_COLOR_HEADER] = nk_rgb(232, 233, 240);
  colors[NK_COLOR_BORDER] = nk_rgb(210, 213, 223);
  colors[NK_COLOR_BUTTON] = nk_rgb(240, 241, 247);
  colors[NK_COLOR_BUTTON_HOVER] = nk_rgb(220, 229, 251);
  colors[NK_COLOR_BUTTON_ACTIVE] = nk_rgb(192, 211, 251);
  colors[NK_COLOR_TOGGLE] = nk_rgb(210, 214, 226);
  colors[NK_COLOR_TOGGLE_HOVER] = nk_rgb(187, 204, 239);
  colors[NK_COLOR_TOGGLE_CURSOR] = nk_rgb(66, 117, 245);
  colors[NK_COLOR_SELECT] = nk_rgb(231, 234, 243);
  colors[NK_COLOR_SELECT_ACTIVE] = nk_rgb(179, 205, 251);
  colors[NK_COLOR_SLIDER] = nk_rgb(210, 214, 225);
  colors[NK_COLOR_SLIDER_CURSOR] = nk_rgb(93, 136, 242);
  colors[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb(70, 120, 250);
  colors[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb(40, 95, 240);
  colors[NK_COLOR_PROPERTY] = nk_rgb(236, 238, 245);
  colors[NK_COLOR_EDIT] = nk_rgb(255, 255, 255);
  colors[NK_COLOR_EDIT_CURSOR] = nk_rgb(30, 35, 50);
  colors[NK_COLOR_COMBO] = nk_rgb(239, 240, 247);
  colors[NK_COLOR_CHART] = nk_rgb(240, 240, 248);
  colors[NK_COLOR_CHART_COLOR] = nk_rgb(70, 120, 230);
  colors[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb(240, 140, 70);
  colors[NK_COLOR_SCROLLBAR] = nk_rgb(237, 239, 246);
  colors[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb(180, 187, 206);
  colors[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb(145, 160, 196);
  colors[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb(110, 140, 195);
  colors[NK_COLOR_TAB_HEADER] = nk_rgb(227, 231, 241);
  nk_style_from_table(a->ctx, colors);
}
void editor_draw(App *a) {
  theme(a);
  if (a->player) {
    a->viewport = nk_rect(0, 0, (float)a->width, (float)a->height);
    struct nk_context *c = a->ctx;
    struct nk_vec2 padding = c->style.window.padding;
    c->style.window.padding = nk_vec2(0, 0);
    if (nk_begin(c, "Player", a->viewport, NK_WINDOW_NO_SCROLLBAR)) {
      nk_layout_row_dynamic(c, (float)a->height, 1);
      nk_image(c, nk_image_id((int)a->renderer.color));
    }
    nk_end(c);
    c->style.window.padding = padding;
    if (a->scene.gui_count)
      gui_canvas(a);
    return;
  }

  ribbon(a);
  float top = 126, bottom = a->show_output ? 205 : 30,
        left = a->show_toolbox ? 230 : 0,
        right = (a->show_explorer || a->show_properties) ? 290 : 0;
  if (a->fullscreen_code && a->document >= 0) {
    left = right = 0;
    bottom = 30;
  }
  float height = (float)a->height - top - bottom;
  if (height < 150)
    height = 150;
  if (left)
    toolbox(a, 0, top, left, height);
  if (right) {
    float split = a->show_explorer && a->show_properties ? height * .38f
                  : a->show_explorer                     ? height
                                                         : 0;
    if (a->show_explorer)
      explorer(a, (float)a->width - right, top, right, split);
    if (a->show_properties)
      properties(a, (float)a->width - right, top + split, right,
                 height - split);
  }
  code_workspace(a, left, top, (float)a->width - left - right, height);
  if (a->document < 0 && (a->gui_mode || a->playing) && a->scene.gui_count)
    gui_canvas(a);
  if (a->show_output && !a->fullscreen_code)
    output(a, 0, (float)a->height - bottom, (float)a->width, bottom - 28);
  if (nk_begin(a->ctx, "Status",
               nk_rect(0, (float)a->height - 28, (float)a->width, 28),
               NK_WINDOW_NO_SCROLLBAR)) {
    nk_layout_row_dynamic(a->ctx, 18, 1);
    char status[1024];
    snprintf(status, sizeof(status),
             "%s%s | %zu objects | %s | yaw %.1f | %.400s",
             a->playing ? a->paused ? "PAUSED" : "PLAY" : "EDIT",
             a->runtime.state == 2 ? " / BREAKPOINT" : "", a->scene.count,
             a->settings.local_space ? "LOCAL" : "WORLD", a->yaw, a->status);
    nk_label(a->ctx, status, NK_TEXT_LEFT);
  }
  nk_end(a->ctx);
  if (a->show_assets)
    asset_browser(a);
  if (a->show_settings)
    settings_window(a);
  if (a->show_docs)
    docs_window(a);
}
Ray editor_ray(const App *a, float x, float y) {
  float nx = (x - a->viewport.x) / a->viewport.w * 2 - 1,
        ny = 1 - (y - a->viewport.y) / a->viewport.h * 2;
  V3 f = unit(sub(a->target, a->camera)), r = unit(cross(f, v3(0, 1, 0))),
     u = cross(r, f);
  float t = tanf(a->settings.fov * LX_PI / 360);
  Ray ray = {a->camera,
             unit(add(f, add(mul(r, nx * t * a->viewport.w / a->viewport.h),
                             mul(u, ny * t))))};
  return ray;
}
bool editor_project_point(const App *a, V3 p, float *x, float *y) {
  V3 f = unit(sub(a->target, a->camera)), r = unit(cross(f, v3(0, 1, 0))),
     u = cross(r, f), d = sub(p, a->camera);
  float z = dot(d, f);
  if (z <= a->settings.near_clip)
    return false;
  float t = tanf(a->settings.fov * LX_PI / 360);
  *x = a->viewport.x + a->viewport.w * .5f +
       dot(d, r) / z / t * a->viewport.h * .5f;
  *y = a->viewport.y + a->viewport.h * .5f -
       dot(d, u) / z / t * a->viewport.h * .5f;
  return true;
}
static bool plane_hit(Ray ray, V3 point_on, V3 normal, V3 *hit) {
  float d = dot(ray.direction, normal);
  if (fabsf(d) < 1e-6f)
    return false;
  float t = dot(sub(point_on, ray.origin), normal) / d;
  if (t < 0)
    return false;
  *hit = add(ray.origin, mul(ray.direction, t));
  return true;
}
static float segment_distance(float px, float py, float ax, float ay, float bx,
                              float by) {
  float dx = bx - ax, dy = by - ay, d = dx * dx + dy * dy;
  float t = d > 0 ? ((px - ax) * dx + (py - ay) * dy) / d : 0;
  t = fmaxf(0, fminf(1, t));
  float x = ax + t * dx - px, y = ay + t * dy - py;
  return sqrtf(x * x + y * y);
}
static V3 tool_axis(const App *a, const Object *o, int k) {
  V3 v = k == 0 ? v3(1, 0, 0) : k == 1 ? v3(0, 1, 0) : v3(0, 0, 1);
  return a->settings.local_space
             ? unit(direction(object_world(&a->scene, o), v))
             : v;
}
static int gizmo_axis(const App *a, const Object *o, float mx, float my) {
  V3 center = point(object_world(&a->scene, o), o->pivot);
  float cx, cy;
  if (!editor_project_point(a, center, &cx, &cy))
    return -1;
  if (hypotf(mx - cx, my - cy) < 9)
    return -1;
  float size = fmaxf(.1f, length3(sub(center, a->camera)) * .12f), best = 12;
  int axis = -1;
  for (int k = 0; k < 3; k++) {
    V3 v = tool_axis(a, o, k);
    if (a->tool == 3) {
      V3 u = unit(cross(v, k == 1 ? v3(1, 0, 0) : v3(0, 1, 0))),
         w = cross(v, u);
      for (int j = 0; j < 64; j++) {
        float t = j * 2 * LX_PI / 64, n = (j + 1) * 2 * LX_PI / 64, x1, y1, x2,
              y2;
        if (editor_project_point(
                a,
                add(center, mul(add(mul(u, cosf(t)), mul(w, sinf(t))), size)),
                &x1, &y1) &&
            editor_project_point(
                a,
                add(center, mul(add(mul(u, cosf(n)), mul(w, sinf(n))), size)),
                &x2, &y2)) {
          float d = segment_distance(mx, my, x1, y1, x2, y2);
          if (d < best) {
            best = d;
            axis = k;
          }
        }
      }
    } else {
      float x, y;
      if (editor_project_point(a, add(center, mul(v, size)), &x, &y)) {
        float d = segment_distance(mx, my, cx, cy, x, y);
        if (d < best) {
          best = d;
          axis = k;
        }
      }
    }
  }
  return axis;
}
static void drag_begin(App *a, Object *o, int axis, float mx, float my) {
  free(a->drag_objects);
  a->drag_objects = NULL;
  a->drag_count = 0;
  size_t n = 0;
  for (size_t i = 0; i < a->scene.count; i++)
    if (a->scene.objects[i].selected && !a->scene.objects[i].locked)
      n++;
  a->drag_objects = calloc(n ? n : 1, sizeof(Object));
  if (!a->drag_objects)
    return;
  for (size_t i = 0; i < a->scene.count; i++)
    if (a->scene.objects[i].selected && !a->scene.objects[i].locked)
      a->drag_objects[a->drag_count++] = a->scene.objects[i];
  a->drag_origin = point(object_world(&a->scene, o), o->pivot);
  a->drag_start_x = mx;
  a->drag_start_y = my;
  a->drag_axis = axis;
  V3 normal = unit(sub(a->camera, a->drag_origin));
  if (axis >= 0) {
    V3 v = tool_axis(a, o, axis);
    normal = unit(cross(v, cross(normal, v)));
  }
  if (!plane_hit(editor_ray(a, mx, my), a->drag_origin, normal, &a->drag_hit))
    a->drag_hit = a->drag_origin;
  a->dragging = true;
  history_capture(&a->history, &a->scene);
}
static float snapped(float value, float step, bool snap) {
  return snap && step > 0 ? roundf(value / step) * step : value;
}
static void drag_update(App *a, Object *selected, float mx, float my) {
  V3 normal = unit(sub(a->camera, a->drag_origin)), axis = v3(0, 0, 0);
  if (a->drag_axis >= 0) {
    axis = tool_axis(a, selected, a->drag_axis);
    normal = unit(cross(axis, cross(normal, axis)));
  }
  V3 hit;
  if (!plane_hit(editor_ray(a, mx, my), a->drag_origin, normal, &hit))
    return;
  V3 delta = sub(hit, a->drag_hit);
  if (a->drag_axis >= 0)
    delta = mul(axis, dot(delta, axis));
  delta = v3(snapped(delta.x, a->settings.move_snap, a->settings.snap),
             snapped(delta.y, a->settings.move_snap, a->settings.snap),
             snapped(delta.z, a->settings.move_snap, a->settings.snap));
  float amount = snapped((mx - a->drag_start_x - (my - a->drag_start_y)) * .01f,
                         a->settings.scale_snap, a->settings.snap);
  float angle = snapped((mx - a->drag_start_x - (my - a->drag_start_y)) * .6f,
                        a->settings.rotate_snap, a->settings.snap);
  for (size_t i = 0; i < a->drag_count; i++) {
    const Object *start = &a->drag_objects[i];
    Object *o = scene_find(&a->scene, start->id);
    if (!o)
      continue; /* Selected descendants follow selected parents instead of
                   applying the delta twice. */
    bool inherited = false;
    uint32_t parent = o->parent;
    for (size_t depth = 0; parent && depth < a->scene.count; depth++) {
      const Object *p = scene_get(&a->scene, parent);
      if (!p)
        break;
      if (p->selected)
        inherited = true;
      parent = p->parent;
    }
    if (inherited)
      continue;
    if (a->tool == 1 || a->tool == 4) {
      V3 local = delta;
      const Object *p = scene_get(&a->scene, o->parent);
      if (p)
        local = direction(inverse(object_world(&a->scene, p)), delta);
      o->position = add(start->position, local);
    } else if (a->tool == 5) {
      M4 world = object_world(&a->scene, o);
      V3 local = direction(inverse(world), delta);
      o->pivot =
          add(start->pivot,
              local); /* Keep geometry stationary when changing the pivot. */
      M4 transform = trs(v3(0, 0, 0), quat(start->rotation), start->scale);
      o->position = add(start->position, direction(transform, local));
    } else if (a->tool == 2) {
      V3 value = start->scale;
      if (a->drag_axis < 0)
        value = mul(value, fmaxf(.001f, 1 + amount));
      else {
        float *v = a->drag_axis == 0   ? &value.x
                   : a->drag_axis == 1 ? &value.y
                                       : &value.z;
        *v = fmaxf(.001f, *v * (1 + amount));
      }
      o->scale = value;
    } else if (a->tool == 3) {
      o->rotation = start->rotation;
      float *v = a->drag_axis == 0   ? &o->rotation.x
                 : a->drag_axis == 1 ? &o->rotation.y
                                     : &o->rotation.z;
      *v = wrap_angle(*v + angle);
    }
  }
  mark(a);
}
void editor_interaction(App *a, float dt) {
  struct nk_input *input = &a->ctx->input;
  float mx = input->mouse.pos.x, my = input->mouse.pos.y;
  bool hover = inside(a->viewport, mx, my) && a->document < 0 &&
               !a->show_assets && !a->show_settings && !a->show_docs;
  bool left = nk_input_is_mouse_down(input, NK_BUTTON_LEFT);
  if (!left && a->dragging) {
    a->dragging = false;
    free(a->drag_objects);
    a->drag_objects = NULL;
    a->drag_count = 0;
    history_capture(&a->history, &a->scene);
  }
  if (!hover && !a->dragging)
    return;
  if (a->player)
    return;
  if (a->gui_mode && !a->playing) {
    if (a->selected_gui >= 0 && (size_t)a->selected_gui < a->scene.gui_count) {
      Gui *g = &a->scene.gui[a->selected_gui];
      struct nk_rect rect =
          gui_rect(&a->scene, (size_t)a->selected_gui, a->viewport, 0);
      if (nk_input_is_mouse_pressed(input, NK_BUTTON_LEFT) &&
          inside(rect, mx, my)) {
        a->drag_started = true;
        a->drag_axis =
            mx > rect.x + rect.w - 12 && my > rect.y + rect.h - 12 ? 1 : 0;
        a->drag_start_x = mx;
        a->drag_start_y = my;
        a->drag_origin = v3(g->x, g->y, 0);
        a->drag_hit = v3(g->w, g->h, 0);
        history_capture(&a->history, &a->scene);
      }
      if (left && a->drag_started) {
        if (a->drag_axis) {
          g->w = fmaxf(1, a->drag_hit.x + mx - a->drag_start_x);
          g->h = fmaxf(1, a->drag_hit.y + my - a->drag_start_y);
        } else {
          g->x = a->drag_origin.x + mx - a->drag_start_x;
          g->y = a->drag_origin.y + my - a->drag_start_y;
        }
        mark(a);
      }
      if (!left)
        a->drag_started = false;
    }
    return;
  }
  Object *o = scene_find(&a->scene, a->selected_id);
  if (a->tool == 6 && o && !o->locked && !a->playing) {
    uint32_t id = 0;
    V3 hit;
    float distance = 1e9f;
    if (scene_raycast(&a->scene, editor_ray(a, mx, my), &id, &hit, &distance) &&
        id == o->id) {
      if (nk_input_is_mouse_pressed(input, NK_BUTTON_LEFT))
        history_capture(&a->history, &a->scene);
      if (left) {
        V3 local = point(inverse(object_world(&a->scene, o)), hit);
        float sign = (SDL_GetModState() & KMOD_SHIFT) ? -1 : 1;
        mesh_sculpt(&o->mesh, local, a->brush_radius,
                    a->brush_strength * dt * 10 * sign, a->brush_mode);
        mark(a);
      }
    }
    return;
  }
  if (nk_input_is_mouse_pressed(input, NK_BUTTON_LEFT) && !a->playing) {
    int axis = o && a->tool > 0 && a->tool < 6 ? gizmo_axis(a, o, mx, my) : -1;
    if (axis >= 0 && o && !o->locked) {
      drag_begin(a, o, axis, mx, my);
      return;
    }
    uint32_t id = 0;
    V3 hit;
    float dist = 1e9f;
    if (!scene_raycast(&a->scene, editor_ray(a, mx, my), &id, &hit,
                       &dist)) { /* Editor-only markers for non-mesh objects are
                                    pickable in screen space. */
      float best = 14;
      for (size_t i = 0; i < a->scene.count; i++) {
        Object *p = &a->scene.objects[i];
        if (p->mesh.nv || p->locked || !object_visible(&a->scene, p))
          continue;
        float x, y;
        if (editor_project_point(
                a, point(object_world(&a->scene, p), v3(0, 0, 0)), &x, &y)) {
          float d = hypotf(x - mx, y - my);
          if (d < best) {
            best = d;
            id = p->id;
          }
        }
      }
    }
    bool toggle = (SDL_GetModState() & KMOD_CTRL) != 0;
    bool already = id && scene_find(&a->scene, id)->selected;
    if (!already || toggle)
      editor_select(a, id, toggle);
    else
      a->selected_id = id;
    o = scene_find(&a->scene, a->selected_id);
    if (o && already && !toggle && a->tool > 0 && a->tool < 6 && !o->locked)
      drag_begin(a, o, -1, mx, my);
  }
  if (a->dragging && left && o)
    drag_update(a, o, mx, my);
}
void editor_event(App *a, SDL_Event *event) {
  int mx, my;
  Uint32 buttons = SDL_GetMouseState(&mx, &my);
  bool hover = inside(a->viewport, (float)mx, (float)my) && a->document < 0 &&
               !a->show_assets && !a->show_settings && !a->show_docs;
  bool text = a->ctx->text_edit.active != 0;
  if (event->type == SDL_DROPFILE) {
    snprintf(a->asset_path, 512, "%s", event->drop.file);
    SDL_free(event->drop.file);
    import_asset(a);
  }
  if (event->type == SDL_MOUSEWHEEL && hover) {
    a->distance *= powf(.85f, (float)event->wheel.y);
    a->distance = fmaxf(.05f, fminf(a->distance, a->settings.far_clip * .1f));
  }
  if (event->type == SDL_MOUSEMOTION && hover) {
    if (buttons & SDL_BUTTON_RMASK) {
      a->yaw =
          wrap_angle(a->yaw + event->motion.xrel * a->settings.orbit_speed);
      float sign = a->settings.invert_y ? -1 : 1;
      a->pitch =
          fmaxf(-89, fminf(89, a->pitch + event->motion.yrel *
                                              a->settings.orbit_speed * sign));
    } else if (buttons & SDL_BUTTON_MMASK) {
      V3 f = unit(sub(a->target, a->camera)), r = unit(cross(f, v3(0, 1, 0))),
         u = cross(r, f);
      float speed = a->distance * .0015f;
      a->target = add(a->target, add(mul(r, -event->motion.xrel * speed),
                                     mul(u, event->motion.yrel * speed)));
    }
  }
  if (a->player && event->type == SDL_KEYDOWN &&
      event->key.keysym.sym == SDLK_ESCAPE) {
    a->running = false;
    return;
  }
  if (event->type != SDL_KEYDOWN || event->key.repeat)
    return;
  SDL_Keycode k = event->key.keysym.sym;
  bool ctrl = (event->key.keysym.mod & KMOD_CTRL) != 0;
  if (k == SDLK_F5) {
    if (a->document >= 0)
      editor_action(a, "Run");
    else
      editor_play(a);
    return;
  }
  if (k == SDLK_F6) {
    editor_stop(a);
    return;
  }
  if (k == SDLK_F10) {
    runtime_continue(&a->runtime, true);
    return;
  }
  if (k == SDLK_F11) {
    a->fullscreen_code = !a->fullscreen_code;
    return;
  }
  if (k == SDLK_F9) {
    if (a->active_script >= 0 &&
        (size_t)a->active_script < a->scene.script_count) {
      Script *s = &a->scene.scripts[a->active_script];
      int i;
      for (i = 0; i < s->break_count; i++)
        if (s->breakpoints[i] == a->goto_line)
          break;
      if (i < s->break_count) {
        memmove(&s->breakpoints[i], &s->breakpoints[i + 1],
                (size_t)(s->break_count - i - 1) * sizeof(int));
        s->break_count--;
      } else if (i < 64)
        s->breakpoints[s->break_count++] = a->goto_line;
    }
    return;
  }
  if (ctrl && k == SDLK_s) {
    save_project(a);
    return;
  }
  if (text)
    return;
  if (ctrl && k == SDLK_z) {
    editor_action(a, "Undo");
    return;
  }
  if (ctrl && k == SDLK_y) {
    editor_action(a, "Redo");
    return;
  }
  if (ctrl && k == SDLK_d) {
    editor_action(a, "Duplicate");
    return;
  }
  if (ctrl && k == SDLK_g) {
    editor_action(a, "Group");
    return;
  }
  if (k == SDLK_DELETE) {
    editor_action(a, "Delete");
    return;
  }
  if (!hover || (buttons & SDL_BUTTON_RMASK) || a->playing)
    return;
  if (k == SDLK_q)
    a->tool = 0;
  if (k == SDLK_w)
    a->tool = 1;
  if (k == SDLK_e)
    a->tool = 3;
  if (k == SDLK_r)
    a->tool = 2;
  if (k == SDLK_f)
    editor_action(a, "Focus");
  if (k == SDLK_ESCAPE) {
    a->dragging = false;
    a->gui_mode = false;
  }
}
