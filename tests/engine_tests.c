#include "lunoryx.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void run(Runtime *r, const char *code) {
  if (!runtime_command(r, code)) {
    fprintf(stderr, "%s\n", r->output);
    abort();
  }
}
int main(void) {
  Scene s;
  scene_init(&s);
  Runtime r;
  assert(runtime_init(&r, &s, false));
  run(&r,
      "assert(os==nil and io==nil and package==nil); local "
      "a=add_object('Cube','cube'); assert(a>0); "
      "assert(set_position(a,1,2,3)); local x,y,z=get_position(a); assert(x==1 "
      "and y==2 and z==3); assert(not set_position(99999999,1,2,3)); "
      "assert(not pcall(set_position,a,0/0,2,3)); assert(not "
      "pcall(set_scale,a,math.huge,2,3)); assert(not set_scale(a,0,1,1)); "
      "set_property(a,'hp',100); assert(get_property(a,'hp')==100); local "
      "b=duplicate_object(a); assert(b~=a); assert(remove_object(a)); "
      "assert(set_color(b,.1,.2,.3)); print('API ok')");
  assert(s.count == 1);
  assert(!runtime_command(&r, "while true do end"));
  assert(strstr(r.output, "instruction budget exceeded"));
  assert(lua_gettop(r.L) == 0);
  Script *script = scene_script(&s, "debug.lua");
  assert(script);
  snprintf(script->code, LX_CODE_SIZE,
           "local x = 1\nx = x + 2\nprint(x)\nfunction on_update(dt) counter = "
           "(counter or 0) + 1 end\n");
  script->breakpoints[0] = 2;
  script->break_count = 1;
  assert(runtime_run(&r, 0));
  assert(r.state == 2 && r.line == 2);
  runtime_watch(&r, "x");
  assert(strstr(r.watch, "1"));
  runtime_continue(&r, true);
  assert(r.state == 2 && r.line == 3);
  runtime_watch(&r, "x");
  assert(strstr(r.watch, "3"));
  runtime_continue(&r, false);
  assert(r.state == 0);
  assert(strstr(r.output, "3\n"));
  runtime_update(&r, .01f);
  run(&r, "assert(counter==1)");
  runtime_update(&r, .01f);
  run(&r, "assert(counter==2)");
  assert(runtime_analyze(&r, 0));
  snprintf(script->code, LX_CODE_SIZE, "function (");
  assert(!runtime_analyze(&r, 0));
  run(&r, "register_tool('Make part', function() add_object('Plugin "
          "part','sphere') end)");
  assert(r.tool_count == 1);
  size_t count = s.count;
  runtime_tool(&r, 0);
  assert(s.count == count + 1);
  for (int i = 0; i < 100; i++) {
    run(&r, "print(true,nil,123)");
    assert(lua_gettop(r.L) == 0);
  }
  runtime_free(&r);
  scene_free(&s);
  puts("PASS: Lua stable IDs, sandbox, instruction limit, debugger stepping, "
       "locals, callbacks and plugin tools");
  return 0;
}
