#!/bin/bash

set -e

COMPILER_FLAGS="-g -O0 -Wall -Wextra -std=c++23 -fno-exceptions -fno-rtti -Wno-address-of-temporary"
INCLUDE_FLAGS="-Iinclude -Isrc"

mkdir -p out

echo "Building game.dylib..."
touch lock.tmp
clang++ -x objective-c++ $COMPILER_FLAGS $INCLUDE_FLAGS \
    -dynamiclib src/game.cpp -o out/libgame.dylib
rm -f lock.tmp

echo "Building main..."
clang++ -x objective-c++ $COMPILER_FLAGS $INCLUDE_FLAGS \
    src/main.cpp src/renderer.cpp \
    -o out/main \
    -fobjc-arc \
    -framework Metal \
    -framework MetalKit \
    -framework Cocoa \
    -framework QuartzCore

echo "Build complete!"
