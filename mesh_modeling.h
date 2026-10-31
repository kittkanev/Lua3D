#pragma once
#include "extensions/modeling/boolean_adapter.h"
#include <array>
#include <map>
#include <set>
#include <stdexcept>
namespace Model {
inline uint64_t Edge(uint32_t a, uint32_t b) {
  if (a > b)
    std::swap(a, b);
  return (uint64_t(a) << 32) | b;
}
inline Vec3 Point(const StudioMesh &m, uint32_t i) {
  return {m.Positions.at(size_t(i) * 3), m.Positions.at(size_t(i) * 3 + 1),
          m.Positions.at(size_t(i) * 3 + 2)};
}
inline Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 Mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 Unit(Vec3 p) {
  double n = std::sqrt(Dot(p, p));
  return n > 1e-20 ? Mul(p, float(1 / n)) : Vec3{0, 1, 0};
}
inline Vec3 Normal(const StudioMesh &m, const std::vector<uint32_t> &f) {
  Vec3 n = {0, 0, 0};
  for (size_t i = 0; i < f.size(); ++i) {
    Vec3 a = Point(m, f[i]), b = Point(m, f[(i + 1) % f.size()]);
    n.x += (a.y - b.y) * (a.z + b.z);
    n.y += (a.z - b.z) * (a.x + b.x);
    n.z += (a.x - b.x) * (a.y + b.y);
  }
  return Unit(n);
}
inline void Check(const StudioMesh &m) {
  if (m.Positions.empty() || m.Positions.size() % 3 ||
      m.Positions.size() / 3 > MAX_EDIT_VERTICES ||
      m.Indices.size() > MAX_EDIT_INDICES || m.Faces.size() > 400000)
    throw std::runtime_error("Mesh exceeds geometry budget");
  for (float x : m.Positions)
    if (!std::isfinite(x) || std::abs(x) > 1e8f)
      throw std::runtime_error("Invalid mesh position");
  for (const auto &f : m.Faces) {
    if (f.size() < 3 || f.size() > 4096)
      throw std::runtime_error("Invalid polygon");
    for (auto i : f)
      if (i >= m.Positions.size() / 3)
        throw std::runtime_error("Invalid vertex index");
  }
  if (!m.FaceMaterials.empty() && m.FaceMaterials.size() != m.Faces.size())
    throw std::runtime_error("Invalid polygon material count");
  if (!m.Normals.empty() && m.Normals.size() != m.Positions.size())
    throw std::runtime_error("Invalid normal count");
  for (float n : m.Normals)
    if (!std::isfinite(n))
      throw std::runtime_error("Invalid normal value");
  if (!m.UV.empty() && m.UV.size() != m.Positions.size() / 3 * 2)
    throw std::runtime_error("Invalid UV count");
  if (!m.Colors.empty() && m.Colors.size() != m.Positions.size())
    throw std::runtime_error("Invalid color count");
}
inline void Triangulate(StudioMesh &m) {
  m.Indices.clear();
  for (const auto &face : m.Faces) {
    auto f = face;
    Vec3 normal = Normal(m, f);
    int axis = 0;
    if (std::abs(normal.y) > std::abs(normal.x))
      axis = 1;
    if (std::abs(normal.z) >
        (axis == 0 ? std::abs(normal.x) : std::abs(normal.y)))
      axis = 2;
    auto cross2 = [&](uint32_t a, uint32_t b, uint32_t c) {
      Vec3 p = Point(m, a), q = Point(m, b), r = Point(m, c);
      float v[3] = {p.x, p.y, p.z}, w[3] = {q.x, q.y, q.z},
            z[3] = {r.x, r.y, r.z};
      int u = (axis + 1) % 3, vv = (axis + 2) % 3;
      return double(w[u] - v[u]) * (z[vv] - v[vv]) -
             double(w[vv] - v[vv]) * (z[u] - v[u]);
    };
    double direction = (axis == 0   ? normal.x
                        : axis == 1 ? normal.y
                                    : normal.z) >= 0
                           ? 1
                           : -1;
    while (f.size() > 3) {
      bool clipped = false;
      for (size_t i = 0; i < f.size(); ++i) {
        uint32_t a = f[(i + f.size() - 1) % f.size()], b = f[i],
                 c = f[(i + 1) % f.size()];
        if (cross2(a, b, c) * direction <= 1e-14)
          continue;
        bool inside = false;
        for (auto p : f)
          if (p != a && p != b && p != c &&
              cross2(a, b, p) * direction >= -1e-12 &&
              cross2(b, c, p) * direction >= -1e-12 &&
              cross2(c, a, p) * direction >= -1e-12) {
            inside = true;
            break;
          }
        if (!inside) {
          m.Indices.insert(m.Indices.end(), {a, b, c});
          f.erase(f.begin() + i);
          clipped = true;
          break;
        }
      }
      if (!clipped)
        throw std::runtime_error("Polygon is degenerate or self-intersecting");
    }
    if (f.size() == 3)
      m.Indices.insert(m.Indices.end(), f.begin(), f.end());
  }
  if (m.Indices.size() > MAX_EDIT_INDICES)
    throw std::runtime_error("Triangle budget exceeded");
  m.Normals.assign(m.Positions.size(), 0);
  for (size_t i = 0; i < m.Indices.size(); i += 3) {
    auto a = m.Indices[i], b = m.Indices[i + 1], c = m.Indices[i + 2];
    Vec3 n = Cross(Subtract(Point(m, b), Point(m, a)),
                   Subtract(Point(m, c), Point(m, a)));
    for (auto v : {a, b, c}) {
      m.Normals[v * 3] += n.x;
      m.Normals[v * 3 + 1] += n.y;
      m.Normals[v * 3 + 2] += n.z;
    }
  }
  for (size_t i = 0; i < m.Normals.size(); i += 3) {
    Vec3 n = Unit({m.Normals[i], m.Normals[i + 1], m.Normals[i + 2]});
    m.Normals[i] = n.x;
    m.Normals[i + 1] = n.y;
    m.Normals[i + 2] = n.z;
  }
}
inline StudioMesh FromObject(const GameObject &o) {
  StudioMesh m;
  ObjectMesh(o, m.Positions, m.Indices);
  m.Colors = o.VertexColors;
  m.UV = o.UV;
  m.Normals = o.Normals;
  m.Faces = o.Faces;
  m.FaceMaterials = o.FaceMaterials;
  m.Seams = o.Seams;
  if (m.Faces.empty())
    for (size_t i = 0; i < m.Indices.size(); i += 3)
      m.Faces.push_back({m.Indices[i], m.Indices[i + 1], m.Indices[i + 2]});
  return m;
}
inline void ToObject(GameObject &o, const StudioMesh &m) {
  Check(m);
  o.Vertices = m.Positions;
  o.Indices = m.Indices;
  o.VertexColors = m.Colors;
  o.UV = m.UV;
  o.Normals = m.Normals;
  o.Faces = m.Faces;
  o.FaceMaterials = m.FaceMaterials;
  o.Seams = m.Seams;
}
inline void Polygonize(StudioMesh &m) {
  // Remove coplanar triangle diagonals without merging material boundaries.
  bool changed = true;
  size_t pass = 0;
  while (changed && pass++ < 32) {
    changed = false;
    std::map<uint64_t, std::pair<size_t, size_t>> edges;
    for (size_t fi = 0; fi < m.Faces.size() && !changed; ++fi)
      for (size_t ei = 0; ei < m.Faces[fi].size(); ++ei) {
        auto &f = m.Faces[fi];
        uint32_t a = f[ei], b = f[(ei + 1) % f.size()];
        uint64_t key = Edge(a, b);
        auto it = edges.find(key);
        if (it == edges.end()) {
          edges[key] = {fi, ei};
          continue;
        }
        size_t gi = it->second.first, ge = it->second.second;
        auto &g = m.Faces[gi];
        if (g[ge] != b || g[(ge + 1) % g.size()] != a ||
            Dot(Normal(m, f), Normal(m, g)) < .999999)
          continue;
        if (!m.FaceMaterials.empty() &&
            m.FaceMaterials[fi] != m.FaceMaterials[gi])
          continue;
        std::vector<uint32_t> out;
        for (size_t j = 1; j < f.size(); ++j)
          out.push_back(f[(ei + j) % f.size()]);
        for (size_t j = 1; j < g.size(); ++j)
          out.push_back(g[(ge + j) % g.size()]);
        if (out.size() > 64)
          continue;
        m.Faces[gi] = out;
        m.Faces.erase(m.Faces.begin() + fi);
        if (!m.FaceMaterials.empty())
          m.FaceMaterials.erase(m.FaceMaterials.begin() + fi);
        changed = true;
        break;
      }
  }
  Triangulate(m);
}
inline uint32_t AppendVertex(StudioMesh &out, const StudioMesh &source,
                             uint32_t i, Vec3 p) {
  if (out.Positions.size() / 3 >= MAX_EDIT_VERTICES)
    throw std::runtime_error("Vertex budget exceeded");
  uint32_t n = uint32_t(out.Positions.size() / 3);
  out.Positions.insert(out.Positions.end(), {p.x, p.y, p.z});
  if (!source.UV.empty())
    out.UV.insert(out.UV.end(), {source.UV[i * 2], source.UV[i * 2 + 1]});
  if (!source.Colors.empty())
    out.Colors.insert(out.Colors.end(),
                      {source.Colors[i * 3], source.Colors[i * 3 + 1],
                       source.Colors[i * 3 + 2]});
  return n;
}
inline void Replicate(StudioMesh &m, const StudioModifier &mod) {
  StudioMesh result = m;
  int copies = mod.Type == StudioModifier::Mirror ? 2 : mod.Count;
  if (copies < 2 || copies > 64)
    throw std::runtime_error("Array count must be 2..64");
  if (m.Positions.size() / 3 * size_t(copies) > MAX_EDIT_VERTICES ||
      m.Indices.size() * size_t(copies) > MAX_EDIT_INDICES)
    throw std::runtime_error("Modifier exceeds mesh budget");
  for (int copy = 1; copy < copies; ++copy) {
    uint32_t offset = uint32_t(result.Positions.size() / 3);
    for (uint32_t i = 0; i < m.Positions.size() / 3; ++i) {
      Vec3 p = Point(m, i);
      if (mod.Type == StudioModifier::Mirror) {
        if (mod.Axis < 0 || mod.Axis > 2)
          throw std::runtime_error("Invalid mirror axis");
        if (mod.Axis == 0)
          p.x = -p.x;
        if (mod.Axis == 1)
          p.y = -p.y;
        if (mod.Axis == 2)
          p.z = -p.z;
      } else {
        p.x += mod.Offset[0] * copy;
        p.y += mod.Offset[1] * copy;
        p.z += mod.Offset[2] * copy;
      }
      AppendVertex(result, m, i, p);
    }
    for (size_t fi = 0; fi < m.Faces.size(); ++fi) {
      auto face = m.Faces[fi];
      for (auto &v : face)
        v += offset;
      if (mod.Type == StudioModifier::Mirror)
        std::reverse(face.begin(), face.end());
      result.Faces.push_back(face);
      if (!m.FaceMaterials.empty())
        result.FaceMaterials.push_back(m.FaceMaterials[fi]);
    }
  }
  m = std::move(result);
  Triangulate(m);
}
inline void Subdivide(StudioMesh &m, int levels) {
  if (levels < 1 || levels > 4)
    throw std::runtime_error("Subdivision level must be 1..4");
  for (int level = 0; level < levels; ++level) {
    Triangulate(m);
    if (m.Indices.size() > MAX_EDIT_INDICES / 4)
      throw std::runtime_error("Subdivision exceeds triangle budget");
    StudioMesh next = m;
    next.Faces.clear();
    next.FaceMaterials.clear();
    next.Seams.clear();
    std::map<uint64_t, std::vector<uint32_t>> opposite;
    std::vector<std::set<uint32_t>> neighbors(m.Positions.size() / 3);
    for (size_t i = 0; i < m.Indices.size(); i += 3)
      for (int e = 0; e < 3; ++e) {
        uint32_t a = m.Indices[i + e], b = m.Indices[i + (e + 1) % 3],
                 c = m.Indices[i + (e + 2) % 3];
        opposite[Edge(a, b)].push_back(c);
        neighbors[a].insert(b);
        neighbors[b].insert(a);
      }
    std::map<uint64_t, uint32_t> mids;
    for (const auto &edge : opposite) {
      uint32_t a = uint32_t(edge.first >> 32), b = uint32_t(edge.first);
      Vec3 p = Mul(Add(Point(m, a), Point(m, b)), .5f);
      if (edge.second.size() == 2)
        p = Add(Mul(Add(Point(m, a), Point(m, b)), .375f),
                Mul(Add(Point(m, edge.second[0]), Point(m, edge.second[1])),
                    .125f));
      auto v = AppendVertex(next, m, a, p);
      mids[edge.first] = v;
      if (!m.UV.empty())
        for (int k = 0; k < 2; ++k)
          next.UV[v * 2 + k] = (m.UV[a * 2 + k] + m.UV[b * 2 + k]) * .5f;
      if (!m.Colors.empty())
        for (int k = 0; k < 3; ++k)
          next.Colors[v * 3 + k] =
              (m.Colors[a * 3 + k] + m.Colors[b * 3 + k]) * .5f;
    }
    for (uint32_t v = 0; v < neighbors.size(); ++v) {
      std::vector<uint32_t> boundary;
      for (auto w : neighbors[v])
        if (opposite[Edge(v, w)].size() == 1)
          boundary.push_back(w);
      Vec3 p = Point(m, v);
      if (boundary.size() == 2)
        p = Add(Mul(p, .75f),
                Mul(Add(Point(m, boundary[0]), Point(m, boundary[1])), .125f));
      else if (boundary.empty() && neighbors[v].size() >= 3) {
        float beta = neighbors[v].size() == 3 ? 3.f / 16
                                              : 3.f / (8 * neighbors[v].size());
        p = Mul(p, 1 - beta * neighbors[v].size());
        for (auto w : neighbors[v])
          p = Add(p, Mul(Point(m, w), beta));
      }
      next.Positions[v * 3] = p.x;
      next.Positions[v * 3 + 1] = p.y;
      next.Positions[v * 3 + 2] = p.z;
    }
    for (size_t i = 0; i < m.Indices.size(); i += 3) {
      auto a = m.Indices[i], b = m.Indices[i + 1], c = m.Indices[i + 2],
           ab = mids[Edge(a, b)], bc = mids[Edge(b, c)], ca = mids[Edge(c, a)];
      next.Faces.insert(next.Faces.end(),
                        {{a, ab, ca}, {ab, b, bc}, {ca, bc, c}, {ab, bc, ca}});
    }
    m = std::move(next);
    Triangulate(m);
  }
}
inline void Bevel(StudioMesh &m, float width) {
  if (!std::isfinite(width) || width <= 0)
    throw std::runtime_error("Bevel width must be positive");
  Polygonize(m);
  StudioMesh out;
  std::map<uint64_t, std::array<uint32_t, 4>> sides;
  std::map<uint32_t, std::map<uint32_t, uint32_t>> caps;
  for (const auto &f : m.Faces) {
    std::vector<uint32_t> inset;
    for (size_t i = 0; i < f.size(); ++i) {
      Vec3 p = Point(m, f[i]),
           a = Subtract(Point(m, f[(i + f.size() - 1) % f.size()]), p),
           b = Subtract(Point(m, f[(i + 1) % f.size()]), p);
      float la = float(std::sqrt(Dot(a, a))), lb = float(std::sqrt(Dot(b, b)));
      if (la < 1e-8 || lb < 1e-8)
        throw std::runtime_error("Bevel cannot process degenerate edges");
      float w = std::min(width, std::min(la, lb) * .24f);
      inset.push_back(AppendVertex(
          out, m, f[i], Add(p, Mul(Add(Mul(a, 1 / la), Mul(b, 1 / lb)), w))));
    }
    out.Faces.push_back(inset);
    for (size_t i = 0; i < f.size(); ++i) {
      size_t j = (i + 1) % f.size();
      uint32_t a = f[i], b = f[j], ia = inset[i], ib = inset[j];
      uint64_t key = Edge(a, b);
      auto it = sides.find(key);
      if (it == sides.end())
        sides[key] = {{a, b, ia, ib}};
      else {
        auto s = it->second;
        if (s[0] != b || s[1] != a)
          throw std::runtime_error(
              "Bevel needs consistently oriented manifold mesh");
        out.Faces.push_back({ib, ia, s[3], s[2]});
        caps[a][s[3]] = ia;
        caps[b][ib] = s[2];
        sides.erase(it);
      }
    }
  }
  if (!sides.empty())
    throw std::runtime_error("Bevel currently requires a closed mesh");
  for (const auto &corner : caps) {
    auto next = corner.second;
    if (next.size() < 3)
      continue;
    std::vector<uint32_t> cap;
    uint32_t start = next.begin()->first, v = start;
    do {
      cap.push_back(v);
      auto it = next.find(v);
      if (it == next.end() || cap.size() > next.size())
        throw std::runtime_error("Invalid bevel vertex fan");
      v = it->second;
    } while (v != start);
    out.Faces.push_back(cap);
  }
  m = std::move(out);
  Check(m);
  Triangulate(m);
}
inline void Evaluate(GameObject &o) {
  StudioMesh m = o.Modifiers.empty() ? FromObject(o) : o.BaseMesh;
  Check(m);
  for (const auto &mod : o.Modifiers)
    if (mod.Enabled) {
      if (mod.Type == StudioModifier::Mirror ||
          mod.Type == StudioModifier::Array)
        Replicate(m, mod);
      else if (mod.Type == StudioModifier::Subdivision)
        Subdivide(m, mod.Levels);
      else
        Bevel(m, mod.Amount);
    }
  Triangulate(m);
  ToObject(o, m);
}
inline void AddModifier(GameObject &o, const StudioModifier &mod) {
  if (o.Modifiers.size() >= 16)
    throw std::runtime_error("At most 16 modifiers");
  if (o.Modifiers.empty())
    o.BaseMesh = FromObject(o);
  o.Modifiers.push_back(mod);
  Evaluate(o);
}
inline void ApplyModifiers(GameObject &o) {
  o.Modifiers.clear();
  o.BaseMesh = StudioMesh();
}
inline StudioMesh Editable(const GameObject &o) {
  return o.Modifiers.empty() ? FromObject(o) : o.BaseMesh;
}
inline void SetEditable(GameObject &o, StudioMesh m) {
  Check(m);
  Triangulate(m);
  if (o.Modifiers.empty())
    ToObject(o, m);
  else {
    o.BaseMesh = std::move(m);
    Evaluate(o);
  }
}
inline void Extrude(StudioMesh &m, const std::set<uint32_t> &selection,
                    float distance) {
  if (selection.empty() || !std::isfinite(distance) ||
      std::abs(distance) > 10000)
    throw std::runtime_error("Select faces and a finite extrude distance");
  StudioMesh out = m;
  std::map<uint64_t, int> counts;
  for (auto fi : selection) {
    if (fi >= m.Faces.size())
      throw std::runtime_error("Invalid face selection");
    const auto &f = m.Faces[fi];
    for (size_t j = 0; j < f.size(); ++j)
      ++counts[Edge(f[j], f[(j + 1) % f.size()])];
  }
  std::map<uint32_t, Vec3> normals;
  for (auto fi : selection) {
    Vec3 n = Normal(m, m.Faces[fi]);
    for (auto v : m.Faces[fi])
      normals[v] = Add(normals.count(v) ? normals[v] : Vec3{0, 0, 0}, n);
  }
  std::map<uint32_t, uint32_t> extruded;
  for (const auto &v : normals)
    extruded[v.first] = AppendVertex(
        out, m, v.first, Add(Point(m, v.first), Mul(Unit(v.second), distance)));
  for (auto fi : selection) {
    const auto &f = m.Faces[fi];
    for (size_t j = 0; j < f.size(); ++j) {
      uint32_t a = f[j], b = f[(j + 1) % f.size()];
      if (counts[Edge(a, b)] == 1)
        out.Faces.push_back({a, b, extruded[b], extruded[a]});
    }
    for (auto &v : out.Faces[fi])
      v = extruded[v];
  }
  out.FaceMaterials.clear();
  out.Seams.clear();
  Triangulate(out);
  m = std::move(out);
}
inline void DeleteElements(StudioMesh &m, const std::set<uint32_t> &selected,
                           int mode) {
  std::vector<std::vector<uint32_t>> faces;
  for (size_t i = 0; i < m.Faces.size(); ++i) {
    bool remove = mode == 2 && selected.count(uint32_t(i));
    if (mode != 2)
      for (auto v : m.Faces[i])
        remove = remove || selected.count(v);
    if (!remove)
      faces.push_back(m.Faces[i]);
  }
  if (faces.empty())
    throw std::runtime_error("Operation would remove every face");
  m.Faces = std::move(faces);
  m.FaceMaterials.clear();
  m.Seams.clear();
  Triangulate(m);
}
inline void Weld(StudioMesh &m, const std::set<uint32_t> &selected,
                 float tolerance) {
  if (selected.size() < 2 || !std::isfinite(tolerance) || tolerance <= 0)
    throw std::runtime_error(
        "Select at least two vertices and a positive weld distance");
  std::map<uint32_t, uint32_t> remap;
  for (auto v : selected) {
    if (v >= m.Positions.size() / 3)
      throw std::runtime_error("Invalid vertex selection");
    remap[v] = v;
    for (const auto &r : remap)
      if (r.first != v) {
        Vec3 d = Subtract(Point(m, v), Point(m, r.first));
        if (Dot(d, d) <= double(tolerance) * tolerance) {
          remap[v] = r.second;
          break;
        }
      }
  }
  std::vector<std::vector<uint32_t>> faces;
  for (auto f : m.Faces) {
    for (auto &v : f)
      if (remap.count(v))
        v = remap[v];
    std::vector<uint32_t> clean;
    for (auto v : f)
      if (clean.empty() || clean.back() != v)
        clean.push_back(v);
    if (clean.size() > 1 && clean.front() == clean.back())
      clean.pop_back();
    std::set<uint32_t> unique(clean.begin(), clean.end());
    if (unique.size() >= 3 && unique.size() == clean.size())
      faces.push_back(clean);
  }
  if (faces.empty())
    throw std::runtime_error("Weld would collapse the mesh");
  m.Faces = std::move(faces);
  m.FaceMaterials.clear();
  m.Seams.clear();
  Triangulate(m);
}
inline GameObject Boolean(const GameObject &a, const GameObject &b, int op) {
  StudioMesh ma = FromObject(a), mb = FromObject(b);
  for (size_t i = 0; i < ma.Positions.size(); i += 3) {
    Vec3 p;
    if (!TransformPoint(
            a, {ma.Positions[i], ma.Positions[i + 1], ma.Positions[i + 2]}, p))
      throw std::runtime_error("Invalid transform");
    ma.Positions[i] = p.x;
    ma.Positions[i + 1] = p.y;
    ma.Positions[i + 2] = p.z;
  }
  for (size_t i = 0; i < mb.Positions.size(); i += 3) {
    Vec3 p;
    if (!TransformPoint(
            b, {mb.Positions[i], mb.Positions[i + 1], mb.Positions[i + 2]}, p))
      throw std::runtime_error("Invalid transform");
    mb.Positions[i] = p.x;
    mb.Positions[i + 1] = p.y;
    mb.Positions[i + 2] = p.z;
  }
  StudioMesh result;
  std::string error;
  if (!StudioBooleanMesh(ma.Positions, ma.Indices, mb.Positions, mb.Indices, op,
                         result.Positions, result.Indices, error))
    throw std::runtime_error(error);
  for (size_t i = 0; i < result.Indices.size(); i += 3)
    result.Faces.push_back(
        {result.Indices[i], result.Indices[i + 1], result.Indices[i + 2]});
  Triangulate(result);
  GameObject out("Boolean Result");
  std::copy(a.Color, a.Color + 3, out.Color);
  out.Metallic = a.Metallic;
  out.Roughness = a.Roughness;
  ToObject(out, result);
  return out;
}
} // namespace Model
