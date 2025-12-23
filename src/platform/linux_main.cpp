// Linux Platform Layer - X11 + GLX + OpenGL 3.3 Core

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "game_interface.h"
#include "platform/dll_loader.h"
#include "platform/memory.h"
#include "platform/opengl_loader.h"
#include "renderer.h"

// GLX extension for creating modern OpenGL context
typedef GLXContext (*glXCreateContextAttribsARBProc)(
    Display*, GLXFBConfig, GLXContext, Bool, const int*
);

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

// Global state
static Display* g_display = nullptr;
static Window g_window = 0;
static GLXContext g_glx_context = nullptr;
static Atom g_wm_delete_window;
static b32 g_running = true;

static GameMemory g_game_memory = {};
static GameInput g_game_input = {};
static RenderCommands g_render_commands = {};
static PlatformDLL g_game_dll = {};
static GameCode g_game_code = {};
static Renderer* g_renderer = nullptr;

static i32 g_window_width = 800;
static i32 g_window_height = 600;

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
                renderer_draw_rect(renderer, cmd->x, cmd->y, cmd->w, cmd->h, cmd->color);
                at += sizeof(RenderCommandRect);
            } break;

            case RenderCommand_Sprite: {
                RenderCommandSprite* cmd = (RenderCommandSprite*)at;
                renderer_draw_sprite(
                    renderer, cmd->x, cmd->y, cmd->w, cmd->h, cmd->texture_id, cmd->tint
                );
                at += sizeof(RenderCommandSprite);
            } break;
        }
    }
}

static void process_keyboard_event(XKeyEvent* event, b32 is_down) {
    KeySym keysym = XLookupKeysym(event, 0);

    switch (keysym) {
        case XK_w:
        case XK_W:
            process_button_event(&g_game_input.move_up, is_down);
            break;
        case XK_s:
        case XK_S:
            process_button_event(&g_game_input.move_down, is_down);
            break;
        case XK_a:
        case XK_A:
            process_button_event(&g_game_input.move_left, is_down);
            break;
        case XK_d:
        case XK_D:
            process_button_event(&g_game_input.move_right, is_down);
            break;
        case XK_space:
            process_button_event(&g_game_input.action, is_down);
            break;
    }
}

static void process_mouse_button_event(XButtonEvent* event, b32 is_down) {
    i32 button_index = -1;
    switch (event->button) {
        case Button1: button_index = 0; break; // Left
        case Button2: button_index = 2; break; // Middle
        case Button3: button_index = 1; break; // Right
    }
    if (button_index >= 0 && button_index < 3) {
        process_button_event(&g_game_input.mouse_buttons[button_index], is_down);
    }
}

static void process_x11_events() {
    while (XPending(g_display)) {
        XEvent event;
        XNextEvent(g_display, &event);

        switch (event.type) {
            case KeyPress:
                process_keyboard_event(&event.xkey, true);
                break;
            case KeyRelease: {
                // Handle key repeat: ignore if next event is same key press
                if (XEventsQueued(g_display, QueuedAfterReading)) {
                    XEvent next;
                    XPeekEvent(g_display, &next);
                    if (next.type == KeyPress &&
                        next.xkey.time == event.xkey.time &&
                        next.xkey.keycode == event.xkey.keycode) {
                        // Skip this release and the next press (key repeat)
                        XNextEvent(g_display, &next);
                        break;
                    }
                }
                process_keyboard_event(&event.xkey, false);
            } break;
            case MotionNotify:
                g_game_input.mouse_x = event.xmotion.x;
                g_game_input.mouse_y = event.xmotion.y;
                break;
            case ButtonPress:
                process_mouse_button_event(&event.xbutton, true);
                break;
            case ButtonRelease:
                process_mouse_button_event(&event.xbutton, false);
                break;
            case ConfigureNotify:
                g_window_width = event.xconfigure.width;
                g_window_height = event.xconfigure.height;
                break;
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == g_wm_delete_window) {
                    g_running = false;
                }
                break;
        }
    }
}

static f64 get_time_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (f64)ts.tv_sec + (f64)ts.tv_nsec / 1000000000.0;
}

static void* linux_gl_get_proc_address(const char* name) {
    // Try glXGetProcAddressARB first
    void* proc = (void*)glXGetProcAddressARB((const GLubyte*)name);
    if (proc) return proc;

    // Fall back to dlsym from libGL
    static void* libgl = nullptr;
    if (!libgl) {
        libgl = dlopen("libGL.so.1", RTLD_LAZY);
        if (!libgl) libgl = dlopen("libGL.so", RTLD_LAZY);
    }
    if (libgl) {
        return dlsym(libgl, name);
    }
    return nullptr;
}

