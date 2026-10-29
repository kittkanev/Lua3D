CXX = g++
CXXFLAGS = -Wall -std=c++11 -I/usr/include/lua5.4 -I/usr/include/SDL2 -I./imgui -I./imgui/backends -I/usr/include/stb
LDFLAGS = -llua5.4 -lSDL2 -lGL -lm -ldl

IMGUI_SRC = imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_sdl2.cpp imgui/backends/imgui_impl_opengl3.cpp
SRC = main.cpp $(IMGUI_SRC)

all: engine

engine: $(SRC)
	$(CXX) $(CXXFLAGS) -o engine $(SRC) $(LDFLAGS)

clean:
	rm -f engine
