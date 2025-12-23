// macOS Platform Layer - Cocoa + NSOpenGL + OpenGL 3.3 Core

#include <Carbon/Carbon.h> // For kVK_* constants
#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>
#import <QuartzCore/QuartzCore.h>
#include <dlfcn.h>
#include <mach/mach_time.h>
#include <print>

using std::println;

#include "game.h"
#include "game_interface.h"
#include "platform/dll_loader.h"
#include "platform/memory.h"
#include "renderer.h"
#include "util/bmp_loader.h"
#include "util/loader.opengl.h"

// Global state
static NSWindow* g_window = nil;
static NSOpenGLContext* g_gl_context = nil;
static b32 g_running = true;

static GameMemory g_game_memory = {};
static GameInput g_game_input = {};
static RenderCommands g_render_commands = {};
static PlatformDLL g_game_dll = {};
static GameCode g_game_code = {};
static Renderer* g_renderer = nullptr;
static u32 g_atlas_texture_id = 0;

static i32 g_window_width = 800;
static i32 g_window_height = 600;

static mach_timebase_info_data_t g_timebase_info;

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

/**
 * Executes all render commands stored in the command buffer.
 *
 * This function iterates through a linear arena of serialized render commands,
 * parsing each command header to determine its type, then dispatching to the
 * appropriate renderer function. Commands are tightly packed in memory with
 * variable sizes based on their type.
 *
 * @param renderer  The renderer instance to execute commands on.
 * @param commands  The render command buffer containing serialized commands.
 *                  Commands are stored sequentially starting at arena.base,
 *                  with arena.used indicating the total bytes of commands.
 *
 * Supported command types:
 *   - RenderCommand_Clear:  Sets the clear color for the frame
 *   - RenderCommand_Rect:   Draws a colored rectangle
 *   - RenderCommand_Sprite: Draws a textured sprite with optional tint
 */
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

            case RenderCommand_AtlasSprite: {
                RenderCommandAtlasSprite* cmd = (RenderCommandAtlasSprite*)at;
                renderer_draw_atlas_sprite(
                    renderer,
                    cmd->x,
                    cmd->y,
                    cmd->w,
                    cmd->h,
                    cmd->u0,
                    cmd->v0,
                    cmd->u1,
                    cmd->v1,
                    cmd->texture_id,
                    cmd->tint
                );
                at += sizeof(RenderCommandAtlasSprite);
            } break;
        }
    }
}

f64 get_time_seconds() {
    u64 time = mach_absolute_time();
    return (f64)(time * g_timebase_info.numer) /
           (f64)(g_timebase_info.denom * 1000000000ULL);
}

void* macos_gl_get_proc_address(const char* name) {
    static void* libgl = nullptr;
    if (!libgl) {
        libgl = dlopen(
            "/System/Library/Frameworks/OpenGL.framework/OpenGL",
            RTLD_LAZY
        );
    }
    return dlsym(libgl, name);
}

@interface GameView : NSOpenGLView
@end

@implementation GameView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent*)event {
    unsigned short keyCode = [event keyCode];
    bool is_down = true;
    switch (keyCode) {
        case kVK_ANSI_W:
            process_button_event(&g_game_input.move_up, is_down);
            break;
        case kVK_ANSI_S:
            process_button_event(&g_game_input.move_down, is_down);
            break;
        case kVK_ANSI_A:
            process_button_event(&g_game_input.move_left, is_down);
            break;
        case kVK_ANSI_D:
            process_button_event(&g_game_input.move_right, is_down);
            break;
        case kVK_Space:
            process_button_event(&g_game_input.action, is_down);
            break;
    }
}

- (void)keyUp:(NSEvent*)event {
    unsigned short keyCode = [event keyCode];
    bool is_down = false;
    switch (keyCode) {
        case kVK_ANSI_W:
            process_button_event(&g_game_input.move_up, is_down);
            break;
        case kVK_ANSI_S:
            process_button_event(&g_game_input.move_down, is_down);
            break;
        case kVK_ANSI_A:
            process_button_event(&g_game_input.move_left, is_down);
            break;
        case kVK_ANSI_D:
            process_button_event(&g_game_input.move_right, is_down);
            break;
        case kVK_Space:
            process_button_event(&g_game_input.action, is_down);
            break;
    }
}

