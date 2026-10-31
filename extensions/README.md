# Extensions to the existing v0.2 Studio

- ImGuizmo: CedricGuillemet/ImGuizmo, commit 105e4fde45e4e74aae8973d98750820ef7aca7e2.
  Only ImGuizmo.cpp/h are vendored. They draw/manipulate transforms through the
  existing ImGui viewport and do not replace the renderer, scene model or UI.
- miniaudio and its stb_vorbis decoder: mackron/miniaudio, commit
  877ea4a7c2601c7b0f2a44e83dfac42e6470fea6. The small C wrapper exposes sound
  playback to the original C++ host; there is no new C scene/runtime layer.

Licenses are under docs/licenses and in Studio's API Docs.
