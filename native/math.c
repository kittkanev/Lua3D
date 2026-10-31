#include "lunoryx.h"
#include <math.h>
V3 v3(float x, float y, float z) {
  V3 r = {x, y, z};
  return r;
}
V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
V3 mul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) {
  return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
}
float length3(V3 a) { return sqrtf(dot(a, a)); }
V3 unit(V3 a) {
  float l = length3(a);
  return l > 1e-10f ? mul(a, 1 / l) : v3(0, 1, 0);
}
float wrap_angle(float x) {
  if (!isfinite(x))
    return 0;
  x = fmodf(x + 180, 360);
  if (x < 0)
    x += 360;
  return x - 180;
}
M4 identity(void) {
  M4 r = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  return r;
}
M4 mmul(M4 a, M4 b) {
  M4 r = {{0}};
  for (int c = 0; c < 4; c++)
    for (int row = 0; row < 4; row++)
      for (int k = 0; k < 4; k++)
        r.m[c * 4 + row] += a.m[k * 4 + row] * b.m[c * 4 + k];
  return r;
}
Q4 quat(V3 d) {
  float x = d.x * LX_PI / 360, y = d.y * LX_PI / 360, z = d.z * LX_PI / 360;
  float cx = cosf(x), sx = sinf(x), cy = cosf(y), sy = sinf(y), cz = cosf(z),
        sz = sinf(z);
  Q4 q = {sx * cy * cz + cx * sy * sz, cx * sy * cz - sx * cy * sz,
          cx * cy * sz + sx * sy * cz, cx * cy * cz - sx * sy * sz};
  return q;
}
M4 trs(V3 p, Q4 q, V3 s) {
  float x = q.x, y = q.y, z = q.z, w = q.w;
  M4 r = {{(1 - 2 * y * y - 2 * z * z) * s.x, (2 * x * y + 2 * w * z) * s.x,
           (2 * x * z - 2 * w * y) * s.x, 0, (2 * x * y - 2 * w * z) * s.y,
           (1 - 2 * x * x - 2 * z * z) * s.y, (2 * y * z + 2 * w * x) * s.y, 0,
           (2 * x * z + 2 * w * y) * s.z, (2 * y * z - 2 * w * x) * s.z,
           (1 - 2 * x * x - 2 * y * y) * s.z, 0, p.x, p.y, p.z, 1}};
  return r;
}
V3 point(M4 a, V3 p) {
  return v3(a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z + a.m[12],
            a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z + a.m[13],
            a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z + a.m[14]);
}
V3 direction(M4 a, V3 p) {
  return v3(a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z,
            a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z,
            a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z);
}
M4 inverse(M4 a) {
  float t[4][8];
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 8; j++)
      t[i][j] = j < 4 ? a.m[j * 4 + i] : (j - 4 == i ? 1 : 0);
  for (int c = 0; c < 4; c++) {
    int pivot = c;
    for (int r = c + 1; r < 4; r++)
      if (fabsf(t[r][c]) > fabsf(t[pivot][c]))
        pivot = r;
    if (fabsf(t[pivot][c]) < 1e-12f)
      return identity();
    for (int j = 0; j < 8; j++) {
      float k = t[c][j];
      t[c][j] = t[pivot][j];
      t[pivot][j] = k;
    }
    float k = t[c][c];
    for (int j = 0; j < 8; j++)
      t[c][j] /= k;
    for (int r = 0; r < 4; r++)
      if (r != c) {
        k = t[r][c];
        for (int j = 0; j < 8; j++)
          t[r][j] -= k * t[c][j];
      }
  }
  M4 out;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      out.m[j * 4 + i] = t[i][j + 4];
  return out;
}
Q4 qslerp(Q4 a, Q4 b, float t) {
  float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  if (d < 0) {
    d = -d;
    b.x = -b.x;
    b.y = -b.y;
    b.z = -b.z;
    b.w = -b.w;
  }
  float x = 1 - t, y = t;
  if (d < .9995f) {
    float ang = acosf(fminf(d, 1)), s = sinf(ang);
    x = sinf((1 - t) * ang) / s;
    y = sinf(t * ang) / s;
  }
  Q4 q = {x * a.x + y * b.x, x * a.y + y * b.y, x * a.z + y * b.z,
          x * a.w + y * b.w};
  float n = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (n > 0) {
    q.x /= n;
    q.y /= n;
    q.z /= n;
    q.w /= n;
  }
  return q;
}
