#include "game.h"

extern "C" {

GAME_UPDATE_AND_RENDER(game_update_and_render) {
    GameState* state = (GameState*)memory->permanent_storage;

    if (!memory->is_initialized) {
        state->player_x = 400.0f;
        state->player_y = 300.0f;
        state->player_vx = 150.0f;
        state->player_vy = 100.0f;
        state->player_color_idx = 0;

        state->permanent_arena = MemoryArena::make(
            (u8*)memory->permanent_storage + sizeof(GameState),
            memory->permanent_storage_size - sizeof(GameState)
        );

        memory->is_initialized = true;
    }

    // MemoryArena transient_arena =
    //     MemoryArena::make(memory->transient_storage,
    //     memory->transient_storage_size);

    f32 dt = input->dt_for_frame;

    if (input->action.ended_down && input->action.half_transition_count > 0) {
        state->player_color_idx = (state->player_color_idx + 1) % 3;
    }

    f32 impulse = 50.0f;
    if (input->move_up.ended_down) {
        state->player_vy -= impulse * dt;
    }
    if (input->move_down.ended_down) {
        state->player_vy += impulse * dt;
    }
    if (input->move_left.ended_down) {
        state->player_vx -= impulse * dt;
    }
    if (input->move_right.ended_down) {
        state->player_vx += impulse * dt;
    }

    state->player_x += state->player_vx * dt;
    state->player_y += state->player_vy * dt;

    f32 player_size = 32.0f;
    if (state->player_x < 0.0f) {
        state->player_x = 0.0f;
        state->player_vx = -state->player_vx;
    }
    if (state->player_x + player_size > (f32)render_cmds->width) {
        state->player_x = (f32)render_cmds->width - player_size;
        state->player_vx = -state->player_vx;
    }
    if (state->player_y < 0.0f) {
        state->player_y = 0.0f;
        state->player_vy = -state->player_vy;
    }
    if (state->player_y + player_size > (f32)render_cmds->height) {
        state->player_y = (f32)render_cmds->height - player_size;
        state->player_vy = -state->player_vy;
    }

    RenderCommandClear* clear_cmd =
        render_cmds->arena.push_struct<RenderCommandClear>();
    clear_cmd->header.type = RenderCommand_Clear;
    clear_cmd->color = 0x1A1A1AFF; // Dark gray

    Color colors[3] = {
        COLOR_BLUE,
        COLOR_GREEN,
        COLOR_BLUE,
    };

    RenderCommandRect* rect_cmd =
        render_cmds->arena.push_struct<RenderCommandRect>();
    rect_cmd->header.type = RenderCommand_Rect;
    rect_cmd->x = state->player_x;
    rect_cmd->y = state->player_y;
    rect_cmd->w = player_size;
    rect_cmd->h = player_size;
    rect_cmd->color = colors[state->player_color_idx];
}

u32 game_get_version() { return GAME_CODE_VERSION; }
}
