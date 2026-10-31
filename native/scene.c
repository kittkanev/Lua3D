#include "lunoryx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void rig_free(Rig *r) {
  if (!r)
    return;
  for (size_t i = 0; r->tracks && i < r->track_count; i++) {
    free(r->tracks[i].time);
    free(r->tracks[i].value);
  }
  free(r->tracks);
  free(r);
}
Rig *rig_copy(const Rig *r) {
  if (!r)
    return NULL;
  Rig *n = calloc(1, sizeof(*n));
  if (!n)
    return NULL;
  *n = *r;
  n->tracks = NULL;
  n->track_count = 0;
  if (r->track_count) {
    n->tracks = calloc(r->track_count, sizeof(Track));
    if (!n->tracks) {
      rig_free(n);
      return NULL;
    }
    for (size_t i = 0; i < r->track_count; i++) {
      Track *t = &n->tracks[i];
      *t = r->tracks[i];
      t->time = malloc(t->count * sizeof(float));
      t->value = malloc(t->count * sizeof(Q4));
      n->track_count++;
      if (!t->time || !t->value) {
        rig_free(n);
        return NULL;
      }
      memcpy(t->time, r->tracks[i].time, t->count * sizeof(float));
      memcpy(t->value, r->tracks[i].value, t->count * sizeof(Q4));
    }
  }
  return n;
}
void rig_pose(const Rig *r, float time, bool moving, M4 out[LX_BONES]) {
  V3 p[LX_BONES], s[LX_BONES];
  Q4 q[LX_BONES];
  M4 world[LX_BONES];
  for (int i = 0; i < r->count; i++) {
    p[i] = r->bones[i].position;
    s[i] = r->bones[i].scale;
    q[i] = r->bones[i].rotation;
  }
  if (r->procedural && moving)
    for (int i = 1; i < r->count; i++) {
      float angle = sinf(time * 9 + (i % 2) * LX_PI) * (i < 3 ? 32 : 24);
      q[i] = quat(v3(angle, 0, 0));
    }
  float t = r->duration > 0 ? fmodf(time, r->duration) : 0;
  for (size_t k = 0; k < r->track_count; k++) {
    const Track *tr = &r->tracks[k];
    if (tr->bone < 0 || tr->bone >= r->count || !tr->count)
      continue;
    size_t a = 0;
    while (a + 1 < tr->count && tr->time[a + 1] <= t)
      a++;
    size_t b = a + 1 < tr->count ? a + 1 : a;
    float f = b != a && !tr->step
                  ? (t - tr->time[a]) / (tr->time[b] - tr->time[a])
                  : 0;
    f = fmaxf(0, fminf(f, 1));
    Q4 v =
        tr->path == 1
            ? qslerp(tr->value[a], tr->value[b], f)
            : (Q4){tr->value[a].x + (tr->value[b].x - tr->value[a].x) * f,
                   tr->value[a].y + (tr->value[b].y - tr->value[a].y) * f,
                   tr->value[a].z + (tr->value[b].z - tr->value[a].z) * f, 0};
    if (tr->path == 0)
      p[tr->bone] = v3(v.x, v.y, v.z);
    else if (tr->path == 1)
      q[tr->bone] = v;
    else
      s[tr->bone] = v3(v.x, v.y, v.z);
  }
  for (int i = 0; i < r->count; i++) {
    M4 local = trs(p[i], q[i], s[i]);
    int parent = r->bones[i].parent;
    world[i] = parent >= 0 && parent < i ? mmul(world[parent], local) : local;
    out[i] = mmul(world[i], r->bones[i].inverse_bind);
  }
}
Vertex skin_vertex(Vertex v, const M4 pose[LX_BONES], int count) {
  V3 p = v3(0, 0, 0), n = v3(0, 0, 0);
  float total = 0;
  for (int k = 0; k < 4; k++)
    if (v.weights[k] > 0 && v.joints[k] < count) {
      p = add(p, mul(point(pose[v.joints[k]], v.p), v.weights[k]));
      n = add(n, mul(direction(pose[v.joints[k]], v.n), v.weights[k]));
      total += v.weights[k];
    }
  if (total > 0) {
    v.p = mul(p, 1 / total);
    v.n = unit(n);
  }
  return v;
}
void scene_init(Scene *s) {
  memset(s, 0, sizeof(*s));
  s->next_id = 1;
  s->gravity = -9.81f;
  s->sky_top[0] = .1f;
  s->sky_top[1] = .25f;
  s->sky_top[2] = .5f;
  s->sky_bottom[0] = .65f;
  s->sky_bottom[1] = .8f;
  s->sky_bottom[2] = .95f;
  s->sun[0] = -.5f;
  s->sun[1] = .8f;
  s->sun[2] = .3f;
  snprintf(s->title, sizeof(s->title), "Untitled project");
}
void scene_free(Scene *s) {
  for (size_t i = 0; i < s->count; i++) {
    mesh_free(&s->objects[i].mesh);
    rig_free(s->objects[i].rig);
  }
  free(s->objects);
  free(s->constraints);
  free(s->scripts);
  free(s->gui);
  memset(s, 0, sizeof(*s));
}
bool scene_copy(Scene *d, const Scene *s) {
  Scene n = *s;
  n.objects = NULL;
  n.constraints = NULL;
  n.scripts = NULL;
  n.gui = NULL;
  n.count = 0;
  n.capacity = 0;
  if (s->count) {
    n.objects = calloc(s->count, sizeof(Object));
    if (!n.objects)
      goto fail;
    n.capacity = s->count;
    for (size_t i = 0; i < s->count; i++) {
      Object *o = &n.objects[i];
      *o = s->objects[i];
      memset(&o->mesh, 0, sizeof(Mesh));
      o->rig = NULL;
      n.count++;
      if (!mesh_copy(&o->mesh, &s->objects[i].mesh))
        goto fail;
      if (s->objects[i].rig && !(o->rig = rig_copy(s->objects[i].rig)))
        goto fail;
    }
  }
  if (s->constraint_count) {
    n.constraints = malloc(s->constraint_count * sizeof(Constraint));
    if (!n.constraints)
      goto fail;
    memcpy(n.constraints, s->constraints,
           s->constraint_count * sizeof(Constraint));
  }
  if (s->script_count) {
    n.scripts = malloc(s->script_count * sizeof(Script));
    if (!n.scripts)
      goto fail;
    memcpy(n.scripts, s->scripts, s->script_count * sizeof(Script));
  }
  if (s->gui_count) {
    n.gui = malloc(s->gui_count * sizeof(Gui));
    if (!n.gui)
      goto fail;
    memcpy(n.gui, s->gui, s->gui_count * sizeof(Gui));
  }
  scene_free(d);
  *d = n;
  return true;
fail:
  scene_free(&n);
  return false;
}
Object *scene_find(Scene *s, uint32_t id) {
  for (size_t i = 0; i < s->count; i++)
    if (s->objects[i].id == id)
      return &s->objects[i];
  return NULL;
}
const Object *scene_get(const Scene *s, uint32_t id) {
  for (size_t i = 0; i < s->count; i++)
    if (s->objects[i].id == id)
      return &s->objects[i];
  return NULL;
}
Object *scene_add(Scene *s, Kind kind, const char *name) {
  if (s->count >= LX_MAX_OBJECTS || s->next_id == UINT32_MAX)
    return NULL;
  if (s->count == s->capacity) {
    size_t cap = s->capacity ? s->capacity * 2 : 32;
    Object *p = realloc(s->objects, cap * sizeof(Object));
    if (!p)
      return NULL;
    s->objects = p;
    s->capacity = cap;
  }
  Object o = {0};
  o.id = s->next_id++;
  o.kind = kind;
  snprintf(o.name, sizeof(o.name), "%s", name ? name : "Part");
  o.scale = v3(1, 1, 1);
  o.visible = true;
  o.anchored = true;
  o.mass = 1;
  o.bounce = .1f;
  o.friction = .6f;
  o.speed = 4.5f;
  o.jump = 6;
  o.volume = 1;
  o.pitch = 1;
  o.min_distance = 1;
  o.max_distance = 50;
  o.spatial = true;
  float c[4] = {.58f, .65f, .85f, 1};
  memcpy(o.material.color, c, sizeof(c));
  o.material.roughness = .5f;
  if ((kind <= LX_TERRAIN || kind == LX_PYRAMID) &&
      !mesh_primitive(&o.mesh, kind))
    return NULL;
  s->objects[s->count] = o;
  return &s->objects[s->count++];
}
bool scene_delete(Scene *s, uint32_t id) {
  size_t i;
  for (i = 0; i < s->count; i++)
    if (s->objects[i].id == id)
      break;
  if (i == s->count)
    return false;
  /* Restart after deletion: descendants can precede parents in storage. */
  for (size_t k = 0; k < s->count;) {
    if (s->objects[k].parent == id) {
      uint32_t child = s->objects[k].id;
      if (!scene_delete(s, child))
        return false;
      k = 0;
    } else
      k++;
  }
  for (i = 0; i < s->count; i++)
    if (s->objects[i].id == id)
      break;
  mesh_free(&s->objects[i].mesh);
  rig_free(s->objects[i].rig);
  memmove(&s->objects[i], &s->objects[i + 1],
          (s->count - i - 1) * sizeof(Object));
  s->count--;
  for (size_t k = s->constraint_count; k > 0; k--)
    if (s->constraints[k - 1].a == id || s->constraints[k - 1].b == id) {
      memmove(&s->constraints[k - 1], &s->constraints[k],
              (s->constraint_count - k) * sizeof(Constraint));
      s->constraint_count--;
    }
  return true;
}
bool scene_duplicate(Scene *s, uint32_t id) {
  const Object *old = scene_get(s, id);
  if (!old)
    return false;
  Object copy = *old;
  copy.mesh = (Mesh){0};
  copy.rig = NULL;
  if (!mesh_copy(&copy.mesh, &old->mesh))
    return false;
  if (old->rig && !(copy.rig = rig_copy(old->rig))) {
    mesh_free(&copy.mesh);
    return false;
  }
  Object *o = scene_add(s, LX_GROUP, copy.name);
  if (!o) {
    mesh_free(&copy.mesh);
    rig_free(copy.rig);
    return false;
  }
  copy.id = o->id;
  copy.position.x += 1;
  copy.selected = true;
  *o = copy;
  return true;
}
M4 object_world(const Scene *s, const Object *o) {
  M4 m = mmul(trs(o->position, quat(o->rotation), o->scale),
              trs(mul(o->pivot, -1), (Q4){0, 0, 0, 1}, v3(1, 1, 1)));
  uint32_t id = o->parent;
  for (size_t depth = 0; id && depth < s->count; depth++) {
    const Object *p = scene_get(s, id);
    if (!p)
      break;
    M4 local = mmul(trs(p->position, quat(p->rotation), p->scale),
                    trs(mul(p->pivot, -1), (Q4){0, 0, 0, 1}, v3(1, 1, 1)));
    m = mmul(local, m);
    id = p->parent;
  }
  return m;
}
bool object_visible(const Scene *s, const Object *o) {
  if (!o->visible || o->operand)
    return false;
  uint32_t id = o->parent;
  for (size_t depth = 0; id && depth < s->count; depth++) {
    const Object *p = scene_get(s, id);
    if (!p || !p->visible || p->operand)
      return false;
    id = p->parent;
  }
  return true;
}
static bool bake(Mesh *m, M4 world, Material mat) {
  M4 inv = inverse(world);
  for (size_t i = 0; i < m->nv; i++) {
    Vertex *v = &m->v[i];
    v->p = point(world, v->p);
    v->n = unit(v3(inv.m[0] * v->n.x + inv.m[1] * v->n.y + inv.m[2] * v->n.z,
                   inv.m[4] * v->n.x + inv.m[5] * v->n.y + inv.m[6] * v->n.z,
                   inv.m[8] * v->n.x + inv.m[9] * v->n.y + inv.m[10] * v->n.z));
    for (int k = 0; k < 4; k++)
      v->color[k] *= mat.color[k];
  }
  float det =
      dot(direction(world, v3(1, 0, 0)),
          cross(direction(world, v3(0, 1, 0)), direction(world, v3(0, 0, 1))));
  if (det < 0)
    for (size_t i = 0; i + 2 < m->ni; i += 3) {
      uint32_t t = m->i[i + 1];
      m->i[i + 1] = m->i[i + 2];
      m->i[i + 2] = t;
    }
  return true;
}
bool scene_reparent(Scene *s, uint32_t id, uint32_t parent) {
  Object *o = scene_find(s, id);
  if (!o || id == parent)
    return false;
  uint32_t walk = parent;
  for (size_t k = 0; walk && k <= s->count; k++) {
    if (walk == id)
      return false;
    const Object *p = scene_get(s, walk);
    if (!p)
      return false;
    walk = p->parent;
  }
  M4 world = object_world(s, o);
  const Object *p = scene_get(s, parent);
  M4 local = p ? mmul(inverse(object_world(s, p)), world)
               : world; /* Bake shear rather than silently dropping it. */
  V3 axes[3] = {v3(local.m[0], local.m[1], local.m[2]),
                v3(local.m[4], local.m[5], local.m[6]),
                v3(local.m[8], local.m[9], local.m[10])};
  V3 scale = v3(length3(axes[0]), length3(axes[1]), length3(axes[2]));
  if (scale.x < .00001f || scale.y < .00001f || scale.z < .00001f)
    return false;
  for (int k = 0; k < 3; k++)
    axes[k] = unit(axes[k]);
  bool shear = fabsf(dot(axes[0], axes[1])) > .0001f ||
               fabsf(dot(axes[0], axes[2])) > .0001f ||
               fabsf(dot(axes[1], axes[2])) > .0001f;
  if (shear) {
    if (!o->mesh.nv || o->rig)
      return false;
    Material white = {0};
    for (int k = 0; k < 4; k++)
      white.color[k] = 1;
    bake(&o->mesh, local, white);
    o->position = v3(0, 0, 0);
    o->rotation = v3(0, 0, 0);
    o->scale = v3(1, 1, 1);
    o->pivot = v3(0, 0, 0);
  } else {
    float sy = fmaxf(-1, fminf(1, axes[2].x)), ry = asinf(sy), rx, rz;
    if (fabsf(cosf(ry)) > .0001f) {
      rx = atan2f(-axes[2].y, axes[2].z);
      rz = atan2f(-axes[1].x, axes[0].x);
    } else {
      rx = atan2f(axes[1].z, axes[1].y);
      rz = 0;
    }
    o->rotation = v3(rx * 180 / LX_PI, ry * 180 / LX_PI, rz * 180 / LX_PI);
    o->scale = scale;
    M4 rs = trs(v3(0, 0, 0), quat(o->rotation), scale);
    o->position = add(point(local, v3(0, 0, 0)), direction(rs, o->pivot));
  }
  o->parent = parent;
  return true;
}
uint32_t scene_group(Scene *s) {
  uint32_t ids[LX_MAX_OBJECTS];
  size_t n = 0;
  V3 center = v3(0, 0, 0);
  for (size_t i = 0; i < s->count; i++) {
    const Object *o = &s->objects[i];
    if (!o->selected || o->locked)
      continue;
    bool inherited = false;
    uint32_t parent = o->parent;
    for (size_t depth = 0; parent && depth < s->count; depth++) {
      const Object *p = scene_get(s, parent);
      if (!p)
        break;
      if (p->selected && !p->locked)
        inherited = true;
      parent = p->parent;
    }
    if (!inherited) {
      ids[n++] = o->id;
      center = add(center, point(object_world(s, o), o->pivot));
    }
  }
  if (!n)
    return 0;
  Scene copy = {0};
  if (!scene_copy(&copy, s))
    return 0;
  Object *g = scene_add(&copy, LX_GROUP, "Model");
  if (!g) {
    scene_free(&copy);
    return 0;
  }
  g->position = mul(center, 1.0f / (float)n);
  uint32_t id = g->id;
  for (size_t i = 0; i < n; i++)
    if (!scene_reparent(&copy, ids[i], id)) {
      scene_free(&copy);
      return 0;
    }
  for (size_t i = 0; i < copy.count; i++)
    copy.objects[i].selected = copy.objects[i].id == id;
  scene_free(s);
  *s = copy;
  return id;
}
bool scene_separate(Scene *s, uint32_t id) {
  const Object *o = scene_get(s, id);
  if (!o || (o->kind != LX_GROUP && o->kind != LX_UNION))
    return false;
  uint32_t parent = o->parent;
  Scene copy = {0};
  if (!scene_copy(&copy, s))
    return false;
  for (size_t i = 0; i < copy.count; i++)
    if (copy.objects[i].parent == id) {
      if (!scene_reparent(&copy, copy.objects[i].id, parent)) {
        scene_free(&copy);
        return false;
      }
      copy.objects[i].operand = false;
      copy.objects[i].selected = true;
    }
  if (!scene_delete(&copy, id)) {
    scene_free(&copy);
    return false;
  }
  scene_free(s);
  *s = copy;
  return true;
}
static bool combine(Scene *s, int op) {
  uint32_t ids[LX_MAX_OBJECTS];
  size_t n = 0;
  for (size_t i = 0; i < s->count; i++)
    if (s->objects[i].selected && s->objects[i].mesh.ni &&
        !s->objects[i].locked)
      ids[n++] = s->objects[i].id;
  if (n < 2)
    return false;
  Mesh result = {0};
  bool ok = true;
  for (size_t k = 0; k < n && ok; k++) {
    Object *o = scene_find(s, ids[k]);
    Mesh m = {0};
    ok = mesh_copy(&m, &o->mesh);
    if (ok)
      bake(&m, object_world(s, o), o->material);
    if (!k) {
      result = m;
      continue;
    }
    if (op < 0) {
      uint32_t base = (uint32_t)result.nv;
      for (size_t j = 0; j < m.nv && ok; j++)
        ok = mesh_vertex(&result, m.v[j]);
      for (size_t j = 0; j + 2 < m.ni && ok; j += 3)
        ok = mesh_triangle(&result, base + m.i[j], base + m.i[j + 1],
                           base + m.i[j + 2]);
    } else {
      Mesh tmp = {0};
      int action = o->negative ? 1 : op;
      ok = mesh_csg(&tmp, &result, &m, action);
      if (ok) {
        mesh_free(&result);
        result = tmp;
      }
    }
    mesh_free(&m);
  }
  if (!ok) {
    mesh_free(&result);
    return false;
  }
  Object *u = scene_add(s, LX_UNION,
                        op < 0    ? "Joined mesh"
                        : op == 0 ? "Union"
                        : op == 1 ? "Difference"
                                  : "Intersection");
  if (!u) {
    mesh_free(&result);
    return false;
  }
  u->mesh = result;
  for (int k = 0; k < 4; k++)
    u->material.color[k] = 1;
  uint32_t id = u->id;
  for (size_t k = 0; k < n; k++) {
    Object *o = scene_find(s, ids[k]);
    scene_reparent(s, o->id, id);
    o = scene_find(s, ids[k]);
    o->operand = true;
    o->selected = false;
  }
  scene_find(s, id)->selected = true;
  return true;
}
bool scene_boolean(Scene *s, int operation) { return combine(s, operation); }
bool scene_join(Scene *s) { return combine(s, -1); }
void scene_align(Scene *s, int axis) {
  V3 total = v3(0, 0, 0);
  int count = 0;
  for (size_t i = 0; i < s->count; i++)
    if (s->objects[i].selected && !s->objects[i].locked) {
      total = add(total, point(object_world(s, &s->objects[i]), v3(0, 0, 0)));
      count++;
    }
  if (!count)
    return;
  float target = axis == 0   ? total.x / count
                 : axis == 1 ? total.y / count
                             : total.z / count;
  for (size_t i = 0; i < s->count; i++) {
    Object *o = &s->objects[i];
    if (!o->selected || o->locked)
      continue;
    V3 p = point(object_world(s, o), v3(0, 0, 0));
    if (axis == 0)
      p.x = target;
    else if (axis == 1)
      p.y = target;
    else
      p.z = target;
    const Object *parent = scene_get(s, o->parent);
    if (parent)
      p = point(inverse(object_world(s, parent)), p);
    o->position = add(p, o->pivot);
  }
}
bool scene_character(Scene *s, float height, float width) {
  if (height < .5f || height > 10 || width < .2f || width > 5)
    return false;
  Mesh mesh = {0};
  Rig *rig = calloc(1, sizeof(Rig));
  if (!rig)
    return false;
  rig->count = 6;
  rig->procedural = true;
  V3 centers[6] = {v3(0, height * .52f, 0),
                   v3(-width * .23f, height * .22f, 0),
                   v3(width * .23f, height * .22f, 0),
                   v3(-width * .7f, height * .56f, 0),
                   v3(width * .7f, height * .56f, 0),
                   v3(0, height * .85f, 0)};
  V3 sizes[6] = {v3(width, height * .33f, width * .5f),
                 v3(width * .4f, height * .43f, width * .48f),
                 v3(width * .4f, height * .43f, width * .48f),
                 v3(width * .3f, height * .35f, width * .35f),
                 v3(width * .3f, height * .35f, width * .35f),
                 v3(width * .65f, height * .23f, width * .6f)};
  const char *names[] = {"Torso",    "Left leg",  "Right leg",
                         "Left arm", "Right arm", "Head"};
  bool ok = true;
  for (int k = 0; k < 6 && ok; k++) {
    Bone *b = &rig->bones[k];
    b->parent = k ? 0 : -1;
    b->position = k ? sub(centers[k], centers[0]) : centers[0];
    b->scale = v3(1, 1, 1);
    b->rotation = (Q4){0, 0, 0, 1};
    b->inverse_bind = inverse(trs(centers[k], b->rotation, b->scale));
    snprintf(b->name, sizeof(b->name), "%s", names[k]);
    Mesh part = {0};
    ok = mesh_primitive(&part, LX_CUBE);
    uint32_t base = (uint32_t)mesh.nv;
    for (size_t j = 0; j < part.nv && ok; j++) {
      Vertex v = part.v[j];
      v.p = add(centers[k],
                v3(v.p.x * sizes[k].x, v.p.y * sizes[k].y, v.p.z * sizes[k].z));
      v.joints[0] = (uint16_t)k;
      v.weights[0] = 1;
      v.color[0] = k == 5 ? 1 : .45f;
      v.color[1] = k == 5 ? .78f : .65f;
      v.color[2] = k == 5 ? .6f : 1;
      ok = mesh_vertex(&mesh, v);
    }
    for (size_t j = 0; j + 2 < part.ni && ok; j += 3)
      ok = mesh_triangle(&mesh, base + part.i[j], base + part.i[j + 1],
                         base + part.i[j + 2]);
    mesh_free(&part);
  }
  Object *o = ok ? scene_add(s, LX_CHARACTER, "Character") : NULL;
  if (!o) {
    mesh_free(&mesh);
    rig_free(rig);
    return false;
  }
  o->mesh = mesh;
  o->rig = rig;
  o->anchored = false;
  for (int k = 0; k < 4; k++)
    o->material.color[k] = 1;
  o->selected = true;
  return true;
}
bool scene_constraint(Scene *s, int type) {
  uint32_t ids[2] = {0};
  int n = 0;
  for (size_t i = 0; i < s->count && n < 2; i++)
    if (s->objects[i].selected)
      ids[n++] = s->objects[i].id;
  if (n != 2 || s->constraint_count >= 4096)
    return false;
  Constraint *p =
      realloc(s->constraints, (s->constraint_count + 1) * sizeof(Constraint));
  if (!p)
    return false;
  s->constraints = p;
  Constraint c = {0};
  c.a = ids[0];
  c.b = ids[1];
  c.type = type;
  c.axis = v3(0, 1, 0);
  c.offset = sub(point(object_world(s, scene_find(s, c.b)), v3(0, 0, 0)),
                 point(object_world(s, scene_find(s, c.a)), v3(0, 0, 0)));
  c.length = length3(c.offset);
  c.stiffness = .8f;
  s->constraints[s->constraint_count++] = c;
  return true;
}
Script *scene_script(Scene *s, const char *name) {
  if (s->script_count >= LX_MAX_SCRIPTS)
    return NULL;
  Script *p = realloc(s->scripts, (s->script_count + 1) * sizeof(Script));
  if (!p)
    return NULL;
  s->scripts = p;
  Script *v = &s->scripts[s->script_count++];
  memset(v, 0, sizeof(*v));
  snprintf(v->name, sizeof(v->name), "%s", name ? name : "Script.lua");
  v->open = v->enabled = true;
  return v;
}
Gui *scene_gui(Scene *s, int type) {
  if (type < 0 || type > 4 || s->gui_count >= 4096)
    return NULL;
  Gui *p = realloc(s->gui, (s->gui_count + 1) * sizeof(Gui));
  if (!p)
    return NULL;
  s->gui = p;
  Gui *g = &s->gui[s->gui_count++];
  memset(g, 0, sizeof(*g));
  g->id = s->next_id++;
  g->type = type;
  snprintf(g->name, sizeof(g->name), "%s %u",
           type == 0   ? "Frame"
           : type == 1 ? "Label"
           : type == 2 ? "Input"
           : type == 3 ? "Button"
                       : "Image",
           g->id);
  snprintf(g->text, sizeof(g->text), "%s", g->name);
  g->x = g->y = 24;
  g->w = type == 0 ? 300 : 180;
  g->h = type == 0 ? 220 : 40;
  g->min_w = g->min_h = 1;
  g->max_w = g->max_h = 10000;
  g->visible = true;
  g->font_size = 16;
  g->padding = 12;
  g->gap = 8;
  g->rounding = 5;
  g->color[0] = .2f;
  g->color[1] = .25f;
  g->color[2] = .35f;
  g->color[3] = 1;
  for (int k = 0; k < 4; k++)
    g->text_color[k] = 1;
  return g;
}
