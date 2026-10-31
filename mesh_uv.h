#pragma once
#include "extensions/xatlas/xatlas.h"
namespace Model {
inline void Unwrap(StudioMesh &mesh) {
  Check(mesh);
  auto authoredNormals = mesh.Normals;
  Triangulate(mesh);
  if (authoredNormals.size() == mesh.Positions.size())
    mesh.Normals = authoredNormals;
  std::unique_ptr<xatlas::Atlas, void (*)(xatlas::Atlas *)> atlas(
      xatlas::Create(), xatlas::Destroy);
  if (!atlas)
    throw std::runtime_error("Cannot allocate UV atlas");
  std::map<std::array<float, 3>, uint32_t> canonical;
  std::vector<uint32_t> vertexMap(mesh.Positions.size() / 3);
  for (uint32_t i = 0; i < vertexMap.size(); ++i) {
    Vec3 p = Point(mesh, i);
    std::array<float, 3> key = {{p.x, p.y, p.z}};
    auto found = canonical.find(key);
    if (found == canonical.end())
      found = canonical.emplace(key, uint32_t(canonical.size())).first;
    vertexMap[i] = found->second;
  }
  std::set<uint64_t> seams;
  for (auto e : mesh.Seams) {
    uint32_t a = uint32_t(e >> 32), b = uint32_t(e);
    if (a < vertexMap.size() && b < vertexMap.size())
      seams.insert(Edge(vertexMap[a], vertexMap[b]));
  }
  std::map<uint64_t, std::vector<uint32_t>> edgeFaces;
  for (uint32_t i = 0; i < mesh.Faces.size(); ++i) {
    const auto &f = mesh.Faces[i];
    for (size_t j = 0; j < f.size(); ++j)
      edgeFaces[Edge(vertexMap[f[j]], vertexMap[f[(j + 1) % f.size()]])]
          .push_back(i);
  }
  std::vector<std::set<uint32_t>> constraints(mesh.Faces.size());
  for (auto seam : seams) {
    const auto &adjacent = edgeFaces[seam];
    for (auto a : adjacent)
      for (auto b : adjacent)
        if (a != b)
          constraints[a].insert(b);
  }
  std::vector<int> colors(mesh.Faces.size(), -1);
  uint32_t colorCount = 1;
  for (uint32_t i = 0; i < colors.size(); ++i) {
    std::set<int> unavailable;
    for (auto other : constraints[i])
      if (colors[other] >= 0)
        unavailable.insert(colors[other]);
    int color = 0;
    while (unavailable.count(color))
      ++color;
    colors[i] = color;
    colorCount = std::max(colorCount, uint32_t(color + 1));
  }
  std::vector<uint32_t> chartGroups, triangleMaterials;
  for (size_t face = 0; face < mesh.Faces.size(); ++face) {
    uint32_t material =
        mesh.FaceMaterials.empty() ? 0 : mesh.FaceMaterials[face];
    for (size_t t = 0; t + 2 < mesh.Faces[face].size(); ++t) {
      chartGroups.push_back(material * colorCount + uint32_t(colors[face]));
      triangleMaterials.push_back(material);
    }
  }
  xatlas::MeshDecl decl;
  decl.vertexPositionData = mesh.Positions.data();
  decl.vertexCount = uint32_t(mesh.Positions.size() / 3);
  decl.vertexPositionStride = 12;
  decl.indexData = mesh.Indices.data();
  decl.indexCount = uint32_t(mesh.Indices.size());
  decl.indexFormat = xatlas::IndexFormat::UInt32;
  decl.vertexNormalData = mesh.Normals.data();
  decl.vertexNormalStride = 12;
  decl.faceMaterialData = chartGroups.data();
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
  if (!atlas->meshCount || !atlas->width || !atlas->height ||
      atlas->atlasCount > 1)
    throw std::runtime_error("UV mesh exceeds a single atlas budget");
  const auto &result = atlas->meshes[0];
  if (result.vertexCount > MAX_EDIT_VERTICES)
    throw std::runtime_error("UV seams exceed vertex budget");
  StudioMesh output;
  for (uint32_t i = 0; i < result.vertexCount; ++i) {
    auto source = result.vertexArray[i].xref;
    AppendVertex(output, mesh, source, Point(mesh, source));
    output.Normals.insert(output.Normals.end(),
                          mesh.Normals.begin() + source * 3,
                          mesh.Normals.begin() + source * 3 + 3);
  }
  output.UV.clear();
  for (uint32_t i = 0; i < result.vertexCount; ++i) {
    output.UV.push_back(result.vertexArray[i].uv[0] / atlas->width);
    output.UV.push_back(result.vertexArray[i].uv[1] / atlas->height);
  }
  output.Indices.assign(result.indexArray,
                        result.indexArray + result.indexCount);
  for (size_t i = 0; i < output.Indices.size(); i += 3) {
    output.Faces.push_back(
        {output.Indices[i], output.Indices[i + 1], output.Indices[i + 2]});
    if (!mesh.FaceMaterials.empty())
      output.FaceMaterials.push_back(triangleMaterials[i / 3]);
    for (int j = 0; j < 3; ++j) {
      auto a = output.Indices[i + j], b = output.Indices[i + (j + 1) % 3];
      auto sa = result.vertexArray[a].xref, sb = result.vertexArray[b].xref;
      if (seams.count(Edge(vertexMap[sa], vertexMap[sb])))
        output.Seams.push_back(Edge(a, b));
    }
  }
  Check(output);
  auto normals = std::move(output.Normals);
  Triangulate(output);
  output.Normals = std::move(normals);
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
