#pragma once

#include "lib/def.h"
#include "lib/memory_arena.h"

struct Renderer;

Renderer* renderer_init(MemoryArena* arena);

void renderer_begin_frame(Renderer* renderer, u32 width, u32 height);
void renderer_end_frame(Renderer* renderer);

void renderer_draw_rect(
    Renderer* renderer,
    f32 x,
    f32 y,
    f32 w,
    f32 h,
    Color color
);

void renderer_draw_sprite(
    Renderer* renderer,
    f32 x,
    f32 y,
    f32 w,
    f32 h,
    u32 texture_id,
    Color tint
);

u32 renderer_load_texture(
    Renderer* renderer,
    void* pixels,
    i32 width,
    i32 height,
    i32 channels
);

void renderer_set_clear_color(Renderer* renderer, Color color);
