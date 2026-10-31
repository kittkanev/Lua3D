#include "../extensions/audio/audio_backend.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  assert(argc > 1);
  for (int i = 1; i < argc; i++) {
    char error[256];
    if (!studio_audio_load((uint64_t)i, argv[i], error)) {
      fprintf(stderr, "%s: %s\n", argv[i], error);
      return 1;
    }
    float position[3] = {0, 0, 0};
    studio_audio_configure((uint64_t)i, 1, 1, 0, 0, position);
    studio_audio_play((uint64_t)i);
    float samples[8192];
    assert(studio_audio_render(samples, 4096));
    double energy = 0;
    for (int k = 0; k < 8192; k++)
      energy += fabs(samples[k]);
    assert(energy > .01);
    studio_audio_unload((uint64_t)i);
    FILE *file = fopen(argv[i], "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long bytes = ftell(file);
    rewind(file);
    unsigned char *encoded = malloc((size_t)bytes);
    assert(encoded && fread(encoded, 1, (size_t)bytes, file) == (size_t)bytes);
    fclose(file);
    assert(
        studio_audio_load_memory((uint64_t)i, encoded, (size_t)bytes, error));
    free(encoded);
    studio_audio_play((uint64_t)i);
    assert(studio_audio_render(samples, 4096));
    energy = 0;
    for (int k = 0; k < 8192; k++)
      energy += fabs(samples[k]);
    assert(energy > .01);
    studio_audio_unload((uint64_t)i);
    printf("PASS: Studio audio decode/mix %s\n", argv[i]);
  }
  studio_audio_shutdown();
  return 0;
}
