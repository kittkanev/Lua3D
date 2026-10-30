#include "../scene_core.h"
#include <cassert>
#include <iostream>

int main() {
    assert(ParseTestTimeoutMs(nullptr) == 0);
    assert(ParseTestTimeoutMs("") == 0);
    assert(ParseTestTimeoutMs("0") == 0);
    assert(ParseTestTimeoutMs("2") == 2000);
    assert(ParseTestTimeoutMs("-1") == 0);
    assert(ParseTestTimeoutMs("2junk") == 0);
    assert(ParseTestTimeoutMs(" 2") == 0);
    assert(ParseTestTimeoutMs("999999999999999999999999") == 3600000000ULL);
    assert(ParseMeshType("SPHERE") == MeshType::Sphere);
    assert(std::string(GetMeshTypeName(MeshType::Cylinder)) == "Cylinder");
    for (int t = 0; t < 5; ++t) {
        std::vector<float> p;
        std::vector<uint16_t> ix;
        GeneratePrimitiveMeshData(static_cast<MeshType>(t), p, ix);
        assert(!p.empty() && p.size() % 3 == 0 && ix.size() % 3 == 0);
        for (auto i : ix) assert(i < p.size() / 3);
        for (size_t i = 0; i < ix.size(); i += 3) {
            double a[3], b[3], c[3], u[3], v[3];
            for (int k = 0; k < 3; ++k) {
                a[k] = p[ix[i]*3+k]; b[k] = p[ix[i+1]*3+k]; c[k] = p[ix[i+2]*3+k];
                u[k] = b[k]-a[k]; v[k] = c[k]-a[k];
            }
            double n[3] = {u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]};
            double dot = 0;
            for (int k = 0; k < 3; ++k) dot += n[k]*(a[k]+b[k]+c[k])/3;
            // Plane has both sides; sphere poles include zero-area triangles.
            if (t != 4) assert(dot >= -1e-7);
        }
    }
    auto q = EulerDegreesToQuaternion(90, 0, 0);
    assert(std::abs(q[0]-std::sqrt(0.5)) < 1e-6);
    assert(std::abs(q[3]-std::sqrt(0.5)) < 1e-6);
    q = EulerDegreesToQuaternion(30, 60, 90);
    double norm = 0; for (double x : q) norm += x*x;
    assert(std::abs(norm-1) < 1e-6);
    std::vector<GameObject> objects;
    objects.emplace_back("source", MeshType::Sphere);
    objects[0].Position[0] = 12; objects[0].Metallic = 0.8f;
    assert(DuplicateObject(objects, -1, "bad") == -1);
    assert(DuplicateObject(objects, 1, "bad") == -1);
    assert(DuplicateObject(objects, 0, "copy") == 1);
    assert(objects[1].Name == "copy" && objects[1].Position[0] == 12);
    assert(objects[1].Type == MeshType::Sphere && objects[1].Metallic == 0.8f);
    objects[1].Position[0] = 99; assert(objects[0].Position[0] == 12);
    std::cout << "PASS: five mesh types, bounds, winding, quaternion, duplication\n";
}
