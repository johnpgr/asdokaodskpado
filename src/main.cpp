#define SOKOL_IMPL
#define SOKOL_METAL
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"

#include "game_interface.h"
#include "platform/dll_loader.h"
#include "platform/file_watcher.h"
#include "platform/memory.h"
#include "renderer.h"

#include "sokol/sokol_log.h"

static GameMemory global_game_memory = {};
static GameInput global_game_input = {};
static RenderCommands global_render_commands = {};
static PlatformDLL global_game_dll = {};
static GameCode global_game_code = {};
static Renderer* global_renderer = nullptr;

static void process_button_event(GameButtonState* state, b32 is_down) {
    if (state->ended_down != is_down) {
        state->ended_down = is_down;
        state->half_transition_count++;
    }
}

static void reset_input_half_transitions(GameInput* input) {
    input->move_up.half_transition_count = 0;
    input->move_down.half_transition_count = 0;
    input->move_left.half_transition_count = 0;
    input->move_right.half_transition_count = 0;
    input->action.half_transition_count = 0;
    for (i32 i = 0; i < 3; i++) {
        input->mouse_buttons[i].half_transition_count = 0;
    }
}

static void
execute_render_commands(Renderer* renderer, RenderCommands* commands) {
    u8* base = (u8*)commands->arena.base;
    u8* at = base;
    u8* end = base + commands->arena.used;

    while (at < end) {
        RenderCommandHeader* header = (RenderCommandHeader*)at;

        switch (header->type) {
            case RenderCommand_Clear: {
                RenderCommandClear* cmd = (RenderCommandClear*)at;
                renderer_set_clear_color(renderer, cmd->color);
                at += sizeof(RenderCommandClear);
            } break;

            case RenderCommand_Rect: {
                RenderCommandRect* cmd = (RenderCommandRect*)at;
                renderer_draw_rect(
                    renderer,
                    cmd->x,
                    cmd->y,
                    cmd->w,
                    cmd->h,
                    cmd->color
                );
                at += sizeof(RenderCommandRect);
            } break;

            case RenderCommand_Sprite: {
                RenderCommandSprite* cmd = (RenderCommandSprite*)at;
                renderer_draw_sprite(
                    renderer,
                    cmd->x,
                    cmd->y,
                    cmd->w,
                    cmd->h,
                    cmd->texture_id,
                    cmd->tint
                );
                at += sizeof(RenderCommandSprite);
            } break;
        }
    }
}

static void init() {
    global_game_memory.permanent_storage_size = MB(64);
    global_game_memory.transient_storage_size = MB(256);

    u64 total_size = global_game_memory.permanent_storage_size +
                     global_game_memory.transient_storage_size;

    void* base_memory = platform_alloc(total_size);
    ASSERT(base_memory != nullptr);

    global_game_memory.permanent_storage = base_memory;
    global_game_memory.transient_storage =
        (u8*)base_memory + global_game_memory.permanent_storage_size;

    // Arena for render commands (reset each frame)
    void* render_memory = platform_alloc(MB(4));
    ASSERT(render_memory != nullptr);

    global_render_commands.width = 320;
    global_render_commands.height = 180;
    global_render_commands.arena = MemoryArena::make(render_memory, MB(4));

    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
    });

    global_renderer = renderer_init();

    global_game_dll = platform_load_game_code(
        "out/libgame.dylib",
        "out/game_temp", // Base name - loader appends unique suffix
        "lock.tmp"
    );
    global_game_code = platform_get_game_code(&global_game_dll);

    if (!global_game_code.is_valid) {
        slog_func(
            "game",
            1,
            0,
            "Failed to load game code",
            __LINE__,
            __FILE__,
            nullptr
        );
    }
}

static void frame() {
    platform_reload_game_code_if_changed(&global_game_dll, &global_game_code);

    global_game_input.dt_for_frame = (f32)sapp_frame_duration();

    global_render_commands.arena.used = 0;

    if (global_game_code.is_valid) {
        global_game_code.update_and_render(
            &global_game_memory,
            &global_game_input,
            &global_render_commands
        );
    }

    renderer_begin_frame(
        global_renderer,
        sapp_width(),
        sapp_height()
    );

    execute_render_commands(global_renderer, &global_render_commands);

    renderer_end_frame(global_renderer);

    reset_input_half_transitions(&global_game_input);
}

static void cleanup() {
    platform_unload_game_code(&global_game_dll);
    sg_shutdown();
}

static void event(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN ||
        ev->type == SAPP_EVENTTYPE_KEY_UP) {
        b32 is_down = (ev->type == SAPP_EVENTTYPE_KEY_DOWN);

        switch (ev->key_code) {
            case SAPP_KEYCODE_W:
                process_button_event(&global_game_input.move_up, is_down);
                break;
            case SAPP_KEYCODE_S:
                process_button_event(&global_game_input.move_down, is_down);
                break;
            case SAPP_KEYCODE_A:
                process_button_event(&global_game_input.move_left, is_down);
                break;
            case SAPP_KEYCODE_D:
                process_button_event(&global_game_input.move_right, is_down);
                break;
            case SAPP_KEYCODE_SPACE:
                process_button_event(&global_game_input.action, is_down);
                break;
            default:
        }
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        global_game_input.mouse_x = (i32)ev->mouse_x;
        global_game_input.mouse_y = (i32)ev->mouse_y;
    } else if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN ||
               ev->type == SAPP_EVENTTYPE_MOUSE_UP) {
        b32 is_down = (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN);
        if (ev->mouse_button < 3) {
            process_button_event(
                &global_game_input.mouse_buttons[ev->mouse_button],
                is_down
            );
        }
    }
}

sapp_desc sokol_main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .window_title = "Sokol Hot-Reload",
        .width = 800,
        .height = 600,
    };
}
