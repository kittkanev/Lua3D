/* A game build is the same C executable with a bounded, self-contained scene
 * archive appended. The player extracts referenced assets into its own cache.
 */
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define PACK_LIMIT (256u * 1024u * 1024u)
typedef struct {
  char source[512], name[64];
  size_t size;
} PackedAsset;
static void put64(unsigned char *p, uint64_t v) {
  for (int k = 0; k < 8; k++)
    p[k] = (unsigned char)(v >> (k * 8));
}
static uint64_t get64(const unsigned char *p) {
  uint64_t v = 0;
  for (int k = 0; k < 8; k++)
    v |= (uint64_t)p[k] << (k * 8);
  return v;
}
static bool footer(FILE *f, uint64_t *start, uint64_t *json_size,
                   uint64_t *count) {
  if (fseek(f, 0, SEEK_END))
    return false;
  long end = ftell(f);
  if (end < 32 || fseek(f, end - 32, SEEK_SET))
    return false;
  unsigned char tail[32];
  if (fread(tail, 1, 32, f) != 32 || memcmp(tail + 24, "LXGAME03", 8))
    return false;
  *start = get64(tail);
  *json_size = get64(tail + 8);
  *count = get64(tail + 16);
  return *start < (uint64_t)end - 32 && *json_size <= 128 * 1024 * 1024 &&
         *count <= 1024 && *start + *json_size < (uint64_t)end &&
         ((uint64_t)end - *start) <= PACK_LIMIT;
}
static bool asset_field(char field[512], PackedAsset *list, size_t *count,
                        size_t *total) {
  if (!field[0])
    return true;
  for (size_t i = 0; i < *count; i++)
    if (!strcmp(list[i].source, field)) {
      snprintf(field, 512, "@asset:%zu", i);
      return true;
    }
  if (*count >= 1024)
    return false;
  FILE *f = fopen(field, "rb");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  if (size < 0 || (uint64_t)size > 128 * 1024 * 1024 ||
      *total + (size_t)size > PACK_LIMIT)
    return false;
  PackedAsset *a = &list[*count];
  snprintf(a->source, 512, "%s", field);
  const char *ext = strrchr(field, '.');
  if (!ext || strlen(ext) > 12)
    ext = ".bin";
  for (const char *p = ext; *p; p++)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
          (*p >= '0' && *p <= '9') || *p == '.'))
      ext = ".bin";
  snprintf(a->name, 64, "asset%04zu%s", *count, ext);
  a->size = (size_t)size;
  *total += (size_t)size;
  snprintf(field, 512, "@asset:%zu", *count);
  (*count)++;
  return true;
}
static bool collect(Scene *s, PackedAsset *assets, size_t *count,
                    size_t *total) {
  for (int k = 0; k < 6; k++)
    if (!asset_field(s->sky[k], assets, count, total))
      return false;
  for (size_t i = 0; i < s->count; i++) {
    Object *o = &s->objects[i];
    if (!asset_field(o->material.texture, assets, count, total) ||
        !asset_field(o->material.normal, assets, count, total) ||
        !asset_field(o->sound, assets, count, total))
      return false;
  }
  for (size_t i = 0; i < s->gui_count; i++)
    if (!asset_field(s->gui[i].image, assets, count, total))
      return false;
  return true;
}
static bool copy_bytes(FILE *out, FILE *in, size_t bytes) {
  unsigned char buf[65536];
  while (bytes) {
    size_t n = bytes < sizeof(buf) ? bytes : sizeof(buf);
    if (fread(buf, 1, n, in) != n || fwrite(buf, 1, n, out) != n)
      return false;
    bytes -= n;
  }
  return true;
}
bool player_build(const Scene *s, const char *self, const char *path,
                  char error[256]) {
  Scene copy = {0};
  if (!scene_copy(&copy, s))
    return false;
  PackedAsset *assets = calloc(1024, sizeof(*assets));
  size_t count = 0, total = 0;
  bool ok = assets && collect(&copy, assets, &count, &total);
  char *json = ok ? scene_json(&copy) : NULL;
  scene_free(&copy);
  if (!json) {
    free(assets);
    snprintf(error, 256,
             "Cannot package project: asset missing or budget exceeded");
    return false;
  }
  size_t json_size = strlen(json);
  FILE *source = fopen(self, "rb");
  if (!source) {
    free(json);
    free(assets);
    snprintf(error, 256, "Cannot read this executable");
    return false;
  }
#ifdef _WIN32
  char source_path[1024], output_path[1024];
  GetFullPathNameA(self, sizeof(source_path), source_path, NULL);
  GetFullPathNameA(path, sizeof(output_path), output_path, NULL);
  bool same_path = _stricmp(source_path, output_path) == 0;
#else
  struct stat a, b;
  bool same_path = stat(self, &a) == 0 && stat(path, &b) == 0 &&
                   a.st_dev == b.st_dev && a.st_ino == b.st_ino;
#endif
  if (same_path) {
    fclose(source);
    free(json);
    free(assets);
    snprintf(error, 256, "Choose an output other than the running executable");
    return false;
  }
  fseek(source, 0, SEEK_END);
  long source_size = ftell(source);
  uint64_t base, old_json, old_count;
  if (footer(source, &base, &old_json, &old_count))
    source_size = (long)base;
  if (source_size < 0 || json_size + total > PACK_LIMIT) {
    fclose(source);
    free(json);
    free(assets);
    return false;
  }
  rewind(source);
  char tmp[1024];
  if (strlen(path) > 990) {
    fclose(source);
    free(json);
    free(assets);
    return false;
  }
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE *out = fopen(tmp, "wb");
  ok = out && copy_bytes(out, source, (size_t)source_size);
  fclose(source);
  if (ok)
    ok = fwrite(json, 1, json_size, out) == json_size;
  for (size_t i = 0; i < count && ok; i++) {
    PackedAsset *asset = &assets[i];
    unsigned char header[16];
    size_t n = strlen(asset->name);
    put64(header, n);
    put64(header + 8, asset->size);
    FILE *f = fopen(asset->source, "rb");
    ok = f && fwrite(header, 1, 16, out) == 16 &&
         fwrite(asset->name, 1, n, out) == n && copy_bytes(out, f, asset->size);
    if (f)
      fclose(f);
  }
  if (ok) {
    unsigned char tail[32];
    put64(tail, (uint64_t)source_size);
    put64(tail + 8, json_size);
    put64(tail + 16, count);
    memcpy(tail + 24, "LXGAME03", 8);
    ok = fwrite(tail, 1, 32, out) == 32;
  }
  if (out && fclose(out))
    ok = false;
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExA(tmp, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = rename(tmp, path) == 0;
    if (ok)
      chmod(path, 0755);
#endif
  }
  if (!ok)
    remove(tmp);
  free(json);
  free(assets);
  snprintf(error, 256, "%s", ok ? "" : "Cannot write standalone game");
  return ok;
}
static bool resolve_field(char field[512], char (*paths)[512], size_t count) {
  if (strncmp(field, "@asset:", 7))
    return true;
  char *end;
  unsigned long i = strtoul(field + 7, &end, 10);
  if (*end || i >= count)
    return false;
  snprintf(field, 512, "%s", paths[i]);
  return true;
}
bool player_load(Scene *s, const char *self, const char *directory,
                 char error[256]) {
  FILE *f = fopen(self, "rb");
  if (!f)
    return false;
  uint64_t start, json_size, count;
  if (!footer(f, &start, &json_size, &count)) {
    fclose(f);
    return false;
  }
  char *json = malloc((size_t)json_size + 1);
  char (*paths)[512] = calloc(count ? count : 1, 512);
  if (!json || !paths) {
    free(json);
    free(paths);
    fclose(f);
    return false;
  }
  bool ok = fseek(f, (long)start, SEEK_SET) == 0 &&
            fread(json, 1, (size_t)json_size, f) == json_size;
  json[json_size] = 0;
  unsigned hash = 2166136261u;
  for (size_t i = 0; i < json_size; i++)
    hash = (hash ^ (unsigned char)json[i]) * 16777619u;
  Scene loaded;
  scene_init(&loaded);
  if (ok)
    ok = scene_parse(&loaded, json, error);
  size_t total = 0;
  for (size_t i = 0; i < count && ok; i++) {
    unsigned char header[16];
    ok = fread(header, 1, 16, f) == 16;
    if (!ok)
      break;
    uint64_t n = get64(header), size = get64(header + 8);
    if (n > 63 || size > 128 * 1024 * 1024 || total + size > PACK_LIMIT) {
      ok = false;
      break;
    }
    char name[64];
    ok = fread(name, 1, (size_t)n, f) == n;
    name[n] = 0;
    for (size_t j = 0; j < n; j++)
      if (!((name[j] >= 'a' && name[j] <= 'z') ||
            (name[j] >= 'A' && name[j] <= 'Z') ||
            (name[j] >= '0' && name[j] <= '9') || name[j] == '.'))
        ok = false;
    if (strncmp(name, "asset", 5))
      ok = false;
    if (strlen(directory) + strlen(name) + 16 >= 512)
      ok = false;
    if (!ok)
      break;
    size_t prefix = strlen(directory);
    memcpy(paths[i], directory, prefix);
    snprintf(paths[i] + prefix, 512 - prefix, "%08x-%.63s", hash, name);
    FILE *out = fopen(paths[i], "wb");
    ok = out && copy_bytes(out, f, (size_t)size);
    if (out && fclose(out))
      ok = false;
    total += (size_t)size;
  }
  fclose(f);
  if (ok) {
    for (int k = 0; k < 6; k++)
      ok = resolve_field(loaded.sky[k], paths, (size_t)count) && ok;
    for (size_t i = 0; i < loaded.count; i++) {
      Object *o = &loaded.objects[i];
      ok = resolve_field(o->material.texture, paths, (size_t)count) &&
           resolve_field(o->material.normal, paths, (size_t)count) &&
           resolve_field(o->sound, paths, (size_t)count) && ok;
    }
    for (size_t i = 0; i < loaded.gui_count; i++)
      ok = resolve_field(loaded.gui[i].image, paths, (size_t)count) && ok;
  }
  free(json);
  free(paths);
  if (ok) {
    scene_free(s);
    *s = loaded;
  } else
    scene_free(&loaded);
  return ok;
}
