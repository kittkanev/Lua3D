CC = gcc
CPPFLAGS = -Inative -Ivendor $(shell pkg-config --cflags sdl2 lua5.4)
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -MMD -MP $(EXTRA_CFLAGS)
LDLIBS = $(shell pkg-config --libs sdl2 lua5.4) -lGL -lm -ldl -lpthread
LDFLAGS += $(EXTRA_LDFLAGS)
CORE = native/math.c native/mesh.c native/csg.c native/scene.c native/physics.c native/project.c native/assets.c vendor/cJSON.c
RUNTIME = native/runtime.c native/mcp.c
APP = native/main.c native/editor.c native/renderer.c native/settings.c native/audio.c native/player.c
OBJS = $(addprefix build/linux/,$(CORE:.c=.o) $(RUNTIME:.c=.o) $(APP:.c=.o))
all: engine
engine: $(OBJS)
	$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@
build/linux/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/linux/native/audio.o: CFLAGS += -Wno-unused-function
build/linux/native/main.o: CFLAGS += -Wno-unused-function
build/linux/tests/core_tests: tests/core_tests.c $(CORE)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -lm -o $@
build/linux/tests/engine_tests: tests/engine_tests.c $(CORE) $(RUNTIME)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@
test: build/linux/tests/core_tests
	./build/linux/tests/core_tests
test-integration: build/linux/tests/engine_tests
	./build/linux/tests/engine_tests
clean:
	rm -rf build/linux engine
-include $(OBJS:.o=.d)
.PHONY: all clean test test-integration
build/linux/tests/audio_tests: tests/audio_tests.c $(CORE) native/audio.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function $^ $(LDFLAGS) -lm -ldl -lpthread -o $@
build/linux/tests/editor_tests: tests/editor_tests.c $(CORE) $(RUNTIME) native/editor.c native/settings.c native/player.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-function $^ $(LDFLAGS) $(LDLIBS) -o $@
test-editor: build/linux/tests/editor_tests
	./build/linux/tests/editor_tests
