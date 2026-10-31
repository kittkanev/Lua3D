#pragma once
struct CachedStudioTexture {
  GLuint Id = 0;
  std::string Error;
};
static std::map<std::string, CachedStudioTexture> g_TextureCache;
static uint32_t g_TextureGeneration = 1;
static std::string StudioTextureKey(const StudioMaterial &m) {
  if (m.Image.empty())
    return m.Texture;
  uint64_t h = 1469598103934665603ULL;
  for (auto b : m.Image)
    h = (h ^ b) * 1099511628211ULL;
  return "embedded:" + std::to_string(h);
}
static GLuint StudioTexture(const StudioMaterial &material) {
  if (material.Texture.empty() && material.Image.empty())
    return 0;
  if (material.TextureGeneration == g_TextureGeneration)
    return material.CachedTexture;
  auto key = StudioTextureKey(material);
  auto found = g_TextureCache.find(key);
  if (found != g_TextureCache.end()) {
    material.CachedTexture = found->second.Id;
    material.TextureGeneration = g_TextureGeneration;
    return found->second.Id;
  }
  CachedStudioTexture value;
  try {
    auto image = DecodeMaterialImage(material);
    glGenTextures(1, &value.Id);
    glBindTexture(GL_TEXTURE_2D, value.Id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.image.data());
    glBindTexture(GL_TEXTURE_2D, 0);
  } catch (const std::exception &e) {
    value.Error = e.what();
    AppendConsole(value.Error + "\n");
  }
  material.CachedTexture = value.Id;
  material.TextureGeneration = g_TextureGeneration;
  g_TextureCache[key] = value;
  return value.Id;
}
static void ClearStudioTextures() {
  for (const auto &t : g_TextureCache)
    if (t.second.Id)
      glDeleteTextures(1, &t.second.Id);
  g_TextureCache.clear();
  ++g_TextureGeneration;
}
static void RenderLitMesh(const GameObject &object) {
  static StudioMesh primitives[5];
  const StudioMesh *primitive = nullptr;
  if (object.Vertices.empty()) {
    auto &mesh = primitives[int(object.Type)];
    if (mesh.Positions.empty()) {
      GameObject shape("Primitive", object.Type);
      mesh = Model::FromObject(shape);
      Model::Triangulate(mesh);
    }
    primitive = &mesh;
  }
  const auto &positions = primitive ? primitive->Positions : object.Vertices;
  const auto &indices = primitive ? primitive->Indices : object.Indices;
  const auto &normals = primitive ? primitive->Normals : object.Normals;
  const auto &uv = primitive ? primitive->UV : object.UV;
  const auto &colors = primitive ? primitive->Colors : object.VertexColors;
  const auto &faces = primitive ? primitive->Faces : object.Faces;
  const auto &materials =
      primitive ? primitive->FaceMaterials : object.FaceMaterials;
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_NORMALIZE);
  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  GLfloat light[] = {-.4f, .8f, 1, 0}, ambient[] = {.25f, .25f, .3f, 1},
          diffuse[] = {.85f, .85f, .85f, 1};
  glLightfv(GL_LIGHT0, GL_POSITION, light);
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  StudioMaterial fallback;
  std::copy(object.Color, object.Color + 3, fallback.Color);
  fallback.Metallic = object.Metallic;
  fallback.Roughness = object.Roughness;
  size_t triangle = 0,
         faceCount = faces.empty() ? indices.size() / 3 : faces.size();
  for (size_t face = 0; face < faceCount; ++face) {
    uint32_t slot = materials.empty() ? 0 : materials[face];
    const auto &material =
        slot < object.Materials.size() ? object.Materials[slot] : fallback;
    GLuint texture = StudioTexture(material);
    if (texture) {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, texture);
    } else
      glDisable(GL_TEXTURE_2D);
    GLfloat specular[] = {.04f + material.Metallic * .7f,
                          .04f + material.Metallic * .7f,
                          .04f + material.Metallic * .7f, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS,
                std::max(1.f, (1 - material.Roughness) * 100));
    glBegin(GL_TRIANGLES);
    size_t triangles = faces.empty() ? 1 : faces[face].size() - 2;
    for (size_t t = 0; t < triangles; ++t, ++triangle) {
      Vec3 normal = {0, 1, 0};
      if (normals.size() != positions.size()) {
        auto a = indices[triangle * 3] * 3, b = indices[triangle * 3 + 1] * 3,
             c = indices[triangle * 3 + 2] * 3;
        normal = Model::Unit(Cross(
            {positions[b] - positions[a], positions[b + 1] - positions[a + 1],
             positions[b + 2] - positions[a + 2]},
            {positions[c] - positions[a], positions[c + 1] - positions[a + 1],
             positions[c + 2] - positions[a + 2]}));
      }
      for (int corner = 0; corner < 3; ++corner) {
        uint32_t v = indices[triangle * 3 + corner];
        float color[3];
        for (int k = 0; k < 3; ++k)
          color[k] = material.Color[k] *
                     (object.Materials.empty() ? 1 : object.Color[k]) *
                     (colors.empty() ? 1 : colors[v * 3 + k]);
        glColor3fv(color);
        if (normals.size() == positions.size())
          glNormal3fv(normals.data() + v * 3);
        else
          glNormal3f(normal.x, normal.y, normal.z);
        if (!uv.empty())
          glTexCoord2f(uv[v * 2], uv[v * 2 + 1]);
        glVertex3fv(positions.data() + v * 3);
      }
    }
    glEnd();
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_NORMALIZE);
  glDisable(GL_LIGHT0);
  glDisable(GL_LIGHTING);
}
