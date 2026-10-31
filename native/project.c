#include "cJSON.h"
#include "lunoryx.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define PROJECT_LIMIT (128u * 1024u * 1024u)
typedef enum { F_NUMBER, F_INT, F_ID, F_BOOL, F_STRING, F_VECTOR } FieldType;
typedef struct {
  const char *key;
  size_t offset, count;
  FieldType type;
} Field;
#define NUM(T, f) {#f, offsetof(T, f), 1, F_NUMBER}
#define INT(T, f) {#f, offsetof(T, f), 1, F_INT}
#define ID(T, f) {#f, offsetof(T, f), 1, F_ID}
#define BOOL(T, f) {#f, offsetof(T, f), 1, F_BOOL}
#define STR(T, f) {#f, offsetof(T, f), sizeof(((T *)0)->f), F_STRING}
#define VEC(T, f, n) {#f, offsetof(T, f), n, F_VECTOR}
static const Field object_fields[] = {ID(Object, id),
                                      ID(Object, parent),
                                      STR(Object, name),
                                      INT(Object, kind),
                                      VEC(Object, position, 3),
                                      VEC(Object, rotation, 3),
                                      VEC(Object, scale, 3),
                                      VEC(Object, pivot, 3),
                                      VEC(Object, velocity, 3),
                                      VEC(Object, angular_velocity, 3),
                                      NUM(Object, mass),
                                      NUM(Object, bounce),
                                      NUM(Object, friction),
                                      NUM(Object, speed),
                                      NUM(Object, jump),
                                      BOOL(Object, visible),
                                      BOOL(Object, locked),
                                      BOOL(Object, anchored),
                                      BOOL(Object, operand),
                                      BOOL(Object, negative),
                                      INT(Object, effect),
                                      NUM(Object, effect_rate),
                                      STR(Object, sound),
                                      NUM(Object, volume),
                                      NUM(Object, pitch),
                                      NUM(Object, min_distance),
                                      NUM(Object, max_distance),
                                      BOOL(Object, loop),
                                      BOOL(Object, spatial),
                                      BOOL(Object, autoplay)};
static const Field material_fields[] = {
    VEC(Material, color, 4), NUM(Material, metallic), NUM(Material, roughness),
    NUM(Material, emission), STR(Material, texture),  STR(Material, normal),
    STR(Material, shader)};
static const Field gui_fields[] = {ID(Gui, id),
                                   ID(Gui, parent),
                                   INT(Gui, type),
                                   INT(Gui, layout),
                                   STR(Gui, name),
                                   STR(Gui, text),
                                   STR(Gui, image),
                                   STR(Gui, on_click),
                                   NUM(Gui, x),
                                   NUM(Gui, y),
                                   NUM(Gui, w),
                                   NUM(Gui, h),
                                   NUM(Gui, anchor_x),
                                   NUM(Gui, anchor_y),
                                   NUM(Gui, scale_x),
                                   NUM(Gui, scale_y),
                                   NUM(Gui, min_w),
                                   NUM(Gui, min_h),
                                   NUM(Gui, max_w),
                                   NUM(Gui, max_h),
                                   NUM(Gui, aspect),
                                   NUM(Gui, padding),
                                   NUM(Gui, gap),
                                   VEC(Gui, color, 4),
                                   VEC(Gui, text_color, 4),
                                   NUM(Gui, rounding),
                                   BOOL(Gui, visible),
                                   INT(Gui, font_size)};
static const Field constraint_fields[] = {
    ID(Constraint, a),         ID(Constraint, b),
    INT(Constraint, type),     VEC(Constraint, offset, 3),
    VEC(Constraint, axis, 3),  NUM(Constraint, length),
    NUM(Constraint, stiffness)};
static const Field scene_fields[] = {
    ID(Scene, next_id),        STR(Scene, title),  VEC(Scene, sky_top, 3),
    VEC(Scene, sky_bottom, 3), VEC(Scene, sun, 3), NUM(Scene, gravity),
    INT(Scene, sky_mode)};
