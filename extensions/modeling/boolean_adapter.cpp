#include "boolean_adapter.h"
#include <exception>
#include <manifold/manifold.h>
#include <stdexcept>
bool StudioBooleanMesh(const std::vector<float> &a,
                       const std::vector<uint32_t> &ai,
                       const std::vector<float> &b,
                       const std::vector<uint32_t> &bi, int operation,
                       std::vector<float> &vertices,
                       std::vector<uint32_t> &indices, std::string &error) {
  try {
    if (operation < 0 || operation > 2)
      throw std::runtime_error("Unknown boolean operation");
    manifold::MeshGL ma, mb;
    ma.numProp = mb.numProp = 3;
    ma.vertProperties = a;
    ma.triVerts = ai;
    mb.vertProperties = b;
    mb.triVerts = bi;
    ma.Merge();
    mb.Merge();
    manifold::Manifold left(ma), right(mb);
    if (left.Status() != manifold::Manifold::Error::NoError ||
        right.Status() != manifold::Manifold::Error::NoError)
      throw std::runtime_error(
          "Boolean needs closed, consistently oriented manifold meshes");
    if (left.Volume() <= 1e-12 || right.Volume() <= 1e-12)
      throw std::runtime_error("Boolean needs nonzero-volume closed solids");
    auto result =
        left.Boolean(right, operation == 0   ? manifold::OpType::Add
                            : operation == 1 ? manifold::OpType::Subtract
                                             : manifold::OpType::Intersect);
    if (result.Status() != manifold::Manifold::Error::NoError)
      throw std::runtime_error("Boolean failed: invalid output topology");
    auto mesh = result.GetMeshGL();
    if (mesh.NumVert() > 200000 || mesh.NumTri() > 400000)
      throw std::runtime_error("Boolean exceeds mesh budget");
    if (mesh.triVerts.empty())
      throw std::runtime_error("Boolean result is empty");
    vertices.swap(mesh.vertProperties);
    indices.swap(mesh.triVerts);
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
