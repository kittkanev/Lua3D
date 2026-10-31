#pragma once
#include "example_script.h"
static std::vector<unsigned char> ExampleTone() {
  std::vector<unsigned char> wav;
  auto text = [&](const char *s) {
    for (int i = 0; i < 4; ++i)
      wav.push_back(static_cast<unsigned char>(s[i]));
  };
  text("RIFF");
  GamePackage::Number(wav, 36 + 4410 * 2, 4);
  text("WAVE");
  text("fmt ");
  GamePackage::Number(wav, 16, 4);
  GamePackage::Number(wav, 1, 2);
  GamePackage::Number(wav, 1, 2);
  GamePackage::Number(wav, 22050, 4);
  GamePackage::Number(wav, 44100, 4);
  GamePackage::Number(wav, 2, 2);
  GamePackage::Number(wav, 16, 2);
  text("data");
  GamePackage::Number(wav, 4410 * 2, 4);
  for (int i = 0; i < 4410; ++i) {
    int16_t sample =
        int16_t(std::sin(i * 2 * PI * 660 / 22050) * 10000 * (1 - i / 4410.f));
    GamePackage::Number(wav, uint16_t(sample), 2);
  }
  return wav;
}
static void CreateSampleGame() {
  if (g_PhysicsRunning)
    StopSimulation();
  for (const auto &sound : g_Sounds)
    studio_audio_unload(sound.Id);
  g_Objects.clear();
  g_Sounds.clear();
  g_Game = StudioGameSettings();
  g_Game.Title = "Lunoryx - Gem Trail";g_Game.StartupScript=0;
  ApplySkyPreset(0);
  g_Physics = PhysicsState();
  GameObject floor("Textured Floor");
  auto mesh = Model::FromObject(floor);
  Model::Unwrap(mesh);
  for(float& uv:mesh.UV)uv*=4;
  Model::ToObject(floor, mesh);
  floor.Scale[0] = 18;
  floor.Scale[1] = .5f;
  floor.Scale[2] = 18;
  floor.Position[1] = -.25f;
  floor.Color[0] = floor.Color[1] = floor.Color[2] = 1;
  tinygltf::Image image;
  image.width = image.height = 32;
  image.component = 4;
  image.bits = 8;
  image.image.resize(32 * 32 * 4);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x) {
      size_t p = size_t(y * 32 + x) * 4;
      bool check = (x / 4 + y / 4) % 2;
      image.image[p] = check ? 42 : 60;
      image.image[p + 1] = check ? 105 : 135;
      image.image[p + 2] = check ? 115 : 145;
      image.image[p + 3] = 255;
    }
  StudioMaterial material;
  material.Image = EncodeImage(image);
  floor.Materials.push_back(material);
  g_Objects.push_back(std::move(floor));
  for (int i = 0; i < 3; ++i) {
    GameObject platform("Platform " + std::to_string(i + 1));
    platform.Scale[0] = 2;
    platform.Scale[1] = .3f;
    platform.Scale[2] = 2;
    platform.Position[0] = -3 + 3.f * i;
    platform.Position[1] = .15f;
    platform.Position[2] = -3;
    platform.Color[0] = .3f;
    platform.Color[1] = .5f;
    platform.Color[2] = .7f;
    g_Objects.push_back(platform);
    GameObject gem("Gem " + std::to_string(i + 1), MeshType::Pyramid);
    gem.Scale[0] = gem.Scale[1] = gem.Scale[2] = .6f;
    gem.Position[0] = platform.Position[0];
    gem.Position[1] = 1;
    gem.Position[2] = -3;
    gem.Color[0] = 1;
    gem.Color[1] = .75f;
    gem.Color[2] = .15f;
    ObjectProperty property;
    property.Type = ObjectProperty::Boolean;
    property.BoolValue = false;
    SetProperty(gem, "Collected", property);
    g_Objects.push_back(gem);
  }
  CharacterConfig character;
  int player = CreateCharacter(g_Objects, "Player", character);
  g_Objects[size_t(player)].Position[2] = 3;
  g_Objects[size_t(player)].Selected = true;
  g_SelectedObjectIndex = player;
  g_Game.CameraTarget = g_Objects[size_t(player)].Id;
  StudioSound sound;
  sound.Name = "Gem pickup";
  sound.Data = ExampleTone();
  g_Sounds.push_back(std::move(sound));
  for (auto &script : g_Studio.Scripts)
    script = ScriptDocument();
  auto &script = g_Studio.Scripts[0];
  script.Allocated = script.Open = true;
  std::snprintf(script.Name, sizeof(script.Name), "Collect.lua");
  std::snprintf(script.Code, sizeof(script.Code), "%s", kCollectScript);
  g_Studio.ActiveScript = 0;
  g_Studio.RequestDocument = -2;
  g_Modeling.Edit = false;
  g_ShowUV = false;
  InitLuaState();
  g_SimulationRunning = false;
  g_ObjectHistory.Clear();
  g_ObjectHistory.Capture(g_Objects);
  g_OpenedProject.clear();
  AppendConsole(
      "Created Gem Trail: edit, Play, save and build a standalone game.\n");
}
