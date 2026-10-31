#include "lunoryx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static bool reserve(void **ptr, size_t *cap, size_t need, size_t size) {
  if (need <= *cap)
    return true;
  size_t n = *cap ? *cap * 2 : 64;
  while (n < need)
    n *= 2;
  void *p = realloc(*ptr, n * size);
  if (!p)
    return false;
  *ptr = p;
  *cap = n;
  return true;
}
void mesh_free(Mesh *m) {
  free(m->v);
  free(m->i);
  memset(m, 0, sizeof(*m));
}
bool mesh_vertex(Mesh *m, Vertex v) {
  if (m->nv >= LX_MAX_VERTICES ||
      !reserve((void **)&m->v, &m->cv, m->nv + 1, sizeof(Vertex)))
    return false;
  m->v[m->nv++] = v;
  return true;
}
bool mesh_triangle(Mesh *m, uint32_t a, uint32_t b, uint32_t c) {
  if (m->ni >= LX_MAX_VERTICES * 12 ||
      !reserve((void **)&m->i, &m->ci, m->ni + 3, sizeof(uint32_t)))
    return false;
  m->i[m->ni++] = a;
  m->i[m->ni++] = b;
  m->i[m->ni++] = c;
  return true;
}
bool mesh_copy(Mesh *d, const Mesh *s) {
  Mesh m = {0};
  if (s->nv) {
    m.v = malloc(s->nv * sizeof(Vertex));
    if (!m.v)
      return false;
    memcpy(m.v, s->v, s->nv * sizeof(Vertex));
    m.nv = m.cv = s->nv;
  }
  if (s->ni) {
    m.i = malloc(s->ni * sizeof(uint32_t));
    if (!m.i) {
      mesh_free(&m);
      return false;
    }
    memcpy(m.i, s->i, s->ni * sizeof(uint32_t));
    m.ni = m.ci = s->ni;
  }
  mesh_free(d);
  *d = m;
  return true;
}
static Vertex vert(V3 p, V3 n, float u, float v) {
  Vertex r = {0};
  r.p = p;
  r.n = n;
  r.uv[0] = u;
  r.uv[1] = v;
  for (int i = 0; i < 4; i++)
    r.color[i] = 1;
  return r;
}
static bool face(Mesh *m, V3 a, V3 b, V3 c, V3 d) {
  uint32_t i = (uint32_t)m->nv;
  V3 n = unit(cross(sub(b, a), sub(c, a)));
  return mesh_vertex(m, vert(a, n, 0, 0)) && mesh_vertex(m, vert(b, n, 1, 0)) &&
         mesh_vertex(m, vert(c, n, 1, 1)) && mesh_vertex(m, vert(d, n, 0, 1)) &&
         mesh_triangle(m, i, i + 1, i + 2) && mesh_triangle(m, i, i + 2, i + 3);
}
bool mesh_primitive(Mesh *out, Kind kind) {
  Mesh m = {0};
  bool ok = true;
  if (kind == LX_GROUP || kind == LX_ATTACHMENT || kind == LX_EFFECT) {
    mesh_free(out);
    return true;
  }
  if (kind == LX_CUBE || kind == LX_WEDGE) {
    V3 a[8] = {v3(-.5f, -.5f, .5f), v3(.5f, -.5f, .5f),   v3(.5f, .5f, .5f),
               v3(-.5f, .5f, .5f),  v3(-.5f, -.5f, -.5f), v3(-.5f, .5f, -.5f),
               v3(.5f, .5f, -.5f),  v3(.5f, -.5f, -.5f)};
    if (kind == LX_WEDGE) {
      a[5] = a[4];
      a[6] = a[7];
    }
    int f[6][4] = {{0, 1, 2, 3}, {7, 4, 5, 6}, {3, 2, 6, 5},
                   {4, 0, 3, 5}, {1, 7, 6, 2}, {4, 7, 1, 0}};
    for (int j = 0; j < 6 && ok; j++)
      ok = face(&m, a[f[j][0]], a[f[j][1]], a[f[j][2]], a[f[j][3]]);
  } else if (kind == LX_PYRAMID) {
    V3 p[5] = {v3(0, .5f, 0), v3(-.5f, -.5f, .5f), v3(.5f, -.5f, .5f),
               v3(.5f, -.5f, -.5f), v3(-.5f, -.5f, -.5f)};
    int triangles[6][3] = {{0, 1, 2}, {0, 2, 3}, {0, 3, 4},
                           {0, 4, 1}, {4, 3, 2}, {4, 2, 1}};
    for (int j = 0; j < 6 && ok; j++) {
      V3 a = p[triangles[j][0]], b = p[triangles[j][1]], c = p[triangles[j][2]];
      V3 n = unit(cross(sub(b, a), sub(c, a)));
      uint32_t base = (uint32_t)m.nv;
      ok = mesh_vertex(&m, vert(a, n, .5f, 0)) &&
           mesh_vertex(&m, vert(b, n, 0, 1)) &&
           mesh_vertex(&m, vert(c, n, 1, 1)) &&
           mesh_triangle(&m, base, base + 1, base + 2);
    }
  } else if (kind == LX_PLANE || kind == LX_TERRAIN) {
    int n = kind == LX_TERRAIN ? 48 : 1;
    for (int z = 0; z <= n && ok; z++)
      for (int x = 0; x <= n && ok; x++) {
        float u = (float)x / n, v = (float)z / n;
        float y = kind == LX_TERRAIN ? .06f * sinf(u * 12) * cosf(v * 9) : 0;
        ok = mesh_vertex(&m, vert(v3(u - .5f, y, v - .5f), v3(0, 1, 0), u, v));
      }
    for (int z = 0; z < n && ok; z++)
      for (int x = 0; x < n && ok; x++) {
        uint32_t i = (uint32_t)(z * (n + 1) + x);
        ok = mesh_triangle(&m, i, i + n + 1, i + 1) &&
             mesh_triangle(&m, i + 1, i + n + 1, i + n + 2);
      }
    mesh_normals(&m);
  } else if (kind == LX_SPHERE) {
    int rings = 16, slices = 24;
    for (int r = 0; r <= rings && ok; r++)
      for (int s = 0; s <= slices && ok; s++) {
        float u = (float)s / slices, v = (float)r / rings;
        V3 n = v3(sinf(v * LX_PI) * cosf(u * 2 * LX_PI), cosf(v * LX_PI),
                  sinf(v * LX_PI) * sinf(u * 2 * LX_PI));
        ok = mesh_vertex(&m, vert(mul(n, .5f), n, u, v));
      }
    for (int r = 0; r < rings && ok; r++)
      for (int s = 0; s < slices && ok; s++) {
        uint32_t i = (uint32_t)(r * (slices + 1) + s), j = i + slices + 1;
        if (r > 0)
          ok = mesh_triangle(&m, i, i + 1, j);
        if (r < rings - 1 && ok)
          ok = mesh_triangle(&m, i + 1, j + 1, j);
      }
  } else if (kind == LX_CYLINDER) {
    for (int k = 0; k < 24 && ok; k++) {
      float a = k * 2 * LX_PI / 24, b = (k + 1) * 2 * LX_PI / 24;
      V3 p = v3(cosf(a) * .5f, 0, sinf(a) * .5f),
         q = v3(cosf(b) * .5f, 0, sinf(b) * .5f);
      ok = face(&m, add(p, v3(0, -.5f, 0)), add(p, v3(0, .5f, 0)),
                add(q, v3(0, .5f, 0)), add(q, v3(0, -.5f, 0)));
      uint32_t i = (uint32_t)m.nv;
      ok = ok && mesh_vertex(&m, vert(v3(0, .5f, 0), v3(0, 1, 0), .5f, .5f)) &&
           mesh_vertex(&m, vert(add(q, v3(0, .5f, 0)), v3(0, 1, 0), q.x + .5f,
                                q.z + .5f)) &&
           mesh_vertex(&m, vert(add(p, v3(0, .5f, 0)), v3(0, 1, 0), p.x + .5f,
                                p.z + .5f)) &&
           mesh_triangle(&m, i, i + 1, i + 2);
      i = (uint32_t)m.nv;
      ok = ok &&
           mesh_vertex(&m, vert(v3(0, -.5f, 0), v3(0, -1, 0), .5f, .5f)) &&
           mesh_vertex(&m, vert(add(p, v3(0, -.5f, 0)), v3(0, -1, 0), p.x + .5f,
                                p.z + .5f)) &&
           mesh_vertex(&m, vert(add(q, v3(0, -.5f, 0)), v3(0, -1, 0), q.x + .5f,
                                q.z + .5f)) &&
           mesh_triangle(&m, i, i + 1, i + 2);
    }
  } else
    ok = false;
  if (!ok) {
    mesh_free(&m);
    return false;
  }
  mesh_free(out);
  *out = m;
  return true;
}
void mesh_normals(Mesh *m) {
  for (size_t i = 0; i < m->nv; i++)
    m->v[i].n = v3(0, 0, 0);
  for (size_t i = 0; i + 2 < m->ni; i += 3) {
    uint32_t a = m->i[i], b = m->i[i + 1], c = m->i[i + 2];
    V3 n = cross(sub(m->v[b].p, m->v[a].p), sub(m->v[c].p, m->v[a].p));
    m->v[a].n = add(m->v[a].n, n);
    m->v[b].n = add(m->v[b].n, n);
    m->v[c].n = add(m->v[c].n, n);
  }
  for (size_t i = 0; i < m->nv; i++)
    m->v[i].n = unit(m->v[i].n);
}
static Vertex mixvertex(Vertex a, Vertex b, float t) {
  Vertex r = a;
  r.p = add(mul(a.p, 1 - t), mul(b.p, t));
  r.n = unit(add(mul(a.n, 1 - t), mul(b.n, t)));
  for (int k = 0; k < 2; k++)
    r.uv[k] = a.uv[k] * (1 - t) + b.uv[k] * t;
  for (int k = 0; k < 4; k++)
    r.color[k] = a.color[k] * (1 - t) + b.color[k] * t;
  return r;
}
bool mesh_subdivide(Mesh *m) {
  if (m->ni / 3 * 6 > LX_MAX_VERTICES)
    return false;
  Mesh r = {0};
  for (size_t i = 0; i + 2 < m->ni; i += 3) {
    Vertex a = m->v[m->i[i]], b = m->v[m->i[i + 1]], c = m->v[m->i[i + 2]];
    uint32_t j = (uint32_t)r.nv;
    if (!mesh_vertex(&r, a) || !mesh_vertex(&r, b) || !mesh_vertex(&r, c) ||
        !mesh_vertex(&r, mixvertex(a, b, .5f)) ||
        !mesh_vertex(&r, mixvertex(b, c, .5f)) ||
        !mesh_vertex(&r, mixvertex(c, a, .5f)) ||
        !mesh_triangle(&r, j, j + 3, j + 5) ||
        !mesh_triangle(&r, j + 3, j + 1, j + 4) ||
        !mesh_triangle(&r, j + 5, j + 4, j + 2) ||
        !mesh_triangle(&r, j + 3, j + 4, j + 5)) {
      mesh_free(&r);
      return false;
    }
  }
  mesh_free(m);
  *m = r;
  return true;
}
bool mesh_sculpt(Mesh *m, V3 center, float radius, float strength, int mode) {
  if (radius <= 0 || !isfinite(strength))
    return false;
  V3 *old = malloc(m->nv * sizeof(V3));
  if (!old)
    return false;
  for (size_t i = 0; i < m->nv; i++)
    old[i] = m->v[i].p;
  for (size_t i = 0; i < m->nv; i++) {
    float d = length3(sub(old[i], center));
    if (d > radius)
      continue;
    float t = 1 - d / radius;
    t = t * t * (3 - 2 * t);
    V3 delta = mul(m->v[i].n, strength * t);
    if (mode == 1) {
      V3 avg = v3(0, 0, 0);
      int n =
          0; /* Shared positions also weld duplicated seams during smoothing. */
      for (size_t j = 0; j < m->ni; j += 3) {
        bool linked = false;
        for (int k = 0; k < 3; k++)
          if (length3(sub(old[m->i[j + k]], old[i])) < 1e-5f)
            linked = true;
        if (linked)
          for (int k = 0; k < 3; k++) {
            avg = add(avg, old[m->i[j + k]]);
            n++;
          }
      }
      if (n)
        delta = mul(sub(mul(avg, 1.0f / n), old[i]),
                    fminf(fabsf(strength) * 5, 1) * t);
    } else if (mode == 2)
      delta = mul(m->v[i].n, -dot(sub(old[i], center), m->v[i].n) *
                                 fminf(fabsf(strength) * 5, 1) * t);
    m->v[i].p = add(old[i], delta);
  }
  free(old);
  mesh_normals(m);
  return true;
}
double mesh_volume(const Mesh *m) {
  double v = 0;
  for (size_t i = 0; i + 2 < m->ni; i += 3)
    v += dot(m->v[m->i[i]].p, cross(m->v[m->i[i + 1]].p, m->v[m->i[i + 2]].p)) /
         6.0;
  return v;
}
