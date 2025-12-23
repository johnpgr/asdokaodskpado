#pragma once

#include "game_interface.h"

// Demo configuration
#define RAVIOLI_COUNT 8192
#define REARRANGE_INTERVAL 0.1f

struct Ravioli {
    f32 x;
    f32 y;
    u32 variant; // 0-3: which sprite in the atlas
};

struct GameState {
    b32 atlas_loaded;
    u32 atlas_texture_id;

    Ravioli raviolis[RAVIOLI_COUNT];
    f32 rearrange_timer;
    u32 rng_state; // Simple RNG state

    MemoryArena permanent_arena;
};
