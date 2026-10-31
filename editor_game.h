#pragma once
static void RenderGameSettings() {
  if (!ImGui::CollapsingHeader("Game / Build", ImGuiTreeNodeFlags_DefaultOpen))
    return;
  if (ImGui::Button("New third-person example"))
    RequestProjectAction(4);
  char title[256];
  std::snprintf(title, sizeof(title), "%s", g_Game.Title.c_str());
  if (ImGui::InputText("Game title", title, sizeof(title)))
    g_Game.Title = title;
  if (ImGui::BeginCombo("Startup script",
                        g_Game.StartupScript < 0
                            ? "(None)"
                            : g_Studio
                                  .Scripts[size_t(std::max(
                                      0, std::min(g_Game.StartupScript, 7)))]
                                  .Name)) {
    if (ImGui::Selectable("(None)", g_Game.StartupScript < 0))
      g_Game.StartupScript = -1;
    for (int i = 0; i < 8; ++i)
      if (g_Studio.Scripts[size_t(i)].Allocated &&
          ImGui::Selectable(g_Studio.Scripts[size_t(i)].Name,
                            i == g_Game.StartupScript))
        g_Game.StartupScript = i;
    ImGui::EndCombo();
  }
  if (ImGui::Button("Follow selected character") &&
      g_SelectedObjectIndex >= 0 &&
      size_t(g_SelectedObjectIndex) < g_Objects.size())
    g_Game.CameraTarget = g_Objects[size_t(g_SelectedObjectIndex)].Id;
  ImGui::DragFloat("Follow distance", &g_Game.CameraDistance, .1f, .2f, 1000);
  ImGui::SliderFloat("Follow pitch", &g_Game.CameraPitch, -85, 85);
  ImGui::SliderFloat("Follow yaw", &g_Game.CameraYaw, -180, 180);
  static char path[512] = "Game.exe";
  ImGui::InputText("Game EXE path", path, sizeof(path));
  ImGui::BeginDisabled(g_PhysicsRunning || g_ModelBusy);
  if (ImGui::Button("Build standalone Game.exe"))
    BuildStudioGame(path);
  ImGui::EndDisabled();
  ImGui::TextWrapped("One executable includes the scene, imported resources "
                     "and Lua startup script. No compiler is needed.");
}
