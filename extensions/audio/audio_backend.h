#ifndef STUDIO_AUDIO_BACKEND_H
#define STUDIO_AUDIO_BACKEND_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int studio_audio_render(float *samples,uint32_t frames);
int studio_audio_load(uint64_t id,const char *path,char error[256]);
void studio_audio_play(uint64_t id);void studio_audio_stop(uint64_t id);
void studio_audio_configure(uint64_t id,float volume,float pitch,int loop,int spatial,const float position[3]);
void studio_audio_listener(const float position[3],float volume);
void studio_audio_unload(uint64_t id);void studio_audio_stop_all(void);void studio_audio_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
