CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -O2 -std=c++11 -MMD -MP -I/usr/include/lua5.4 -I/usr/include/SDL2 -I./imgui -I./imgui/backends -isystem ./win_deps -isystem ./win_deps/nlohmann
LDFLAGS = -llua5.4 -lSDL2 -lGL -lm -ldl -lpthread

CXXFLAGS += $(EXTRA_CXXFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

IMGUI_SRC = \
	imgui/imgui.cpp \
	imgui/imgui_demo.cpp \
	imgui/imgui_draw.cpp \
	imgui/imgui_tables.cpp \
	imgui/imgui_widgets.cpp \
	imgui/backends/imgui_impl_sdl2.cpp \
	imgui/backends/imgui_impl_opengl3.cpp

SRC = main.cpp extensions/ImGuizmo/ImGuizmo.cpp $(IMGUI_SRC)
OBJS = $(SRC:.cpp=.o) extensions/audio/audio_backend.o
DEPS = $(OBJS:.o=.d) tests/engine_tests.d

all: engine

engine: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f engine $(OBJS) $(DEPS) tests/core_tests tests/engine_tests


.PHONY: all clean test test-integration

test: tests/core_tests
	./tests/core_tests

tests/core_tests: tests/core_tests.cpp scene_core.h scene_tools.h editor_transform.h
	$(CXX) -std=c++11 -Wall -Wextra -pedantic -Werror -O2 $< -o $@

test-integration: tests/engine_tests
	./tests/engine_tests

tests/engine_tests: tests/engine_tests.cpp main.cpp scene_core.h scene_tools.h engine_version.h $(filter-out main.o,$(OBJS))
	$(CXX) $(CXXFLAGS) tests/engine_tests.cpp $(filter-out main.o,$(OBJS)) -o $@ $(LDFLAGS)

extensions/audio/audio_backend.o: extensions/audio/audio_backend.c extensions/audio/audio_backend.h extensions/audio/miniaudio.h extensions/audio/stb_vorbis.c
	$(CC) -std=c11 -Wall -Wextra -Werror -Wno-unused-function -O2 $(EXTRA_CXXFLAGS) -c $< -o $@

tests/studio_audio_tests: tests/studio_audio_tests.c extensions/audio/audio_backend.o
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 $^ -lm -ldl -lpthread $(EXTRA_LDFLAGS) -o $@
