#pragma once
static void StartSimulation() {
  if (g_PhysicsRunning)
    return;
  g_PlaySnapshot = g_Objects;
  g_PlaySoundSnapshot = g_Sounds;
  std::copy(g_SkyTop, g_SkyTop + 3, g_PlayTop);
  std::copy(g_SkyBottom, g_SkyBottom + 3, g_PlayBottom);
  g_PlaySky = g_SkyPreset;
  g_PlayPhysics = g_Physics;
  g_PlayGame = g_Game;
  g_GameScore = 0;
  g_GameHud.clear();
  g_Physics.Accumulator = 0;
  g_PhysicsRunning = true;
  g_SimulationRunning = true;
  InitLuaState();
  int startup = g_Game.StartupScript;
  if (startup >= 0 && startup < 8 &&
      g_Studio.Scripts[size_t(startup)].Allocated)
    ExecuteLuaScript(g_Studio.Scripts[size_t(startup)].Code);
  for (const auto &sound : g_Sounds)
    if (sound.Autoplay)
      PlayStudioSound(sound);
}
static void StopSimulation() {
  if (g_PhysicsRunning) {
    for (const auto &sound : g_Sounds)
      studio_audio_unload(sound.Id);
    g_Objects = g_PlaySnapshot;
    g_Sounds = g_PlaySoundSnapshot;
    std::copy(g_PlayTop, g_PlayTop + 3, g_SkyTop);
    std::copy(g_PlayBottom, g_PlayBottom + 3, g_SkyBottom);
    g_SkyPreset = g_PlaySky;
    g_Physics = g_PlayPhysics;
    g_Game = g_PlayGame;
  }
  studio_audio_stop_all();
  g_PhysicsRunning = false;
  g_SimulationRunning = false;
  g_Physics.Accumulator = 0;
  g_SelectedObjectIndex = -1;
  g_SelectedSound = -1;
  g_GameScore = 0;
  g_GameHud.clear();
  InitLuaState();
}
static void FollowGameCamera(float &distance, float &pitch, float &yaw,
                             float target[3]) {
  if (!g_PhysicsRunning)
    return;
  const GameObject *followed = nullptr;
  for (const auto &o : g_Objects) {
    if (o.Id == g_Game.CameraTarget) {
      followed = &o;
      break;
    }
    if (!followed && o.Character)
      followed = &o;
  }
  if (!followed)
    return;
  distance = g_Game.CameraDistance;
  pitch = g_Game.CameraPitch;
  yaw = g_Game.CameraYaw;
  std::copy(followed->Position, followed->Position + 3, target);
  target[1] += .7f;
}
static void DrawGameHUD() {
  if (!g_PhysicsRunning)
    return;
  auto *draw = ImGui::GetForegroundDrawList();
  auto pos = g_PlayerMode
                 ? ImVec2(24, 24)
                 : ImVec2(g_Studio.SceneMin.x + 16, g_Studio.SceneMin.y + 16);
  std::string text = "Score: " + std::to_string(g_GameScore);
  if (!g_GameHud.empty())
    text += "\n" + g_GameHud;
  auto size = ImGui::CalcTextSize(text.c_str());
  draw->AddRectFilled(ImVec2(pos.x - 10, pos.y - 8),
                      ImVec2(pos.x + size.x + 10, pos.y + size.y + 8),
                      IM_COL32(12, 20, 30, 200), 5);
  draw->AddText(pos, IM_COL32(235, 245, 250, 255), text.c_str());
}
