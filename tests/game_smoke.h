#pragma once
struct GameSmokeTest {
  bool Enabled = false, Passed = false, Captured = false;
  double AudioEnergy = 0;
  int Phase=0,Frames=0;
  void Input(const GameObject &player, float &x, float &z, bool &jump) {
    if (!Enabled)
      return;
    ++Frames;
    x = z = 0;
    jump = false;
    for (int gem = 1; gem <= 3; ++gem) {
      const GameObject *target = nullptr;
      for (const auto &o : g_Objects)
        if (o.Name == "Gem " + std::to_string(gem))
          target = &o;
      if (!target)
        continue;
      auto property = target->Properties.find("Collected");
      if (property != target->Properties.end() && property->second.BoolValue)
        continue;
      float dx = target->Position[0] - player.Position[0],
            dz = target->Position[2] - player.Position[2],
            length = std::sqrt(dx * dx + dz * dz);
      if (length > .08f) {
        x = dx / length;
        z = dz / length;
      }
      jump = length < 3 && player.Grounded;
      return;
    }
  }
  void Audio() {
    if (!Enabled)
      return;
    float samples[1024] = {};
    if (studio_audio_render(samples, 512))
      for (float value : samples)
        AudioEnergy += std::abs(value);
  }
};
