#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef i32 b32;
typedef size_t usize;
typedef ptrdiff_t isize;

#define ASSERT(expr)                                                           \
    do {                                                                       \
        if (!(expr)) {                                                         \
            *(volatile int*)0 = 0;                                             \
        }                                                                      \
    } while (0)

// Color type: 0xRRGGBBAA format
typedef u32 Color;

inline f32 color_r(Color c) { return ((c >> 24) & 0xFF) / 255.0f; }
inline f32 color_g(Color c) { return ((c >> 16) & 0xFF) / 255.0f; }
inline f32 color_b(Color c) { return ((c >> 8) & 0xFF) / 255.0f; }
inline f32 color_a(Color c) { return ((c >> 0) & 0xFF) / 255.0f; }

inline Color color_rgba(u8 r, u8 g, u8 b, u8 a) {
    return ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | (u32)a;
}

// Common colors
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_BLACK 0x000000FF
#define COLOR_RED 0xFF0000FF
#define COLOR_GREEN 0x00FF00FF
#define COLOR_BLUE 0x0000FFFF
#define COLOR_TRANSPARENT 0x00000000
