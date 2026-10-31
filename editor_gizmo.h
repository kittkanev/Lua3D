#pragma once
static void RenderTransformGizmo(float distance, float pitch, float yaw,
                                 const float target[3]) {
  if (g_Studio.SculptEnabled || g_PhysicsRunning ||
      g_Studio.TransformMode == 0 || g_SelectedObjectIndex < 0 ||
      size_t(g_SelectedObjectIndex) >= g_Objects.size())
    return;
  auto &selected = g_Objects[size_t(g_SelectedObjectIndex)];
  StudioMatrix matrix = StudioObjectMatrix(selected), before = matrix, view,
               projection;
  StudioCameraMatrices(
      distance, pitch, yaw, target, g_Studio.SceneSize.x / g_Studio.SceneSize.y,
      g_Preferences.Fov, g_Preferences.FarClip, view, projection);
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::AllowAxisFlip(false);
  ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
  ImGuizmo::SetRect(g_Studio.SceneMin.x, g_Studio.SceneMin.y,
                    g_Studio.SceneSize.x, g_Studio.SceneSize.y);
  ImGuizmo::OPERATION operation = g_Studio.TransformMode == 2 ? ImGuizmo::ROTATE
                                  : g_Studio.TransformMode == 3
                                      ? ImGuizmo::SCALE
                                      : ImGuizmo::TRANSLATE;
  float snap[3] = {g_Studio.Snap, g_Studio.Snap, g_Studio.Snap};
  if (operation == ImGuizmo::ROTATE)
    snap[0] = g_Preferences.RotationSnap;
  if (operation == ImGuizmo::SCALE)
    snap[0] = g_Preferences.ScaleSnap;
  if (ImGuizmo::Manipulate(
          view.M, projection.M, operation,
          g_Studio.LocalTransform ? ImGuizmo::LOCAL : ImGuizmo::WORLD, matrix.M,
          nullptr, g_Studio.TransformSnap ? snap : nullptr)) {
    if (!g_Studio.TransformDragging) {
      g_ObjectHistory.Capture(g_Objects);
      g_Studio.TransformDragging = true;
    }
    StudioMatrix delta = StudioMultiply(matrix, StudioInverse(before));
    for (auto &object : g_Objects)
      if (object.Selected || object.Id == selected.Id) {
        StudioMatrix transformed =
            StudioMultiply(delta, StudioObjectMatrix(object));
        StudioApplyMatrix(object, transformed);
      }
    g_SimulationRunning = false;
  }
  if (g_Studio.TransformDragging && !ImGuizmo::IsUsing()) {
    g_ObjectHistory.Capture(g_Objects);
    g_Studio.TransformDragging = false;
  }
}
static void PersistStudioPreferences() {
  g_Preferences.MoveSnap = g_Studio.Snap;
  g_Preferences.SnapEnabled = g_Studio.TransformSnap;
  g_Preferences.LocalTransform = g_Studio.LocalTransform;
  SaveEditorPreferences();
}
static void RenderEditorSettings() {
  if (!g_Preferences.ShowSettings)
    return;
  ImGui::SetNextWindowSize(ImVec2(490, 570), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Settings", &g_Preferences.ShowSettings)) {
    ImGui::TextUnformatted("Camera");
    ImGui::SliderFloat("Orbit sensitivity", &g_Preferences.OrbitSensitivity,
                       .01f, 3);
    ImGui::SliderFloat("Pan sensitivity", &g_Preferences.PanSensitivity, .0001f,
                       .2f, "%.4f");
    ImGui::SliderFloat("Zoom sensitivity", &g_Preferences.ZoomSensitivity, .01f,
                       10);
    ImGui::SliderFloat("Field of view", &g_Preferences.Fov, 15, 120);
    ImGui::SliderFloat("Far clipping", &g_Preferences.FarClip, 500, 1000000,
                       "%.0f", ImGuiSliderFlags_Logarithmic);
    ImGui::SeparatorText("World / transforms");
    ImGui::Checkbox("Grid", &g_ShowGrid);
    ImGui::Checkbox("Adaptive grid", &g_Preferences.AdaptiveGrid);
    ImGui::DragFloat("Grid unit", &g_Preferences.GridStep, .1f, .01f, 10000,
                     "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("World axes", &g_ShowAxes);
    ImGui::Checkbox("Transform snapping", &g_Studio.TransformSnap);
    ImGui::DragFloat("Move snap", &g_Studio.Snap, .01f, .01f, 1000);
    ImGui::DragFloat("Rotation snap", &g_Preferences.RotationSnap, 1, .1f, 180);
    ImGui::DragFloat("Scale snap", &g_Preferences.ScaleSnap, .01f, .001f, 10);
    ImGui::SeparatorText("Editor");
    const char *themes[] = {"Dark", "Light"};
    if (ImGui::Combo("Theme", &g_Preferences.Theme, themes, 2)) {
      if (g_Preferences.Theme == 0)
        ImGui::StyleColorsDark();
      else
        ImGui::StyleColorsLight();
    }
    if (ImGui::Checkbox("VSync", &g_Preferences.VSync))
      SDL_GL_SetSwapInterval(g_Preferences.VSync ? 1 : 0);
    ImGui::SliderFloat("Master volume", &g_Preferences.MasterVolume, 0, 1);
    g_Preferences.MoveSnap = g_Studio.Snap;
    g_Preferences.SnapEnabled = g_Studio.TransformSnap;
    g_Preferences.LocalTransform = g_Studio.LocalTransform;
    if (ImGui::Button("Save settings"))
      AppendConsole(SaveEditorPreferences() ? "Settings saved.\n"
                                            : "Settings could not be saved.\n");
    ImGui::SameLine();
    if (ImGui::Button("Reset settings")) {
      g_Preferences = EditorPreferences();
      g_Preferences.ShowSettings = true;
      g_Studio.Snap = .5f;
      g_Studio.TransformSnap = false;
      g_Studio.LocalTransform = false;
      g_ShowGrid = g_ShowAxes = true;
      SDL_GL_SetSwapInterval(1);
      ImGui::StyleColorsDark();
    }
  }
  ImGui::End();
}
