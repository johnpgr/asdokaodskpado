#!/bin/bash

set -e

# Use clang++ if available, otherwise fall back to g++
if command -v clang++ &> /dev/null; then
    CXX=clang++
else
    CXX=g++
fi

COMPILER_FLAGS="-g -O0 -Wall -Wextra -std=c++23 -fno-exceptions -fno-rtti -Wno-address-of-temporary"
INCLUDE_FLAGS="-Iinclude -Isrc"

mkdir -p out

echo "Using compiler: $CXX"

echo "Building libgame.so..."
touch lock.tmp
$CXX $COMPILER_FLAGS $INCLUDE_FLAGS \
    -shared -fPIC src/game.cpp -o out/libgame.so
rm -f lock.tmp

echo "Building main..."
$CXX $COMPILER_FLAGS $INCLUDE_FLAGS \
    src/main.linux.cpp \
    src/renderer.opengl.cpp \
    src/util/loader.opengl.cpp \
    -o out/main \
    -lGL -lX11 -ldl -lpthread

echo "Build complete!"
echo "Run with: ./out/main"