static b32 create_window_and_context() {
    g_display = XOpenDisplay(nullptr);
    if (!g_display) {
        printf("Failed to open X display\n");
        return false;
    }

    // GLX framebuffer config attributes
    int visual_attribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        None
    };

    int fbcount;
    GLXFBConfig* fbc = glXChooseFBConfig(
        g_display, DefaultScreen(g_display), visual_attribs, &fbcount
    );
    if (!fbc || fbcount == 0) {
        printf("Failed to get GLX framebuffer config\n");
        return false;
    }

    GLXFBConfig best_fbc = fbc[0];
    XFree(fbc);

    XVisualInfo* vi = glXGetVisualFromFBConfig(g_display, best_fbc);
    if (!vi) {
        printf("Failed to get visual info\n");
        return false;
    }

    // Create colormap
    Window root = RootWindow(g_display, vi->screen);
    Colormap cmap = XCreateColormap(g_display, root, vi->visual, AllocNone);

    XSetWindowAttributes swa = {};
    swa.colormap = cmap;
    swa.event_mask =
        ExposureMask | KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
        StructureNotifyMask;

    g_window = XCreateWindow(
        g_display, root,
        0, 0, g_window_width, g_window_height, 0,
        vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa
    );

    XFree(vi);

    if (!g_window) {
        printf("Failed to create window\n");
        return false;
    }

    XStoreName(g_display, g_window, "Game");
    XMapWindow(g_display, g_window);

    // Set up window close handling
    g_wm_delete_window = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete_window, 1);

    // Get glXCreateContextAttribsARB
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB =
        (glXCreateContextAttribsARBProc)glXGetProcAddressARB(
            (const GLubyte*)"glXCreateContextAttribsARB"
        );

    if (!glXCreateContextAttribsARB) {
        printf("glXCreateContextAttribsARB not available\n");
        return false;
    }

    // Create OpenGL 3.3 Core context
    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    g_glx_context = glXCreateContextAttribsARB(
        g_display, best_fbc, nullptr, True, context_attribs
    );

    if (!g_glx_context) {
        printf("Failed to create OpenGL 3.3 context\n");
        return false;
    }

    // Make context current
    glXMakeCurrent(g_display, g_window, g_glx_context);

    // Load OpenGL functions
    if (!gl_load_functions(linux_gl_get_proc_address)) {
        printf("Failed to load OpenGL functions\n");
        return false;
    }

    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));

    return true;
}

static void destroy_window_and_context() {
    if (g_glx_context) {
        glXMakeCurrent(g_display, None, nullptr);
        glXDestroyContext(g_display, g_glx_context);
    }
    if (g_window) {
        XDestroyWindow(g_display, g_window);
    }
    if (g_display) {
        XCloseDisplay(g_display);
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (!create_window_and_context()) {
        return 1;
    }

    // Allocate game memory
    g_game_memory.permanent_storage_size = MB(64);
    g_game_memory.transient_storage_size = MB(256);

    u64 total_size = g_game_memory.permanent_storage_size +
                     g_game_memory.transient_storage_size;

    void* base_memory = platform_alloc(total_size);
    if (!base_memory) {
        printf("Failed to allocate game memory\n");
        return 1;
    }

    g_game_memory.permanent_storage = base_memory;
    g_game_memory.transient_storage =
        (u8*)base_memory + g_game_memory.permanent_storage_size;

    // Arena for render commands
    void* render_memory = platform_alloc(MB(4));
    if (!render_memory) {
        printf("Failed to allocate render memory\n");
        return 1;
    }

    g_render_commands.width = 320;
    g_render_commands.height = 180;
    g_render_commands.arena = MemoryArena::make(render_memory, MB(4));

    // Initialize renderer
    g_renderer = renderer_init();

    // Load game code
    g_game_dll = platform_load_game_code(
        "out/libgame.so",
        "out/game_temp",
        "lock.tmp"
    );
    g_game_code = platform_get_game_code(&g_game_dll);

    if (!g_game_code.is_valid) {
        printf("Warning: Failed to load game code\n");
    }

    f64 last_time = get_time_seconds();

    while (g_running) {
        // Hot-reload game code
        platform_reload_game_code_if_changed(&g_game_dll, &g_game_code);

        // Calculate delta time
        f64 current_time = get_time_seconds();
        g_game_input.dt_for_frame = (f32)(current_time - last_time);
        last_time = current_time;

        // Process input
        process_x11_events();

        // Reset render commands
        g_render_commands.arena.used = 0;

        // Update and render game
        if (g_game_code.is_valid) {
            g_game_code.update_and_render(
                &g_game_memory,
                &g_game_input,
                &g_render_commands
            );
        }

        // Execute render commands
        renderer_begin_frame(g_renderer, g_window_width, g_window_height);
        execute_render_commands(g_renderer, &g_render_commands);
        renderer_end_frame(g_renderer);

        // Swap buffers
        glXSwapBuffers(g_display, g_window);

        // Reset input transitions
        reset_input_half_transitions(&g_game_input);
    }

    platform_unload_game_code(&g_game_dll);
    destroy_window_and_context();

    return 0;
}
