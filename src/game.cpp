#include "game.h"
#include "util/bmp_loader.h"

// Simple xorshift RNG
static u32 xorshift32(u32* state) {
    u32 x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static f32 random_range(u32* state, f32 min, f32 max) {
    return min + (f32)(xorshift32(state) % 10000) / 10000.0f * (max - min);
}

// Atlas UV coordinates for each ravioli variant
// Atlas is 32x32 with 4 sprites in 2x2 grid (16x16 each)
struct SpriteUV {
    f32 u0, v0, u1, v1;
};

static const SpriteUV RAVIOLI_UVS[4] = {
    {0.0f, 0.0f, 0.5f, 0.5f}, // Top-left: Green (happy)
    {0.5f, 0.0f, 1.0f, 0.5f}, // Top-right: Cyan (happy)
    {0.0f, 0.5f, 0.5f, 1.0f}, // Bottom-left: Red (angry)
    {0.5f, 0.5f, 1.0f, 1.0f}, // Bottom-right: Blue (sad)
};

static void randomize_ravioli_positions(
    GameState* state,
    u32 screen_width,
    u32 screen_height
) {
    f32 sprite_size = 16.0f;
    for (i32 i = 0; i < RAVIOLI_COUNT; i++) {
        state->raviolis[i].x =
            random_range(&state->rng_state, 0, (f32)screen_width - sprite_size);
        state->raviolis[i].y = random_range(
            &state->rng_state,
            0,
            (f32)screen_height - sprite_size
        );
        state->raviolis[i].variant = xorshift32(&state->rng_state) % 4;
    }
}

extern "C" {

GAME_UPDATE_AND_RENDER(game_update_and_render) {
    GameState* state = (GameState*)memory->permanent_storage;

    if (!memory->is_initialized) {
        state->permanent_arena = MemoryArena::make(
            (u8*)memory->permanent_storage + sizeof(GameState),
            memory->permanent_storage_size - sizeof(GameState)
        );

        state->atlas_loaded = false;
        state->rng_state = 12345; // Seed
        state->rearrange_timer = REARRANGE_INTERVAL;

        // Initialize ravioli positions
        randomize_ravioli_positions(
            state,
            render_cmds->width,
            render_cmds->height
        );

        memory->is_initialized = true;
    }

    // Load atlas texture on first frame (after renderer is available)
    // Note: This happens in the game DLL context, texture loading is deferred
    // For now, we'll use a simple approach - load via platform if not loaded

    f32 dt = input->dt_for_frame;

    // Update rearrange timer
    state->rearrange_timer -= dt;
    if (state->rearrange_timer <= 0.0f) {
        randomize_ravioli_positions(
            state,
            render_cmds->width,
            render_cmds->height
        );
        state->rearrange_timer = REARRANGE_INTERVAL;
    }

    // Clear command
    RenderCommandClear* clear_cmd =
        render_cmds->arena.push_struct<RenderCommandClear>();
    clear_cmd->header.type = RenderCommand_Clear;
    clear_cmd->color = 0x1A1A1AFF; // Dark gray

    // Render all raviolis
    f32 sprite_size = 16.0f;
    for (i32 i = 0; i < RAVIOLI_COUNT; i++) {
        const Ravioli& r = state->raviolis[i];
        const SpriteUV& uv = RAVIOLI_UVS[r.variant];

        RenderCommandAtlasSprite* cmd =
            render_cmds->arena.push_struct<RenderCommandAtlasSprite>();
        cmd->header.type = RenderCommand_AtlasSprite;
        cmd->x = r.x;
        cmd->y = r.y;
        cmd->w = sprite_size;
        cmd->h = sprite_size;
        cmd->u0 = uv.u0;
        cmd->v0 = uv.v0;
        cmd->u1 = uv.u1;
        cmd->v1 = uv.v1;
        cmd->texture_id = state->atlas_texture_id; // Will be set by platform
        cmd->tint = 0xFFFFFFFF;                    // White (no tint)
    }
}

u32 game_get_version() { return GAME_CODE_VERSION; }
}
