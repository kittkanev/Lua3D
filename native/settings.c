#include "app.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void settings_defaults(Settings *s) {
  memset(s, 0, sizeof(*s));
  s->camera_speed = 7;
  s->orbit_speed = .25f;
  s->fov = 60;
  s->near_clip = .03f;
  s->far_clip = 1000000;
  s->grid_size = 1;
  s->move_snap = .25f;
  s->rotate_snap = 15;
  s->scale_snap = .1f;
  s->master_volume = .8f;
  s->ui_scale = 1;
  s->physics_hz = 120;
  s->grid = s->axes = s->adaptive_grid = s->vsync = s->autosave = true;
  s->autosave_seconds = 120;
  s->theme = 0;
  snprintf(s->project_dir, 512, ".");
}
#define FLOAT_FIELDS(X)                                                        \
  X(camera_speed, .1f, 1000)                                                   \
  X(orbit_speed, .01f, 5)                                                      \
  X(fov, 10, 150)                                                              \
  X(near_clip, .001f, 10) X(far_clip, 100, 10000000) X(grid_size, .01f, 10000) \
      X(move_snap, .001f, 10000) X(rotate_snap, .1f, 180)                      \
          X(scale_snap, .001f, 10) X(master_volume, 0, 1) X(ui_scale, .75f, 2) \
              X(physics_hz, 30, 240)
#define BOOL_FIELDS(X)                                                         \
  X(grid)                                                                      \
  X(axes)                                                                      \
  X(adaptive_grid)                                                             \
  X(vsync) X(snap) X(local_space) X(invert_y) X(autosave) X(trusted)
bool settings_load(Settings *s, const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  char text[16384];
  size_t n = fread(text, 1, sizeof(text) - 1, f);
  bool ok = !ferror(f) && feof(f);
  fclose(f);
  text[n] = 0;
  if (!ok)
    return false;
  cJSON *j = cJSON_Parse(text);
  if (!cJSON_IsObject(j)) {
    cJSON_Delete(j);
    return false;
  }
  Settings value = *s;
#define READ_FLOAT(name, lo, hi)                                               \
  {                                                                            \
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, #name);               \
    if (v) {                                                                   \
      if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) ||                   \
          v->valuedouble < lo || v->valuedouble > hi)                          \
        ok = false;                                                            \
      else                                                                     \
        value.name = (float)v->valuedouble;                                    \
    }                                                                          \
  }
  FLOAT_FIELDS(READ_FLOAT)
#define READ_BOOL(name)                                                        \
  {                                                                            \
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, #name);               \
    if (v) {                                                                   \
      if (!cJSON_IsBool(v))                                                    \
        ok = false;                                                            \
      else                                                                     \
        value.name = cJSON_IsTrue(v);                                          \
    }                                                                          \
  }
  BOOL_FIELDS(READ_BOOL)
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "theme");
  if (v && cJSON_IsNumber(v) && v->valueint >= 0 && v->valueint <= 1)
    value.theme = v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "autosave_seconds");
  if (v && cJSON_IsNumber(v) && v->valueint >= 30 && v->valueint <= 3600)
    value.autosave_seconds = v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "project_dir");
  if (v && cJSON_IsString(v) && strlen(v->valuestring) < 512)
    snprintf(value.project_dir, 512, "%s", v->valuestring);
  cJSON_Delete(j);
  if (ok)
    *s = value;
  return ok;
}
bool settings_save(const Settings *s, const char *path) {
  cJSON *j = cJSON_CreateObject();
#define WRITE_FLOAT(name, lo, hi) cJSON_AddNumberToObject(j, #name, s->name);
  FLOAT_FIELDS(WRITE_FLOAT)
#define WRITE_BOOL(name) cJSON_AddBoolToObject(j, #name, s->name);
  BOOL_FIELDS(WRITE_BOOL)
  cJSON_AddNumberToObject(j, "theme", s->theme);
  cJSON_AddNumberToObject(j, "autosave_seconds", s->autosave_seconds);
  cJSON_AddStringToObject(j, "project_dir", s->project_dir);
  char *text = cJSON_Print(j);
  cJSON_Delete(j);
  if (!text)
    return false;
  FILE *f = fopen(path, "wb");
  bool ok = false;
  if (f) {
    ok = fwrite(text, 1, strlen(text), f) == strlen(text);
    if (fclose(f))
      ok = false;
  }
  free(text);
  return ok;
}
