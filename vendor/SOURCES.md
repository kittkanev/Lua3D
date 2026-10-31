# Vendored C dependencies

| Files | Upstream snapshot | License |
|---|---|---|
| nuklear.h, nuklear_sdl_gl2.h | https://github.com/Immediate-Mode-UI/Nuklear at b3c2a08385f16056bbe8c9a9ec754aa01cc12c02 | public domain / MIT alternative |
| cgltf.h | https://github.com/jkuhlmann/cgltf at 85cd62382dfea638278962690cf515023f33ed00 | MIT |
| cJSON.c/h | https://github.com/DaveGamble/cJSON at 6d9f2443ab071f86e5d9b43025a40929ec41c46c | MIT |
| miniaudio.h, stb_vorbis.c | https://github.com/mackron/miniaudio; exact SHA below | public domain / MIT alternative; stb public domain / MIT |
| stb_image.h, stb_image_write.h | Preserved from Lunoryx v0.2.0-alpha.1 | public domain / MIT |

The SDL backend's include path is adapted from `<SDL2/SDL.h>` to `<SDL.h>`
for static MinGW builds. No other vendor logic is changed.

Roboto-Regular.ttf comes from Nuklear's `extra_font` at the snapshot above.
Copyright 2011 Google Inc., Apache-2.0; binary is embedded in native/font_data.h.
Licenses are in docs/licenses and the executable's Docs selector.

miniaudio snapshot: `877ea4a7c2601c7b0f2a44e83dfac42e6470fea6`.
