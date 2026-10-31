#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <map>
#include <algorithm>
#include <limits>

static const float PI = 3.14159265358979323846f;

enum class MeshType : int {
    Cube = 0,
    Pyramid = 1,
    Sphere = 2,
    Cylinder = 3,
    Plane = 4
};

static const char* GetMeshTypeName(MeshType type) {
    switch (type) {
        case MeshType::Cube:     return "Cube";
        case MeshType::Pyramid:  return "Pyramid";
        case MeshType::Sphere:   return "Sphere";
        case MeshType::Cylinder: return "Cylinder";
        case MeshType::Plane:    return "Plane";
        default:                 return "Cube";
    }
}

static MeshType ParseMeshType(const std::string& typeStr) {
    std::string lower = typeStr;
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    }
    if (lower == "pyramid")  return MeshType::Pyramid;
    if (lower == "sphere")   return MeshType::Sphere;
    if (lower == "cylinder") return MeshType::Cylinder;
    if (lower == "plane")    return MeshType::Plane;
    return MeshType::Cube;
}

struct ObjectProperty {
    enum Kind { Number, Boolean, Text } Type = Number;
    double NumberValue = 0.0;
    bool BoolValue = false;
    std::string TextValue;
};

// Enhanced GameObject structure (Rotation is stored in degrees for UI clarity and converted to radians on GLB export)
inline uint64_t NewObjectId() { static uint64_t next=0; return ++next; }

struct GameObject {
    uint64_t Id = NewObjectId();
    std::string Name;
    MeshType Type = MeshType::Cube;
    float Position[3] = {0.0f, 0.0f, 0.0f};
    float Rotation[3] = {0.0f, 0.0f, 0.0f}; // Degrees (X, Y, Z)
    float Scale[3]    = {1.0f, 1.0f, 1.0f};
    float Color[3]    = {0.80f, 0.45f, 0.20f};
    float Metallic    = 0.1f;
    float Roughness   = 0.5f;
    bool Visible      = true;
    bool Wireframe    = false;
    bool Selected = false;
    bool Dynamic = false;
    bool Grounded = false;
    bool Character = false;
    float Mass = 1.0f;
    float Restitution = 0.2f;
    float Friction = 4.0f;
    float Velocity[3] = {0, 0, 0};
    float MoveSpeed = 4.5f;
    float JumpSpeed = 6.0f;
    // Empty geometry means a procedural primitive. Custom meshes use 32-bit indices.
    std::vector<float> Vertices;
    std::vector<uint32_t> Indices;
    std::vector<float> VertexColors;
    std::map<std::string, ObjectProperty> Properties;

    GameObject(const std::string& name, MeshType type = MeshType::Cube)
        : Name(name), Type(type) {}
};

// Fixes Bug #2: converts UI degrees to radians before computing quaternion (intrinsic XYZ / Qx * Qy * Qz matching glRotatef order)
static std::vector<double> EulerDegreesToQuaternion(float rxDeg, float ryDeg, float rzDeg) {
    const double degToRad = PI / 180.0;
    double hx = (rxDeg * degToRad) * 0.5;
    double hy = (ryDeg * degToRad) * 0.5;
    double hz = (rzDeg * degToRad) * 0.5;

    double cx = std::cos(hx), sx = std::sin(hx);
    double cy = std::cos(hy), sy = std::sin(hy);
    double cz = std::cos(hz), sz = std::sin(hz);

    // Quaternion product Q = Qx * Qy * Qz (matches OpenGL glRotatef(rx,1,0,0) * glRotatef(ry,0,1,0) * glRotatef(rz,0,0,1))
    double qw = cx * cy * cz - sx * sy * sz;
    double qx = sx * cy * cz + cx * sy * sz;
    double qy = cx * sy * cz - sx * cy * sz;
    double qz = cx * cy * sz + sx * sy * cz;

    return {qx, qy, qz, qw};
}