- (void)mouseMoved:(NSEvent*)event {
    NSPoint location = [self convertPoint:[event locationInWindow]
                                 fromView:nil];
    g_game_input.mouse_x = location.x;
    g_game_input.mouse_y =
        g_window_height - location.y; // Flip Y for typical game coords
}

- (void)mouseDown:(NSEvent*)event {
    process_button_event(&g_game_input.mouse_buttons[0], true);
}

- (void)mouseUp:(NSEvent*)event {
    process_button_event(&g_game_input.mouse_buttons[0], false);
}

- (void)rightMouseDown:(NSEvent*)event {
    process_button_event(&g_game_input.mouse_buttons[1], true);
}

- (void)rightMouseUp:(NSEvent*)event {
    process_button_event(&g_game_input.mouse_buttons[1], false);
}

- (void)reshape {
    [super reshape];
    // Use backing store dimensions (pixels) instead of points for Retina
    // support
    NSRect backing = [self convertRectToBacking:[self bounds]];
    g_window_width = backing.size.width;
    g_window_height = backing.size.height;
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

@implementation AppDelegate

- (void)applicationWillFinishLaunching:(NSNotification*)notification {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}

- (void)windowWillClose:(NSNotification*)notification {
    g_running = false;
}

@end

int main([[maybe_unused]] int argc, [[maybe_unused]] const char* argv[]) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        AppDelegate* delegate = [[AppDelegate alloc] init];
        [NSApp setDelegate:delegate];

        NSRect frame = NSMakeRect(0, 0, g_window_width, g_window_height);
        NSUInteger styleMask =
            NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

        g_window = [[NSWindow alloc] initWithContentRect:frame
                                               styleMask:styleMask
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        [g_window setTitle:@"Game"];
        [g_window setDelegate:delegate];
        [g_window center];

        NSOpenGLPixelFormatAttribute attrs[] = {
            NSOpenGLPFAOpenGLProfile,
            NSOpenGLProfileVersion3_2Core,
            NSOpenGLPFAColorSize,
            24,
            NSOpenGLPFAAlphaSize,
            8,
            NSOpenGLPFADepthSize,
            24,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAAccelerated,
            0
        };
        NSOpenGLPixelFormat* format =
            [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        GameView* view = [[GameView alloc] initWithFrame:frame
                                             pixelFormat:format];
        [g_window setContentView:view];
        [g_window makeFirstResponder:view];
        [g_window makeKeyAndOrderFront:nil];
        [view setWantsBestResolutionOpenGLSurface:YES];

        g_gl_context = [view openGLContext];
        [g_gl_context makeCurrentContext];

        if (!gl_load_functions(macos_gl_get_proc_address)) {
            println("Failed to load OpenGL functions");
            return 1;
        }

        // Initialize timing
        mach_timebase_info(&g_timebase_info);

        // Allocate game memory
        g_game_memory.permanent_storage_size = MB(64);
        g_game_memory.transient_storage_size = MB(256);
        u64 total_size = g_game_memory.permanent_storage_size +
                         g_game_memory.transient_storage_size;
        void* base_memory = platform_alloc(total_size);
        if (!base_memory) {
            println("Failed to allocate game memory");
            return 1;
        }
        g_game_memory.permanent_storage = base_memory;
        g_game_memory.transient_storage =
            (u8*)base_memory + g_game_memory.permanent_storage_size;

        // Arena for render commands
        void* render_memory = platform_alloc(MB(4));
        if (!render_memory) {
            println("Failed to allocate render memory");
            return 1;
        }
        g_render_commands.width = 320;
        g_render_commands.height = 180;
        g_render_commands.arena = MemoryArena::make(render_memory, MB(4));

        // Initialize renderer
        g_renderer = renderer_init();

        // Load ravioli atlas texture using transient memory for temp allocation
        MemoryArena temp_arena = MemoryArena::make(
            g_game_memory.transient_storage,
            MB(1) // 1MB is plenty for a 32x32 image
        );
        BMPImage atlas = bmp_load("assets/ravioli_atlas.bmp", &temp_arena);
        if (atlas.valid) {
            g_atlas_texture_id = renderer_load_texture(
                g_renderer,
                atlas.pixels,
                atlas.width,
                atlas.height,
                4 // RGBA
            );
            println(
                "Loaded atlas texture: {}x{}, id={}",
                atlas.width,
                atlas.height,
                g_atlas_texture_id
            );
            // No need to free - arena memory is transient
        } else {
            println("Failed to load ravioli_atlas.bmp");
        }

        // Load game code
        g_game_dll = platform_load_game_code(
            "out/libgame" DLL_EXT,
            "out/game_temp",
            "lock.tmp"
        );
        g_game_code = platform_get_game_code(&g_game_dll);

        f64 last_time = get_time_seconds();

        [NSApp finishLaunching];

        while (g_running) {
            @autoreleasepool {
                NSEvent* event;
                while (
                    (event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:[NSDate distantPast]
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES])
                ) {
                    [NSApp sendEvent:event];
                }

                // Hot-reload game code
                platform_reload_game_code_if_changed(&g_game_dll, &g_game_code);

                // Calculate delta time
                f64 current_time = get_time_seconds();
                g_game_input.dt_for_frame = (f32)(current_time - last_time);
                last_time = current_time;

                // Log FPS (averaged over 1 second)
                static f32 fps_accumulator = 0.0f;
                static i32 fps_frame_count = 0;
                fps_accumulator += g_game_input.dt_for_frame;
                fps_frame_count++;
                if (fps_accumulator >= 1.0f) {
                    f32 avg_fps = (f32)fps_frame_count / fps_accumulator;
                    println("FPS: {:.1f}", avg_fps);
                    fps_accumulator = 0.0f;
                    fps_frame_count = 0;
                }

                // Reset render commands
                g_render_commands.arena.used = 0;

                // Calculate dynamic target dimensions based on window aspect
                // ratio Base: 320x180 (16:9), Max: 384x216
                constexpr u32 BASE_WIDTH = 320;
                constexpr u32 BASE_HEIGHT = 180;
                constexpr u32 MAX_TARGET_WIDTH = 384;
                constexpr u32 MAX_TARGET_HEIGHT = 216;

                f32 base_aspect = (f32)BASE_WIDTH / (f32)BASE_HEIGHT;
                f32 window_aspect = (f32)g_window_width / (f32)g_window_height;

                u32 target_width, target_height;
                if (window_aspect > base_aspect) {
                    // Window is wider - expand width (pillarbox -> overscan)
                    target_height = BASE_HEIGHT;
                    target_width = (u32)(BASE_HEIGHT * window_aspect);
                    if (target_width > MAX_TARGET_WIDTH) {
                        target_width = MAX_TARGET_WIDTH;
                    }
                } else {
                    // Window is taller - expand height (letterbox -> overscan)
                    target_width = BASE_WIDTH;
                    target_height = (u32)(BASE_WIDTH / window_aspect);
                    if (target_height > MAX_TARGET_HEIGHT) {
                        target_height = MAX_TARGET_HEIGHT;
                    }
                }

                // Update render commands with current target dimensions
                g_render_commands.width = target_width;
                g_render_commands.height = target_height;

                // Update and render game
                if (g_game_code.is_valid) {
                    // Set atlas texture ID in game state (platform owns the
                    // texture)
                    GameState* game_state =
                        (GameState*)g_game_memory.permanent_storage;
                    game_state->atlas_texture_id = g_atlas_texture_id;

                    g_game_code.update_and_render(
                        &g_game_memory,
                        &g_game_input,
                        &g_render_commands
                    );
                }

                // Execute render commands
                renderer_begin_frame(
                    g_renderer,
                    g_window_width,
                    g_window_height,
                    target_width,
                    target_height
                );
                execute_render_commands(g_renderer, &g_render_commands);
                renderer_end_frame(g_renderer);

                // Swap buffers
                [g_gl_context flushBuffer];

                // Reset input transitions
                reset_input_half_transitions(&g_game_input);
            }
        }

        platform_unload_game_code(&g_game_dll);
    }
    return 0;
}
