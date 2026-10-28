# Lua 3D Engine

A lightweight 3D game engine written in C/C++ with integrated Lua scripting.

## Requirements

To build and run this engine, you need the following dependencies:
- GCC / G++
- Make
- Lua 5.4 (`liblua5.4-dev`)
- SDL2 (`libsdl2-dev`)
- OpenGL (`libgl1-mesa-dev`)

### Ubuntu / Debian Installation
```bash
sudo apt-get update
sudo apt-get install -y build-essential liblua5.4-dev libsdl2-dev libgl1-mesa-dev
```

## Building
To compile the engine, simply run:
```bash
make
```

## Running
After a successful build, start the engine by running:
```bash
./engine
```

## Features
- Embedded Lua 5.4 engine for scripting.
- Basic 3D rendering context using SDL2 and OpenGL.