// Generate vertex positions and triangle indices for any MeshType for GLB export
static void GeneratePrimitiveMeshData(MeshType type, std::vector<float>& outPositions, std::vector<uint16_t>& outIndices) {
    outPositions.clear();
    outIndices.clear();

    if (type == MeshType::Cube) {
        outPositions = {
            -0.5f, -0.5f,  0.5f, // 0: Front-Bottom-Left
             0.5f, -0.5f,  0.5f, // 1: Front-Bottom-Right
             0.5f,  0.5f,  0.5f, // 2: Front-Top-Right
            -0.5f,  0.5f,  0.5f, // 3: Front-Top-Left
            -0.5f, -0.5f, -0.5f, // 4: Back-Bottom-Left
            -0.5f,  0.5f, -0.5f, // 5: Back-Top-Left
             0.5f,  0.5f, -0.5f, // 6: Back-Top-Right
             0.5f, -0.5f, -0.5f  // 7: Back-Bottom-Right
        };
        outIndices = {
            0, 1, 2,  0, 2, 3, // Front  (+Z)
            7, 4, 5,  7, 5, 6, // Back   (-Z)
            3, 2, 6,  3, 6, 5, // Top    (+Y)
            4, 0, 3,  4, 3, 5, // Left   (-X)
            1, 7, 6,  1, 6, 2, // Right  (+X)
            4, 7, 1,  4, 1, 0  // Bottom (-Y)
        };
    } else if (type == MeshType::Pyramid) {
        outPositions = {
             0.0f,  0.5f,  0.0f, // 0: Apex
            -0.5f, -0.5f,  0.5f, // 1: Front-Left
             0.5f, -0.5f,  0.5f, // 2: Front-Right
             0.5f, -0.5f, -0.5f, // 3: Back-Right
            -0.5f, -0.5f, -0.5f  // 4: Back-Left
        };
        outIndices = {
            0, 1, 2, // Front
            0, 2, 3, // Right
            0, 3, 4, // Back
            0, 4, 1, // Left
            4, 3, 2,  4, 2, 1 // Base
        };
    } else if (type == MeshType::Plane) {
        outPositions = {
            -0.5f, 0.0f,  0.5f,
             0.5f, 0.0f,  0.5f,
             0.5f, 0.0f, -0.5f,
            -0.5f, 0.0f, -0.5f
        };
        outIndices = { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 };
    } else if (type == MeshType::Sphere) {
        const int rings = 12;
        const int sectors = 16;
        const float radius = 0.5f;
        for (int r = 0; r <= rings; ++r) {
            float theta = (float)r / rings * PI;
            for (int s = 0; s <= sectors; ++s) {
                float phi = (float)s / sectors * 2.0f * PI;
                outPositions.push_back(std::sin(theta) * std::cos(phi) * radius);
                outPositions.push_back(std::cos(theta) * radius);
                outPositions.push_back(std::sin(theta) * std::sin(phi) * radius);
            }
        }
        for (int r = 0; r < rings; ++r) {
            for (int s = 0; s < sectors; ++s) {
                uint16_t cur = (uint16_t)(r * (sectors + 1) + s);
                uint16_t next = (uint16_t)((r + 1) * (sectors + 1) + s);
                outIndices.push_back(cur);
                outIndices.push_back((uint16_t)(cur + 1));
                outIndices.push_back(next);

                outIndices.push_back((uint16_t)(cur + 1));
                outIndices.push_back((uint16_t)(next + 1));
                outIndices.push_back(next);
            }
        }
    } else if (type == MeshType::Cylinder) {
        const int segments = 16;
        const float radius = 0.5f;
        const float halfH = 0.5f;
        outPositions.insert(outPositions.end(), {0.0f, halfH, 0.0f, 0.0f, -halfH, 0.0f});
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)i / segments * 2.0f * PI;
            float x = std::cos(angle) * radius;
            float z = std::sin(angle) * radius;
            outPositions.insert(outPositions.end(), {x, halfH, z});
            outPositions.insert(outPositions.end(), {x, -halfH, z});
        }
        for (int i = 0; i < segments; ++i) {
            uint16_t t0 = (uint16_t)(2 + i * 2);
            uint16_t b0 = (uint16_t)(t0 + 1);
            uint16_t t1 = (uint16_t)(t0 + 2);
            uint16_t b1 = (uint16_t)(t0 + 3);
            outIndices.insert(outIndices.end(), {t0, t1, b0, t1, b1, b0});
            outIndices.insert(outIndices.end(), {0, t1, t0});
            outIndices.insert(outIndices.end(), {1, b0, b1});
        }
    }
}


// Copy before growing the vector: callers may reference an element of objects.
static int DuplicateObject(std::vector<GameObject>& objects, int index, const std::string& name) {
    if (index < 0 || static_cast<size_t>(index) >= objects.size() || name.size()>255) return -1;
    GameObject copy = objects[index];
    copy.Name = name;
    copy.Selected = false;
    copy.Id = NewObjectId();
    objects.push_back(copy);
    return static_cast<int>(objects.size()) - 1;
}

// Strict decimal seconds; malformed or zero values disable the timeout.
// Clamp below the SDL 32-bit tick wrap period.
static uint64_t ParseTestTimeoutMs(const char* value) {
    if (!value || !*value) return 0;
    const uint64_t limit = 3600000;
    uint64_t seconds = 0;
    for (const char* p = value; *p; ++p) {
        if (*p < '0' || *p > '9') return 0;
        seconds = seconds > limit / 10 ? limit : seconds * 10 + (*p - '0');
        if (seconds > limit) seconds = limit;
    }
    return seconds * 1000;
}