static const Field bone_fields[] = {
    STR(Bone, name),     INT(Bone, parent),      VEC(Bone, position, 3),
    VEC(Bone, scale, 3), VEC(Bone, rotation, 4), VEC(Bone, inverse_bind, 16)};
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static cJSON *pack_fields(const void *data, const Field *fields, size_t n) {
  cJSON *o = cJSON_CreateObject();
  for (size_t i = 0; i < n; i++) {
    const Field *f = &fields[i];
    const unsigned char *p = (const unsigned char *)data + f->offset;
    switch (f->type) {
    case F_NUMBER:
      cJSON_AddNumberToObject(o, f->key, *(const float *)p);
      break;
    case F_INT:
      cJSON_AddNumberToObject(o, f->key, *(const int *)p);
      break;
    case F_ID:
      cJSON_AddNumberToObject(o, f->key, *(const uint32_t *)p);
      break;
    case F_BOOL:
      cJSON_AddBoolToObject(o, f->key, *(const bool *)p);
      break;
    case F_STRING:
      cJSON_AddStringToObject(o, f->key, (const char *)p);
      break;
    case F_VECTOR:
      cJSON_AddItemToObject(
          o, f->key, cJSON_CreateFloatArray((const float *)p, (int)f->count));
      break;
    }
  }
  return o;
}
static bool unpack_fields(void *data, const cJSON *o, const Field *fields,
                          size_t n) {
  if (!cJSON_IsObject(o))
    return false;
  for (size_t i = 0; i < n; i++) {
    const Field *f = &fields[i];
    unsigned char *p = (unsigned char *)data + f->offset;
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, f->key);
    if (!v)
      continue;
    switch (f->type) {
    case F_NUMBER:
      if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) ||
          fabs(v->valuedouble) > FLT_MAX)
        return false;
      *(float *)p = (float)v->valuedouble;
      break;
    case F_INT:
      if (!cJSON_IsNumber(v) || v->valuedouble < -2147483648.0 ||
          v->valuedouble > 2147483647 ||
          floor(v->valuedouble) != v->valuedouble)
        return false;
      *(int *)p = (int)v->valuedouble;
      break;
    case F_ID:
      if (!cJSON_IsNumber(v) || v->valuedouble < 0 ||
          v->valuedouble > UINT32_MAX ||
          floor(v->valuedouble) != v->valuedouble)
        return false;
      *(uint32_t *)p = (uint32_t)v->valuedouble;
      break;
    case F_BOOL:
      if (!cJSON_IsBool(v))
        return false;
      *(bool *)p = cJSON_IsTrue(v);
      break;
    case F_STRING:
      if (!cJSON_IsString(v) || strlen(v->valuestring) >= f->count)
        return false;
      memcpy(p, v->valuestring, strlen(v->valuestring) + 1);
      break;
    case F_VECTOR:
      if (!cJSON_IsArray(v) || cJSON_GetArraySize(v) != (int)f->count)
        return false;
      for (size_t j = 0; j < f->count; j++) {
        const cJSON *a = cJSON_GetArrayItem(v, (int)j);
        if (!cJSON_IsNumber(a) || !isfinite(a->valuedouble) ||
            fabs(a->valuedouble) > FLT_MAX)
          return false;
        ((float *)p)[j] = (float)a->valuedouble;
      }
      break;
    }
  }
  return true;
}
static const char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static char *encode(const unsigned char *p, size_t n) {
  size_t len = (n + 2) / 3 * 4;
  char *s = malloc(len + 1);
  if (!s)
    return NULL;
  for (size_t i = 0, j = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)p[i] << 16;
    if (i + 1 < n)
      v |= (uint32_t)p[i + 1] << 8;
    if (i + 2 < n)
      v |= p[i + 2];
    s[j++] = alphabet[(v >> 18) & 63];
    s[j++] = alphabet[(v >> 12) & 63];
    s[j++] = i + 1 < n ? alphabet[(v >> 6) & 63] : '=';
    s[j++] = i + 2 < n ? alphabet[v & 63] : '=';
  }
  s[len] = 0;
  return s;
}
static unsigned char *decode(const cJSON *v, size_t limit, size_t *len) {
  *len = 0;
  if (!cJSON_IsString(v))
    return NULL;
  const char *s = v->valuestring;
  size_t n = strlen(s);
  if (n % 4 || n / 4 * 3 > limit + 2)
    return NULL;
  size_t bytes = n / 4 * 3;
  if (n && s[n - 1] == '=')
    bytes--;
  if (n > 1 && s[n - 2] == '=')
    bytes--;
  unsigned char *p = malloc(bytes ? bytes : 1);
  if (!p)
    return NULL;
  size_t j = 0;
  for (size_t i = 0; i < n; i += 4) {
    uint32_t b = 0;
    for (int k = 0; k < 4; k++) {
      const char *a = strchr(alphabet, s[i + k]);
      if (s[i + k] == '=') {
        if (i + 4 != n || k < 2) {
          free(p);
          return NULL;
        }
      } else if (!a || s[i + k] == 0) {
        free(p);
        return NULL;
      }
      b = (b << 6) | (a ? (uint32_t)(a - alphabet) : 0);
    }
    if (j < bytes)
      p[j++] = (unsigned char)(b >> 16);
    if (j < bytes)
      p[j++] = (unsigned char)(b >> 8);
    if (j < bytes)
      p[j++] = (unsigned char)b;
  }
  *len = bytes;
  return p;
}
static void put32(unsigned char *p, uint32_t n) {
  p[0] = (unsigned char)n;
  p[1] = (unsigned char)(n >> 8);
  p[2] = (unsigned char)(n >> 16);
  p[3] = (unsigned char)(n >> 24);
}
static uint32_t get32(const unsigned char *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static void putfloat(unsigned char *p, float f) {
  uint32_t v;
  memcpy(&v, &f, 4);
  put32(p, v);
}
static float getfloat(const unsigned char *p) {
  uint32_t v = get32(p);
  float f;
  memcpy(&f, &v, 4);
  return f;
}
static bool mesh_pack(cJSON *o, const Mesh *m) {
  size_t bytes = m->nv * 80;
  unsigned char *data = malloc(bytes ? bytes : 1);
  if (!data)
    return false;
  for (size_t i = 0; i < m->nv; i++) {
    const Vertex *v = &m->v[i];
    float a[20] = {v->p.x,        v->p.y,        v->p.z,        v->n.x,
                   v->n.y,        v->n.z,        v->uv[0],      v->uv[1],
                   v->color[0],   v->color[1],   v->color[2],   v->color[3],
                   v->joints[0],  v->joints[1],  v->joints[2],  v->joints[3],
                   v->weights[0], v->weights[1], v->weights[2], v->weights[3]};
    for (int k = 0; k < 20; k++)
      putfloat(data + i * 80 + (size_t)k * 4, a[k]);
  }
  char *v = encode(data, bytes);
  free(data);
  data = malloc(m->ni * 4 + 1);
  if (!v || !data) {
    free(v);
    free(data);
    return false;
  }
  for (size_t i = 0; i < m->ni; i++)
    put32(data + i * 4, m->i[i]);
  char *indices = encode(data, m->ni * 4);
  free(data);
  if (!indices) {
    free(v);
    return false;
  }
  cJSON_AddStringToObject(o, "vertices_f32le", v);
  cJSON_AddStringToObject(o, "indices_u32le", indices);
  free(v);
  free(indices);
  return true;
}
static bool mesh_unpack(Mesh *m, const cJSON *o) {
  size_t nv, ni;
  unsigned char *vp = decode(
                    cJSON_GetObjectItemCaseSensitive(o, "vertices_f32le"),
                    LX_MAX_VERTICES * 80, &nv),
                *ip =
                    decode(cJSON_GetObjectItemCaseSensitive(o, "indices_u32le"),
                           LX_MAX_VERTICES * 48, &ni);
  if (!vp || !ip || nv % 80 || ni % 12) {
    free(vp);
    free(ip);
    return false;
  }
  Mesh out = {0};
  bool ok = true;
  for (size_t i = 0; i < nv / 80 && ok; i++) {
    float a[20];
    for (int k = 0; k < 20; k++) {
      a[k] = getfloat(vp + i * 80 + (size_t)k * 4);
      if (!isfinite(a[k]))
        ok = false;
    }
    Vertex v = {0};
    v.p = v3(a[0], a[1], a[2]);
    v.n = v3(a[3], a[4], a[5]);
    for (int k = 0; k < 2; k++)
      v.uv[k] = a[6 + k];
    for (int k = 0; k < 4; k++) {
      v.color[k] = a[8 + k];
      if (a[12 + k] < 0 || a[12 + k] >= LX_BONES ||
          floorf(a[12 + k]) != a[12 + k])
        ok = false;
      else
        v.joints[k] = (uint16_t)a[12 + k];
      v.weights[k] = a[16 + k];
      if (v.weights[k] < 0 || v.weights[k] > 1)
        ok = false;
    }
    if (ok)
      ok = mesh_vertex(&out, v);
  }
  for (size_t i = 0; i < ni / 4 && ok; i += 3)
    ok = mesh_triangle(&out, get32(ip + i * 4), get32(ip + (i + 1) * 4),
                       get32(ip + (i + 2) * 4));
  free(vp);
  free(ip);
  if (!ok) {
    mesh_free(&out);
    return false;
  }
  mesh_free(m);
  *m = out;
  return true;
}
static cJSON *rig_pack(const Rig *r) {
  cJSON *o = cJSON_CreateObject(), *bones = cJSON_AddArrayToObject(o, "bones"),
        *tracks = cJSON_AddArrayToObject(o, "tracks");
  cJSON_AddBoolToObject(o, "procedural", r->procedural);
  cJSON_AddNumberToObject(o, "duration", r->duration);
  for (int i = 0; i < r->count; i++)
    cJSON_AddItemToArray(
        bones, pack_fields(&r->bones[i], bone_fields, COUNT(bone_fields)));
  for (size_t i = 0; i < r->track_count; i++) {
    const Track *t = &r->tracks[i];
    cJSON *a = cJSON_CreateObject();
    cJSON_AddNumberToObject(a, "bone", t->bone);
    cJSON_AddNumberToObject(a, "path", t->path);
    cJSON_AddNumberToObject(a, "step", t->step);
    cJSON_AddItemToObject(a, "time",
                          cJSON_CreateFloatArray(t->time, (int)t->count));
    cJSON *vals = cJSON_AddArrayToObject(a, "value");
    for (size_t k = 0; k < t->count; k++)
      cJSON_AddItemToArray(
          vals, cJSON_CreateFloatArray((const float *)&t->value[k], 4));
    cJSON_AddItemToArray(tracks, a);
  }
  return o;
}
static Rig *rig_unpack(const cJSON *o) {
  const cJSON *bones = cJSON_GetObjectItemCaseSensitive(o, "bones"),
              *tracks = cJSON_GetObjectItemCaseSensitive(o, "tracks");
  if (!cJSON_IsArray(bones) || !cJSON_IsArray(tracks) ||
      cJSON_GetArraySize(bones) > LX_BONES || cJSON_GetArraySize(tracks) > 1024)
    return NULL;
  Rig *r = calloc(1, sizeof(Rig));
  if (!r)
    return NULL;
  r->count = cJSON_GetArraySize(bones);
  r->procedural =
      cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, "procedural"));
  const cJSON *dur = cJSON_GetObjectItemCaseSensitive(o, "duration");
  if (!cJSON_IsNumber(dur) || !isfinite(dur->valuedouble) ||
      dur->valuedouble < 0)
    goto fail;
  r->duration = (float)dur->valuedouble;
  for (int i = 0; i < r->count; i++)
    if (!unpack_fields(&r->bones[i], cJSON_GetArrayItem(bones, i), bone_fields,
                       COUNT(bone_fields)))
      goto fail;
  r->track_count = (size_t)cJSON_GetArraySize(tracks);
  r->tracks = calloc(r->track_count ? r->track_count : 1, sizeof(Track));
  if (!r->tracks)
    goto fail;
  for (size_t i = 0; i < r->track_count; i++) {
    const cJSON *j = cJSON_GetArrayItem(tracks, (int)i),
                *tm = cJSON_GetObjectItemCaseSensitive(j, "time"),
                *val = cJSON_GetObjectItemCaseSensitive(j, "value"),
                *b = cJSON_GetObjectItemCaseSensitive(j, "bone"),
                *p = cJSON_GetObjectItemCaseSensitive(j, "path");
    Track *t = &r->tracks[i];
    if (!cJSON_IsNumber(b) || !cJSON_IsNumber(p) || b->valueint < 0 ||
        b->valueint >= r->count || p->valueint < 0 || p->valueint > 2 ||
        !cJSON_IsArray(tm) || !cJSON_IsArray(val) ||
        cJSON_GetArraySize(tm) != cJSON_GetArraySize(val) ||
        cJSON_GetArraySize(tm) > 100000)
      goto fail;
    t->bone = b->valueint;
    t->path = p->valueint;
    t->step =
        cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(j, "step")) == 1;
    t->count = (size_t)cJSON_GetArraySize(tm);
    t->time = calloc(t->count ? t->count : 1, sizeof(float));
    t->value = calloc(t->count ? t->count : 1, sizeof(Q4));
    if (!t->time || !t->value)
      goto fail;
    for (size_t k = 0; k < t->count; k++) {
      const cJSON *a = cJSON_GetArrayItem(tm, (int)k),
                  *v = cJSON_GetArrayItem(val, (int)k);
      if (!cJSON_IsNumber(a) || !isfinite(a->valuedouble) ||
          a->valuedouble < 0 || !cJSON_IsArray(v) || cJSON_GetArraySize(v) != 4)
        goto fail;
      t->time[k] = (float)a->valuedouble;
      if (k && t->time[k] <= t->time[k - 1])
        goto fail;
      for (int n = 0; n < 4; n++) {
        const cJSON *f = cJSON_GetArrayItem(v, n);
        if (!cJSON_IsNumber(f) || !isfinite(f->valuedouble))
          goto fail;
        ((float *)&t->value[k])[n] = (float)f->valuedouble;
      }
    }
  }
  return r;
