#ifndef LX_AUDIO_H
#define LX_AUDIO_H
#include "lunoryx.h"
typedef struct Audio Audio;
Audio *audio_init(void);
bool audio_mix(Audio *a, float *output, size_t frames);
void audio_free(Audio *a);
void audio_update(Audio *a, const Scene *s, V3 listener, float master);
void audio_command(void *a, uint32_t id, int action);
bool audio_load(Audio *a, const Object *o, char error[256]);
void audio_stop_all(Audio *a);
bool audio_extension(const char *path);
#endif
