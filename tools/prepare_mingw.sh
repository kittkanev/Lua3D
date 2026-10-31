#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/deps
cd build/deps
curl --fail --location --retry 3 https://www.lua.org/ftp/lua-5.4.6.tar.gz -o lua.tar.gz
curl --fail --location --retry 3 https://github.com/libsdl-org/SDL/releases/download/release-2.30.0/SDL2-devel-2.30.0-mingw.tar.gz -o sdl.tar.gz
sha256sum --check <<'HASHES'
7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88  lua.tar.gz
d44bd8cd869219f43634a898d7735d0c92d4b2cca129f709dc8b98402b36d5f3  sdl.tar.gz
HASHES
tar --no-same-owner -xzf lua.tar.gz
tar --no-same-owner -xzf sdl.tar.gz
cd lua-5.4.6/src
mkdir -p mingw-objects
for source in *.c; do
    case "$source" in lua.c|luac.c) continue ;; esac
    x86_64-w64-mingw32-gcc -O2 -c "$source" -o "mingw-objects/${source%.c}.o"
done
x86_64-w64-mingw32-ar rcs liblua.a mingw-objects/*.o
