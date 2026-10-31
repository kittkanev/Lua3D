#pragma once
#include <fstream>
struct EditorPreferences {
  float OrbitSensitivity = .4f, PanSensitivity = .02f, ZoomSensitivity = .8f,
        Fov = 45, FarClip = 50000, GridStep = 1, RotationSnap = 15,
        ScaleSnap = .1f, MasterVolume = .8f;
  bool AdaptiveGrid = true, ShowSettings = false, VSync = true;
  int Theme = 0;
};
static EditorPreferences g_Preferences;
static std::string g_PreferencesPath;
static bool SaveEditorPreferences() {
  if (g_PreferencesPath.empty())
    return false;
  nlohmann::json j = {{"orbit", g_Preferences.OrbitSensitivity},
                      {"pan", g_Preferences.PanSensitivity},
                      {"zoom", g_Preferences.ZoomSensitivity},
                      {"fov", g_Preferences.Fov},
                      {"far_clip", g_Preferences.FarClip},
                      {"grid_step", g_Preferences.GridStep},
                      {"adaptive_grid", g_Preferences.AdaptiveGrid},
                      {"grid", g_ShowGrid},
                      {"axes", g_ShowAxes},
                      {"vsync", g_Preferences.VSync},
                      {"theme", g_Preferences.Theme},
                      {"rotation_snap", g_Preferences.RotationSnap},
                      {"scale_snap", g_Preferences.ScaleSnap},
                      {"master_volume", g_Preferences.MasterVolume}};
  std::string temporary = g_PreferencesPath + ".tmp";
  std::ofstream f(temporary, std::ios::binary | std::ios::trunc);
  std::string text = j.dump(2);
  f.write(text.data(), std::streamsize(text.size()));
  f.close();
  if (!f)
    return false;
#ifdef _WIN32
  if (!MoveFileExA(temporary.c_str(), g_PreferencesPath.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    return false;
#else
  if (std::rename(temporary.c_str(), g_PreferencesPath.c_str()))
    return false;
#endif
  return true;
}
static void LoadEditorPreferences() {
  try {
    std::ifstream f(g_PreferencesPath);
    if (!f)
      return;
    nlohmann::json j;
    f >> j;
    EditorPreferences p = g_Preferences;
    auto number = [&](const char *key, float fallback, float lo, float hi) {
      if (!j.contains(key))
        return fallback;
      if (!j[key].is_number())
        throw std::runtime_error("Invalid setting");
      double n = j[key].get<double>();
      if (!std::isfinite(n) || n < lo || n > hi)
        throw std::runtime_error("Invalid setting range");
      return float(n);
    };
    p.OrbitSensitivity = number("orbit", p.OrbitSensitivity, .01f, 3);
    p.PanSensitivity = number("pan", p.PanSensitivity, .0001f, 2);
    p.ZoomSensitivity = number("zoom", p.ZoomSensitivity, .01f, 100);
    p.Fov = number("fov", p.Fov, 15, 120);
    p.FarClip = number("far_clip", p.FarClip, 500, 1000000);
    p.GridStep = number("grid_step", p.GridStep, .01f, 10000);
    p.RotationSnap = number("rotation_snap", p.RotationSnap, .1f, 180);
    p.ScaleSnap = number("scale_snap", p.ScaleSnap, .001f, 10);
    p.MasterVolume = number("master_volume", p.MasterVolume, 0, 1);
    p.AdaptiveGrid = j.value("adaptive_grid", true);
    p.VSync = j.value("vsync", true);
    p.Theme = j.value("theme", 0);
    if (p.Theme < 0 || p.Theme > 1)
      throw std::runtime_error("Invalid theme");
    bool grid = j.value("grid", true), axes = j.value("axes", true);
    g_Preferences = p;
    g_ShowGrid = grid;
    g_ShowAxes = axes;
  } catch (const std::exception &e) {
    AppendConsole(std::string("Settings ignored: ") + e.what() + "\n");
  }
}
