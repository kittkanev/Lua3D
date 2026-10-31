#pragma once
inline std::vector<unsigned char>
MaterialImageBytes(const StudioMaterial &material) {
  return material.Image.empty()
             ? ReadAssetFile(material.Texture, 16 * 1024 * 1024)
             : material.Image;
}
inline tinygltf::Image DecodeMaterialImage(const StudioMaterial &material) {
  auto bytes = MaterialImageBytes(material);
  int w = 0, h = 0, n = 0;
  if (bytes.empty() ||
      !stbi_info_from_memory(bytes.data(), int(bytes.size()), &w, &h, &n) ||
      w <= 0 || h <= 0 || w > 4096 || h > 4096)
    throw std::runtime_error(
        "Texture must be a PNG/JPEG image up to 4096 x 4096");
  stbi_uc *pixels =
      stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &n, 4);
  if (!pixels)
    throw std::runtime_error("Cannot decode texture");
  std::unique_ptr<stbi_uc, void (*)(void *)> owner(pixels, stbi_image_free);
  tinygltf::Image image;
  image.mimeType = "image/png";
  image.name = "Texture";
  image.width = w;
  image.height = h;
  image.component = 4;
  image.bits = 8;
  image.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
  image.image.assign(pixels, pixels + size_t(w) * h * 4);
  return image;
}
inline bool BoundedImageLoad(tinygltf::Image *image, int index,
                             std::string *error, std::string *warning,
                             int width, int height, const unsigned char *bytes,
                             int length, void *data) {
  int w = 0, h = 0, c = 0;
  if (length <= 0 || !stbi_info_from_memory(bytes, length, &w, &h, &c) ||
      w < 1 || h < 1 || w > 4096 || h > 4096) {
    if (error)
      *error += "Image dimensions exceed import budget";
    return false;
  }
  return tinygltf::LoadImageData(image, index, error, warning, width, height,
                                 bytes, length, data);
}
inline std::vector<unsigned char> EncodeImage(const tinygltf::Image &image) {
  std::vector<unsigned char> data;
  if (image.bits != 8 || image.component < 1 || image.component > 4 ||
      image.width < 1 || image.height < 1)
    throw std::runtime_error("Only 8-bit GLB textures are supported");
  auto write = [](void *context, void *bytes, int length) {
    auto &out = *static_cast<std::vector<unsigned char> *>(context);
    auto p = static_cast<unsigned char *>(bytes);
    out.insert(out.end(), p, p + length);
  };
  if (!stbi_write_png_to_func(write, &data, image.width, image.height,
                              image.component, image.image.data(),
                              image.width * image.component))
    throw std::runtime_error("Cannot encode imported image");
  return data;
}
inline std::vector<double> ReadGLBAccessor(const tinygltf::Model &model,
                                           int accessorIndex, int expected) {
  if (accessorIndex < 0 || size_t(accessorIndex) >= model.accessors.size())
    throw std::runtime_error("Missing GLB accessor");
  const auto &a = model.accessors[size_t(accessorIndex)];
  if (a.sparse.isSparse || a.bufferView < 0 ||
      size_t(a.bufferView) >= model.bufferViews.size() ||
      a.count > MAX_EDIT_INDICES)
    throw std::runtime_error("Unsupported sparse or oversized GLB accessor");
  int components = tinygltf::GetNumComponentsInType(a.type),
      bytes = tinygltf::GetComponentSizeInBytes(a.componentType);
  if (components != expected || bytes < 1)
    throw std::runtime_error("Invalid GLB accessor format");
  const auto &view = model.bufferViews[size_t(a.bufferView)];
  if (view.buffer < 0 || size_t(view.buffer) >= model.buffers.size())
    throw std::runtime_error("Invalid GLB buffer");
  const auto &buffer = model.buffers[size_t(view.buffer)].data;
  size_t element = size_t(bytes) * components,
         stride = view.byteStride ? view.byteStride : element;
  if (stride < element || view.byteOffset > buffer.size() ||
      view.byteLength > buffer.size() - view.byteOffset ||
      a.byteOffset > view.byteLength ||
      (a.count &&
       ((a.count - 1) > (view.byteLength - a.byteOffset) / stride ||
        element > view.byteLength - a.byteOffset - (a.count - 1) * stride)))
    throw std::runtime_error("GLB accessor exceeds buffer");
  std::vector<double> out;
  out.reserve(a.count * components);
  for (size_t i = 0; i < a.count; ++i)
    for (int c = 0; c < components; ++c) {
      const unsigned char *p = buffer.data() + view.byteOffset + a.byteOffset +
                               i * stride + size_t(c) * bytes;
      double value = 0;
      switch (a.componentType) {
      case TINYGLTF_COMPONENT_TYPE_FLOAT: {
        float v;
        std::memcpy(&v, p, 4);
        value = v;
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        uint32_t v;
        std::memcpy(&v, p, 4);
        value = v;
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        uint16_t v;
        std::memcpy(&v, p, 2);
        value = a.normalized ? v / 65535.0 : v;
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        value = a.normalized ? *p / 255.0 : *p;
        break;
      default:
        throw std::runtime_error("Unsupported GLB component type");
      }
      if (!std::isfinite(value) || std::abs(value) > 1e8)
        throw std::runtime_error("Nonfinite GLB value");
      out.push_back(value);
    }
  return out;
}
inline StudioMatrix GLBNodeMatrix(const tinygltf::Node &node) {
  auto matrix = StudioIdentity();
  if (node.matrix.size() == 16) {
    for (int i = 0; i < 16; ++i) {
      if (!std::isfinite(node.matrix[i]) || std::abs(node.matrix[i]) > 1e8)
        throw std::runtime_error("Invalid node matrix");
      matrix.M[i] = float(node.matrix[i]);
    }
    return matrix;
  }
  double x = 0, y = 0, z = 0, w = 1;
  if (node.rotation.size() == 4) {
    x = node.rotation[0];
    y = node.rotation[1];
    z = node.rotation[2];
    w = node.rotation[3];
    double norm = std::sqrt(x * x + y * y + z * z + w * w);
    if (norm < 1e-8 || !std::isfinite(norm))
      throw std::runtime_error("Invalid quaternion");
    x /= norm;
    y /= norm;
    z /= norm;
    w /= norm;
  }
  matrix.M[0] = float(1 - 2 * y * y - 2 * z * z);
  matrix.M[1] = float(2 * x * y + 2 * w * z);
  matrix.M[2] = float(2 * x * z - 2 * w * y);
  matrix.M[4] = float(2 * x * y - 2 * w * z);
  matrix.M[5] = float(1 - 2 * x * x - 2 * z * z);
  matrix.M[6] = float(2 * y * z + 2 * w * x);
  matrix.M[8] = float(2 * x * z + 2 * w * y);
  matrix.M[9] = float(2 * y * z - 2 * w * x);
  matrix.M[10] = float(1 - 2 * x * x - 2 * y * y);
  if (node.scale.size() == 3)
    for (int c = 0; c < 3; ++c)
      for (int r = 0; r < 3; ++r)
        matrix.M[c * 4 + r] *= float(node.scale[c]);
  if (node.translation.size() == 3)
    for (int i = 0; i < 3; ++i)
      matrix.M[12 + i] = float(node.translation[i]);
  for (float v : matrix.M)
    if (!std::isfinite(v))
      throw std::runtime_error("Invalid node transform");
  return matrix;
}
static std::vector<GameObject> ReadGLBObjects(const std::string &path,
                                              std::string &report) {
  auto bytes = ReadAssetFile(path, 64 * 1024 * 1024);
  tinygltf::TinyGLTF reader;
  tinygltf::Model model;
  std::string error, warning;
  tinygltf::FsCallbacks fs;
  fs.FileExists = [](const std::string &p, void *) { return AssetExists(p); };
  fs.ExpandFilePath = [](const std::string &p, void *) { return p; };
  fs.ReadWholeFile = [](std::vector<unsigned char> *out, std::string *err,
                        const std::string &p, void *) {
    try {
      *out = ReadAssetFile(p, 64 * 1024 * 1024);
      return true;
    } catch (const std::exception &e) {
      if (err)
        *err = e.what();
      return false;
    }
  };
  fs.WriteWholeFile = nullptr;
  fs.GetFileSizeInBytes = nullptr;
  fs.user_data = nullptr;
  reader.SetFsCallbacks(fs);
  reader.SetImageLoader(BoundedImageLoad, nullptr);
  if (!reader.LoadBinaryFromMemory(&model, &error, &warning, bytes.data(),
                                   unsigned(bytes.size()),
                                   PathDirectory(AbsoluteAssetPath(path))))
    throw std::runtime_error("GLB import: " + error);
  for (const auto &extension : model.extensionsRequired)
    throw std::runtime_error("Required GLB extension is unsupported: " +
                             extension);
  report = warning;
  for (const auto &extension : model.extensionsUsed)
    report += "Ignored extension: " + extension + "\n";
  if (!model.animations.empty())
    report += "Animation tracks ignored; static poses imported.\n";
  if (!model.skins.empty())
    report += "Skinning ignored; static mesh imported.\n";
  std::vector<StudioMaterial> materials;
  for (const auto &source : model.materials) {
    if (materials.size() >= 63)
      throw std::runtime_error("GLB material budget exceeded");
    StudioMaterial m;
    m.Name = source.name;
    for (int k = 0; k < 3; ++k) {
      double value = source.pbrMetallicRoughness.baseColorFactor[k];
      if (!std::isfinite(value))
        throw std::runtime_error("Invalid material");
      m.Color[k] = float(std::max(0.0, std::min(1.0, value)));
    }
    m.Metallic = float(source.pbrMetallicRoughness.metallicFactor);
    m.Roughness = float(source.pbrMetallicRoughness.roughnessFactor);
    int texture = source.pbrMetallicRoughness.baseColorTexture.index;
    if (texture >= 0) {
      if (size_t(texture) >= model.textures.size())
        throw std::runtime_error("Invalid GLB texture");
      int image = model.textures[size_t(texture)].source;
      if (image < 0 || size_t(image) >= model.images.size())
        throw std::runtime_error("Invalid GLB image");
      m.Image = EncodeImage(model.images[size_t(image)]);
    }
    materials.push_back(std::move(m));
  }
  materials.push_back(StudioMaterial());
  std::vector<GameObject> objects;
  std::set<int> visited;
  size_t vertices = 0;
  std::function<void(int, const StudioMatrix &, int)> visit =
      [&](int index, const StudioMatrix &parent, int depth) {
        if (depth > 128 || index < 0 || size_t(index) >= model.nodes.size() ||
            !visited.insert(index).second)
          throw std::runtime_error("Invalid GLB node hierarchy");
        const auto &node = model.nodes[size_t(index)];
        auto transform = StudioMultiply(parent, GLBNodeMatrix(node));
        if (node.mesh >= 0) {
          if (size_t(node.mesh) >= model.meshes.size() ||
              objects.size() >= 10000)
            throw std::runtime_error("Invalid GLB mesh reference");
          GameObject object(node.name.empty() ? "Imported Mesh" : node.name);
          object.Materials = materials;
          object.Color[0] = object.Color[1] = object.Color[2] = 1;
          StudioMesh mesh;
          std::vector<float> authoredNormals;
          bool preserveNormals = true;
          for (const auto &primitive :
               model.meshes[size_t(node.mesh)].primitives) {
            if (primitive.mode != -1 &&
                primitive.mode != TINYGLTF_MODE_TRIANGLES) {
              report += "Skipped a non-triangle primitive.\n";
              continue;
            }
            auto position = primitive.attributes.find("POSITION");
            if (position == primitive.attributes.end())
              throw std::runtime_error("GLB primitive has no positions");
            auto positions = ReadGLBAccessor(model, position->second, 3);
            size_t count = positions.size() / 3;
            if (count + mesh.Positions.size() / 3 > MAX_EDIT_VERTICES)
              throw std::runtime_error("GLB vertex budget exceeded");
            uint32_t offset = uint32_t(mesh.Positions.size() / 3);
            for (size_t v = 0; v < count; ++v) {
              double x = positions[v * 3], y = positions[v * 3 + 1],
                     z = positions[v * 3 + 2];
              mesh.Positions.push_back(
                  float(transform.M[0] * x + transform.M[4] * y +
                        transform.M[8] * z + transform.M[12]));
              mesh.Positions.push_back(
                  float(transform.M[1] * x + transform.M[5] * y +
                        transform.M[9] * z + transform.M[13]));
              mesh.Positions.push_back(
                  float(transform.M[2] * x + transform.M[6] * y +
                        transform.M[10] * z + transform.M[14]));
            }
            auto normalIt = primitive.attributes.find("NORMAL");
            if (normalIt == primitive.attributes.end())
              preserveNormals = false;
            else {
              auto normals = ReadGLBAccessor(model, normalIt->second, 3);
              if (normals.size() != count * 3)
                throw std::runtime_error("GLB normal count mismatch");
              auto inverse = StudioInverse(transform);
              for (size_t v = 0; v < count; ++v) {
                Vec3 n =
                    Model::Unit({float(inverse.M[0] * normals[v * 3] +
                                       inverse.M[1] * normals[v * 3 + 1] +
                                       inverse.M[2] * normals[v * 3 + 2]),
                                 float(inverse.M[4] * normals[v * 3] +
                                       inverse.M[5] * normals[v * 3 + 1] +
                                       inverse.M[6] * normals[v * 3 + 2]),
                                 float(inverse.M[8] * normals[v * 3] +
                                       inverse.M[9] * normals[v * 3 + 1] +
                                       inverse.M[10] * normals[v * 3 + 2])});
                authoredNormals.insert(authoredNormals.end(), {n.x, n.y, n.z});
              }
            }
            std::vector<double> colors;
            int colorComponents = 3;
            auto colorIt = primitive.attributes.find("COLOR_0");
            if (colorIt != primitive.attributes.end()) {
              if (colorIt->second < 0 ||
                  size_t(colorIt->second) >= model.accessors.size())
                throw std::runtime_error("Invalid color accessor");
              colorComponents = tinygltf::GetNumComponentsInType(
                  model.accessors[size_t(colorIt->second)].type);
              if (colorComponents != 3 && colorComponents != 4)
                throw std::runtime_error("Invalid color format");
              colors = ReadGLBAccessor(model, colorIt->second, colorComponents);
              if (colors.size() != count * colorComponents)
                throw std::runtime_error("GLB color count mismatch");
            }
            for (size_t v = 0; v < count; ++v)
              for (int k = 0; k < 3; ++k)
                mesh.Colors.push_back(
                    colors.empty()
                        ? 1.f
                        : float(std::max(
                              0.0,
                              std::min(1.0, colors[v * colorComponents + k]))));
            std::vector<double> uv;
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            if (uvIt != primitive.attributes.end())
              uv = ReadGLBAccessor(model, uvIt->second, 2);
            if (!uv.empty() && uv.size() != count * 2)
              throw std::runtime_error("GLB UV count mismatch");
            for (size_t v = 0; v < count * 2; ++v)
              mesh.UV.push_back(uv.empty() ? 0 : float(uv[v]));
            std::vector<double> indices;
            if (primitive.indices >= 0)
              indices = ReadGLBAccessor(model, primitive.indices, 1);
            else
              for (size_t v = 0; v < count; ++v)
                indices.push_back(double(v));
            if (indices.size() % 3)
              throw std::runtime_error("GLB triangle count mismatch");
            uint32_t material = primitive.material < 0
                                    ? uint32_t(materials.size() - 1)
                                    : uint32_t(primitive.material);
            if (material >= materials.size())
              throw std::runtime_error("Invalid material reference");
            for (size_t t = 0; t < indices.size(); t += 3) {
              std::vector<uint32_t> f;
              for (int k = 0; k < 3; ++k) {
                double value = indices[t + k];
                if (value < 0 || value >= count || std::floor(value) != value)
                  throw std::runtime_error("GLB index outside vertices");
                f.push_back(offset + uint32_t(value));
              }
              double determinant =
                  transform.M[0] * (transform.M[5] * transform.M[10] -
                                    transform.M[6] * transform.M[9]) -
                  transform.M[4] * (transform.M[1] * transform.M[10] -
                                    transform.M[2] * transform.M[9]) +
                  transform.M[8] * (transform.M[1] * transform.M[6] -
                                    transform.M[2] * transform.M[5]);
              if (determinant < 0)
                std::swap(f[1], f[2]);
              mesh.Faces.push_back(f);
              mesh.FaceMaterials.push_back(material);
            }
            if (!primitive.targets.empty())
              report += "Morph targets ignored; base mesh imported.\n";
          }
          if (!mesh.Faces.empty()) {
            Model::Check(mesh);
            Model::Triangulate(mesh);
            if (preserveNormals &&
                authoredNormals.size() == mesh.Positions.size())
              mesh.Normals = std::move(authoredNormals);
            Model::ToObject(object, mesh);
            vertices += mesh.Positions.size() / 3;
            if (vertices > 1000000)
              throw std::runtime_error("GLB scene budget exceeded");
            objects.push_back(std::move(object));
          }
        }
        for (auto child : node.children)
          visit(child, transform, depth + 1);
      };
  if (model.scenes.empty())
    throw std::runtime_error("GLB has no scene");
  int scene = model.defaultScene >= 0 ? model.defaultScene : 0;
  if (size_t(scene) >= model.scenes.size())
    throw std::runtime_error("Invalid GLB scene");
  for (auto node : model.scenes[size_t(scene)].nodes)
    visit(node, StudioIdentity(), 0);
  if (objects.empty())
    throw std::runtime_error("GLB has no supported static meshes");
  report += "Imported " + std::to_string(objects.size()) + " static meshes.\n";
  return objects;
}
