#pragma once
static std::set<uint32_t> g_UVSelection;
static uint64_t g_UVObject = 0;
static bool g_ReportUV=false;
static void RenderMaterialTools(GameObject &object) {
  if (ImGui::CollapsingHeader("Materials / Texture")) {
    static int slot = 0;
    static char path[512] = "";
    if (ImGui::Button("Add material slot") && object.Materials.size() < 64) {
      object.Materials.push_back(StudioMaterial());
      object.Color[0] = object.Color[1] = object.Color[2] = 1;
      slot = int(object.Materials.size()) - 1;
    }
    if (!object.Materials.empty()) {
      slot = std::max(0, std::min(slot, int(object.Materials.size()) - 1));
      ImGui::SliderInt("Material slot", &slot, 0,
                       int(object.Materials.size()) - 1);
      auto &material = object.Materials[size_t(slot)];
      ImGui::ColorEdit3("Material color", material.Color);
      ImGui::SliderFloat("Material metallic", &material.Metallic, 0, 1);
      ImGui::SliderFloat("Material roughness", &material.Roughness, 0, 1);
      ImGui::InputText("PNG / JPEG path", path, sizeof(path));
      if (ImGui::Button("Load texture") && path[0]) {
        try {
          StudioMaterial candidate = material;
          candidate.Texture = AbsoluteAssetPath(path);
          candidate.Image.clear();
          DecodeMaterialImage(candidate);
          material = std::move(candidate);
          ClearStudioTextures();
        } catch (const std::exception &e) {
          AppendConsole(std::string(e.what()) + "\n");
        }
      }
      ImGui::TextWrapped("%s", material.Image.empty() ? material.Texture.c_str()
                                                      : "Embedded GLB texture");
      if (!material.Texture.empty() && !AssetExists(material.Texture))
        ImGui::TextColored(ImVec4(1, .5f, .3f, 1),
                           "Missing texture: choose replacement above");
      if (ImGui::Button("Assign to selected faces") && g_Modeling.Mode == 2 &&
          !g_Modeling.Selected.empty()) {
        try {
          auto mesh = Model::Editable(object);
          if (mesh.FaceMaterials.empty())
            mesh.FaceMaterials.assign(mesh.Faces.size(), 0);
          for (auto face : g_Modeling.Selected)
            if (face < mesh.Faces.size())
              mesh.FaceMaterials[face] = uint32_t(slot);
          Model::SetEditable(object, std::move(mesh));
        } catch (const std::exception &e) {
          AppendConsole(e.what());
        }
      }
      if (ImGui::Button("Remove texture")) {
        material.Texture.clear();
        material.Image.clear();
      }
    }
    if (ImGui::Button("Open UV editor"))
      g_ShowUV = true;
  }
}
static void RenderUVEditor() {
  if (!g_ShowUV)
    return;
  if(g_ReportUV)ImGui::SetNextWindowPos(ImVec2(735,85),ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(610, 650), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("UV Editor", &g_ShowUV)) {
    int index = g_SelectedObjectIndex;
    if (index < 0 || size_t(index) >= g_Objects.size()) {
      ImGui::TextUnformatted("Select a mesh in Explorer.");
      ImGui::End();
      return;
    }
    auto &object = g_Objects[size_t(index)];
    if (g_UVObject != object.Id) {
      g_UVObject = object.Id;
      g_UVSelection.clear();
    }
    ImGui::BeginDisabled(g_ModelBusy || g_PhysicsRunning ||
                         !object.Modifiers.empty());
    if (!object.Modifiers.empty())
      ImGui::TextUnformatted("Apply modifiers before editing UVs.");
    if (ImGui::Button("Auto unwrap and pack"))
      QueueSelectedMesh([](GameObject &o) {
        auto m = Model::Editable(o);
        Model::Unwrap(m);
        Model::SetEditable(o, std::move(m));
      });
    ImGui::SameLine();
    if (ImGui::Button("Select all UVs"))
      for (uint32_t v = 0; v < object.UV.size() / 2; ++v)
        g_UVSelection.insert(v);
    if (ImGui::Button("Mark selected edges as seams") && g_Modeling.Mode == 1) {
      auto mesh = Model::Editable(object);
      for (auto edge : g_Modeling.Selected)
        if (edge < g_Modeling.Edges.size()) {
          auto e = g_Modeling.Edges[edge];
          mesh.Seams.push_back(Model::Edge(e.first, e.second));
        }
      Model::SetEditable(object, std::move(mesh));
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear seams")) {
      object.Seams.clear();
    }
    static float shift[2] = {0, 0}, angle = 0, scale = 1;
    ImGui::DragFloat2("UV offset", shift, .005f);
    ImGui::DragFloat("UV rotation", &angle, .5f);
    ImGui::DragFloat("UV scale", &scale, .01f, .001f, 100);
    if (ImGui::Button("Transform selected island")) {
      try {
        auto mesh = Model::Editable(object);
        Model::TransformUV(mesh, g_UVSelection, shift[0], shift[1], angle,
                           scale);
        Model::SetEditable(object, std::move(mesh));
      } catch (const std::exception &e) {
        AppendConsole(std::string(e.what()) + "\n");
      }
    }
    ImGui::TextWrapped(
        "Click an island to select it; drag to move. Ctrl adds an island. Mark "
        "seams using edge selection in Mesh Edit.");
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float size = std::max(100.f, std::min(ImGui::GetContentRegionAvail().x,
                                          ImGui::GetContentRegionAvail().y));
    auto *draw = ImGui::GetWindowDrawList();
    int grid = 16;
    for (int y = 0; y < grid; ++y)
      for (int x = 0; x < grid; ++x)
        draw->AddRectFilled(
            ImVec2(origin.x + x * size / grid, origin.y + y * size / grid),
            ImVec2(origin.x + (x + 1) * size / grid,
                   origin.y + (y + 1) * size / grid),
            (x + y) % 2 ? IM_COL32(35, 45, 58, 255)
                        : IM_COL32(50, 62, 76, 255));
    if (!object.Materials.empty()) {
      GLuint texture = StudioTexture(object.Materials[0]);
      if (texture)
        draw->AddImage(ImTextureID(texture), origin,
                       ImVec2(origin.x + size, origin.y + size));
    }
    ImGui::InvisibleButton("UV canvas", ImVec2(size, size));
    bool hover = ImGui::IsItemHovered();
    auto mouse = ImGui::GetIO().MousePos;
    auto screen = [&](uint32_t v) {
      return ImVec2(origin.x + object.UV[v * 2] * size,
                    origin.y + (1 - object.UV[v * 2 + 1]) * size);
    };
    if (object.UV.size() == object.Vertices.size() / 3 * 2 &&
        !object.UV.empty())
      for (size_t t = 0; t < object.Indices.size(); t += 3) {
        uint32_t a = object.Indices[t], b = object.Indices[t + 1],
                 c = object.Indices[t + 2];
        ImVec2 p = screen(a), q = screen(b), r = screen(c);
        ImU32 color = g_UVSelection.count(a) ? IM_COL32(255, 190, 70, 255)
                                             : IM_COL32(70, 225, 235, 255);
        draw->AddTriangle(p, q, r, color, 1.3f);
        if (hover && ImGui::IsMouseClicked(0)) {
          auto cross = [](ImVec2 u, ImVec2 v, ImVec2 w) {
            return (v.x - u.x) * (w.y - u.y) - (v.y - u.y) * (w.x - u.x);
          };
          float x = cross(p, q, mouse), y = cross(q, r, mouse),
                z = cross(r, p, mouse);
          if ((x >= 0 && y >= 0 && z >= 0) || (x <= 0 && y <= 0 && z <= 0)) {
            if (!ImGui::GetIO().KeyCtrl)
              g_UVSelection.clear();
            auto island = Model::UVIsland(Model::FromObject(object), a);
            g_UVSelection.insert(island.begin(), island.end());
          }
        }
      }
    if (hover && ImGui::IsMouseDragging(0) && !g_UVSelection.empty() &&
        !g_PhysicsRunning && object.Modifiers.empty()) {
      auto delta = ImGui::GetIO().MouseDelta;
      for (auto v : g_UVSelection)
        if (v < object.UV.size() / 2) {
          object.UV[v * 2] += delta.x / size;
          object.UV[v * 2 + 1] -= delta.y / size;
        }
    }
    ImGui::EndDisabled();
  }
  ImGui::End();
}

static void ImportStudioGLB(const std::string &path) {
  QueueModelJob([path](std::vector<GameObject> &objects) {
    std::string report;
    auto imported = ReadGLBObjects(path, report);
    g_ModelWorkerReport = report;
    if (objects.size() + imported.size() > 10000)
      throw std::runtime_error("Scene object budget exceeded");
    objects.insert(objects.end(), imported.begin(), imported.end());
  });
}
