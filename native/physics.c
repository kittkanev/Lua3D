#include "lunoryx.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  V3 center, axis[3], half;
} Box;
static Box bounds(const Scene *s, const Object *o) {
  V3 lo = v3(FLT_MAX, FLT_MAX, FLT_MAX), hi = v3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (size_t i = 0; i < o->mesh.nv; i++) {
    V3 p = o->mesh.v[i].p;
    lo.x = fminf(lo.x, p.x);
    lo.y = fminf(lo.y, p.y);
    lo.z = fminf(lo.z, p.z);
    hi.x = fmaxf(hi.x, p.x);
    hi.y = fmaxf(hi.y, p.y);
    hi.z = fmaxf(hi.z, p.z);
  }
  M4 world = object_world(s, o);
  Box b;
  b.center = point(world, mul(add(lo, hi), .5f));
  b.half = mul(sub(hi, lo), .5f);
  for (int k = 0; k < 3; k++) {
    V3 a = v3(world.m[k * 4], world.m[k * 4 + 1], world.m[k * 4 + 2]);
    float n = length3(a);
    b.axis[k] = unit(a);
    if (k == 0)
      b.half.x = fmaxf(.002f, b.half.x * n);
    if (k == 1)
      b.half.y = fmaxf(.002f, b.half.y * n);
    if (k == 2)
      b.half.z = fmaxf(.002f, b.half.z * n);
  }
  return b;
}
static float radius(Box b, V3 n) {
  return fabsf(dot(b.axis[0], n)) * b.half.x +
         fabsf(dot(b.axis[1], n)) * b.half.y +
         fabsf(dot(b.axis[2], n)) * b.half.z;
}
static bool test_axis(Box a, Box b, V3 axis, V3 *normal, float *depth) {
  float len = length3(axis);
  if (len < 1e-6f)
    return true;
  V3 n = mul(axis, 1 / len);
  float dist = dot(sub(a.center, b.center), n),
        d = radius(a, n) + radius(b, n) - fabsf(dist);
  if (d <= 0)
    return false;
  if (d < *depth) {
    *depth = d;
    *normal = mul(n, dist < 0 ? -1 : 1);
  }
  return true;
}
static bool box_box(Box a, Box b, V3 *normal, float *depth) {
  *depth = FLT_MAX;
  for (int i = 0; i < 3; i++)
    if (!test_axis(a, b, a.axis[i], normal, depth) ||
        !test_axis(a, b, b.axis[i], normal, depth))
      return false;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      if (!test_axis(a, b, cross(a.axis[i], b.axis[j]), normal, depth))
        return false;
  return true;
}
static bool tri_axis(Box b, V3 a, V3 c, V3 d, V3 axis, V3 *normal,
                     float *depth) {
  float len = length3(axis);
  if (len < 1e-6f)
    return true;
  V3 n = mul(axis, 1 / len);
  float pa = dot(sub(a, b.center), n), pc = dot(sub(c, b.center), n),
        pd = dot(sub(d, b.center), n);
  float lo = fminf(pa, fminf(pc, pd)), hi = fmaxf(pa, fmaxf(pc, pd)),
        r = radius(b, n);
  if (lo > r || hi < -r)
    return false;
  float neg = r + hi, pos = r - lo, pen = fminf(neg, pos);
  if (pen < *depth) {
    *depth = pen;
    *normal = mul(n, neg < pos ? -1 : 1);
  }
  return true;
}
static bool box_triangle(Box b, V3 a, V3 c, V3 d, V3 *normal, float *depth) {
  *depth = FLT_MAX;
  V3 edges[3] = {sub(c, a), sub(d, c), sub(a, d)};
  if (!tri_axis(b, a, c, d, cross(edges[0], edges[1]), normal, depth))
    return false;
  for (int i = 0; i < 3; i++) {
    if (!tri_axis(b, a, c, d, b.axis[i], normal, depth))
      return false;
    for (int j = 0; j < 3; j++)
      if (!tri_axis(b, a, c, d, cross(b.axis[i], edges[j]), normal, depth))
        return false;
  }
  return true;
}
static void translate_world(Scene *s, Object *o, V3 delta) {
  const Object *p = scene_get(s, o->parent);
  if (p)
    delta = direction(inverse(object_world(s, p)), delta);
  o->position = add(o->position, delta);
}
static void resolve(Scene *s, Object *a, Object *b, V3 n, float d) {
  float ia = a->anchored ? 0 : 1 / fmaxf(a->mass, .001f),
        ib = b->anchored ? 0 : 1 / fmaxf(b->mass, .001f), sum = ia + ib;
  if (sum == 0)
    return;
  float correction = fmaxf(d - .0005f, 0) * .8f;
  translate_world(s, a, mul(n, correction * ia / sum));
  translate_world(s, b, mul(n, -correction * ib / sum));
  V3 relative = sub(a->velocity, b->velocity);
  float speed = dot(relative, n);
  if (speed < 0) {
    float impulse = -(1 + fminf(a->bounce, b->bounce)) * speed / sum;
    a->velocity = add(a->velocity, mul(n, impulse * ia));
    b->velocity = sub(b->velocity, mul(n, impulse * ib));
    V3 tangent = sub(relative, mul(n, speed));
    float tl = length3(tangent);
    if (tl > 1e-6f) {
      float friction =
          fminf(tl / sum, impulse * sqrtf(a->friction * b->friction));
      a->velocity = sub(a->velocity, mul(tangent, friction * ia / tl));
      b->velocity = add(b->velocity, mul(tangent, friction * ib / tl));
    }
  }
  if (n.y > .55f)
    a->grounded = true;
  if (n.y < -.55f)
    b->grounded = true;
}
void scene_step(Scene *s, float dt, float time, V3 input, bool jump) {
  (void)time;
  if (!isfinite(dt) || dt <= 0 || dt > .05f)
    return;
  for (size_t i = 0; i < s->count; i++) {
    Object *o = &s->objects[i];
    if (o->anchored || o->operand || !o->mesh.nv)
      continue;
    if (o->kind == LX_CHARACTER) {
      o->velocity.x = input.x * o->speed;
      o->velocity.z = input.z * o->speed;
      if (jump && o->grounded)
        o->velocity.y = o->jump;
    }
    o->grounded = false;
    o->velocity.y += s->gravity * dt;
    translate_world(s, o, mul(o->velocity, dt));
    o->rotation = add(o->rotation, mul(o->angular_velocity, dt));
    for (int k = 0; k < 3; k++) {
      float *p = k == 0   ? &o->rotation.x
                 : k == 1 ? &o->rotation.y
                          : &o->rotation.z;
      *p = wrap_angle(*p);
    }
  }
  for (int iteration = 0; iteration < 5; iteration++) {
    for (size_t i = 0; i < s->count; i++) {
      Object *a = &s->objects[i];
      if (!a->mesh.ni || a->operand || !object_visible(s, a))
        continue;
      for (size_t j = i + 1; j < s->count; j++) {
        Object *b = &s->objects[j];
        if (!b->mesh.ni || b->operand || !object_visible(s, b) ||
            (a->anchored && b->anchored))
          continue;
        Box ba = bounds(s, a), bb = bounds(s, b);
        V3 n;
        float depth;
        if (!box_box(ba, bb, &n, &depth))
          continue; /* Static triangle meshes use their actual surfaces as the
                       narrow phase. */
        if ((a->anchored && (a->kind == LX_TERRAIN || a->kind == LX_MESH ||
                             a->kind == LX_UNION || a->kind == LX_WEDGE)) ||
            (b->anchored && (b->kind == LX_TERRAIN || b->kind == LX_MESH ||
                             b->kind == LX_UNION || b->kind == LX_WEDGE))) {
          Object *fixed = a->anchored ? a : b, *body = a->anchored ? b : a;
          M4 world = object_world(s, fixed);
          for (size_t k = 0; k + 2 < fixed->mesh.ni; k += 3) {
            Box box = bounds(s, body);
            V3 p = point(world, fixed->mesh.v[fixed->mesh.i[k]].p),
               q = point(world, fixed->mesh.v[fixed->mesh.i[k + 1]].p),
               r = point(world, fixed->mesh.v[fixed->mesh.i[k + 2]].p);
            if (box_triangle(box, p, q, r, &n, &depth))
              resolve(s, body, fixed, n, depth);
          }
        } else
          resolve(s, a, b, n, depth);
      }
    }
    for (size_t k = 0; k < s->constraint_count; k++) {
      Constraint *c = &s->constraints[k];
      Object *a = scene_find(s, c->a), *b = scene_find(s, c->b);
      if (!a || !b)
        continue;
      V3 pa = point(object_world(s, a), v3(0, 0, 0)),
         pb = point(object_world(s, b), v3(0, 0, 0)), delta = sub(pb, pa);
      float len = length3(delta);
      V3 correction = c->type == 0 ? sub(delta, c->offset)
                                   : mul(unit(delta), len - c->length);
      float ia = a->anchored ? 0 : 1 / fmaxf(a->mass, .001f),
            ib = b->anchored ? 0 : 1 / fmaxf(b->mass, .001f);
      if (ia + ib > 0) {
        translate_world(s, a, mul(correction, c->stiffness * ia / (ia + ib)));
        translate_world(s, b, mul(correction, -c->stiffness * ib / (ia + ib)));
        if (c->type == 0 && !b->anchored) {
          b->rotation = a->rotation;
          b->velocity = a->velocity;
        }
      }
    }
  }
}
static bool triangle_hit(Ray ray, V3 a, V3 b, V3 c, float *t) {
  V3 e1 = sub(b, a), e2 = sub(c, a), h = cross(ray.direction, e2);
  float det = dot(e1, h);
  if (fabsf(det) < 1e-8f)
    return false;
  float inv = 1 / det;
  V3 s = sub(ray.origin, a);
  float u = dot(s, h) * inv;
  if (u < 0 || u > 1)
    return false;
  V3 q = cross(s, e1);
  float v = dot(ray.direction, q) * inv;
  if (v < 0 || u + v > 1)
    return false;
  float dist = dot(e2, q) * inv;
  if (dist < 0 || dist >= *t)
    return false;
  *t = dist;
  return true;
}
bool scene_raycast(const Scene *s, Ray ray, uint32_t *id, V3 *hit,
                   float *distance) {
  bool found = false;
  float nearest = *distance;
  for (size_t i = 0; i < s->count; i++) {
    const Object *o = &s->objects[i];
    if (!object_visible(s, o) || o->locked)
      continue;
    M4 world = object_world(s, o);
    for (size_t k = 0; k + 2 < o->mesh.ni; k += 3)
      if (triangle_hit(ray, point(world, o->mesh.v[o->mesh.i[k]].p),
                       point(world, o->mesh.v[o->mesh.i[k + 1]].p),
                       point(world, o->mesh.v[o->mesh.i[k + 2]].p), &nearest)) {
        *id = o->id;
        found = true;
      }
  }
  if (found) {
    *hit = add(ray.origin, mul(ray.direction, nearest));
    *distance = nearest;
  }
  return found;
}
static bool finite_range(float v, float low, float high) {
  return isfinite(v) && v >= low && v <= high;
}
bool scene_validate(const Scene *s, char error[256]) {
  if (s->count > LX_MAX_OBJECTS || s->script_count > LX_MAX_SCRIPTS ||
      s->gui_count > 4096 || s->constraint_count > 4096) {
    snprintf(error, 256, "Project exceeds resource limits");
    return false;
  }
  if (!finite_range(s->gravity, -100, 100) || s->sky_mode < 0 ||
      s->sky_mode > 2) {
    snprintf(error, 256, "Invalid environment");
    return false;
  }
  for (int k = 0; k < 3; k++)
    if (!finite_range(s->sky_top[k], 0, 1) ||
        !finite_range(s->sky_bottom[k], 0, 1) ||
        !finite_range(s->sun[k], -1e6f, 1e6f)) {
      snprintf(error, 256, "Invalid sky or sunlight");
      return false;
    }
  size_t total_vertices = 0;
  for (size_t i = 0; i < s->count; i++) {
    const Object *o = &s->objects[i];
    if (!o->id || o->id >= s->next_id || o->kind < 0 || o->kind > LX_PYRAMID ||
        o->mesh.nv > LX_MAX_VERTICES || o->mesh.ni > LX_MAX_VERTICES * 12 ||
        o->mesh.ni % 3 || o->property_count < 0 || o->property_count > 32) {
      snprintf(error, 256, "Invalid object or geometry limits");
      return false;
    }
    for (size_t j = 0; j < i; j++)
      if (s->objects[j].id == o->id) {
        snprintf(error, 256, "Duplicate object ID");
        return false;
      }
    V3 values[] = {o->position, o->rotation, o->scale, o->pivot, o->velocity};
    for (int k = 0; k < 5; k++)
      if (!isfinite(values[k].x) || !isfinite(values[k].y) ||
          !isfinite(values[k].z)) {
        snprintf(error, 256, "Non-finite transform");
        return false;
      }
    if (o->scale.x <= .00001f || o->scale.y <= .00001f ||
        o->scale.z <= .00001f || !isfinite(o->mass) || o->mass <= 0 ||
        !isfinite(o->bounce) || o->bounce < 0 || o->bounce > 1 ||
        !isfinite(o->friction) || o->friction < 0) {
      snprintf(error, 256, "Invalid scale or physical properties");
      return false;
    }
    total_vertices += o->mesh.nv;
    if (total_vertices > 1000000) {
      snprintf(error, 256, "Project mesh budget exceeded");
      return false;
    }
    for (int k = 0; k < 4; k++)
      if (!finite_range(o->material.color[k], 0, 1)) {
        snprintf(error, 256, "Invalid material colour");
        return false;
      }
    if (!finite_range(o->material.metallic, 0, 1) ||
        !finite_range(o->material.roughness, 0, 1) ||
        !finite_range(o->material.emission, 0, 100) ||
        !finite_range(o->volume, 0, 2) || !finite_range(o->pitch, .1f, 4) ||
        !finite_range(o->min_distance, .01f, 100000) ||
        !finite_range(o->max_distance, o->min_distance, 1000000) ||
        !finite_range(o->speed, 0, 1000) || !finite_range(o->jump, 0, 1000)) {
      snprintf(error, 256, "Invalid material, sound or movement values");
      return false;
    }
    for (int k = 0; k < o->property_count; k++)
      if (o->properties[k].type < 0 || o->properties[k].type > 2 ||
          !isfinite(o->properties[k].number)) {
        snprintf(error, 256, "Invalid custom property");
        return false;
      }
    uint32_t walk = o->parent;
    size_t depth = 0;
    while (walk) {
      const Object *p = scene_get(s, walk);
      if (!p || walk == o->id || ++depth > s->count) {
        snprintf(error, 256, "Invalid or cyclic object hierarchy");
        return false;
      }
      walk = p->parent;
    }
    for (size_t j = 0; j < o->mesh.nv; j++) {
      Vertex v = o->mesh.v[j];
      if (!isfinite(v.p.x) || !isfinite(v.p.y) || !isfinite(v.p.z) ||
          !isfinite(v.n.x) || !isfinite(v.n.y) || !isfinite(v.n.z)) {
        snprintf(error, 256, "Invalid vertex");
        return false;
      }
    }
    for (size_t j = 0; j < o->mesh.ni; j++)
      if (o->mesh.i[j] >= o->mesh.nv) {
        snprintf(error, 256, "Mesh index out of range");
        return false;
      }
    if (o->rig) {
      const Rig *r = o->rig;
      if (r->count < 0 || r->count > LX_BONES) {
        snprintf(error, 256, "Rig limit exceeded");
        return false;
      }
      for (int j = 0; j < r->count; j++)
        if (r->bones[j].parent >= j || r->bones[j].parent < -1) {
          snprintf(error, 256, "Rig must be topologically ordered");
          return false;
        }
    }
  }
  for (size_t i = 0; i < s->constraint_count; i++)
    if (!scene_get(s, s->constraints[i].a) ||
        !scene_get(s, s->constraints[i].b)) {
      snprintf(error, 256, "Missing constraint endpoint");
      return false;
    }
  for (size_t i = 0; i < s->gui_count; i++) {
    const Gui *g = &s->gui[i];
    if (!g->id || g->id >= s->next_id || g->type < 0 || g->type > 4 ||
        g->layout < 0 || g->layout > 2 || !finite_range(g->w, 1, 10000) ||
        !finite_range(g->h, 1, 10000) || !finite_range(g->aspect, 0, 10) ||
        g->min_w < 1 || g->min_h < 1 || g->max_w < g->min_w ||
        g->max_h < g->min_h) {
      snprintf(error, 256, "Invalid GUI constraints");
      return false;
    }
    for (size_t j = 0; j < i; j++)
      if (s->gui[j].id == g->id) {
        snprintf(error, 256, "Duplicate GUI ID");
        return false;
      }
    for (int k = 0; k < 4; k++)
      if (!finite_range(g->color[k], 0, 1) ||
          !finite_range(g->text_color[k], 0, 1)) {
        snprintf(error, 256, "Invalid GUI colour");
        return false;
      }
    uint32_t walk = g->parent;
    size_t depth = 0;
    while (walk) {
      const Gui *p = NULL;
      for (size_t j = 0; j < s->gui_count; j++)
        if (s->gui[j].id == walk)
          p = &s->gui[j];
      if (!p || walk == g->id || ++depth > s->gui_count) {
        snprintf(error, 256, "Invalid GUI hierarchy");
        return false;
      }
      walk = p->parent;
    }
  }
  error[0] = 0;
  return true;
}
