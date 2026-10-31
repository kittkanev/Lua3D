#pragma once
static bool ImportStudioSound(const std::string &path) {
  if (path.empty() || path.size() > 511 || g_Sounds.size() >= 64)
    return false;
  StudioSound sound;
  sound.Path = AbsoluteAssetPath(path);
  size_t slash = path.find_last_of("/\\");
  sound.Name = path.substr(slash == std::string::npos ? 0 : slash + 1);
  if (sound.Name.size() > 255)
    sound.Name.resize(255);
  char error[256];
  if (!studio_audio_load(sound.Id, path.c_str(), error)) {
    AppendConsole(std::string(error) + "\n");
    return false;
  }
  g_Sounds.push_back(sound);
  g_SelectedSound = int(g_Sounds.size()) - 1;
  g_ShowAudio = true;
  return true;
}
static void RenderStudioAudio() {
  if (!g_ShowAudio)
    return;
  ImGui::SetNextWindowSize(ImVec2(570, 430), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Audio", &g_ShowAudio)) {
    static char path[512] = "";
    ImGui::InputTextWithHint("Audio file",
                             "WAV / MP3 / FLAC / OGG / W64 / RF64", path,
                             sizeof(path));
    if (ImGui::Button("Import sound")) {
      ImportStudioSound(path);
    }
    ImGui::Separator();
    for (size_t i = 0; i < g_Sounds.size(); ++i) {
      ImGui::PushID(int(i));
      if (ImGui::Selectable(g_Sounds[i].Name.c_str(),
                            g_SelectedSound == int(i)))
        g_SelectedSound = int(i);
      ImGui::PopID();
    }
    if (g_SelectedSound >= 0 && size_t(g_SelectedSound) < g_Sounds.size()) {
      auto &sound = g_Sounds[size_t(g_SelectedSound)];
      char name[256];
      std::snprintf(name, sizeof(name), "%s", sound.Name.c_str());
      if (ImGui::InputText("Name", name, sizeof(name)))
        sound.Name = name;
      ImGui::TextWrapped("%s", sound.Path.c_str());
      if(sound.Data.empty()&&!AssetExists(sound.Path))ImGui::TextColored(ImVec4(1,.5f,.3f,1),"Missing resource: select a replacement below");
      if(ImGui::Button("Replace with path above")&&path[0]&&AssetExists(path)){studio_audio_unload(sound.Id);sound.Data.clear();sound.Path=AbsoluteAssetPath(path);}

      ImGui::SliderFloat("Volume", &sound.Volume, 0, 2);
      ImGui::SliderFloat("Pitch / speed", &sound.Pitch, .1f, 4);
      ImGui::Checkbox("Loop", &sound.Loop);
      ImGui::SameLine();
      ImGui::Checkbox("Play on simulation start", &sound.Autoplay);
      ImGui::Checkbox("3D position", &sound.Spatial);
      if (sound.Spatial)
        ImGui::DragFloat3("Position", sound.Position, .1f);
      ConfigureStudioSound(sound);
      if (ImGui::Button("Play / preview"))
        PlayStudioSound(sound);
      ImGui::SameLine();
      if (ImGui::Button("Stop"))
        studio_audio_stop(sound.Id);
      ImGui::SameLine();
      if (ImGui::Button("Remove")) {
        studio_audio_unload(sound.Id);
        g_Sounds.erase(g_Sounds.begin() + g_SelectedSound);
        g_SelectedSound = -1;
      }
    }
    ImGui::Separator();
    ImGui::SliderFloat("Master volume", &g_Preferences.MasterVolume, 0, 1);
  }
  ImGui::End();
}
static int l_add_sound(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  const char *name = luaL_optstring(L, 2, "Sound");
  if (std::strlen(path) > 511 || std::strlen(name) > 255 ||
      g_Sounds.size() >= 64)
    return luaL_error(L, "Invalid sound path, name or sound limit");
  StudioSound sound;
  sound.Path = AbsoluteAssetPath(path);
  sound.Name = name;
  g_Sounds.push_back(sound);
  lua_pushinteger(L, lua_Integer(g_Sounds.size() - 1));
  return 1;
}
static int l_play_sound(lua_State *L) {
  lua_Integer i = luaL_checkinteger(L, 1);
  lua_pushboolean(L, i >= 0 && uint64_t(i) < g_Sounds.size() &&
                         PlayStudioSound(g_Sounds[size_t(i)]));
  return 1;
}
static int l_stop_sound(lua_State *L) {
  lua_Integer i = luaL_checkinteger(L, 1);
  bool ok = i >= 0 && uint64_t(i) < g_Sounds.size();
  if (ok)
    studio_audio_stop(g_Sounds[size_t(i)].Id);
  lua_pushboolean(L, ok);
  return 1;
}
static void RegisterSoundBindings(lua_State *L) {
  lua_register(L, "add_sound", LuaBinding<l_add_sound>);
  lua_register(L, "play_sound", LuaBinding<l_play_sound>);
  lua_register(L, "stop_sound", LuaBinding<l_stop_sound>);
}
