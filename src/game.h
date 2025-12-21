#pragma once

#include "game_interface.h"

struct GameState {
    f32 player_x;
    f32 player_y;
    f32 player_vx;
    f32 player_vy;
    u32 player_color_idx;

    MemoryArena permanent_arena;
};
