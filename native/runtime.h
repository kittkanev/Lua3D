#ifndef LX_RUNTIME_H
#define LX_RUNTIME_H
#include "lunoryx.h"
#include <lua.h>
typedef void (*SoundCommand)(void *userdata, uint32_t id, int action);
typedef struct {
  Scene *scene;
  lua_State *L, *thread;
  int locals_ref;
  bool exhausted;
  int thread_ref, active_script, callbacks[LX_MAX_SCRIPTS];
  int state, line, skip_line;
  bool step, loading_callback;
  int budget;
  size_t memory;
  char output[65536], stack[8192], watch[2048], analysis[2048];
  double script_ms[LX_MAX_SCRIPTS];
  unsigned calls[LX_MAX_SCRIPTS];
  SoundCommand sound;
  void *sound_userdata;
  char tool_names[32][64];
  int tool_refs[32], tool_count;
} Runtime;
bool runtime_init(Runtime *r, Scene *s, bool trusted);
void runtime_free(Runtime *r);
bool runtime_run(Runtime *r, int script);
void runtime_update(Runtime *r, float dt);
void runtime_continue(Runtime *r, bool step);
void runtime_stop(Runtime *r);
bool runtime_command(Runtime *r, const char *code);
void runtime_log(Runtime *r, const char *text);
bool runtime_analyze(Runtime *r, int script);
void runtime_watch(Runtime *r, const char *expression);
void runtime_tool(Runtime *r, int index);
#endif
