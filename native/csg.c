/* BSP solid modelling. Inputs are closed, consistently wound triangle solids.
 * Splits interpolate UVs, colours and normals. A failed operation is
 * transactional. */
#include "lunoryx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define EPS 0.00001f
#define POLY_MAX 64
#define BSP_DEPTH 192
typedef struct {
  Vertex v[POLY_MAX];
  int n;
  V3 normal;
  float w;
} Poly;
typedef struct {
  Poly *p;
  size_t n, cap;
} List;
typedef struct Node {
  V3 normal;
  float w;
  bool plane;
  List polygons;
  struct Node *front, *back;
} Node;
typedef struct {
  bool failed;
  size_t work;
} Work;
static void list_free(List *l) {
  free(l->p);
  memset(l, 0, sizeof(*l));
}
static void push(List *l, Poly p, Work *w) {
  if (w->failed)
    return;
  if (++w->work > 500000 || l->n >= 32768) {
    w->failed = true;
    return;
  }
  if (l->n == l->cap) {
    size_t cap = l->cap ? l->cap * 2 : 16;
    Poly *a = realloc(l->p, cap * sizeof(Poly));
    if (!a) {
      w->failed = true;
      return;
    }
    l->p = a;
    l->cap = cap;
  }
  l->p[l->n++] = p;
}
static void plane(Poly *p) {
  p->normal = unit(cross(sub(p->v[1].p, p->v[0].p), sub(p->v[2].p, p->v[0].p)));
  p->w = dot(p->normal, p->v[0].p);
}
static Vertex lerp(Vertex a, Vertex b, float t) {
  Vertex v = a;
  v.p = add(a.p, mul(sub(b.p, a.p), t));
  v.n = unit(add(a.n, mul(sub(b.n, a.n), t)));
  for (int i = 0; i < 2; i++)
    v.uv[i] = a.uv[i] + (b.uv[i] - a.uv[i]) * t;
  for (int i = 0; i < 4; i++)
    v.color[i] = a.color[i] + (b.color[i] - a.color[i]) * t;
  return v;
}
static void split(V3 n, float d, Poly p, List *cf, List *cb, List *front,
                  List *back, Work *w) {
  int types[POLY_MAX], type = 0;
  for (int i = 0; i < p.n; i++) {
    float t = dot(n, p.v[i].p) - d;
    types[i] = t < -EPS ? 2 : t > EPS ? 1 : 0;
    type |= types[i];
  }
  if (type == 0)
    push(dot(n, p.normal) > 0 ? cf : cb, p, w);
  else if (type == 1)
    push(front, p, w);
  else if (type == 2)
    push(back, p, w);
  else {
    Poly f = {0}, b = {0};
    for (int i = 0; i < p.n; i++) {
      int j = (i + 1) % p.n, ti = types[i], tj = types[j];
      if (ti != 2 && f.n < POLY_MAX)
        f.v[f.n++] = p.v[i];
      if (ti != 1 && b.n < POLY_MAX)
        b.v[b.n++] = p.v[i];
      if ((ti | tj) == 3) {
        V3 edge = sub(p.v[j].p, p.v[i].p);
        float t = (d - dot(n, p.v[i].p)) / dot(n, edge);
        Vertex v = lerp(p.v[i], p.v[j], t);
        if (f.n >= POLY_MAX || b.n >= POLY_MAX) {
          w->failed = true;
          return;
        }
        f.v[f.n++] = v;
        b.v[b.n++] = v;
      }
    }
    if (f.n >= 3) {
      plane(&f);
      push(front, f, w);
    }
    if (b.n >= 3) {
      plane(&b);
      push(back, b, w);
    }
  }
}
static void destroy(Node *n) {
  if (!n)
    return;
  destroy(n->front);
  destroy(n->back);
  list_free(&n->polygons);
  free(n);
}
static Node *node(Work *w) {
  Node *n = calloc(1, sizeof(Node));
  if (!n)
    w->failed = true;
  return n;
}
static void build(Node *n, const List *p, int depth, Work *w) {
  if (!n || !p->n || w->failed)
    return;
  if (depth > BSP_DEPTH) {
    w->failed = true;
    return;
  }
  if (!n->plane) {
    n->normal = p->p[0].normal;
    n->w = p->p[0].w;
    n->plane = true;
  }
  List f = {0}, b = {0};
  for (size_t i = 0; i < p->n && !w->failed; i++)
    split(n->normal, n->w, p->p[i], &n->polygons, &n->polygons, &f, &b, w);
  if (f.n) {
    if (!n->front)
      n->front = node(w);
    build(n->front, &f, depth + 1, w);
  }
  if (b.n) {
    if (!n->back)
      n->back = node(w);
    build(n->back, &b, depth + 1, w);
  }
  list_free(&f);
  list_free(&b);
}
static void invert(Node *n) {
  if (!n)
    return;
  for (size_t j = 0; j < n->polygons.n; j++) {
    Poly *p = &n->polygons.p[j];
    for (int i = 0; i < p->n / 2; i++) {
      Vertex v = p->v[i];
      p->v[i] = p->v[p->n - 1 - i];
      p->v[p->n - 1 - i] = v;
    }
    for (int i = 0; i < p->n; i++)
      p->v[i].n = mul(p->v[i].n, -1);
    p->normal = mul(p->normal, -1);
    p->w = -p->w;
  }
  n->normal = mul(n->normal, -1);
  n->w = -n->w;
  invert(n->front);
  invert(n->back);
  Node *tmp = n->front;
  n->front = n->back;
  n->back = tmp;
}
static List clip(const Node *n, const List *p, int depth, Work *w) {
  List f = {0}, b = {0};
  if (w->failed)
    return f;
  if (depth > BSP_DEPTH) {
    w->failed = true;
    return f;
  }
  if (!n || !n->plane) {
    for (size_t i = 0; i < p->n; i++)
      push(&f, p->p[i], w);
    return f;
  }
  for (size_t i = 0; i < p->n; i++)
    split(n->normal, n->w, p->p[i], &f, &b, &f, &b, w);
  if (n->front) {
    List t = clip(n->front, &f, depth + 1, w);
    list_free(&f);
    f = t;
  }
  if (n->back) {
    List t = clip(n->back, &b, depth + 1, w);
    list_free(&b);
    b = t;
  } else
    list_free(&b);
  for (size_t i = 0; i < b.n; i++)
    push(&f, b.p[i], w);
  list_free(&b);
  return f;
}
static void clip_to(Node *a, const Node *b, Work *w) {
  if (!a || w->failed)
    return;
  List t = clip(b, &a->polygons, 0, w);
  list_free(&a->polygons);
  a->polygons = t;
  clip_to(a->front, b, w);
  clip_to(a->back, b, w);
}
static void all(const Node *n, List *l, Work *w) {
  if (!n || w->failed)
    return;
  for (size_t i = 0; i < n->polygons.n; i++)
    push(l, n->polygons.p[i], w);
  all(n->front, l, w);
  all(n->back, l, w);
}
static List polygons(const Mesh *m, Work *w) {
  List l = {0};
  for (size_t i = 0; i + 2 < m->ni; i += 3) {
    Poly p = {0};
    p.n = 3;
    for (int j = 0; j < 3; j++)
      p.v[j] = m->v[m->i[i + j]];
    if (length3(cross(sub(p.v[1].p, p.v[0].p), sub(p.v[2].p, p.v[0].p))) <
        EPS * EPS)
      continue;
    plane(&p);
    push(&l, p, w);
  }
  return l;
}
bool mesh_csg(Mesh *result, const Mesh *ma, const Mesh *mb, int operation) {
  Work w = {0};
  if (operation < 0 || operation > 2 || !ma->ni || !mb->ni ||
      ma->ni + mb->ni > 90000)
    return false;
  Node *a = node(&w), *b = node(&w);
  List pa = polygons(ma, &w), pb = polygons(mb, &w);
  build(a, &pa, 0, &w);
  build(b, &pb, 0, &w);
  list_free(&pa);
  list_free(&pb);
  if (!w.failed) {
    if (operation == 1)
      invert(a);
    if (operation == 2) {
      invert(a);
      clip_to(b, a, &w);
      invert(b);
      clip_to(a, b, &w);
      clip_to(b, a, &w);
    } else {
      clip_to(a, b, &w);
      clip_to(b, a, &w);
      invert(b);
      clip_to(b, a, &w);
      invert(b);
    }
    List tmp = {0};
    all(b, &tmp, &w);
    build(a, &tmp, 0, &w);
    list_free(&tmp);
    if (operation == 1 || operation == 2)
      invert(a);
  }
  List l = {0};
  all(a, &l, &w);
  Mesh out = {0};
  for (size_t i = 0; i < l.n && !w.failed; i++) {
    Poly *p = &l.p[i];
    uint32_t base = (uint32_t)out.nv;
    for (int j = 0; j < p->n; j++)
      if (!mesh_vertex(&out, p->v[j]))
        w.failed = true;
    for (int j = 1; j + 1 < p->n; j++)
      if (!mesh_triangle(&out, base, base + (uint32_t)j,
                         base + (uint32_t)j + 1))
        w.failed = true;
  }
  destroy(a);
  destroy(b);
  list_free(&l);
  if (w.failed) {
    mesh_free(&out);
    return false;
  }
  mesh_free(result);
  *result = out;
  return true;
}
