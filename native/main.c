#define NK_IMPLEMENTATION
#include "app.h"
#define NK_SDL_GL2_IMPLEMENTATION
#include "nuklear_sdl_gl2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../engine_version.h"
#include "font_data.h"
#include "stb_image_write.h"
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void capture(App *a, const char *path) {
  size_t size = (size_t)a->width * (size_t)a->height * 4;
  unsigned char *pixels = malloc(size), *flip = malloc(size);
  if (pixels && flip) {
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, a->width, a->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (int y = 0; y < a->height; y++)
      memcpy(flip + (size_t)y * a->width * 4,
             pixels + (size_t)(a->height - y - 1) * a->width * 4,
             (size_t)a->width * 4);
    stbi_write_png(path, a->width, a->height, 4, flip, a->width * 4);
  }
  free(pixels);
  free(flip);
}
static void default_scene(App *a) {
  Object *o = scene_add(&a->scene, LX_CUBE, "Cube");
  if (o) {
    o->position = v3(0, .5f, 0);
    o->selected = true;
    a->selected_id = o->id;
  }
  o = scene_add(&a->scene, LX_SPHERE, "Sphere");
  if (o) {
    o->position = v3(3, .75f, 0);
    o->scale = v3(1.5f, 1.5f, 1.5f);
    o->material.color[0] = .95f;
    o->material.color[1] = .5f;
    o->material.color[2] = .25f;
  }
  o = scene_add(&a->scene, LX_CUBE, "Platform");
  if (o) {
    o->position = v3(0, -.2f, 0);
    o->scale = v3(8, .35f, 8);
    o->material.color[0] = .18f;
    o->material.color[1] = .23f;
    o->material.color[2] = .3f;
  }
  Script *s = scene_script(&a->scene, "Main.lua");
  if (s)
    snprintf(s->code, LX_CODE_SIZE,
             "-- Lunoryx C Studio\n-- Stable IDs survive object deletion and "
             "undo.\nlocal cube = find_object('Cube')\nlocal angle = "
             "0\nfunction on_update(dt)\n    angle = angle + dt * 30\n    if "
             "cube then set_rotation(cube, 0, angle, 0) end\nend\n");
}
int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "--mcp"))
    return mcp_server();
  if (argc > 3 && !strcmp(argv[1], "--build-game")) {
    Scene scene;
    scene_init(&scene);
    char error[256];
    bool ok = scene_load(&scene, argv[2], error) &&
              player_build(&scene, argv[0], argv[3], error);
    if (!ok)
      fprintf(stderr, "%s\n", error);
    scene_free(&scene);
    return ok ? 0 : 1;
  }
  if (argc > 2 && !strcmp(argv[1], "--validate")) {
    Scene s;
    scene_init(&s);
    char error[256];
    bool ok = scene_load(&s, argv[2], error);
    if (ok)
      printf("Valid project: %zu objects, %zu scripts, %zu GUI nodes\n",
             s.count, s.script_count, s.gui_count);
    else
      fprintf(stderr, "%s\n", error);
    scene_free(&s);
    return ok ? 0 : 1;
  }
  App *a = calloc(1, sizeof(App));
  if (!a)
    return 1;
#ifdef _WIN32
  GetModuleFileNameA(NULL, a->executable_path, sizeof(a->executable_path));
#else
  snprintf(a->executable_path, sizeof(a->executable_path), "/proc/self/exe");
