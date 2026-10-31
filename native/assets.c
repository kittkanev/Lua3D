#include "cJSON.h"
#include "lunoryx.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static bool import_obj(Scene *s, const char *path, char error[256]) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  V3 *positions = NULL, *normals = NULL;
  float (*uv)[2] = NULL;
  size_t np = 0, nn = 0, nt = 0;
  Mesh mesh = {0};
  bool ok = true;
  char line[8192];
  while (ok && fgets(line, sizeof(line), f)) {
    if (!strncmp(line, "v ", 2)) {
      V3 p;
      if (sscanf(line + 2, "%f %f %f", &p.x, &p.y, &p.z) != 3 ||
          !isfinite(p.x) || !isfinite(p.y) || !isfinite(p.z) ||
          np >= LX_MAX_VERTICES) {
        ok = false;
        break;
      }
      V3 *q = realloc(positions, (np + 1) * sizeof(V3));
      if (!q) {
        ok = false;
        break;
      }
      positions = q;
      positions[np++] = p;
    } else if (!strncmp(line, "vn ", 3)) {
      V3 p;
      if (sscanf(line + 3, "%f %f %f", &p.x, &p.y, &p.z) != 3 ||
          nn >= LX_MAX_VERTICES) {
        ok = false;
        break;
      }
      V3 *q = realloc(normals, (nn + 1) * sizeof(V3));
      if (!q) {
        ok = false;
        break;
      }
      normals = q;
      normals[nn++] = unit(p);
    } else if (!strncmp(line, "vt ", 3)) {
      float u, v;
      if (sscanf(line + 3, "%f %f", &u, &v) != 2 || nt >= LX_MAX_VERTICES) {
        ok = false;
        break;
      }
      void *q = realloc(uv, (nt + 1) * sizeof(*uv));
      if (!q) {
        ok = false;
        break;
      }
      uv = q;
      uv[nt][0] = u;
      uv[nt++][1] = v;
    } else if (!strncmp(line, "f ", 2)) {
      uint32_t face[256];
      int n = 0;
      char *token = strtok(line + 2, " \t\r\n");
      while (token && n < 256) {
        int a = 0, b = 0, c = 0;
        if (sscanf(token, "%d/%d/%d", &a, &b, &c) != 3) {
          if (sscanf(token, "%d//%d", &a, &c) != 2) {
            c = 0;
            sscanf(token, "%d/%d", &a, &b);
          }
        }
        long pi = a < 0 ? (long)np + a : a - 1,
             ti = b < 0 ? (long)nt + b : b - 1,
             ni = c < 0 ? (long)nn + c : c - 1;
        if (pi < 0 || (size_t)pi >= np) {
          ok = false;
          break;
        }
        Vertex v = {0};
        v.p = positions[pi];
        v.n = ni >= 0 && (size_t)ni < nn ? normals[ni] : v3(0, 1, 0);
        if (ti >= 0 && (size_t)ti < nt) {
          v.uv[0] = uv[ti][0];
          v.uv[1] = uv[ti][1];
        }
        for (int k = 0; k < 4; k++)
          v.color[k] = 1;
        face[n++] = (uint32_t)mesh.nv;
        if (!mesh_vertex(&mesh, v)) {
          ok = false;
          break;
        }
        token = strtok(NULL, " \t\r\n");
      }
      if (token || n < 3)
        ok = false;
      for (int i = 1; i + 1 < n && ok; i++)
        ok = mesh_triangle(&mesh, face[0], face[i], face[i + 1]);
    }
  }
  if (ferror(f))
    ok = false;
  fclose(f);
  free(positions);
  free(normals);
  free(uv);
  if (!nn)
    mesh_normals(&mesh);
  if (ok && mesh.ni) {
    Object *o = scene_add(s, LX_MESH, "Imported OBJ");
    if (o) {
      o->mesh = mesh;
      return true;
    }
  }
  mesh_free(&mesh);
  snprintf(error, 256, "Invalid or over-budget OBJ");
  return false;
}
static const cgltf_accessor *attribute(const cgltf_primitive *p,
                                       cgltf_attribute_type type, int index) {
  for (cgltf_size i = 0; i < p->attributes_count; i++)
    if (p->attributes[i].type == type && p->attributes[i].index == index)
      return p->attributes[i].data;
  return NULL;
}
static void texture_path(char out[512], const char *path,
                         const cgltf_texture_view *view) {
  if (!view->texture || !view->texture->image || !view->texture->image->uri)
    return;
  const char *uri = view->texture->image->uri;
  if (strstr(uri, ":") || uri[0] == '/')
    return;
  const char *slash = strrchr(path, '/'), *back = strrchr(path, '\\');
  if (back && (!slash || back > slash))
    slash = back;
  size_t prefix = slash ? (size_t)(slash - path) + 1 : 0;
  if (prefix + strlen(uri) >= 512)
    return;
  memcpy(out, path, prefix);
  snprintf(out + prefix, 512 - prefix, "%s", uri);
}
static Rig *import_rig(const cgltf_data *data, const cgltf_node *node,
                       int mapping[LX_BONES]) {
  const cgltf_skin *skin = node->skin;
  if (!skin || skin->joints_count > LX_BONES)
    return NULL;
  Rig *r = calloc(1, sizeof(Rig));
  if (!r)
    return NULL;
  int old_to_new[LX_BONES];
  for (int i = 0; i < LX_BONES; i++)
    old_to_new[i] =
        -1; /* Topological order even when glTF joint indices are unordered. */
  while (r->count < (int)skin->joints_count) {
    bool progress = false;
    for (cgltf_size j = 0; j < skin->joints_count; j++) {
      if (old_to_new[j] >= 0)
        continue;
      const cgltf_node *jn = skin->joints[j], *parent = jn->parent;
      int parent_old = -1;
      while (parent) {
        for (cgltf_size k = 0; k < skin->joints_count; k++)
          if (skin->joints[k] == parent)
            parent_old = (int)k;
        if (parent_old >= 0)
          break;
        parent = parent->parent;
      }
      if (parent_old >= 0 && old_to_new[parent_old] < 0)
        continue;
      int idx = r->count++;
      mapping[idx] = (int)j;
      old_to_new[j] = idx;
      Bone *b = &r->bones[idx];
      b->parent = parent_old >= 0 ? old_to_new[parent_old] : -1;
      snprintf(b->name, sizeof(b->name), "%s", jn->name ? jn->name : "Joint");
      M4 world, parent_world = identity();
      cgltf_node_transform_world(jn, world.m);
      if (parent)
        cgltf_node_transform_world(parent, parent_world.m);
      M4 local = mmul(inverse(parent_world),
                      world); /* Convert affine bind transform into TRS. */
      b->position = point(local, v3(0, 0, 0));
      b->scale = v3(length3(v3(local.m[0], local.m[1], local.m[2])),
                    length3(v3(local.m[4], local.m[5], local.m[6])),
                    length3(v3(local.m[8], local.m[9], local.m[10])));
      float a = local.m[0] / b->scale.x, d = local.m[5] / b->scale.y,
            f = local.m[10] / b->scale.z;
      float trace = a + d + f;
      Q4 q;
      if (trace > 0) {
        float t = sqrtf(trace + 1) * 2;
        q = (Q4){(local.m[6] / b->scale.y - local.m[9] / b->scale.z) / t,
                 (local.m[8] / b->scale.z - local.m[2] / b->scale.x) / t,
                 (local.m[1] / b->scale.x - local.m[4] / b->scale.y) / t,
                 t * .25f};
      } else if (a > d && a > f) {
        float t = sqrtf(1 + a - d - f) * 2;
        q = (Q4){t * .25f,
                 (local.m[4] / b->scale.y + local.m[1] / b->scale.x) / t,
                 (local.m[8] / b->scale.z + local.m[2] / b->scale.x) / t,
                 (local.m[6] / b->scale.y - local.m[9] / b->scale.z) / t};
      } else if (d > f) {
        float t = sqrtf(1 + d - a - f) * 2;
        q = (Q4){(local.m[4] / b->scale.y + local.m[1] / b->scale.x) / t,
                 t * .25f,
                 (local.m[9] / b->scale.z + local.m[6] / b->scale.y) / t,
                 (local.m[8] / b->scale.z - local.m[2] / b->scale.x) / t};
      } else {
        float t = sqrtf(1 + f - a - d) * 2;
        q = (Q4){(local.m[8] / b->scale.z + local.m[2] / b->scale.x) / t,
                 (local.m[9] / b->scale.z + local.m[6] / b->scale.y) / t,
                 t * .25f,
                 (local.m[1] / b->scale.x - local.m[4] / b->scale.y) / t};
      }
      b->rotation = q;
      M4 bind = identity(), mesh_world;
      cgltf_node_transform_world(node, mesh_world.m);
      if (skin->inverse_bind_matrices)
        cgltf_accessor_read_float(skin->inverse_bind_matrices, j, bind.m, 16);
      b->inverse_bind = mmul(bind, inverse(mesh_world));
      progress = true;
    }
    if (!progress) {
      rig_free(r);
      return NULL;
    }
  }
  if (data->animations_count) {
    const cgltf_animation *anim = &data->animations[0];
    for (cgltf_size i = 0; i < anim->channels_count; i++) {
      const cgltf_animation_channel *ch = &anim->channels[i];
      int bone = -1;
      for (int k = 0; k < r->count; k++)
        if (skin->joints[mapping[k]] == ch->target_node)
          bone = k;
      if (bone < 0 || ch->target_path == cgltf_animation_path_type_weights ||
          ch->sampler->interpolation == cgltf_interpolation_type_cubic_spline)
        continue;
      size_t n = ch->sampler->input->count;
      if (!n || n > 100000 || ch->sampler->output->count != n)
        continue;
      Track *tr = realloc(r->tracks, (r->track_count + 1) * sizeof(Track));
      if (!tr) {
        rig_free(r);
        return NULL;
      }
      r->tracks = tr;
      Track *t = &r->tracks[r->track_count++];
      memset(t, 0, sizeof(*t));
      t->bone = bone;
      t->path = ch->target_path == cgltf_animation_path_type_translation ? 0
                : ch->target_path == cgltf_animation_path_type_rotation  ? 1
                                                                         : 2;
      t->step = ch->sampler->interpolation == cgltf_interpolation_type_step;
      t->count = n;
      t->time = calloc(n, sizeof(float));
      t->value = calloc(n, sizeof(Q4));
      if (!t->time || !t->value) {
        rig_free(r);
        return NULL;
      }
      for (size_t k = 0; k < n; k++) {
        cgltf_accessor_read_float(ch->sampler->input, k, &t->time[k], 1);
        cgltf_accessor_read_float(ch->sampler->output, k, (float *)&t->value[k],
                                  t->path == 1 ? 4 : 3);
        r->duration = fmaxf(r->duration, t->time[k]);
      }
    }
  }
  return r;
}
static bool import_gltf(Scene *s, const char *path, char error[256]) {
  cgltf_options options = {0};
  cgltf_data *data = NULL;
  if (cgltf_parse_file(&options, path, &data) != cgltf_result_success) {
    snprintf(error, 256, "Cannot parse glTF/GLB");
    return false;
  }
  bool ok = false;
  if (data->meshes_count > 4096 || data->nodes_count > 10000)
    goto done;
  size_t budget = 0;
  for (cgltf_size i = 0; i < data->buffers_count; i++) {
    budget += data->buffers[i].size;
    if (budget > 128 * 1024 * 1024)
      goto done;
  }
  if (cgltf_load_buffers(&options, data, path) != cgltf_result_success ||
      cgltf_validate(data) != cgltf_result_success)
    goto done;
  size_t imported = 0;
  for (cgltf_size k = 0; k < data->nodes_count; k++) {
    const cgltf_node *node = &data->nodes[k];
    if (!node->mesh)
      continue;
    M4 world;
    cgltf_node_transform_world(node, world.m);
    M4 inv = inverse(world);
    for (cgltf_size pi = 0; pi < node->mesh->primitives_count; pi++) {
      const cgltf_primitive *p = &node->mesh->primitives[pi];
      if (p->type != cgltf_primitive_type_triangles)
        continue;
      const cgltf_accessor *pos =
                               attribute(p, cgltf_attribute_type_position, 0),
                           *nor = attribute(p, cgltf_attribute_type_normal, 0),
                           *uv = attribute(p, cgltf_attribute_type_texcoord, 0),
                           *color = attribute(p, cgltf_attribute_type_color, 0),
                           *joints =
                               attribute(p, cgltf_attribute_type_joints, 0),
                           *weights =
                               attribute(p, cgltf_attribute_type_weights, 0);
      if (!pos || pos->count > LX_MAX_VERTICES)
        goto done;
      Object *o =
          scene_add(s, LX_MESH, node->name ? node->name : "Imported mesh");
      if (!o)
        goto done;
      int map[LX_BONES], remap[LX_BONES];
      for (int i = 0; i < LX_BONES; i++)
        remap[i] = 0;
      if (node->skin) {
        o->rig = import_rig(data, node, map);
        if (!o->rig)
          goto done;
        for (int i = 0; i < o->rig->count; i++)
          remap[map[i]] = i;
      }
      for (cgltf_size i = 0; i < pos->count; i++) {
        Vertex v = {0};
        float a[4] = {0};
        cgltf_accessor_read_float(pos, i, a, 3);
        v.p = point(world, v3(a[0], a[1], a[2]));
        if (nor) {
          cgltf_accessor_read_float(nor, i, a, 3);
          v.n = unit(v3(inv.m[0] * a[0] + inv.m[1] * a[1] + inv.m[2] * a[2],
                        inv.m[4] * a[0] + inv.m[5] * a[1] + inv.m[6] * a[2],
                        inv.m[8] * a[0] + inv.m[9] * a[1] + inv.m[10] * a[2]));
        }
        if (uv)
          cgltf_accessor_read_float(uv, i, v.uv, 2);
        for (int n = 0; n < 4; n++)
          v.color[n] = 1;
        if (color)
          cgltf_accessor_read_float(color, i, v.color,
                                    color->type == cgltf_type_vec4 ? 4 : 3);
        if (joints && weights && o->rig) {
          cgltf_uint ji[4] = {0};
          cgltf_accessor_read_uint(joints, i, ji, 4);
          cgltf_accessor_read_float(weights, i, v.weights, 4);
          for (int n = 0; n < 4; n++) {
            if (ji[n] >= LX_BONES)
              goto done;
            v.joints[n] = (uint16_t)remap[ji[n]];
          }
        }
        if (!mesh_vertex(&o->mesh, v))
          goto done;
      }
      size_t count = p->indices ? p->indices->count : pos->count;
      if (count % 3 || count > LX_MAX_VERTICES * 12)
        goto done;
      for (size_t i = 0; i < count; i += 3) {
        uint32_t tri[3];
        for (int n = 0; n < 3; n++)
          tri[n] = (uint32_t)(p->indices ? cgltf_accessor_read_index(
                                               p->indices, i + (size_t)n)
                                         : i + (size_t)n);
        if (!mesh_triangle(&o->mesh, tri[0], tri[1], tri[2]))
          goto done;
      }
      if (!nor)
        mesh_normals(&o->mesh);
      if (p->material) {
        const cgltf_material *m = p->material;
        if (m->has_pbr_metallic_roughness) {
          memcpy(o->material.color, m->pbr_metallic_roughness.base_color_factor,
                 sizeof(float) * 4);
          o->material.metallic = m->pbr_metallic_roughness.metallic_factor;
          o->material.roughness = m->pbr_metallic_roughness.roughness_factor;
          texture_path(o->material.texture, path,
                       &m->pbr_metallic_roughness.base_color_texture);
        }
        texture_path(o->material.normal, path, &m->normal_texture);
      }
      o->selected = true;
      imported++;
    }
  }
  ok = imported > 0;
done:
  cgltf_free(data);
  if (!ok)
    snprintf(error, 256, "Unsupported or over-budget glTF asset");
  return ok;
}
bool scene_import(Scene *s, const char *path, char error[256]) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    snprintf(error, 256, "Cannot open asset");
    return false;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  if (size < 0 || size > 128 * 1024 * 1024) {
    snprintf(error, 256, "Asset exceeds 128 MiB");
    return false;
  }
  Scene n = {0};
  if (!scene_copy(&n, s)) {
    snprintf(error, 256, "Out of memory");
    return false;
  }
  const char *ext = strrchr(path, '.');
  bool ok = ext && (!strcmp(ext, ".obj") || !strcmp(ext, ".OBJ"))
                ? import_obj(&n, path, error)
                : import_gltf(&n, path, error);
  if (ok)
    ok = scene_validate(&n, error);
  if (ok) {
    scene_free(s);
    *s = n;
    error[0] = 0;
  } else
    scene_free(&n);
  return ok;
}
/* Export interleaved attributes with explicit offsets; JSON strings are escaped
 * by cJSON. */
