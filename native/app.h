#ifndef LX_APP_H
#define LX_APP_H
#include "audio.h"
#include "lunoryx.h"
#include "runtime.h"
#include <SDL.h>
#include <SDL_opengl.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "nuklear.h"
typedef struct {
  float camera_speed, orbit_speed, fov, near_clip, far_clip, grid_size,
      move_snap, rotate_snap, scale_snap, master_volume, ui_scale, physics_hz;
  bool grid, axes, adaptive_grid, vsync, snap, local_space, invert_y, autosave,
      trusted;
  int theme, autosave_seconds;
  char project_dir[512];
} Settings;
typedef struct {
  GLuint fbo, color, depth, program, grid_program;
  int w, h;
  char error[4096];
  void *cache;
} Renderer;
typedef struct {
  Scene scene, snapshot;
  History history;
  Runtime runtime;
  Settings settings;
  Renderer renderer;
  Audio *audio;
  SDL_Window *window;
  SDL_GLContext gl;
  struct nk_context *ctx;
  bool player;
  char executable_path[1024];
  int play_script_next;
  bool running, playing, paused, fullscreen_code, show_explorer,
      show_properties, show_toolbox, show_assets, show_output, show_docs,
      show_settings, gui_mode;
  int tab, tool, active_script, document, selected_gui, selected_bone, axis,
      drag_axis;
  uint32_t selected_id;
  float distance, yaw, pitch, time, accumulator, brush_radius, brush_strength;
  int brush_mode;
  V3 target, camera, drag_origin, drag_hit;
  float drag_start_x, drag_start_y, drag_value;
  bool dragging, drag_started;
  Object *drag_objects;
  size_t drag_count;
  struct nk_rect viewport;
  int width, height;
  char path[512], asset_path[512], filter[128], command[4096], find[128],
      watch_expr[128], status[512], settings_path[1024];
  int output_page;
  int goto_line, history_doc[128], doc_history_count, doc_history_cursor;
  unsigned last_save;
  bool dirty;
} App;
void settings_defaults(Settings *s);
bool settings_load(Settings *s, const char *path);
bool settings_save(const Settings *s, const char *path);
bool renderer_init(Renderer *r);
void renderer_free(Renderer *r);
void renderer_draw(App *a);
GLuint renderer_texture(Renderer *r, const char *path);
void renderer_clear_cache(Renderer *r);
void editor_draw(App *a);
void editor_event(App *a, SDL_Event *event);
void editor_play(App *a);
void editor_stop(App *a);
void editor_select(App *a, uint32_t id, bool toggle);
void editor_document(App *a, int doc);
void editor_action(App *a, const char *action);
void editor_log(App *a, const char *text);
void editor_sound(void *app, uint32_t id, int action);
Ray editor_ray(const App *a, float x, float y);
bool editor_project_point(const App *a, V3 p, float *x, float *y);
void editor_interaction(App *a, float dt);
bool player_build(const Scene *s, const char *self, const char *path,
                  char error[256]);
bool player_load(Scene *s, const char *self, const char *directory,
                 char error[256]);
int mcp_server(void);
#endif
