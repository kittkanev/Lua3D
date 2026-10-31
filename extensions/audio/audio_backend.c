#include "audio_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
static ma_engine engine;
static int initialized;
typedef struct {
  uint64_t id;
  ma_sound sound;
  ma_decoder decoder;
  unsigned char *encoded;
  int memory;
  size_t encoded_size;
  char path[512];
} Voice;
static Voice voices[64];
static Voice *find_voice(uint64_t id) {
  for (int i = 0; i < 64; i++)
    if (voices[i].id == id)
      return &voices[i];
  return NULL;
}
static int ensure_engine(char error[256]) {
  if (!initialized) {
    ma_engine_config config = ma_engine_config_init();
    if (getenv("LUNORYX_AUDIO_NULL")) {
      config.noDevice = 1;
      config.sampleRate = 48000;
      config.channels = 2;
    }
    ma_result r = ma_engine_init(&config, &engine);
    if (r != MA_SUCCESS) {
      snprintf(error, 256, "Audio device unavailable (%d)", (int)r);
      return 0;
    }
    initialized = 1;
  }
  return 1;
}
int studio_audio_load(uint64_t id, const char *path, char error[256]) {
  if (!id || !path || strlen(path) > 511) {
    snprintf(error, 256, "Invalid audio path or ID");
    return 0;
  }
  if (!ensure_engine(error))
    return 0;
  Voice *v = find_voice(id);
  if (v && !strcmp(v->path, path))
    return 1;
  if (v)
    studio_audio_unload(id);
  for (int i = 0; i < 64; i++)
    if (!voices[i].id) {
      v = &voices[i];
      break;
    }
  if (!v) {
    snprintf(error, 256, "64 simultaneous sounds maximum");
    return 0;
  }
  ma_result r = ma_sound_init_from_file(&engine, path, MA_SOUND_FLAG_STREAM,
                                        NULL, NULL, &v->sound);
  if (r != MA_SUCCESS) {
    snprintf(error, 256, "Cannot decode audio (%d)", (int)r);
    return 0;
  }
  v->id = id;
  snprintf(v->path, sizeof(v->path), "%s", path);
  return 1;
}
int studio_audio_load_memory(uint64_t id, const void *data, size_t size,
                             char error[256]) {
  if (!id || !data || !size || size > 256 * 1024 * 1024) {
    snprintf(error, 256, "Invalid embedded audio");
    return 0;
  }
  if (!ensure_engine(error))
    return 0;
  Voice *v = find_voice(id);
  if (v && v->memory && v->encoded_size == size &&
      !memcmp(v->encoded, data, size))
    return 1;
  if (v)
    studio_audio_unload(id);
  v = NULL;
  for (int i = 0; i < 64; ++i)
    if (!voices[i].id) {
      v = &voices[i];
      break;
    }
  if (!v) {
    snprintf(error, 256, "64 simultaneous sounds maximum");
    return 0;
  }
  v->encoded = malloc(size);
  if (!v->encoded) {
    snprintf(error, 256, "Cannot allocate audio data");
    return 0;
  }
  memcpy(v->encoded, data, size);
  ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 48000);
  ma_result result =
      ma_decoder_init_memory(v->encoded, size, &config, &v->decoder);
  if (result != MA_SUCCESS) {
    free(v->encoded);
    v->encoded = NULL;
    snprintf(error, 256, "Cannot decode embedded audio");
    return 0;
  }
  result =
      ma_sound_init_from_data_source(&engine, &v->decoder, 0, NULL, &v->sound);
  if (result != MA_SUCCESS) {
    ma_decoder_uninit(&v->decoder);
    free(v->encoded);
    v->encoded = NULL;
    snprintf(error, 256, "Cannot initialize embedded sound");
    return 0;
  }
  v->id = id;
  v->encoded_size = size;
  v->memory = 1;
  return 1;
}
void studio_audio_play(uint64_t id) {
  Voice *v = find_voice(id);
  if (v) {
    ma_sound_seek_to_pcm_frame(&v->sound, 0);
    ma_sound_start(&v->sound);
  }
}
void studio_audio_stop(uint64_t id) {
  Voice *v = find_voice(id);
  if (v) {
    ma_sound_stop(&v->sound);
    ma_sound_seek_to_pcm_frame(&v->sound, 0);
  }
}
void studio_audio_configure(uint64_t id, float volume, float pitch, int loop,
                            int spatial, const float p[3]) {
  Voice *v = find_voice(id);
  if (!v)
    return;
  ma_sound_set_volume(&v->sound, volume);
  ma_sound_set_pitch(&v->sound, pitch);
  ma_sound_set_looping(&v->sound, loop);
  ma_sound_set_spatialization_enabled(&v->sound, spatial);
  ma_sound_set_position(&v->sound, p[0], p[1], p[2]);
}
void studio_audio_listener(const float p[3], float volume) {
  if (!initialized)
    return;
  ma_engine_set_volume(&engine, volume);
  ma_engine_listener_set_position(&engine, 0, p[0], p[1], p[2]);
}
void studio_audio_unload(uint64_t id) {
  Voice *v = find_voice(id);
  if (v) {
    ma_sound_uninit(&v->sound);
    if (v->memory) {
      ma_decoder_uninit(&v->decoder);
      free(v->encoded);
    }
    memset(v, 0, sizeof(*v));
  }
}
void studio_audio_stop_all(void) {
  for (int i = 0; i < 64; i++)
    if (voices[i].id)
      studio_audio_stop(voices[i].id);
}
void studio_audio_shutdown(void) {
  if (!initialized)
    return;
  for (int i = 0; i < 64; i++)
    if (voices[i].id)
      studio_audio_unload(voices[i].id);
  ma_engine_uninit(&engine);
  initialized = 0;
}

int studio_audio_render(float *samples, uint32_t frames) {
  return initialized && ma_engine_read_pcm_frames(&engine, samples, frames,
                                                  NULL) == MA_SUCCESS;
}
