#include "audio.h"
#include <math.h>
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
typedef struct {
  uint32_t id;
  char path[512];
  ma_sound sound;
  ma_audio_buffer buffer;
  float *pcm;
  bool loaded, custom;
} Voice;
struct Audio {
  ma_engine engine;
  Voice voices[64];
  char error[256];
};
static const char *extension(const char *path) {
  const char *p = strrchr(path, '.');
  return p ? p : "";
}
static bool equal_ext(const char *a, const char *b) {
  while (*a && *b) {
    char x = *a++, y = *b++;
    if (x >= 'A' && x <= 'Z')
      x += 32;
    if (x != y)
      return false;
  }
  return !*a && !*b;
}
bool audio_extension(const char *path) {
  const char *e = extension(path);
  const char *names[] = {".wav",  ".wave", ".w64", ".rf64", ".mp3",
                         ".flac", ".ogg",  ".oga", ".aiff", ".aif",
                         ".aifc", ".au",   ".snd"};
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    if (equal_ext(e, names[i]))
      return true;
  return false;
}
static uint32_t be32(const unsigned char *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
         p[3];
}
static uint16_t be16(const unsigned char *p) {
  return (uint16_t)((unsigned)p[0] << 8 | p[1]);
}
static bool decode_pcm(const char *path, float **out, ma_uint64 *frames,
                       ma_uint32 *channels, ma_uint32 *rate) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  if (len < 24 || len > 64 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) {
    fclose(f);
    return false;
  }
  unsigned char *p = malloc((size_t)len);
  if (!p) {
    fclose(f);
    return false;
  }
  bool ok = fread(p, 1, (size_t)len, f) == (size_t)len;
  fclose(f);
  if (!ok) {
    free(p);
    return false;
  }
  size_t off = 0, bytes = 0;
  int bits = 0, encoding = 0;
  bool little = false;
  *channels = *rate = 0;
  if (!memcmp(p, "FORM", 4) &&
      (!memcmp(p + 8, "AIFF", 4) || !memcmp(p + 8, "AIFC", 4))) {
    size_t cursor = 12;
    while (cursor + 8 <= (size_t)len) {
      uint32_t size = be32(p + cursor + 4);
      size_t start = cursor + 8;
      if (size > (size_t)len - start) {
        ok = false;
        break;
      }
      if (!memcmp(p + cursor, "COMM", 4) && size >= 18) {
        *channels = be16(p + start);
        bits = be16(p + start + 6);
        uint16_t exp = be16(p + start + 8);
        uint64_t mant =
            (uint64_t)be32(p + start + 10) << 32 | be32(p + start + 14);
        double hz = ldexp((double)mant, (int)(exp & 0x7fff) - 16383 - 63);
        if (!isfinite(hz) || hz < 8000 || hz > 384000) {
          ok = false;
          break;
        }
        *rate = (ma_uint32)hz;
        encoding = 1;
        if (size >= 22 && !memcmp(p + start + 18, "sowt", 4))
          little = true;
        else if (size >= 22 && !memcmp(p + start + 18, "fl32", 4))
          encoding = 2;
        else if (size >= 22 && memcmp(p + start + 18, "NONE", 4) &&
                 memcmp(p + start + 18, "twos", 4)) {
          ok = false;
          break;
        }
      } else if (!memcmp(p + cursor, "SSND", 4) && size >= 8) {
        uint32_t shift = be32(p + start);
        if (shift > size - 8) {
          ok = false;
          break;
        }
        off = start + 8 + shift;
        bytes = size - 8 - shift;
      }
      cursor = start + size + (size & 1);
    }
  } else if (!memcmp(p, ".snd", 4)) {
    off = be32(p + 4);
    bytes = be32(p + 8);
    uint32_t fmt = be32(p + 12);
    *rate = be32(p + 16);
    *channels = be32(p + 20);
    if (bytes == UINT32_MAX && off <= (size_t)len)
      bytes = (size_t)len - off;
    if (fmt >= 2 && fmt <= 5) {
      bits = (int)(fmt - 1) * 8;
      encoding = 1;
    } else if (fmt == 6) {
      bits = 32;
      encoding = 2;
    } else if (fmt == 7) {
      bits = 64;
      encoding = 2;
    }
  } else
    ok = false;
  if (!ok || !encoding || !bytes || off > (size_t)len ||
      bytes > (size_t)len - off || *channels < 1 || *channels > 8 ||
      *rate < 8000 || *rate > 384000 ||
      (bits != 8 && bits != 16 && bits != 24 && bits != 32 && bits != 64)) {
    free(p);
    return false;
  }
  size_t samples = bytes / (size_t)(bits / 8);
  if (samples > 16 * 1024 * 1024) {
    free(p);
    return false;
  }
  float *pcm = malloc(samples * sizeof(float));
  if (!pcm) {
    free(p);
    return false;
  }
  for (size_t i = 0; i < samples; i++) {
    const unsigned char *b = p + off + i * (size_t)(bits / 8);
    uint64_t v = 0;
    for (int k = 0; k < bits / 8; k++)
      v |= (uint64_t)b[k] << (8 * (little ? k : bits / 8 - 1 - k));
    double value = 0;
    if (encoding == 2) {
      if (bits == 32) {
        uint32_t x = (uint32_t)v;
        float xval;
        memcpy(&xval, &x, 4);
        value = xval;
      } else
        memcpy(&value, &v, 8);
    } else {
      int64_t signed_value = (int64_t)(v << (64 - bits));
      signed_value >>= 64 - bits;
      value = (double)signed_value / ldexp(1, bits - 1);
    }
    pcm[i] = isfinite(value) ? (float)fmax(-1, fmin(1, value)) : 0;
  }
  free(p);
  *frames = (ma_uint64)(samples / *channels);
  *out = pcm;
  return true;
}
Audio *audio_init(void) {
  Audio *a = calloc(1, sizeof(*a));
  if (!a)
    return NULL;
  ma_engine_config config = ma_engine_config_init();
  config.listenerCount = 1;
  if (getenv("LUNORYX_AUDIO_NULL")) {
    config.noDevice = MA_TRUE;
    config.channels = 2;
    config.sampleRate = 48000;
  }
  if (ma_engine_init(&config, &a->engine) != MA_SUCCESS) {
    free(a);
    return NULL;
  }
  return a;
}
static void voice_free(Voice *v) {
  if (v->loaded)
    ma_sound_uninit(&v->sound);
  if (v->custom)
    ma_audio_buffer_uninit(&v->buffer);
  free(v->pcm);
  memset(v, 0, sizeof(*v));
}
void audio_free(Audio *a) {
  if (!a)
    return;
  for (int i = 0; i < 64; i++)
    voice_free(&a->voices[i]);
  ma_engine_uninit(&a->engine);
  free(a);
}
bool audio_load(Audio *a, const Object *o, char error[256]) {
  if (!a) {
    snprintf(error, 256, "Audio device is unavailable");
    return false;
  }
  Voice *v = NULL;
  for (int i = 0; i < 64; i++)
    if (a->voices[i].id == o->id) {
      v = &a->voices[i];
      break;
    }
  if (v && v->loaded && !strcmp(v->path, o->sound))
    return true;
  if (!v)
    for (int i = 0; i < 64; i++)
      if (!a->voices[i].id) {
        v = &a->voices[i];
        break;
      }
  if (!v) {
    snprintf(error, 256, "64 sound voice limit reached");
    return false;
  }
  voice_free(v);
  ma_result result;
  const char *e = extension(o->sound);
  bool custom = equal_ext(e, ".aif") || equal_ext(e, ".aiff") ||
                equal_ext(e, ".aifc") || equal_ext(e, ".au") ||
                equal_ext(e, ".snd");
  if (custom) {
    ma_uint64 frames;
    ma_uint32 channels, rate;
    if (!decode_pcm(o->sound, &v->pcm, &frames, &channels, &rate)) {
      snprintf(error, 256, "Unsupported AIFF/AU encoding or invalid file");
      return false;
    }
    ma_audio_buffer_config c = ma_audio_buffer_config_init(
        ma_format_f32, channels, frames, v->pcm, NULL);
    c.sampleRate = rate;
    result = ma_audio_buffer_init(&c, &v->buffer);
    if (result == MA_SUCCESS) {
      v->custom = true;
      result = ma_sound_init_from_data_source(&a->engine, &v->buffer, 0, NULL,
                                              &v->sound);
    }
  } else
    result = ma_sound_init_from_file(&a->engine, o->sound, MA_SOUND_FLAG_STREAM,
                                     NULL, NULL, &v->sound);
  if (result != MA_SUCCESS) {
    voice_free(v);
    snprintf(error, 256, "Cannot decode audio file (%d)", (int)result);
    return false;
  }
  v->id = o->id;
  v->loaded = true;
  snprintf(v->path, sizeof(v->path), "%s", o->sound);
  return true;
}
void audio_command(void *data, uint32_t id, int action) {
  Audio *a = data;
  if (!a)
    return;
  for (int i = 0; i < 64; i++)
    if (a->voices[i].id == id && a->voices[i].loaded) {
      if (action == 0) {
        ma_sound_seek_to_pcm_frame(&a->voices[i].sound, 0);
        ma_sound_start(&a->voices[i].sound);
      } else {
        ma_sound_stop(&a->voices[i].sound);
        ma_sound_seek_to_pcm_frame(&a->voices[i].sound, 0);
      }
    }
}
void audio_stop_all(Audio *a) {
  if (!a)
    return;
  for (int i = 0; i < 64; i++)
    if (a->voices[i].loaded)
      ma_sound_stop(&a->voices[i].sound);
}
void audio_update(Audio *a, const Scene *s, V3 listener, float master) {
  if (!a)
    return;
  ma_engine_set_volume(&a->engine, master);
  ma_engine_listener_set_position(&a->engine, 0, listener.x, listener.y,
                                  listener.z);
  for (int i = 0; i < 64; i++) {
    Voice *v = &a->voices[i];
    if (!v->loaded)
      continue;
    const Object *o = scene_get(s, v->id);
    if (!o || strcmp(v->path, o->sound)) {
      voice_free(v);
      continue;
    }
    ma_sound_set_volume(&v->sound, o->volume);
    ma_sound_set_pitch(&v->sound, o->pitch);
    ma_sound_set_looping(&v->sound, o->loop);
    ma_sound_set_spatialization_enabled(&v->sound, o->spatial);
    ma_sound_set_min_distance(&v->sound, o->min_distance);
    ma_sound_set_max_distance(&v->sound, o->max_distance);
    V3 p = point(object_world(s, o), v3(0, 0, 0));
    ma_sound_set_position(&v->sound, p.x, p.y, p.z);
  }
}

bool audio_mix(Audio *a, float *output, size_t frames) {
  return a && ma_engine_read_pcm_frames(&a->engine, output, (ma_uint64)frames,
                                        NULL) == MA_SUCCESS;
}
