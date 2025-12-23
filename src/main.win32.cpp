// Windows Platform Layer - Win32 + WGL + OpenGL 3.3 Core

#define WIN32_LEAN_AND_MEAN
#include <gl/gl.h>
#include <windows.h>

#include "game_interface.h"
#include "platform/dll_loader.h"
#include "platform/loader.opengl.h"
#include "platform/memory.h"
#include "renderer.h"

#include <cstdio>
#include <print>

using std::println;

// WGL extension function types
typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(
    HDC hDC,
    HGLRC hShareContext,
    const int* attribList
);
typedef BOOL(WINAPI* PFNWGLCHOOSEPIXELFORMATARBPROC)(
    HDC hdc,
    const int* piAttribIList,
    const FLOAT* pfAttribFList,
    UINT nMaxFormats,
    int* piFormats,
    UINT* nNumFormats
);

// WGL constants
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

#define WGL_DRAW_TO_WINDOW_ARB 0x2001
#define WGL_SUPPORT_OPENGL_ARB 0x2010
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_PIXEL_TYPE_ARB 0x2013
#define WGL_TYPE_RGBA_ARB 0x202B
#define WGL_COLOR_BITS_ARB 0x2014
#define WGL_DEPTH_BITS_ARB 0x2022
#define WGL_STENCIL_BITS_ARB 0x2023

// Global state
static HWND g_window = nullptr;
static HDC g_device_context = nullptr;
static HGLRC g_gl_context = nullptr;
static b32 g_running = true;

static GameMemory g_game_memory = {};
static GameInput g_game_input = {};
static RenderCommands g_render_commands = {};
static PlatformDLL g_game_dll = {};
static GameCode g_game_code = {};
static Renderer* g_renderer = nullptr;

static i32 g_window_width = 800;
static i32 g_window_height = 600;

static LARGE_INTEGER g_perf_frequency;

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
        }
    }
}

static void process_keyboard_message(WPARAM wParam, b32 is_down) {
    switch (wParam) {
        case 'W':
            process_button_event(&g_game_input.move_up, is_down);
            break;
        case 'S':
            process_button_event(&g_game_input.move_down, is_down);
            break;
        case 'A':
            process_button_event(&g_game_input.move_left, is_down);
            break;
        case 'D':
            process_button_event(&g_game_input.move_right, is_down);
            break;
        case VK_SPACE:
            process_button_event(&g_game_input.action, is_down);
            break;
    }
}

static LRESULT CALLBACK
window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            g_running = false;
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            g_window_width = LOWORD(lParam);
            g_window_height = HIWORD(lParam);
            return 0;

        case WM_KEYDOWN:
            if (!(lParam & 0x40000000)) { // Ignore key repeat
                process_keyboard_message(wParam, true);
            }
            return 0;

        case WM_KEYUP:
            process_keyboard_message(wParam, false);
            return 0;

        case WM_MOUSEMOVE:
            g_game_input.mouse_x = LOWORD(lParam);
            g_game_input.mouse_y = HIWORD(lParam);
            return 0;

        case WM_LBUTTONDOWN:
            process_button_event(&g_game_input.mouse_buttons[0], true);
            return 0;

        case WM_LBUTTONUP:
            process_button_event(&g_game_input.mouse_buttons[0], false);
            return 0;

        case WM_RBUTTONDOWN:
            process_button_event(&g_game_input.mouse_buttons[1], true);
            return 0;

        case WM_RBUTTONUP:
            process_button_event(&g_game_input.mouse_buttons[1], false);
            return 0;

        case WM_MBUTTONDOWN:
            process_button_event(&g_game_input.mouse_buttons[2], true);
            return 0;

        case WM_MBUTTONUP:
            process_button_event(&g_game_input.mouse_buttons[2], false);
            return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void* win32_gl_get_proc_address(const char* name) {
    void* proc = (void*)wglGetProcAddress(name);
    if (proc == nullptr || proc == (void*)0x1 || proc == (void*)0x2 ||
        proc == (void*)0x3 || proc == (void*)-1) {
        // Try loading from opengl32.dll directly
        static HMODULE opengl32 = nullptr;
        if (!opengl32) {
            opengl32 = LoadLibraryA("opengl32.dll");
        }
        if (opengl32) {
            proc = (void*)GetProcAddress(opengl32, name);
        }
    }
    return proc;
}

