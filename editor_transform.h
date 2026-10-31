#pragma once
#include "scene_tools.h"
#include <cstring>

struct StudioMatrix {
  float M[16];
};
inline StudioMatrix StudioIdentity() {
  StudioMatrix m = {};
  for (int i = 0; i < 4; ++i)
    m.M[i * 5] = 1;
  return m;
}
inline StudioMatrix StudioMultiply(const StudioMatrix &a,
                                   const StudioMatrix &b) {
  StudioMatrix m = {};
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r)
      for (int k = 0; k < 4; ++k)
        m.M[c * 4 + r] += a.M[k * 4 + r] * b.M[c * 4 + k];
  return m;
}
inline StudioMatrix StudioInverse(const StudioMatrix &m) {
  double a[4][8];
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 8; ++c)
      a[r][c] = c < 4 ? m.M[c * 4 + r] : (c - 4 == r ? 1 : 0);
  for (int c = 0; c < 4; ++c) {
    int pivot = c;
    for (int r = c + 1; r < 4; ++r)
      if (std::abs(a[r][c]) > std::abs(a[pivot][c]))
        pivot = r;
    if (std::abs(a[pivot][c]) < 1e-12)
      return StudioIdentity();
    for (int j = 0; j < 8; ++j)
      std::swap(a[c][j], a[pivot][j]);
    double d = a[c][c];
    for (int j = 0; j < 8; ++j)
      a[c][j] /= d;
    for (int r = 0; r < 4; ++r)
      if (r != c) {
        d = a[r][c];
        for (int j = 0; j < 8; ++j)
          a[r][j] -= d * a[c][j];
      }
  }
  StudioMatrix out;
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      out.M[c * 4 + r] = float(a[r][c + 4]);
  return out;
}
inline StudioMatrix StudioObjectMatrix(const GameObject &object) {
  StudioMatrix out = StudioIdentity();
  Vec3 origin = {};
  TransformPoint(object, {0, 0, 0}, origin);
  for (int c = 0; c < 3; ++c) {
    Vec3 axis = {float(c == 0), float(c == 1), float(c == 2)}, p = {};
    TransformPoint(object, axis, p);
    out.M[c * 4] = p.x - origin.x;
    out.M[c * 4 + 1] = p.y - origin.y;
    out.M[c * 4 + 2] = p.z - origin.z;
  }
  out.M[12] = origin.x;
  out.M[13] = origin.y;
  out.M[14] = origin.z;
  return out;
}
inline bool StudioApplyMatrix(GameObject &object, const StudioMatrix &m) {
  for (float v : m.M)
    if (!std::isfinite(v))
      return false;
  float scale[3];
  Vec3 axes[3];
  for (int c = 0; c < 3; ++c) {
    axes[c] = {m.M[c * 4], m.M[c * 4 + 1], m.M[c * 4 + 2]};
    scale[c] = float(std::sqrt(Dot(axes[c], axes[c])));
    if (scale[c] < .001f || scale[c] > 100000)
      return false;
    axes[c].x /= scale[c];
    axes[c].y /= scale[c];
    axes[c].z /= scale[c];
  }
  if (std::abs(Dot(axes[0], axes[1])) > .002f ||
      std::abs(Dot(axes[0], axes[2])) > .002f ||
      std::abs(Dot(axes[1], axes[2])) > .002f ||
      Dot(axes[0], Cross(axes[1], axes[2])) < 0)
    return false;
  float ry = std::asin(std::max(-1.0f, std::min(1.0f, axes[2].x))), rx, rz;
  if (std::abs(std::cos(ry)) > .0001f) {
    rx = std::atan2(-axes[2].y, axes[2].z);
    rz = std::atan2(-axes[1].x, axes[0].x);
  } else {
    rx = std::atan2(axes[1].z, axes[1].y);
    rz = 0;
  }
  object.Position[0] = m.M[12];
  object.Position[1] = m.M[13];
  object.Position[2] = m.M[14];
  object.Rotation[0] = WrapYaw(rx * 180 / PI);
  object.Rotation[1] = WrapYaw(ry * 180 / PI);
  object.Rotation[2] = WrapYaw(rz * 180 / PI);
  std::copy(scale, scale + 3, object.Scale);
  return true;
}
inline void StudioCameraMatrices(float distance, float pitch, float yaw,
                                 const float target[3], float aspect, float fov,
                                 float farClip, StudioMatrix &view,
                                 StudioMatrix &projection) {
  GameObject orbit("camera");
  orbit.Rotation[0] = pitch;
  orbit.Rotation[1] = yaw;
  view = StudioObjectMatrix(orbit);
  StudioMatrix shift = StudioIdentity();
  shift.M[12] = -target[0];
  shift.M[13] = -target[1];
  shift.M[14] = -target[2];
  view = StudioMultiply(view, shift);
  view.M[14] -= distance;
  const float n = .1f, f = 1 / std::tan(fov * PI / 360);
  projection = {};
  projection.M[0] = f / std::max(aspect, .001f);
  projection.M[5] = f;
  projection.M[10] = (farClip + n) / (n - farClip);
  projection.M[11] = -1;
  projection.M[14] = 2 * farClip * n / (n - farClip);
}
struct StudioGridPlan {
  float Step, CenterX, CenterZ, Extent;
  int Lines;
};
inline StudioGridPlan StudioPlanGrid(float distance, float targetX,
                                     float targetZ, float minimum = 1,
                                     bool adaptive = true) {
  float step = minimum;
  if (adaptive)
    step = std::max(minimum, std::pow(10.0f, std::floor(std::log10(std::max(
                                                 .01f, distance * .08f)))));
  const int lines = 120;
  return {step, std::floor(targetX / step) * step,
          std::floor(targetZ / step) * step, step * lines, lines};
}
inline uint64_t StudioSceneFingerprint(const std::vector<GameObject> &objects) {
  uint64_t hash = 1469598103934665603ULL;
  auto bytes = [&](const void *data, size_t n) {
    auto p = static_cast<const unsigned char *>(data);
    for (size_t i = 0; i < n; ++i)
      hash = (hash ^ p[i]) * 1099511628211ULL;
  };
  for (const auto &o : objects) {
    bytes(&o.Id, sizeof(o.Id));
    bytes(o.Name.data(), o.Name.size());
    bytes(&o.Type, sizeof(o.Type));
    bytes(o.Position, sizeof(o.Position));
    bytes(o.Rotation, sizeof(o.Rotation));
    bytes(o.Scale, sizeof(o.Scale));
    bytes(o.Color, sizeof(o.Color));
    bytes(&o.Dynamic, sizeof(o.Dynamic));
    bytes(o.Velocity, sizeof(o.Velocity));
    bytes(&o.Mass, sizeof(o.Mass));
    bytes(&o.Restitution, sizeof(o.Restitution));
    bytes(&o.Friction, sizeof(o.Friction));
    bytes(&o.Wireframe, sizeof(o.Wireframe));
    bytes(&o.Character, sizeof(o.Character));
    bytes(&o.MoveSpeed, sizeof(o.MoveSpeed));
    bytes(&o.JumpSpeed, sizeof(o.JumpSpeed));
    bytes(&o.Visible, sizeof(o.Visible));
    bytes(&o.Metallic, sizeof(o.Metallic));
    bytes(&o.Roughness, sizeof(o.Roughness));
    bytes(o.Vertices.data(), o.Vertices.size() * sizeof(float));
    bytes(o.Indices.data(), o.Indices.size() * sizeof(uint32_t));
    bytes(o.VertexColors.data(), o.VertexColors.size() * sizeof(float));
    for (const auto &p : o.Properties) {
      bytes(p.first.data(), p.first.size());
      bytes(&p.second.Type, sizeof(p.second.Type));
      bytes(&p.second.NumberValue, sizeof(double));
      bytes(&p.second.BoolValue, sizeof(bool));
      bytes(p.second.TextValue.data(), p.second.TextValue.size());
    }
  }
  return hash;
}
struct StudioObjectHistory {
  std::vector<std::vector<GameObject>> States;
  size_t Cursor = 0, Bytes = 0;
  uint64_t Fingerprint = 0;
  static size_t Size(const std::vector<GameObject> &objects) {
    size_t n = objects.size() * sizeof(GameObject);
    for (const auto &o : objects)
      n += o.Vertices.size() * 4 + o.Indices.size() * 4 +
           o.VertexColors.size() * 4;
    return n;
  }
  void Capture(const std::vector<GameObject> &objects) {
    uint64_t hash = StudioSceneFingerprint(objects);
    if (!States.empty() && hash == Fingerprint)
      return;
    size_t n = Size(objects);
    if (n > 64 * 1024 * 1024)
      return;
    while (States.size() > Cursor + 1) {
      Bytes -= Size(States.back());
      States.pop_back();
    }
    while (!States.empty() &&
           (States.size() >= 32 || Bytes + n > 64 * 1024 * 1024)) {
      Bytes -= Size(States.front());
      States.erase(States.begin());
      if (Cursor)
        --Cursor;
    }
    States.push_back(objects);
    Cursor = States.size() - 1;
    Bytes += n;
    Fingerprint = hash;
  }
  bool Undo(std::vector<GameObject> &objects) {
    Capture(objects);
    if (!Cursor || States.empty())
      return false;
    objects = States[--Cursor];
    Fingerprint = StudioSceneFingerprint(objects);
    return true;
  }
  bool Redo(std::vector<GameObject> &objects) {
    Capture(objects);
    if (States.empty() || Cursor + 1 >= States.size())
      return false;
    objects = States[++Cursor];
    Fingerprint = StudioSceneFingerprint(objects);
    return true;
  }
  void Clear() {
    States.clear();
    Cursor = Bytes = 0;
    Fingerprint = 0;
  }
};
