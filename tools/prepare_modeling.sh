#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/deps
archive=build/deps/manifold.tar.gz
if ! test -f "$archive"; then
  curl --fail --location --retry 3 https://api.github.com/repos/elalish/manifold/tarball/ce50d78021d64507f89e8c9fc2c2e51018117857 -o "$archive"
fi
echo 'c997e35eb62f6ad2b8665cdaa9f0db405ad2f244416a9633fbfce629fb5f471d  build/deps/manifold.tar.gz' | sha256sum --check
mkdir -p build/deps/manifold
tar --no-same-owner -xzf "$archive" --strip-components=1 -C build/deps/manifold
cmake_bin=${CMAKE:-cmake}
build_dir=build/deps/manifold-linux
options=()
if [ "${1:-}" = windows ]; then
  build_dir=build/deps/manifold-windows
  options+=(-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++-posix)
fi
"$cmake_bin" -S build/deps/manifold -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DMANIFOLD_CROSS_SECTION=OFF -DMANIFOLD_PAR=OFF -DMANIFOLD_TEST=OFF -DMANIFOLD_CBIND=OFF -DMANIFOLD_PYBIND=OFF "${options[@]}"
"$cmake_bin" --build "$build_dir" --parallel 2
cp build/deps/manifold/LICENSE docs/licenses/Manifold.txt
