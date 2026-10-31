#pragma once
#include <chrono>
#include <functional>
#include <future>
struct ModelingState {
  bool Edit = false;
  int Mode = 0;
  uint64_t Object = 0;
  std::set<uint32_t> Selected;
  std::vector<std::pair<uint32_t, uint32_t>> Edges;
};
static ModelingState g_Modeling;
struct ModelingResult {
  std::vector<GameObject> Objects;
  std::string Error, Report;
};
static thread_local std::string g_ModelWorkerReport;
static std::future<ModelingResult> g_ModelJob;
struct ModifierDraft {
  bool Active = false;
  uint64_t Object = 0;
  size_t Index = 0;
  StudioModifier Value;
};
static ModifierDraft g_ModifierDraft;
static bool g_ModelBusy = false;
static uint64_t g_ModelInputHash = 0;
static void
QueueModelJob(const std::function<void(std::vector<GameObject> &)> &operation) {
  if (g_ModelBusy || g_PhysicsRunning)
    return;
  g_ObjectHistory.Capture(g_Objects);
  g_ModelInputHash = StudioSceneFingerprint(g_Objects);
  auto snapshot = g_Objects;
  g_ModelBusy = true;
  g_ModelJob = std::async(std::launch::async, [snapshot, operation]() mutable {
    ModelingResult r;
    try {
      g_ModelWorkerReport.clear();
      operation(snapshot);
      r.Report = g_ModelWorkerReport;
      r.Objects = std::move(snapshot);
    } catch (const std::exception &e) {
      r.Error = e.what();
    }
    return r;
  });
}
static void PollModelJob() {
  if (!g_ModelBusy ||
      g_ModelJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return;
  auto result = g_ModelJob.get();
  g_ModelBusy = false;
  if (!result.Error.empty()) {
    AppendConsole("Modeling: " + result.Error + "\n");
    return;
  }
  if (StudioSceneFingerprint(g_Objects) != g_ModelInputHash) {
    AppendConsole("Modeling result discarded because the scene changed.\n");
    return;
  }
  if (!result.Report.empty())
    AppendConsole(result.Report);
  g_Objects = std::move(result.Objects);
  g_SelectedObjectIndex = -1;
  for (size_t i = 0; i < g_Objects.size(); ++i)
    if (g_Objects[i].Selected)
      g_SelectedObjectIndex = int(i);
  g_Modeling.Selected.clear();
  g_ObjectHistory.Capture(g_Objects);
  AppendConsole("Modeling operation completed.\n");
}
static void
QueueSelectedMesh(const std::function<void(GameObject &)> &operation) {
  int i = g_SelectedObjectIndex;
  if (i < 0 || size_t(i) >= g_Objects.size())
    return;
  uint64_t id = g_Objects[size_t(i)].Id;
  QueueModelJob([id, operation](std::vector<GameObject> &objects) {
    for (auto &o : objects)
      if (o.Id == id) {
        operation(o);
        return;
      }
    throw std::runtime_error("Selected object no longer exists");
  });
}
static std::set<uint32_t> SelectedModelVertices(const StudioMesh &m) {
  std::set<uint32_t> vertices;
  if (g_Modeling.Mode == 0)
    return g_Modeling.Selected;
  if (g_Modeling.Mode == 1) {
    for (auto edge : g_Modeling.Selected)
      if (edge < g_Modeling.Edges.size()) {
        vertices.insert(g_Modeling.Edges[edge].first);
        vertices.insert(g_Modeling.Edges[edge].second);
      }
  } else
    for (auto face : g_Modeling.Selected)
      if (face < m.Faces.size())
        vertices.insert(m.Faces[face].begin(), m.Faces[face].end());
  return vertices;
}
static void RenderModelTools() {
  if (ImGui::CollapsingHeader("Mesh Edit / Modifiers",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextWrapped("Modeling uses the selected mesh. Heavy operations run "
                       "in the background.");
    ImGui::BeginDisabled(g_ModelBusy || g_PhysicsRunning ||
                         g_SelectedObjectIndex < 0);
    if (ImGui::Checkbox("Edit mesh", &g_Modeling.Edit)) {
      g_Modeling.Selected.clear();
      if (g_Modeling.Edit)
        QueueSelectedMesh([](GameObject &o) {
          auto m = Model::Editable(o);
          Model::Polygonize(m);
          Model::SetEditable(o, std::move(m));
        });
    }
    if (g_Modeling.Edit) {
      const char *modes[] = {"Vertices", "Edges", "Faces"};
      if (ImGui::Combo("Selection", &g_Modeling.Mode, modes, 3))
        g_Modeling.Selected.clear();
      ImGui::Text("Selected: %d", int(g_Modeling.Selected.size()));
      if (ImGui::Button("Select all")) {
        if (g_SelectedObjectIndex >= 0 &&
            size_t(g_SelectedObjectIndex) < g_Objects.size()) {
          auto m = Model::Editable(g_Objects[size_t(g_SelectedObjectIndex)]);
          size_t n = g_Modeling.Mode == 0   ? m.Positions.size() / 3
                     : g_Modeling.Mode == 1 ? g_Modeling.Edges.size()
                                            : m.Faces.size();
          for (uint32_t v = 0; v < n; ++v)
            g_Modeling.Selected.insert(v);
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Deselect"))
        g_Modeling.Selected.clear();
      static float offset[3] = {0, .1f, 0}, extrude = .25f, weld = .001f;
      ImGui::DragFloat3("Element offset", offset, .01f);
      if (ImGui::Button("Move elements")) {
        auto m = Model::Editable(g_Objects[size_t(g_SelectedObjectIndex)]);
        auto vertices = SelectedModelVertices(m);
        std::array<float, 3> delta = {{offset[0], offset[1], offset[2]}};
        QueueSelectedMesh([vertices, delta](GameObject &o) {
          auto data = Model::Editable(o);
          for (auto v : vertices)
            if (v < data.Positions.size() / 3)
              for (int k = 0; k < 3; ++k)
                data.Positions[v * 3 + k] += delta[k];
          Model::SetEditable(o, std::move(data));
        });
      }
      ImGui::DragFloat("Extrude distance", &extrude, .01f, -100, 100);
      auto selected = g_Modeling.Selected;
      ImGui::BeginDisabled(g_Modeling.Mode != 2);
      if (ImGui::Button("Extrude faces"))
        QueueSelectedMesh([selected](GameObject &o) {
          auto m = Model::Editable(o);
          Model::Extrude(m, selected, extrude);
          Model::SetEditable(o, std::move(m));
        });
      ImGui::EndDisabled();
      if (ImGui::Button("Delete elements")) {
        int mode = g_Modeling.Mode;
        auto m = Model::Editable(g_Objects[size_t(g_SelectedObjectIndex)]);
        auto vertices = SelectedModelVertices(m);
        std::set<uint64_t> edges;
        if (mode == 1)
          for (auto selectedEdge : selected)
            if (selectedEdge < g_Modeling.Edges.size()) {
              auto e = g_Modeling.Edges[selectedEdge];
              edges.insert(Model::Edge(e.first, e.second));
            }
        QueueSelectedMesh([selected, vertices, edges, mode](GameObject &o) {
          auto data = Model::Editable(o);
          if (mode == 1)
            Model::DeleteEdges(data, edges);
          else
            Model::DeleteElements(data, mode == 2 ? selected : vertices, mode);
          Model::SetEditable(o, std::move(data));
        });
      }
      ImGui::DragFloat("Weld distance", &weld, .0001f, .00001f, 10, "%.5f");
      if (ImGui::Button("Merge vertices by distance")) {
        auto vertices = SelectedModelVertices(
            Model::Editable(g_Objects[size_t(g_SelectedObjectIndex)]));
        QueueSelectedMesh([vertices](GameObject &o) {
          auto m = Model::Editable(o);
          Model::Weld(m, vertices, weld);
          Model::SetEditable(o, std::move(m));
        });
      }
      if (ImGui::Button("Recalculate normals"))
        QueueSelectedMesh([](GameObject &o) {
          auto m = Model::Editable(o);
          Model::Triangulate(m);
          Model::SetEditable(o, std::move(m));
        });
    }
    const char *names[] = {"Mirror", "Array", "Subdivision", "Bevel"};
    static int kind = 0;
    ImGui::Combo("Add modifier", &kind, names, 4);
    if (ImGui::Button("Add to stack")) {
      StudioModifier modifier;
      modifier.Type = StudioModifier::Kind(kind);
      QueueSelectedMesh(
          [modifier](GameObject &o) { Model::AddModifier(o, modifier); });
    }
    if (g_SelectedObjectIndex >= 0 &&
        size_t(g_SelectedObjectIndex) < g_Objects.size()) {
      auto &o = g_Objects[size_t(g_SelectedObjectIndex)];
      for (size_t i = 0; i < o.Modifiers.size(); ++i) {
        ImGui::PushID(int(i));
        auto mod = o.Modifiers[i];
        if (g_ModifierDraft.Active && g_ModifierDraft.Object == o.Id &&
            g_ModifierDraft.Index == i)
          mod = g_ModifierDraft.Value;
        bool changed = ImGui::Checkbox(names[mod.Type], &mod.Enabled);
        if (mod.Type == StudioModifier::Mirror) {
          const char *axes[] = {"X", "Y", "Z"};
          changed = ImGui::Combo("Axis", &mod.Axis, axes, 3) || changed;
        }
        if (mod.Type == StudioModifier::Array) {
          changed = ImGui::SliderInt("Count", &mod.Count, 2, 64) || changed;
          changed = ImGui::DragFloat3("Offset", mod.Offset, .05f) || changed;
        }
        if (mod.Type == StudioModifier::Subdivision)
          changed = ImGui::SliderInt("Levels", &mod.Levels, 1, 4) || changed;
        if (mod.Type == StudioModifier::Bevel)
          changed = ImGui::DragFloat("Width", &mod.Amount, .01f, .0001f, 100) ||
                    changed;
        if (changed) {
          g_ModifierDraft.Active = true;
          g_ModifierDraft.Object = o.Id;
          g_ModifierDraft.Index = i;
          g_ModifierDraft.Value = mod;
        }
        if (g_ModifierDraft.Active && g_ModifierDraft.Object == o.Id &&
            g_ModifierDraft.Index == i && !ImGui::IsAnyItemActive()) {
          g_ModifierDraft.Active = false;
          QueueSelectedMesh([i, mod](GameObject &a) {
            a.Modifiers.at(i) = mod;
            Model::Evaluate(a);
          });
        }
        if (ImGui::SmallButton("Update"))
          QueueSelectedMesh([](GameObject &a) { Model::Evaluate(a); });
        ImGui::SameLine();
        if (ImGui::SmallButton("Up") && i)
          QueueSelectedMesh([i](GameObject &a) {
            std::swap(a.Modifiers[i], a.Modifiers[i - 1]);
            Model::Evaluate(a);
          });
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
          QueueSelectedMesh([i](GameObject &a) {
            auto base = a.BaseMesh;
            a.Modifiers.erase(a.Modifiers.begin() + i);
            if (a.Modifiers.empty()) {
              Model::ToObject(a, base);
              a.BaseMesh = StudioMesh();
            } else
              Model::Evaluate(a);
          });
        ImGui::PopID();
      }
      if (!o.Modifiers.empty() && ImGui::Button("Apply modifier stack"))
        QueueSelectedMesh([](GameObject &a) { Model::ApplyModifiers(a); });
    }
    ImGui::SeparatorText("Boolean (two selected objects)");
    static int boolean = 0;
    const char *ops[] = {"Union", "Difference", "Intersection"};
    ImGui::Combo("Operation", &boolean, ops, 3);
    if (ImGui::Button("Create Boolean result")) {
      int op = boolean;
      QueueModelJob([op](std::vector<GameObject> &objects) {
        std::vector<size_t> selected;
        for (size_t i = 0; i < objects.size(); ++i)
          if (objects[i].Selected)
            selected.push_back(i);
        if (selected.size() != 2)
          throw std::runtime_error("Select exactly two objects");
        auto out =
            Model::Boolean(objects[selected[0]], objects[selected[1]], op);
        for (auto i : selected) {
          objects[i].Visible = false;
          objects[i].Selected = false;
        }
        out.Selected = true;
        objects.push_back(std::move(out));
      });
    }
    ImGui::EndDisabled();
    if (g_ModelBusy)
      ImGui::TextColored(ImVec4(.4f, .9f, .9f, 1), "Computing mesh...");
  }
}
static void DrawModelSelection(float distance, float pitch, float yaw,
                               const float target[3]) {
  if (!g_Modeling.Edit || g_SelectedObjectIndex < 0 ||
      size_t(g_SelectedObjectIndex) >= g_Objects.size() || g_PhysicsRunning)
    return;
  auto &o = g_Objects[size_t(g_SelectedObjectIndex)];
  if (g_Modeling.Object != o.Id) {
    g_Modeling.Object = o.Id;
    g_Modeling.Selected.clear();
  }
  auto m = Model::Editable(o);
  if (m.Positions.empty())
    return;
  std::set<uint64_t> edges;
  g_Modeling.Edges.clear();
  for (const auto &f : m.Faces)
    for (size_t i = 0; i < f.size(); ++i)
      edges.insert(Model::Edge(f[i], f[(i + 1) % f.size()]));
  for (auto e : edges)
    g_Modeling.Edges.push_back({uint32_t(e >> 32), uint32_t(e)});
  StudioMatrix view, projection;
  StudioCameraMatrices(
      distance, pitch, yaw, target, g_Studio.SceneSize.x / g_Studio.SceneSize.y,
      g_Preferences.Fov, g_Preferences.FarClip, view, projection);
  auto model = StudioObjectMatrix(o);
  auto clip = StudioMultiply(projection, StudioMultiply(view, model));
  auto screen = [&](Vec3 p) {
    float x = clip.M[0] * p.x + clip.M[4] * p.y + clip.M[8] * p.z + clip.M[12],
          y = clip.M[1] * p.x + clip.M[5] * p.y + clip.M[9] * p.z + clip.M[13],
          w = clip.M[3] * p.x + clip.M[7] * p.y + clip.M[11] * p.z + clip.M[15];
    if (w <= .001f)
      return ImVec2(-100000, -100000);
    return ImVec2(
        g_Studio.SceneMin.x + (x / w + 1) * .5f * g_Studio.SceneSize.x,
        g_Studio.SceneMin.y + (1 - y / w) * .5f * g_Studio.SceneSize.y);
  };
  auto *draw = ImGui::GetWindowDrawList();
  auto mouse = ImGui::GetIO().MousePos;
  uint32_t nearest = 0;
  float best = 144;
  bool found = false;
  size_t count = g_Modeling.Mode == 0   ? m.Positions.size() / 3
                 : g_Modeling.Mode == 1 ? g_Modeling.Edges.size()
                                        : m.Faces.size();
  for (uint32_t i = 0; i < count; ++i) {
    Vec3 p = {0, 0, 0};
    if (g_Modeling.Mode == 0)
      p = Model::Point(m, i);
    else if (g_Modeling.Mode == 1) {
      auto e = g_Modeling.Edges[i];
      p = Model::Mul(
          Model::Add(Model::Point(m, e.first), Model::Point(m, e.second)), .5f);
    } else {
      for (auto v : m.Faces[i])
        p = Model::Add(p, Model::Point(m, v));
      p = Model::Mul(p, 1.f / m.Faces[i].size());
    }
    ImVec2 q = screen(p);
    float d =
        (q.x - mouse.x) * (q.x - mouse.x) + (q.y - mouse.y) * (q.y - mouse.y);
    if (d < best) {
      best = d;
      nearest = i;
      found = true;
    }
    bool selected = g_Modeling.Selected.count(i);
    ImU32 color =
        selected ? IM_COL32(255, 190, 70, 255) : IM_COL32(80, 220, 230, 180);
    if (g_Modeling.Mode == 1) {
      auto e = g_Modeling.Edges[i];
      draw->AddLine(screen(Model::Point(m, e.first)),
                    screen(Model::Point(m, e.second)), color, selected ? 3 : 1);
    } else
      draw->AddCircleFilled(q, selected ? 4 : 2, color);
  }
  if (g_Studio.SceneHovered && ImGui::IsMouseClicked(0) &&
      !ImGuizmo::IsOver()) {
    if (!ImGui::GetIO().KeyCtrl)
      g_Modeling.Selected.clear();
    if (found) {
      if (g_Modeling.Selected.count(nearest))
        g_Modeling.Selected.erase(nearest);
      else
        g_Modeling.Selected.insert(nearest);
    }
  }
  auto selected = SelectedModelVertices(m);
  if (selected.empty() || g_Studio.TransformMode == 0 || !o.Modifiers.empty())
    return;
  Vec3 center = {0, 0, 0};
  for (auto v : selected)
    center = Model::Add(center, Model::Point(m, v));
  center = Model::Mul(center, 1.f / selected.size());
  GameObject pivot("Edit Pivot");
  TransformPoint(o, center, center);
  pivot.Position[0] = center.x;
  pivot.Position[1] = center.y;
  pivot.Position[2] = center.z;
  auto before = StudioObjectMatrix(pivot), matrix = before;
  ImGuizmo::SetDrawlist(draw);
  ImGuizmo::SetRect(g_Studio.SceneMin.x, g_Studio.SceneMin.y,
                    g_Studio.SceneSize.x, g_Studio.SceneSize.y);
  auto mode = g_Studio.TransformMode == 2   ? ImGuizmo::ROTATE
              : g_Studio.TransformMode == 3 ? ImGuizmo::SCALE
                                            : ImGuizmo::TRANSLATE;
  if (ImGuizmo::Manipulate(view.M, projection.M, mode, ImGuizmo::WORLD,
                           matrix.M, nullptr)) {
    auto delta = StudioMultiply(
        StudioInverse(model),
        StudioMultiply(StudioMultiply(matrix, StudioInverse(before)), model));
    for (auto v : selected) {
      Vec3 p = Model::Point(m, v);
      m.Positions[v * 3] =
          delta.M[0] * p.x + delta.M[4] * p.y + delta.M[8] * p.z + delta.M[12];
      m.Positions[v * 3 + 1] =
          delta.M[1] * p.x + delta.M[5] * p.y + delta.M[9] * p.z + delta.M[13];
      m.Positions[v * 3 + 2] =
          delta.M[2] * p.x + delta.M[6] * p.y + delta.M[10] * p.z + delta.M[14];
    }
    try {
      Model::SetEditable(o, std::move(m));
    } catch (const std::exception &e) {
      AppendConsole(e.what());
    }
  }
}
