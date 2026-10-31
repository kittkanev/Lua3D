#pragma once
struct StudioGameSettings {
  int StartupScript = -1;
  uint64_t CameraTarget = 0;
  float CameraDistance = 6, CameraPitch = 22, CameraYaw = 0;
  std::string Title = "Lunoryx Game";
};
static StudioGameSettings g_Game, g_PlayGame;
static bool g_PlayerMode = false;
static int g_GameScore = 0;
static std::string g_GameHud;
static bool g_RestartGame = false;
