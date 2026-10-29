#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cassert>
#include <cmath>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

struct GameObject {
    std::string Name;
    float Position[3] = {0.0f, 0.0f, 0.0f};
    float Rotation[3] = {0.0f, 0.0f, 0.0f};
    float Scale[3] = {1.0f, 1.0f, 1.0f};

    GameObject(const std::string& name) : Name(name) {}
};

std::vector<GameObject> g_Objects;

static bool ExportGLB(const std::string& filename) {
    tinygltf::Model model;
    tinygltf::Scene scene;

    // Add nodes for all game objects
    for (size_t i = 0; i < g_Objects.size(); i++) {
        const auto& obj = g_Objects[i];
        tinygltf::Node node;
        node.name = obj.Name;

        node.translation.push_back(obj.Position[0]);
        node.translation.push_back(obj.Position[1]);
        node.translation.push_back(obj.Position[2]);

        node.scale.push_back(obj.Scale[0]);
        node.scale.push_back(obj.Scale[1]);
        node.scale.push_back(obj.Scale[2]);

        // Convert euler to quaternion for tinygltf
        // Very basic conversion (Y-up standard assumed, order XYZ for simplicity)
        float cr = cos(obj.Rotation[0] * 0.5f);
        float sr = sin(obj.Rotation[0] * 0.5f);
        float cp = cos(obj.Rotation[1] * 0.5f);
        float sp = sin(obj.Rotation[1] * 0.5f);
        float cy = cos(obj.Rotation[2] * 0.5f);
        float sy = sin(obj.Rotation[2] * 0.5f);

        node.rotation.push_back(sr * cp * cy - cr * sp * sy); // x
        node.rotation.push_back(cr * sp * cy + sr * cp * sy); // y
        node.rotation.push_back(cr * cp * sy - sr * sp * cy); // z
        node.rotation.push_back(cr * cp * cy + sr * sp * sy); // w

        model.nodes.push_back(node);
        scene.nodes.push_back(i); // add root nodes
    }

    model.scenes.push_back(scene);
    model.defaultScene = 0;

    // Asset info
    tinygltf::Asset asset;
    asset.version = "2.0";
    asset.generator = "3D Lua Engine Editor";
    model.asset = asset;

    tinygltf::TinyGLTF gltf;
    // Write out the glTF as a .glb (binary file)
    bool ret = gltf.WriteGltfSceneToFile(&model, filename, true, true, true, false);
    return ret;
}

void test_ExportGLB_CreatesFile() {
    std::string testFilename = "test_output.glb";

    // Clean up before test
    std::remove(testFilename.c_str());

    // Setup dummy data
    g_Objects.clear();
    g_Objects.emplace_back("TestCube");
    g_Objects.back().Position[0] = 1.0f;
    g_Objects.back().Position[1] = 2.0f;
    g_Objects.back().Position[2] = 3.0f;

    // Act
    bool success = ExportGLB(testFilename);

    // Assert
    assert(success == true);

    // Check if file exists
    std::ifstream file(testFilename);
    assert(file.good() == true);
    file.close();

    std::cout << "test_ExportGLB_CreatesFile passed." << std::endl;

    // Cleanup
    std::remove(testFilename.c_str());
}

int main() {
    test_ExportGLB_CreatesFile();
    std::cout << "All tests passed." << std::endl;
    return 0;
}
