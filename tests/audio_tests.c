#include "audio.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(int argc, char **argv) {
  assert(argc > 1);
  Audio *a = audio_init();
  assert(a);
  Scene s;
  scene_init(&s);
  Object *o = scene_add(&s, LX_SOUND, "Sound");
  assert(o);
  char error[256];
  for (int i = 1; i < argc; i++) {
    snprintf(o->sound, sizeof(o->sound), "%s", argv[i]);
    if (!audio_load(a, o, error)) {
      fprintf(stderr, "%s: %s\n", argv[i], error);
      return 1;
    }
    audio_command(a, o->id, 0);
    float samples[8192];
    assert(audio_mix(a, samples, 4096));
    double energy = 0;
    for (int k = 0; k < 8192; k++)
      energy += fabs(samples[k]);
    assert(energy > 0);
    audio_command(a, o->id, 1);
    printf("PASS: decoded and mixed %s\n", argv[i]);
  }
  audio_free(a);
  scene_free(&s);
  return 0;
}