typedef struct {
  unsigned char *data;
  size_t n, cap;
} Bytes;
static bool bytes(Bytes *b, const void *p, size_t n) {
  if (b->n + n > 256 * 1024 * 1024)
    return false;
  if (b->n + n > b->cap) {
    size_t cap = b->cap ? b->cap * 2 : 4096;
    while (cap < b->n + n)
      cap *= 2;
    void *q = realloc(b->data, cap);
    if (!q)
      return false;
    b->data = q;
    b->cap = cap;
  }
  memcpy(b->data + b->n, p, n);
  b->n += n;
  return true;
}
static cJSON *arr(cJSON *o, const char *name) {
  return cJSON_AddArrayToObject(o, name);
}
static int view(cJSON *views, size_t off, size_t len, int stride, int target) {
  int i = cJSON_GetArraySize(views);
  cJSON *v = cJSON_CreateObject();
  cJSON_AddNumberToObject(v, "buffer", 0);
  cJSON_AddNumberToObject(v, "byteOffset", (double)off);
  cJSON_AddNumberToObject(v, "byteLength", (double)len);
  if (stride)
    cJSON_AddNumberToObject(v, "byteStride", stride);
  if (target)
    cJSON_AddNumberToObject(v, "target", target);
  cJSON_AddItemToArray(views, v);
  return i;
}
static int accessor(cJSON *accessors, int vi, int offset, int component,
                    size_t count, const char *type) {
  int i = cJSON_GetArraySize(accessors);
  cJSON *a = cJSON_CreateObject();
  cJSON_AddNumberToObject(a, "bufferView", vi);
  cJSON_AddNumberToObject(a, "byteOffset", offset);
  cJSON_AddNumberToObject(a, "componentType", component);
  cJSON_AddNumberToObject(a, "count", (double)count);
  cJSON_AddStringToObject(a, "type", type);
  cJSON_AddItemToArray(accessors, a);
  return i;
}
bool scene_export_glb(const Scene *s, const char *path, char error[256]) {
  if (!scene_validate(s, error))
    return false;
  cJSON *root = cJSON_CreateObject(),
        *asset = cJSON_AddObjectToObject(root, "asset");
  cJSON_AddStringToObject(asset, "version", "2.0");
  cJSON_AddStringToObject(asset, "generator", "Lunoryx C Studio");
  cJSON_AddNumberToObject(root, "scene", 0);
  cJSON *scenes = arr(root, "scenes"), *sc = cJSON_CreateObject(),
        *roots = arr(sc, "nodes"), *nodes = arr(root, "nodes"),
        *meshes = arr(root, "meshes"), *materials = arr(root, "materials"),
        *views = arr(root, "bufferViews"), *acc = arr(root, "accessors");
  cJSON_AddItemToArray(scenes, sc);
  Bytes bin = {0};
  bool ok = true;
  int mi = 0;
  for (size_t i = 0; i < s->count && ok; i++) {
    const Object *o = &s->objects[i];
    if (!object_visible(s, o) || !o->mesh.ni)
      continue;
    Mesh m = {0};
    if (!mesh_copy(&m, &o->mesh)) {
      ok = false;
      break;
    }
    M4 world = object_world(s, o);
    for (size_t j = 0; j < m.nv; j++) {
      m.v[j].p = point(world, m.v[j].p);
      m.v[j].n = unit(direction(world, m.v[j].n));
    }
    size_t vo = bin.n;
    ok = bytes(&bin, m.v, m.nv * sizeof(Vertex));
    int vv = view(views, vo, m.nv * sizeof(Vertex), (int)sizeof(Vertex), 34962);
    size_t io = bin.n;
    ok = ok && bytes(&bin, m.i, m.ni * sizeof(uint32_t));
    int iv = view(views, io, m.ni * 4, 0, 34963);
    int pa = accessor(acc, vv, (int)offsetof(Vertex, p), 5126, m.nv, "VEC3"),
        na = accessor(acc, vv, (int)offsetof(Vertex, n), 5126, m.nv, "VEC3"),
        ta = accessor(acc, vv, (int)offsetof(Vertex, uv), 5126, m.nv, "VEC2"),
        ca =
            accessor(acc, vv, (int)offsetof(Vertex, color), 5126, m.nv, "VEC4"),
        ia = accessor(acc, iv, 0, 5125, m.ni, "SCALAR");
    float lo[3] = {FLT_MAX, FLT_MAX, FLT_MAX},
          hi[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (size_t j = 0; j < m.nv; j++)
      for (int k = 0; k < 3; k++) {
        float x = ((float *)&m.v[j].p)[k];
        lo[k] = fminf(lo[k], x);
        hi[k] = fmaxf(hi[k], x);
      }
    cJSON *pos = cJSON_GetArrayItem(acc, pa);
    cJSON_AddItemToObject(pos, "min", cJSON_CreateFloatArray(lo, 3));
    cJSON_AddItemToObject(pos, "max", cJSON_CreateFloatArray(hi, 3));
    cJSON *mesh = cJSON_CreateObject(), *prims = arr(mesh, "primitives"),
          *prim = cJSON_CreateObject(),
          *attrs = cJSON_AddObjectToObject(prim, "attributes");
    cJSON_AddNumberToObject(attrs, "POSITION", pa);
    cJSON_AddNumberToObject(attrs, "NORMAL", na);
    cJSON_AddNumberToObject(attrs, "TEXCOORD_0", ta);
    cJSON_AddNumberToObject(attrs, "COLOR_0", ca);
    cJSON_AddNumberToObject(prim, "indices", ia);
    cJSON_AddNumberToObject(prim, "material", mi);
    cJSON_AddItemToArray(prims, prim);
    cJSON_AddItemToArray(meshes, mesh);
    cJSON *mat = cJSON_CreateObject(),
          *pbr = cJSON_AddObjectToObject(mat, "pbrMetallicRoughness");
    cJSON_AddStringToObject(mat, "name", o->name);
    cJSON_AddItemToObject(pbr, "baseColorFactor",
                          cJSON_CreateFloatArray(o->material.color, 4));
    cJSON_AddNumberToObject(pbr, "metallicFactor", o->material.metallic);
    cJSON_AddNumberToObject(pbr, "roughnessFactor", o->material.roughness);
    cJSON_AddBoolToObject(mat, "doubleSided", true);
    cJSON_AddItemToArray(materials, mat);
    cJSON *node = cJSON_CreateObject();
    cJSON_AddStringToObject(node, "name", o->name);
    cJSON_AddNumberToObject(node, "mesh", mi++);
    cJSON_AddItemToArray(roots, cJSON_CreateNumber(cJSON_GetArraySize(nodes)));
    cJSON_AddItemToArray(nodes, node);
    mesh_free(&m);
  }
  cJSON *buffers = arr(root, "buffers"), *buffer = cJSON_CreateObject();
  cJSON_AddNumberToObject(buffer, "byteLength", (double)bin.n);
  cJSON_AddItemToArray(buffers, buffer);
  char *json = ok ? cJSON_PrintUnformatted(root) : NULL;
  if (!json)
    ok = false;
  if (ok) {
    size_t jn = strlen(json), pad = (4 - jn % 4) % 4,
           bpad = (4 - bin.n % 4) % 4;
    uint32_t header[5] = {0x46546c67, 2,
                          (uint32_t)(28 + jn + pad + bin.n + bpad),
                          (uint32_t)(jn + pad), 0x4e4f534a};
    char tmp[1024];
    if (strlen(path) > 990)
      ok = false;
    else {
      snprintf(tmp, sizeof(tmp), "%s.tmp", path);
      FILE *f = fopen(tmp, "wb");
      if (!f)
        ok = false;
      else {
        ok = fwrite(header, 4, 5, f) == 5 && fwrite(json, 1, jn, f) == jn;
        for (size_t k = 0; k < pad; k++)
          ok = ok && fputc(' ', f) != EOF;
        uint32_t bh[2] = {(uint32_t)(bin.n + bpad), 0x004e4942};
        ok = ok && fwrite(bh, 4, 2, f) == 2 &&
             fwrite(bin.data, 1, bin.n, f) == bin.n;
        for (size_t k = 0; k < bpad; k++)
          ok = ok && fputc(0, f) != EOF;
        if (fclose(f))
          ok = false;
        if (ok) {
#ifdef _WIN32
          ok = MoveFileExA(tmp, path,
                           MOVEFILE_REPLACE_EXISTING |
                               MOVEFILE_WRITE_THROUGH) != 0;
#else
          ok = rename(tmp, path) == 0;
#endif
        }
        if (!ok)
          remove(tmp);
      }
    }
  }
  free(json);
  free(bin.data);
  cJSON_Delete(root);
  snprintf(error, 256, "%s",
           ok ? "" : "GLB export failed (empty scene or output unavailable)");
  return ok;
}