fail:
  rig_free(r);
  return NULL;
}
char *scene_json(const Scene *s) {
  char error[256];
  if (!scene_validate(s, error))
    return NULL;
  cJSON *root = pack_fields(s, scene_fields, COUNT(scene_fields));
  cJSON_AddNumberToObject(root, "schema", 2);
  cJSON *objects = cJSON_AddArrayToObject(root, "objects"),
        *scripts = cJSON_AddArrayToObject(root, "scripts"),
        *gui = cJSON_AddArrayToObject(root, "gui"),
        *constraints = cJSON_AddArrayToObject(root, "constraints"),
        *sky = cJSON_AddArrayToObject(root, "sky");
  for (int k = 0; k < 6; k++)
    cJSON_AddItemToArray(sky, cJSON_CreateString(s->sky[k]));
  for (size_t i = 0; i < s->count; i++) {
    const Object *o = &s->objects[i];
    cJSON *j = pack_fields(o, object_fields, COUNT(object_fields));
    cJSON_AddItemToObject(
        j, "material",
        pack_fields(&o->material, material_fields, COUNT(material_fields)));
    if (!mesh_pack(j, &o->mesh)) {
      cJSON_Delete(j);
      cJSON_Delete(root);
      return NULL;
    }
    if (o->rig)
      cJSON_AddItemToObject(j, "rig", rig_pack(o->rig));
    cJSON *props = cJSON_AddArrayToObject(j, "properties");
    for (int k = 0; k < o->property_count; k++) {
      const Property *p = &o->properties[k];
      cJSON *v = cJSON_CreateObject();
      cJSON_AddStringToObject(v, "name", p->name);
      cJSON_AddStringToObject(v, "text", p->text);
      cJSON_AddNumberToObject(v, "type", p->type);
      cJSON_AddNumberToObject(v, "number", p->number);
      cJSON_AddItemToArray(props, v);
    }
    cJSON_AddItemToArray(objects, j);
  }
  for (size_t i = 0; i < s->script_count; i++) {
    const Script *sct = &s->scripts[i];
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "name", sct->name);
    cJSON_AddStringToObject(j, "code", sct->code);
    cJSON_AddBoolToObject(j, "open", sct->open);
    cJSON_AddBoolToObject(j, "enabled", sct->enabled);
    cJSON_AddItemToObject(
        j, "breakpoints",
        cJSON_CreateIntArray(sct->breakpoints, sct->break_count));
    cJSON_AddItemToArray(scripts, j);
  }
  for (size_t i = 0; i < s->gui_count; i++)
    cJSON_AddItemToArray(
        gui, pack_fields(&s->gui[i], gui_fields, COUNT(gui_fields)));
  for (size_t i = 0; i < s->constraint_count; i++)
    cJSON_AddItemToArray(constraints,
                         pack_fields(&s->constraints[i], constraint_fields,
                                     COUNT(constraint_fields)));
  char *text = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return text;
}
static bool read_string(const cJSON *o, const char *key, char *p, size_t cap) {
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
  if (!cJSON_IsString(v) || strlen(v->valuestring) >= cap)
    return false;
  memcpy(p, v->valuestring, strlen(v->valuestring) + 1);
  return true;
}
static bool legacy_mesh(Object *o, const cJSON *j) {
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "vertices"),
              *idx = cJSON_GetObjectItemCaseSensitive(j, "indices"),
              *col = cJSON_GetObjectItemCaseSensitive(j, "vertex_colors");
  if (!cJSON_IsArray(v) || !cJSON_IsArray(idx) || cJSON_GetArraySize(v) % 3 ||
      cJSON_GetArraySize(idx) % 3 ||
      cJSON_GetArraySize(v) > LX_MAX_VERTICES * 3)
    return false;
  if (!cJSON_GetArraySize(v))
    return true;
  mesh_free(&o->mesh);
  size_t n = (size_t)cJSON_GetArraySize(v) / 3;
  for (size_t i = 0; i < n; i++) {
    Vertex a = {0};
    for (int k = 0; k < 3; k++) {
      const cJSON *p = cJSON_GetArrayItem(v, (int)(i * 3) + (int)k);
      if (!cJSON_IsNumber(p) || !isfinite(p->valuedouble))
        return false;
      ((float *)&a.p)[k] = (float)p->valuedouble;
      const cJSON *c = cJSON_GetArrayItem(col, (int)(i * 3) + (int)k);
      a.color[k] = cJSON_IsNumber(c) ? (float)c->valuedouble : 1;
    }
    a.color[3] = 1;
    if (!mesh_vertex(&o->mesh, a))
      return false;
  }
  size_t ni = (size_t)cJSON_GetArraySize(idx);
  for (size_t i = 0; i < ni; i += 3) {
    uint32_t tri[3];
    for (int k = 0; k < 3; k++) {
      const cJSON *vtx = cJSON_GetArrayItem(idx, (int)i + k);
      if (!cJSON_IsNumber(vtx) || vtx->valuedouble < 0 ||
          vtx->valuedouble >= n || floor(vtx->valuedouble) != vtx->valuedouble)
        return false;
      tri[k] = (uint32_t)vtx->valuedouble;
    }
    if (!mesh_triangle(&o->mesh, tri[0], tri[1], tri[2]))
      return false;
  }
  mesh_normals(&o->mesh);
  o->kind = LX_MESH;
  return true;
}
bool scene_parse(Scene *s, const char *text, char error[256]) {
  if (!text || strlen(text) > PROJECT_LIMIT) {
    snprintf(error, 256, "Project is too large");
    return false;
  }
  const char *end = NULL;
  cJSON *root = cJSON_ParseWithOpts(text, &end, 1);
  Scene n;
  scene_init(&n);
  if (!root)
    goto invalid;
  const cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "schema");
  if (!cJSON_IsNumber(schema) ||
      (schema->valuedouble != 1 && schema->valuedouble != 2))
    goto invalid;
  int version = schema->valueint;
  const cJSON *objects = cJSON_GetObjectItemCaseSensitive(root, "objects"),
              *scripts = cJSON_GetObjectItemCaseSensitive(root, "scripts");
  if (!cJSON_IsArray(objects) || cJSON_GetArraySize(objects) > LX_MAX_OBJECTS ||
      !cJSON_IsArray(scripts) || cJSON_GetArraySize(scripts) > LX_MAX_SCRIPTS ||
      !unpack_fields(&n, root, scene_fields, COUNT(scene_fields)))
    goto invalid;
  uint32_t requested_next =
      n.next_id; /* IDs are validated after construction. */
  n.next_id = 1;
  const cJSON *j;
  size_t total_vertices = 0;
  cJSON_ArrayForEach(j, objects) {
    const cJSON *kind =
        cJSON_GetObjectItemCaseSensitive(j, version == 1 ? "type" : "kind");
    if (!cJSON_IsNumber(kind))
      goto invalid;
    int type = kind->valueint;
    if (version == 1) {
      static const Kind map[] = {LX_CUBE, LX_PYRAMID, LX_SPHERE, LX_CYLINDER,
                                 LX_PLANE};
      if (type < 0 || type > 4)
        goto invalid;
      type = map[type];
    }
    if (type < 0 || type > LX_PYRAMID)
      goto invalid;
    Object *o = scene_add(&n, version == 1 ? (Kind)type : LX_GROUP, "Part");
    if (!o || !unpack_fields(o, j, object_fields, COUNT(object_fields)))
      goto invalid;
    o->kind = (Kind)type;
    if (version == 1) {
      o->anchored =
          !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j, "dynamic"));
      if (!legacy_mesh(o, j))
        goto invalid;
      const cJSON *properties =
          cJSON_GetObjectItemCaseSensitive(j, "properties");
      if (properties && !cJSON_IsObject(properties))
        goto invalid;
      const cJSON *prop;
      cJSON_ArrayForEach(prop, properties) {
        if (o->property_count >= 32 || !prop->string ||
            strlen(prop->string) >= 64)
          goto invalid;
        Property *p = &o->properties[o->property_count++];
        snprintf(p->name, 64, "%s", prop->string);
        const cJSON *t = cJSON_GetObjectItemCaseSensitive(prop, "kind"),
                    *v = cJSON_GetObjectItemCaseSensitive(prop, "number");
        if (!cJSON_IsNumber(t) || !cJSON_IsNumber(v) ||
            !isfinite(v->valuedouble) || t->valueint < 0 || t->valueint > 2)
          goto invalid;
        p->type = t->valueint;
        p->number = p->type == 1
                        ? cJSON_IsTrue(
                              cJSON_GetObjectItemCaseSensitive(prop, "boolean"))
                        : v->valuedouble;
        if (!read_string(prop, "text", p->text, sizeof(p->text)))
          goto invalid;
      }
      if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j, "character")))
        o->kind = LX_CHARACTER;

      const cJSON *color = cJSON_GetObjectItemCaseSensitive(j, "color");
      for (int k = 0; k < 3; k++) {
        const cJSON *c = cJSON_GetArrayItem(color, k);
        if (!cJSON_IsNumber(c))
          goto invalid;
        o->material.color[k] = (float)c->valuedouble;
      }
      const cJSON *m = cJSON_GetObjectItemCaseSensitive(j, "metallic"),
                  *r = cJSON_GetObjectItemCaseSensitive(j, "roughness");
      if (cJSON_IsNumber(m))
        o->material.metallic = (float)m->valuedouble;
      if (cJSON_IsNumber(r))
        o->material.roughness = (float)r->valuedouble;
    } else {
      if (!unpack_fields(&o->material,
                         cJSON_GetObjectItemCaseSensitive(j, "material"),
                         material_fields, COUNT(material_fields)) ||
          !mesh_unpack(&o->mesh, j))
        goto invalid;
      const cJSON *rig = cJSON_GetObjectItemCaseSensitive(j, "rig");
      if (rig && !(o->rig = rig_unpack(rig)))
        goto invalid;
      const cJSON *props = cJSON_GetObjectItemCaseSensitive(j, "properties");
      if (!cJSON_IsArray(props) || cJSON_GetArraySize(props) > 32)
        goto invalid;
      const cJSON *p;
      cJSON_ArrayForEach(p, props) {
        Property *prop = &o->properties[o->property_count++];
        if (!read_string(p, "name", prop->name, sizeof(prop->name)) ||
            !read_string(p, "text", prop->text, sizeof(prop->text)))
          goto invalid;
        const cJSON *t = cJSON_GetObjectItemCaseSensitive(p, "type"),
                    *v = cJSON_GetObjectItemCaseSensitive(p, "number");
        if (!cJSON_IsNumber(t) || t->valueint < 0 || t->valueint > 2 ||
            !cJSON_IsNumber(v) || !isfinite(v->valuedouble))
          goto invalid;
        prop->type = t->valueint;
        prop->number = v->valuedouble;
      }
    }
    total_vertices += o->mesh.nv;
    if (total_vertices > 1000000)
      goto invalid;
  }
  cJSON_ArrayForEach(j, scripts) {
    Script *a = scene_script(&n, "Script.lua");
    if (!a || !read_string(j, "name", a->name, sizeof(a->name)) ||
        !read_string(j, "code", a->code, sizeof(a->code)))
      goto invalid;
    const cJSON *open = cJSON_GetObjectItemCaseSensitive(j, "open"),
                *enabled = cJSON_GetObjectItemCaseSensitive(j, "enabled");
    if (open)
      a->open = cJSON_IsTrue(open);
    if (enabled)
      a->enabled = cJSON_IsTrue(enabled);
    const cJSON *bp = cJSON_GetObjectItemCaseSensitive(j, "breakpoints");
    if (bp) {
      if (!cJSON_IsArray(bp) || cJSON_GetArraySize(bp) > 64)
        goto invalid;
      a->break_count = cJSON_GetArraySize(bp);
      for (int k = 0; k < a->break_count; k++) {
        const cJSON *v = cJSON_GetArrayItem(bp, k);
        if (!cJSON_IsNumber(v) || v->valueint < 1)
          goto invalid;
        a->breakpoints[k] = v->valueint;
      }
    }
  }
  if (version == 2) {
    const cJSON *gui = cJSON_GetObjectItemCaseSensitive(root, "gui"),
                *cons = cJSON_GetObjectItemCaseSensitive(root, "constraints"),
                *sky = cJSON_GetObjectItemCaseSensitive(root, "sky");
    if (!cJSON_IsArray(gui) || cJSON_GetArraySize(gui) > 4096 ||
        !cJSON_IsArray(cons) || cJSON_GetArraySize(cons) > 4096 ||
        !cJSON_IsArray(sky) || cJSON_GetArraySize(sky) != 6)
      goto invalid;
    cJSON_ArrayForEach(j, gui) {
      Gui *g = scene_gui(&n, 0);
      if (!g || !unpack_fields(g, j, gui_fields, COUNT(gui_fields)))
        goto invalid;
    }
    n.constraint_count = (size_t)cJSON_GetArraySize(cons);
    n.constraints =
        calloc(n.constraint_count ? n.constraint_count : 1, sizeof(Constraint));
    if (!n.constraints)
      goto invalid;
    for (size_t i = 0; i < n.constraint_count; i++)
      if (!unpack_fields(&n.constraints[i], cJSON_GetArrayItem(cons, (int)i),
                         constraint_fields, COUNT(constraint_fields)))
        goto invalid;
    for (int k = 0; k < 6; k++) {
      const cJSON *v = cJSON_GetArrayItem(sky, k);
      if (!cJSON_IsString(v) || strlen(v->valuestring) >= 512)
        goto invalid;
      snprintf(n.sky[k], 512, "%s", v->valuestring);
    }
  } else {
    const cJSON *ground = cJSON_GetObjectItemCaseSensitive(root, "ground");
    if (cJSON_IsTrue(ground)) {
      Object *floor = scene_add(&n, LX_CUBE, "Legacy ground");
      if (!floor)
        goto invalid;
      floor->position = v3(0, -.1f, 0);
      floor->scale = v3(100, .2f, 100);
    }
  }
  n.next_id = requested_next;
  for (size_t i = 0; i < n.count; i++)
    if (n.objects[i].id >= n.next_id) {
      if (n.objects[i].id == UINT32_MAX)
        goto invalid;
      n.next_id = n.objects[i].id + 1;
    }
  for (size_t i = 0; i < n.gui_count; i++)
    if (n.gui[i].id >= n.next_id) {
      if (n.gui[i].id == UINT32_MAX)
        goto invalid;
      n.next_id = n.gui[i].id + 1;
    }
  if (!scene_validate(&n, error)) {
    cJSON_Delete(root);
    scene_free(&n);
    return false;
  }
  cJSON_Delete(root);
  scene_free(s);
  *s = n;
  error[0] = 0;
  return true;
