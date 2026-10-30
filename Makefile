CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++11 -MMD -MP -I/usr/include/lua5.4 -I/usr/include/SDL2 -I./imgui -I./imgui/backends -I./win_deps -I./win_deps/nlohmann
LDFLAGS = -llua5.4 -lSDL2 -lGL -lm -ldl

IMGUI_SRC = \
	imgui/imgui.cpp \
	imgui/imgui_demo.cpp \
	imgui/imgui_draw.cpp \
	imgui/imgui_tables.cpp \
	imgui/imgui_widgets.cpp \
	imgui/backends/imgui_impl_sdl2.cpp \
	imgui/backends/imgui_impl_opengl3.cpp

SRC = main.cpp $(IMGUI_SRC)
OBJS = $(SRC:.cpp=.o)
DEPS = $(OBJS:.o=.d)

all: engine

engine: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f engine $(OBJS) $(DEPS) *.glb
