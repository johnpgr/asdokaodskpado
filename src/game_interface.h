#pragma once

#include "lib/def.h"
#include "lib/memory_arena.h"

#define GAME_CODE_VERSION 1

#define MB(value) ((value) * 1024LL * 1024LL)
#define KB(value) ((value) * 1024LL)

struct GameMemory {
    b32 is_initialized;
    u64 permanent_storage_size;
    void* permanent_storage;
    u64 transient_storage_size;
    void* transient_storage;
};

struct GameButtonState {
    b32 ended_down;
    i32 half_transition_count;
};

struct GameInput {
    f32 dt_for_frame;

    GameButtonState move_up;
    GameButtonState move_down;
    GameButtonState move_left;
    GameButtonState move_right;
    GameButtonState action;

    i32 mouse_x;
    i32 mouse_y;
    GameButtonState mouse_buttons[3];
};

enum RenderCommandType {
    RenderCommand_Clear,
    RenderCommand_Rect,
    RenderCommand_Sprite,
};

struct RenderCommandHeader {
    RenderCommandType type;
};

struct RenderCommandClear {
    RenderCommandHeader header;
    Color color;
};

struct RenderCommandRect {
    RenderCommandHeader header;
    f32 x, y, w, h;
    Color color;
};

struct RenderCommandSprite {
    RenderCommandHeader header;
    f32 x, y, w, h;
    u32 texture_id;
    Color tint;
};

struct RenderCommands {
    u32 width;
    u32 height;
    MemoryArena arena;
};

#define GAME_UPDATE_AND_RENDER(name)                                           \
    void name(GameMemory* memory, GameInput* input, RenderCommands* render_cmds)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render_func);

struct Texture2D {
    u32 id;
    i32 width;
    i32 height;
    void* pixels;
};

struct GameCode {
    game_update_and_render_func* update_and_render;
    b32 is_valid;
    u32 version;
};
