@echo off

setlocal

set COMPILER_FLAGS=-g -O0 -Wall -Wextra -std=c++23 -fno-exceptions -fno-rtti -Wno-address-of-temporary
set INCLUDE_FLAGS=-Iinclude -Isrc

if not exist out mkdir out

echo Building game.dll...
echo lock > lock.tmp
clang++ %COMPILER_FLAGS% %INCLUDE_FLAGS% -shared src/game.cpp -o out/game.dll
del lock.tmp

echo Building main.exe...
clang++ %COMPILER_FLAGS% %INCLUDE_FLAGS% ^
    src/main.win32.cpp ^
    src/renderer.opengl.cpp ^
    src/util/loader.opengl.cpp ^
    -o out/main.exe ^
    -lopengl32 -lgdi32 -luser32

echo Build complete!
echo Run with: out\main.exe
