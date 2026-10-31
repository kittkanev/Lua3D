#pragma once
struct CachedStudioTexture {
  GLuint Id = 0;
  std::string Error;
};
static std::map<std::string, CachedStudioTexture> g_TextureCache;
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
  auto key = StudioTextureKey(material);
  auto found = g_TextureCache.find(key);
  if (found != g_TextureCache.end())
    return found->second.Id;
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
  g_TextureCache[key] = value;
  return value.Id;
}
static void ClearStudioTextures() {
  for (const auto &t : g_TextureCache)
    if (t.second.Id)
      glDeleteTextures(1, &t.second.Id);
  g_TextureCache.clear();
}
static void RenderLitMesh(const GameObject &object) {
  auto mesh = Model::FromObject(object);
  if (mesh.Normals.size() != mesh.Positions.size())
    Model::Triangulate(mesh);
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
  size_t triangle = 0;
  for (size_t face = 0; face < mesh.Faces.size(); ++face) {
    uint32_t slot = mesh.FaceMaterials.empty() ? 0 : mesh.FaceMaterials[face];
    StudioMaterial fallback;
    std::copy(object.Color, object.Color + 3, fallback.Color);
    fallback.Metallic = object.Metallic;
    fallback.Roughness = object.Roughness;
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
    for (size_t t = 0; t + 2 < mesh.Faces[face].size(); ++t, ++triangle)
      for (int corner = 0; corner < 3; ++corner) {
        uint32_t v = mesh.Indices[triangle * 3 + corner];
        float color[3];
        for (int k = 0; k < 3; ++k)
          color[k] = material.Color[k] *
                     (object.Materials.empty() ? 1 : object.Color[k]) *
                     (mesh.Colors.empty() ? 1 : mesh.Colors[v * 3 + k]);
        glColor3fv(color);
        glNormal3fv(mesh.Normals.data() + v * 3);
        if (!mesh.UV.empty())
          glTexCoord2f(mesh.UV[v * 2], mesh.UV[v * 2 + 1]);
        glVertex3fv(mesh.Positions.data() + v * 3);
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