#endif
  a->player = argc > 2 && !strcmp(argv[1], "--play");

  scene_init(&a->scene);
  settings_defaults(&a->settings);
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
    fprintf(stderr, "SDL: %s\n", SDL_GetError());
    free(a);
    return 1;
  }
  char *prefs = SDL_GetPrefPath("Lunoryx", "Studio");
  snprintf(a->settings_path, sizeof(a->settings_path), "%ssettings.json",
           prefs ? prefs : "./");
  SDL_free(prefs);
  settings_load(&a->settings, a->settings_path);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  a->width = 1600;
  a->height = 950;
  a->window = SDL_CreateWindow(LUNORYX_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, a->width, a->height,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!a->window) {
    fprintf(stderr, "Window: %s\n", SDL_GetError());
    SDL_Quit();
    free(a);
    return 1;
  }
  SDL_SetWindowMinimumSize(a->window, 1000, 680);
  a->gl = SDL_GL_CreateContext(a->window);
  if (!a->gl) {
    fprintf(stderr, "OpenGL context: %s\n", SDL_GetError());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lunoryx", SDL_GetError(),
                             a->window);
    SDL_DestroyWindow(a->window);
    SDL_Quit();
    free(a);
    return 1;
  }
  SDL_GL_SetSwapInterval(a->settings.vsync ? 1 : 0);
  if (!renderer_init(&a->renderer)) {
    fprintf(stderr, "Renderer: %s\n", a->renderer.error);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenGL initialization",
                             a->renderer.error, a->window);
    SDL_GL_DeleteContext(a->gl);
    SDL_DestroyWindow(a->window);
    SDL_Quit();
    free(a);
    return 1;
  }
  a->ctx = nk_sdl_init(a->window);
  struct nk_font_atlas *atlas;
  nk_sdl_font_stash_begin(&atlas);
  struct nk_font_config font_config = nk_font_config(15 * a->settings.ui_scale);
  font_config.ttf_data_owned_by_atlas = 0;
  atlas->default_font = nk_font_atlas_add_from_memory(
      atlas, (void *)lx_font_data, sizeof(lx_font_data),
      15 * a->settings.ui_scale, &font_config);
  nk_sdl_font_stash_end();
  a->running = true;
  a->show_explorer = a->show_properties = a->show_toolbox = a->show_output =
      true;
  a->document = -1;
  a->selected_gui = -1;
  a->selected_bone = -1;
  a->goto_line = 1;
  a->distance = 13;
  a->pitch = 24;
  a->yaw = 35;
  a->target = v3(0, 1, 0);
  a->brush_radius = .6f;
  a->brush_strength = .1f;
  a->tool = 1;
  snprintf(a->path, 512, "%.495s/project.lunoryx", a->settings.project_dir);
  if (!getenv("LUNORYX_DISABLE_AUDIO"))
    a->audio = audio_init();
  if (!runtime_init(&a->runtime, &a->scene, a->settings.trusted)) {
    a->running = false;
  }
  a->runtime.sound = editor_sound;
  a->runtime.sound_userdata = a;
  default_scene(a);
  if (argc > 1) {
    const char *project = a->player ? argv[2] : argv[1];
    char error[256];
    if (scene_load(&a->scene, project, error))
      snprintf(a->path, 512, "%s", project);
    else
      editor_log(a, error);
  }
  char pack_error[256] = {0};
  char *player_directory = SDL_GetPrefPath("Lunoryx", "Player");
  if (player_directory &&
      player_load(&a->scene, a->executable_path, player_directory, pack_error))
    a->player = true;
  SDL_free(player_directory);
  if (a->player) {
    for (size_t i = 0; i < a->scene.script_count; i++)
      a->scene.scripts[i].break_count = 0;
    editor_play(a);
    a->settings.grid = false;
    a->settings.axes = false;
    SDL_SetWindowTitle(a->window, a->scene.title);
  }
  history_capture(&a->history, &a->scene);
  editor_log(a, "Lunoryx C Studio: W move / E rotate / R scale. Drag axes or "
                "selected objects. F focuses.");
  Uint64 start = SDL_GetTicks64(), last = start;
  double timeout = 0;
  const char *limit = getenv("TEST_TIMEOUT");
  if (limit) {
    char *end;
    double v = strtod(limit, &end);
    if (*limit && !*end && isfinite(v) && v > 0 && v < 3600)
      timeout = v;
  }
  int frame = 0;
  bool captured = false;
  unsigned history_time = 0;
  while (a->running) {
    Uint64 now = SDL_GetTicks64();
    float dt = fminf((float)(now - last) / 1000, .1f);
    last = now;
    SDL_GetWindowSize(a->window, &a->width, &a->height);
    nk_input_begin(a->ctx);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        a->running = false;
      editor_event(a, &event);
      nk_sdl_handle_event(&event);
    }
    nk_input_end(a->ctx);
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    int mx, my;
    Uint32 mouse = SDL_GetMouseState(&mx, &my);
    if (a->document < 0 && !a->gui_mode && (mouse & SDL_BUTTON_RMASK)) {
      V3 f = unit(sub(a->target, a->camera)),
         right = unit(cross(f, v3(0, 1, 0)));
      float speed = a->settings.camera_speed * dt *
                    ((SDL_GetModState() & KMOD_SHIFT) ? 4 : 1);
      V3 move = v3(0, 0, 0);
      if (keys[SDL_SCANCODE_W])
        move = add(move, f);
      if (keys[SDL_SCANCODE_S])
        move = sub(move, f);
      if (keys[SDL_SCANCODE_D])
        move = add(move, right);
      if (keys[SDL_SCANCODE_A])
        move = sub(move, right);
      if (keys[SDL_SCANCODE_E])
        move.y += 1;
      if (keys[SDL_SCANCODE_Q])
        move.y -= 1;
      a->target = add(a->target, mul(move, speed));
    }
    float pitch = a->pitch * LX_PI / 180, yaw = a->yaw * LX_PI / 180;
    a->camera = add(a->target, mul(v3(cosf(pitch) * sinf(yaw), sinf(pitch),
                                      cosf(pitch) * cosf(yaw)),
                                   a->distance));
    if (a->playing && !a->paused && a->runtime.state != 2) {
      while (a->play_script_next < (int)a->scene.script_count) {
        int script = a->play_script_next++;
        if (a->scene.scripts[script].enabled) {
          runtime_run(&a->runtime, script);
          if (a->runtime.state == 2)
            break;
        }
      }
      a->accumulator += dt;
      float step = 1 / a->settings.physics_hz;
      int steps = 0;
      while (a->accumulator >= step && steps++ < 24) {
        V3 input = v3((float)(keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]), 0,
                      (float)(keys[SDL_SCANCODE_S] - keys[SDL_SCANCODE_W]));
        if (length3(input) > 1)
          input = unit(input);
        runtime_update(&a->runtime, step);
        scene_step(&a->scene, step, a->time, input,
                   keys[SDL_SCANCODE_SPACE] != 0);
        a->time += step;
        a->accumulator -= step;
        if (a->runtime.state == 2)
          break;
      }
    }
    audio_update(a->audio, &a->scene, a->camera, a->settings.master_volume);
    glClearColor(.92f, .93f, .96f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    editor_draw(a);
    editor_interaction(a, dt);
    if (a->document < 0 || a->player)
      renderer_draw(a);
    if (a->renderer.error[0]) {
      editor_log(a, a->renderer.error);
      a->renderer.error[0] = 0;
    }
    nk_sdl_render(NK_ANTI_ALIASING_ON);
    if (!a->playing && !(mouse & SDL_BUTTON_LMASK) &&
        !a->ctx->text_edit.active && now - history_time > 400) {
      history_capture(&a->history, &a->scene);
      history_time = (unsigned)now;
    }
    if (a->settings.autosave && !a->playing && a->dirty &&
        now - a->last_save > (unsigned)a->settings.autosave_seconds * 1000) {
      char path[1024], error[256];
      snprintf(path, sizeof(path), "%s.autosave.lunoryx", a->path);
      scene_save(&a->scene, path, error);
      a->last_save = (unsigned)now;
    }
    frame++;
    const char *capture_path = getenv("LUNORYX_CAPTURE_PATH");
    if (capture_path && !captured && frame > 10 &&
        ((double)(now - start) / 1000 > 1)) {
      capture(a, capture_path);
      captured = true;
    }
    SDL_GL_SwapWindow(a->window);
    if (timeout && (double)(now - start) / 1000 >= timeout)
      a->running = false;
    SDL_Delay(1);
  }
  const char *state_path = getenv("LUNORYX_STATE_PATH");
  if (state_path) {
    char error[256];
    if (!scene_save(&a->scene, state_path, error))
      fprintf(stderr, "State capture: %s\n", error);
  }
  if (!a->player)
    settings_save(&a->settings, a->settings_path);
  audio_free(a->audio);
  runtime_free(&a->runtime);
  history_free(&a->history);
  scene_free(&a->snapshot);
  scene_free(&a->scene);
  free(a->drag_objects);
  renderer_free(&a->renderer);
  nk_sdl_shutdown();
  SDL_GL_DeleteContext(a->gl);
  SDL_DestroyWindow(a->window);
  SDL_Quit();
  free(a);
  return 0;
}
