#pragma once

#include "lib/def.h"

struct Renderer;

Renderer* renderer_init(void);

void renderer_begin_frame(
    Renderer* renderer,
    u32 width,
    u32 height,
    u32 target_width,
    u32 target_height
);
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

void renderer_draw_atlas_sprite(
    Renderer* renderer,
    f32 x,
    f32 y,
    f32 w,
    f32 h,
    f32 u0,
    f32 v0,
    f32 u1,
    f32 v1,
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
