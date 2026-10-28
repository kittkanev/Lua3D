CXX=g++
CXXFLAGS=-Wall -I/usr/include/lua5.4 -I/usr/include/SDL2
LDFLAGS=-llua5.4 -lSDL2 -lGL -lm -ldl

all: engine

engine: main.cpp
	$(CXX) $(CXXFLAGS) -o engine main.cpp $(LDFLAGS)

clean:
	rm -f engine
