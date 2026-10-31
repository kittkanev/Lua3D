#pragma once
#include "extensions/audio/audio_backend.h"
struct StudioSound {
  uint64_t Id = NewObjectId();
  std::string Name = "Sound", Path;
  float Position[3] = {0, 0, 0}, Volume = 1, Pitch = 1;
  bool Loop = false, Spatial = false, Autoplay = false;
};
static std::vector<StudioSound> g_Sounds;
static std::vector<StudioSound> g_PlaySoundSnapshot;
static int g_SelectedSound = -1;
static bool g_ShowAudio = false;
static void ConfigureStudioSound(const StudioSound &s) {
  studio_audio_configure(s.Id, s.Volume, s.Pitch, s.Loop, s.Spatial,
                         s.Position);
}
static bool PlayStudioSound(const StudioSound &s) {
  char error[256];
  if (!studio_audio_load(s.Id, s.Path.c_str(), error)) {
    AppendConsole(std::string(error) + "\n");
    return false;
  }
  ConfigureStudioSound(s);
  studio_audio_play(s.Id);
  return true;
}
static void RegisterSoundBindings(lua_State *state);
