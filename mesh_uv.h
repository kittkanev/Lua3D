#pragma once
#include "extensions/xatlas/xatlas.h"
namespace Model {
inline void Unwrap(StudioMesh &mesh) {
  Check(mesh);
  Triangulate(mesh);
  std::unique_ptr<xatlas::Atlas, void (*)(xatlas::Atlas *)> atlas(
      xatlas::Create(), xatlas::Destroy);
  if (!atlas)
    throw std::runtime_error("Cannot allocate UV atlas");
  StudioMesh input = mesh;
  std::vector<float> seamNormals;
  // Split vertex fans at explicit seams. Normal discontinuities carry these
  // cuts through xatlas's colocal-vertex matching without changing the surface.
  if (!mesh.Seams.empty()) {
    std::set<uint64_t> seams(mesh.Seams.begin(), mesh.Seams.end());
    std::vector<std::vector<uint32_t>> incident(mesh.Positions.size() / 3);
    for (uint32_t f = 0; f < mesh.Faces.size(); ++f)
      for (auto v : mesh.Faces[f])
        incident[v].push_back(f);
    std::map<uint64_t, std::vector<uint32_t>> edgeFaces;
    for (uint32_t f = 0; f < mesh.Faces.size(); ++f)
      for (size_t j = 0; j < mesh.Faces[f].size(); ++j)
        edgeFaces[Edge(mesh.Faces[f][j],
                       mesh.Faces[f][(j + 1) % mesh.Faces[f].size()])]
            .push_back(f);
    input = StudioMesh();
    input.Faces = mesh.Faces;
    input.FaceMaterials = mesh.FaceMaterials;
    for (uint32_t v = 0; v < incident.size(); ++v) {
      std::set<uint32_t> left(incident[v].begin(), incident[v].end());
      int sector = 0;
      while (!left.empty()) {
        std::vector<uint32_t> pending = {*left.begin()}, component;
        left.erase(pending[0]);
        while (!pending.empty()) {
          uint32_t f = pending.back();
          pending.pop_back();
          component.push_back(f);
          const auto &face = mesh.Faces[f];
          for (size_t j = 0; j < face.size(); ++j) {
            auto a = face[j], b = face[(j + 1) % face.size()];
            if (a != v && b != v)
              continue;
            uint64_t edge = Edge(a, b);
            if (seams.count(edge))
              continue;
            for (auto next : edgeFaces[edge])
              if (left.erase(next))
                pending.push_back(next);
          }
        }
        uint32_t vertex = AppendVertex(input, mesh, v, Point(mesh, v));
        float angle = float(sector++) * .25f;
        seamNormals.insert(seamNormals.end(),
                           {std::sin(angle), 0, std::cos(angle)});
        for (auto f : component)
          for (size_t j = 0; j < mesh.Faces[f].size(); ++j)
            if (mesh.Faces[f][j] == v)
              input.Faces[f][j] = vertex;
      }
    }
    Triangulate(input);
  }
  xatlas::MeshDecl decl;
  decl.vertexPositionData = input.Positions.data();
  decl.vertexCount = uint32_t(input.Positions.size() / 3);
  decl.vertexPositionStride = 12;
  decl.indexData = input.Indices.data();
  decl.indexCount = uint32_t(input.Indices.size());
  decl.indexFormat = xatlas::IndexFormat::UInt32;
  if (!seamNormals.empty()) {
    decl.vertexNormalData = seamNormals.data();
    decl.vertexNormalStride = 12;
  }
  auto status = xatlas::AddMesh(atlas.get(), decl);
  if (status != xatlas::AddMeshError::Success)
    throw std::runtime_error(std::string("UV input rejected: ") +
                             xatlas::StringForEnum(status));
  xatlas::ChartOptions charts;
  charts.normalSeamWeight = 1001;
  xatlas::PackOptions packing;
  packing.resolution = 1024;
  packing.padding = 4;
  xatlas::Generate(atlas.get(), charts, packing);
  if (!atlas->meshCount || !atlas->width || !atlas->height)
    throw std::runtime_error("UV atlas is empty");
  const auto &result = atlas->meshes[0];
  if (result.vertexCount > MAX_EDIT_VERTICES)
    throw std::runtime_error("UV seams exceed vertex budget");
  StudioMesh output;
  for (uint32_t i = 0; i < result.vertexCount; ++i) {
    const auto &v = result.vertexArray[i];
    AppendVertex(output, input, v.xref, Point(input, v.xref));
    output.UV.push_back(v.uv[0] / atlas->width);
    output.UV.push_back(v.uv[1] / atlas->height);
  }
  // AppendVertex copied old UVs when present; unwrap always replaces them.
  output.UV.clear();
  for (uint32_t i = 0; i < result.vertexCount; ++i) {
    output.UV.push_back(result.vertexArray[i].uv[0] / atlas->width);
    output.UV.push_back(result.vertexArray[i].uv[1] / atlas->height);
  }
  output.Indices.assign(result.indexArray,
                        result.indexArray + result.indexCount);
  for (size_t i = 0; i < output.Indices.size(); i += 3)
    output.Faces.push_back(
        {output.Indices[i], output.Indices[i + 1], output.Indices[i + 2]});
  if (!input.FaceMaterials.empty()) {
    size_t triangle = 0;
    for (size_t f = 0; f < input.Faces.size(); ++f)
      for (size_t t = 0; t + 2 < input.Faces[f].size(); ++t) {
        (void)triangle;
        output.FaceMaterials.push_back(input.FaceMaterials[f]);
        ++triangle;
      }
  }
  Check(output);
  Triangulate(output);
  mesh = std::move(output);
}
inline std::set<uint32_t> UVIsland(const StudioMesh &m, uint32_t start) {
  std::vector<std::vector<uint32_t>> adjacent(m.Positions.size() / 3);
  for (const auto &f : m.Faces)
    for (size_t j = 0; j < f.size(); ++j) {
      auto a = f[j], b = f[(j + 1) % f.size()];
      adjacent[a].push_back(b);
      adjacent[b].push_back(a);
    }
  std::set<uint32_t> found;
  std::vector<uint32_t> pending = {start};
  while (!pending.empty()) {
    auto v = pending.back();
    pending.pop_back();
    if (v >= adjacent.size() || !found.insert(v).second)
      continue;
    pending.insert(pending.end(), adjacent[v].begin(), adjacent[v].end());
  }
  return found;
}
inline void TransformUV(StudioMesh &m, const std::set<uint32_t> &selected,
                        float x, float y, float angle, float scale) {
  if (m.UV.size() != m.Positions.size() / 3 * 2 || selected.empty() ||
      !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(angle) ||
      !std::isfinite(scale) || scale <= 0 || scale > 1000)
    throw std::runtime_error("Invalid UV transform");
  double cx = 0, cy = 0;
  for (auto v : selected) {
    if (v >= m.UV.size() / 2)
      throw std::runtime_error("Invalid UV selection");
    cx += m.UV[v * 2];
    cy += m.UV[v * 2 + 1];
  }
  cx /= selected.size();
  cy /= selected.size();
  double a = angle * PI / 180;
  for (auto v : selected) {
    double u = (m.UV[v * 2] - cx) * scale, w = (m.UV[v * 2 + 1] - cy) * scale;
    m.UV[v * 2] = float(cx + u * std::cos(a) - w * std::sin(a) + x);
    m.UV[v * 2 + 1] = float(cy + u * std::sin(a) + w * std::cos(a) + y);
  }
}
} // namespace Model