static b32 create_window_and_context(HINSTANCE hInstance) {
    // Register window class
    WNDCLASSA wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "GameWindowClass";

    if (!RegisterClassA(&wc)) {
        println("Failed to register window class");
        return false;
    }

    // Create window
    RECT rect = {0, 0, g_window_width, g_window_height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    g_window = CreateWindowExA(
        0,
        wc.lpszClassName,
        "Game",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_window) {
        println("Failed to create window");
        return false;
    }

    g_device_context = GetDC(g_window);

    // Create a dummy context to get WGL extensions
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pixel_format = ChoosePixelFormat(g_device_context, &pfd);
    if (!pixel_format) {
        println("Failed to choose pixel format");
        return false;
    }

    if (!SetPixelFormat(g_device_context, pixel_format, &pfd)) {
        println("Failed to set pixel format");
        return false;
    }

    HGLRC dummy_context = wglCreateContext(g_device_context);
    if (!dummy_context) {
        println("Failed to create dummy context");
        return false;
    }

    wglMakeCurrent(g_device_context, dummy_context);

    // Get WGL extension functions
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC
        )wglGetProcAddress("wglCreateContextAttribsARB");

    if (!wglCreateContextAttribsARB) {
        println("wglCreateContextAttribsARB not available");
        return false;
    }

    // Create OpenGL 3.3 Core context
    int context_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB,
        3,
        WGL_CONTEXT_MINOR_VERSION_ARB,
        3,
        WGL_CONTEXT_PROFILE_MASK_ARB,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    g_gl_context =
        wglCreateContextAttribsARB(g_device_context, nullptr, context_attribs);
    if (!g_gl_context) {
        println("Failed to create OpenGL 3.3 context");
        return false;
    }

    // Delete dummy context and switch to real one
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(dummy_context);
    wglMakeCurrent(g_device_context, g_gl_context);

    // Load OpenGL functions
    if (!gl_load_functions(win32_gl_get_proc_address)) {
        println("Failed to load OpenGL functions");
        return false;
    }

    println("OpenGL Version: {}", (const char*)glGetString(GL_VERSION));
    println("OpenGL Renderer: {}", (const char*)glGetString(GL_RENDERER));

    return true;
}

static void destroy_window_and_context() {
    if (g_gl_context) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_gl_context);
    }
    if (g_device_context && g_window) {
        ReleaseDC(g_window, g_device_context);
    }
    if (g_window) {
        DestroyWindow(g_window);
    }
}

static f64 get_time_seconds() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (f64)counter.QuadPart / (f64)g_perf_frequency.QuadPart;
}

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Initialize timing
    QueryPerformanceFrequency(&g_perf_frequency);

    // Create console for debug output
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    if (!create_window_and_context(hInstance)) {
        return 1;
    }

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

    // Load game code
    g_game_dll =
        platform_load_game_code("out/game.dll", "out/game_temp", "lock.tmp");
    g_game_code = platform_get_game_code(&g_game_dll);

    if (!g_game_code.is_valid) {
        println("Warning: Failed to load game code");
    }

    f64 last_time = get_time_seconds();

    while (g_running) {
        // Hot-reload game code
        platform_reload_game_code_if_changed(&g_game_dll, &g_game_code);

        // Calculate delta time
        f64 current_time = get_time_seconds();
        g_game_input.dt_for_frame = (f32)(current_time - last_time);
        last_time = current_time;

        // Process Windows messages
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

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
        SwapBuffers(g_device_context);

        // Reset input transitions
        reset_input_half_transitions(&g_game_input);
    }

    platform_unload_game_code(&g_game_dll);
    destroy_window_and_context();

    return 0;
}
