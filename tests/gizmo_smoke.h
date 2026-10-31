#pragma once
// Exercises the production ImGui/ImGuizmo input path inside the running editor.
struct GizmoSmokeTest {
  bool Enabled = false, Done = false, Passed = false;
  int Stage = 0, Frames = 0, Attempts = 0;
  float X = 0, Y = 0;
  void Initialize() {
    if (!Enabled)
      return;
    g_Objects.clear();
    g_Objects.emplace_back("Drag test");
    g_Objects[0].Position[1] = .5f;
    g_Objects[0].Selected = true;
    g_Objects.emplace_back("Multi-selection");
    g_Objects[1].Position[1] = .5f;
    g_Objects[1].Position[2] = 2;
    g_Objects[1].Selected = true;
    g_SelectedObjectIndex = 0;
    g_SimulationRunning = false;
    g_ObjectHistory.Clear();
    g_ObjectHistory.Capture(g_Objects);
  }
  void Input(float distance, float pitch, float yaw, const float target[3]) {
    if (!Enabled || Done || g_Studio.SceneSize.x < 10)
      return;
    auto &io = ImGui::GetIO();
    io.ConfigInputTrickleEventQueue = false;
    if (Stage == 0) {
      StudioMatrix view, projection;
      StudioCameraMatrices(distance, pitch, yaw, target,
                           g_Studio.SceneSize.x / g_Studio.SceneSize.y,
                           g_Preferences.Fov, g_Preferences.FarClip, view,
                           projection);
      StudioMatrix vp = StudioMultiply(projection, view);
      float p[4] = {.08f + Attempts * .06f, .5f, 0, 1}, clip[4] = {};
      for (int row = 0; row < 4; row++)
        for (int k = 0; k < 4; k++)
          clip[row] += vp.M[k * 4 + row] * p[k];
      X = g_Studio.SceneMin.x +
          (clip[0] / clip[3] + 1) * .5f * g_Studio.SceneSize.x;
      Y = g_Studio.SceneMin.y +
          (1 - clip[1] / clip[3]) * .5f * g_Studio.SceneSize.y;
      io.AddMousePosEvent(X, Y);
    } else {
      io.AddMousePosEvent(X + (Stage == 2   ? float((Frames + 1) * 25)
                               : Stage == 3 ? 100.0f
                                            : 0),
                          Y);
      io.AddMouseButtonEvent(0, Stage == 1 || Stage == 2);
    }
  }
  void Check() {
    if (!Enabled || Done)
      return;
    Frames++;
    if (Stage == 0) {
      if (Frames > 5 &&
          ImGuizmo::GetHoveredHandleType() == ImGuizmo::MT_MOVE_X) {
        Stage = 1;
        Frames = 0;
      } else if (Frames > 5) {
        Attempts++;
        if (Attempts > 40) {
          Done = true;
          std::cerr << "FAIL: could not acquire the X gizmo handle\n";
        }
      }
    } else if (Stage == 1 && Frames >= 3) {
      Stage = 2;
      Frames = 0;
    } else if (Stage == 2 && Frames >= 4) {
      Stage = 3;
      Frames = 0;
    } else if (Stage == 3 && Frames >= 3) {
      Done = true;
      float x = g_Objects[0].Position[0];
      Passed = x > .1f && std::abs(g_Objects[0].Position[1] - .5f) < .01f &&
               std::abs(g_Objects[0].Position[2]) < .01f &&
               std::abs(g_Objects[1].Position[0] - x) < .01f &&
               std::abs(g_Objects[1].Position[2] - 2) < .01f;
      Passed = Passed && g_ObjectHistory.Undo(g_Objects) &&
               std::abs(g_Objects[0].Position[0]) < .001f &&
               g_ObjectHistory.Redo(g_Objects) &&
               std::abs(g_Objects[0].Position[0] - x) < .001f;
      if (!Passed)
        std::cerr << "Position " << g_Objects[0].Position[0] << ","
                  << g_Objects[0].Position[1] << "," << g_Objects[0].Position[2]
                  << " History " << g_ObjectHistory.Cursor << "/"
                  << g_ObjectHistory.States.size() << "\n";
      std::cout << (Passed ? "PASS: original editor mouse X-axis drag, "
                             "multi-selection and undo/redo\n"
                           : "FAIL: gizmo drag or undo/redo\n");
    }
  }
};