invalid:
  cJSON_Delete(root);
  scene_free(&n);
  snprintf(error, 256,
           "Invalid, unsupported or over-budget project; current scene was "
           "preserved");
  return false;
}
static bool write_atomic(const char *path, const char *data, size_t size,
                         char error[256]) {
  char tmp[1024];
  if (!path || !*path || strlen(path) > 990) {
    snprintf(error, 256, "Invalid path");
    return false;
  }
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE *f = fopen(tmp, "wb");
  if (!f) {
    snprintf(error, 256, "Cannot open output file");
    return false;
  }
  bool ok = fwrite(data, 1, size, f) == size;
  if (fclose(f))
    ok = false;
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExA(tmp, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = rename(tmp, path) == 0;
#endif
  }
  if (!ok) {
    remove(tmp);
    snprintf(error, 256, "Cannot write or replace output");
  } else
    error[0] = 0;
  return ok;
}
bool scene_save(const Scene *s, const char *path, char error[256]) {
  if (!scene_validate(s, error))
    return false;
  char *text = scene_json(s);
  if (!text) {
    snprintf(error, 256, "Cannot serialize project");
    return false;
  }
  size_t size = strlen(text);
  bool ok = size <= PROJECT_LIMIT && write_atomic(path, text, size, error);
  free(text);
  if ((unsigned long)size > PROJECT_LIMIT)
    snprintf(error, 256, "Project is too large");
  return ok;
}
bool scene_load(Scene *s, const char *path, char error[256]) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    snprintf(error, 256, "Cannot open project");
    return false;
  }
  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    return false;
  }
  long size = ftell(f);
  if (size < 0 || (unsigned long)size > PROJECT_LIMIT ||
      fseek(f, 0, SEEK_SET)) {
    fclose(f);
    snprintf(error, 256, "Project exceeds file limit");
    return false;
  }
  char *text = malloc((size_t)size + 1);
  if (!text) {
    fclose(f);
    return false;
  }
  bool ok = fread(text, 1, (size_t)size, f) == (size_t)size;
  fclose(f);
  text[size] = 0;
  if (memchr(text, 0, (size_t)size))
    ok = false;
  if (ok)
    ok = scene_parse(s, text, error);
  else
    snprintf(error, 256, "Project read failed");
  free(text);
  return ok;
}
void history_free(History *h) {
  for (size_t i = 0; i < h->count; i++)
    free(h->states[i]);
  free(h->states);
  memset(h, 0, sizeof(*h));
}
bool history_capture(History *h, const Scene *s) {
  char *text = scene_json(s);
  if (!text)
    return false;
  if (h->count && !strcmp(text, h->states[h->cursor])) {
    free(text);
    return true;
  }
  size_t len = strlen(text);
  if (len > 64 * 1024 * 1024) {
    free(text);
    return false;
  }
  for (size_t i = h->cursor + 1; i < h->count; i++) {
    h->bytes -= strlen(h->states[i]);
    free(h->states[i]);
  }
  if (h->count)
    h->count = h->cursor + 1;
  while (h->count && (h->count >= 64 || h->bytes + len > 64 * 1024 * 1024)) {
    h->bytes -= strlen(h->states[0]);
    free(h->states[0]);
    memmove(h->states, h->states + 1, (h->count - 1) * sizeof(char *));
    h->count--;
    if (h->cursor)
      h->cursor--;
  }
  char **p = realloc(h->states, (h->count + 1) * sizeof(char *));
  if (!p) {
    free(text);
    return false;
  }
  h->states = p;
  h->states[h->count++] = text;
  h->cursor = h->count - 1;
  h->bytes += len;
  return true;
}
bool history_undo(History *h, Scene *s) {
  if (!h->count || !h->cursor)
    return false;
  char error[256];
  if (!scene_parse(s, h->states[h->cursor - 1], error))
    return false;
  h->cursor--;
  return true;
}
bool history_redo(History *h, Scene *s) {
  if (h->cursor + 1 >= h->count)
    return false;
  char error[256];
  if (!scene_parse(s, h->states[h->cursor + 1], error))
    return false;
  h->cursor++;
  return true;
}
