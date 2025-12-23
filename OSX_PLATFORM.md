 Plan: macOS Objective-C++ Platform Layer

 Goal

 Create a macOS platform layer using Objective-C++ that matches the existing Windows
 (Win32+WGL) and Linux (X11+GLX) implementations, using Cocoa for windowing and
 NSOpenGL for the OpenGL 3.3 Core context.

 Files to Modify

 src/platform/dll_loader.h

 Fix DLL extension for macOS (currently uses .so for all non-Windows):
 #ifdef _WIN32
     #define DLL_EXT ".dll"
 #elif defined(__APPLE__)
     #define DLL_EXT ".dylib"
 #else
     #define DLL_EXT ".so"
 #endif

 Files to Create

 1. src/platform/macos_main.mm

 Objective-C++ platform layer with:

 Cocoa Application Setup:
 - NSApplication for app lifecycle
 - NSWindow for window management
 - Custom NSOpenGLView subclass for OpenGL context and input handling
 - NSOpenGLPixelFormat with OpenGL 3.3 Core profile attributes

 Input Handling:
 - Override keyDown:, keyUp:, flagsChanged: for keyboard
 - Override mouseMoved:, mouseDown:, mouseUp:, rightMouseDown:, etc. for mouse
 - Map key codes: kVK_ANSI_W, kVK_ANSI_A, kVK_ANSI_S, kVK_ANSI_D, kVK_Space
 - Use same process_button_event() pattern for half-transitions

 Timing:
 - Use mach_absolute_time() with mach_timebase_info for high-precision timing
 - Or use CACurrentMediaTime() from QuartzCore (simpler)

 OpenGL Function Loading:
 - Use dlopen("libGL.dylib") or NSOpenGLContext methods
 - Same gl_load_functions() pattern as Linux

 Hot-Reload:
 - Reuse existing dll_loader.h (POSIX dlopen/dlsym)
 - Use .dylib extension (already handled in dll_loader.h)

 Main Loop:
 - Use NSApplication run loop with custom NSTimer or manual polling
 - Or: Run our own while loop after [NSApp finishLaunching]

 2. build/build_osx.sh

 Build script for macOS:
 #!/bin/bash
 clang++ -std=c++23 -x objective-c++ \
     src/platform/macos_main.mm \
     src/renderer_opengl.cpp \
     src/platform/opengl_loader.cpp \
     -o out/main \
     -framework Cocoa \
     -framework OpenGL \
     -framework QuartzCore \
     -ldl

 Architecture

 ┌─────────────────────────────────────────┐
 │           macos_main.mm                  │
 │  ┌─────────────────────────────────┐    │
 │  │      GameView : NSOpenGLView     │    │
 │  │  - keyDown/keyUp (keyboard)      │    │
 │  │  - mouseMoved/mouseDown (mouse)  │    │
 │  │  - drawRect (render trigger)     │    │
 │  └─────────────────────────────────┘    │
 │                                          │
 │  main() {                                │
 │    Create NSApplication                  │
 │    Create NSWindow + GameView            │
 │    while (g_running) {                   │
 │      Poll NSEvents                       │
 │      Calculate dt                        │
 │      Hot-reload check                    │
 │      game_update_and_render()            │
 │      renderer_begin/end_frame()          │
 │      [[context] flushBuffer]             │
 │    }                                     │
 │  }                                       │
 └─────────────────────────────────────────┘

 Key Implementation Details

 OpenGL Context Creation

 NSOpenGLPixelFormatAttribute attrs[] = {
     NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
     NSOpenGLPFAColorSize, 24,
     NSOpenGLPFAAlphaSize, 8,
     NSOpenGLPFADepthSize, 24,
     NSOpenGLPFADoubleBuffer,
     NSOpenGLPFAAccelerated,
     0
 };
 NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc]
 initWithAttributes:attrs];
 NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:format
 shareContext:nil];

 Key Code Mapping (Carbon/Events.h)

 #include <Carbon/Carbon.h>  // For kVK_* constants
 case kVK_ANSI_W: process_button_event(&g_game_input.move_up, is_down); break;
 case kVK_ANSI_S: process_button_event(&g_game_input.move_down, is_down); break;
 case kVK_ANSI_A: process_button_event(&g_game_input.move_left, is_down); break;
 case kVK_ANSI_D: process_button_event(&g_game_input.move_right, is_down); break;
 case kVK_Space:  process_button_event(&g_game_input.action, is_down); break;

 Timing

 #include <mach/mach_time.h>

 static mach_timebase_info_data_t g_timebase_info;

 f64 get_time_seconds() {
     u64 time = mach_absolute_time();
     return (f64)(time * g_timebase_info.numer) / (f64)(g_timebase_info.denom *
 1000000000ULL);
 }

 Event Polling (Manual Loop)

 while (g_running) {
     @autoreleasepool {
         NSEvent* event;
         while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:nil
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES])) {
             [NSApp sendEvent:event];
         }

         // Game update and render...
         [[g_gl_context] flushBuffer];
     }
 }

 GL Function Loading

 void* macos_gl_get_proc_address(const char* name) {
     static void* libgl = nullptr;
     if (!libgl) {
         libgl = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL",
 RTLD_LAZY);
     }
     return dlsym(libgl, name);
 }

 Global Variables (matching other platforms)

 static NSWindow* g_window = nil;
 static NSOpenGLContext* g_gl_context = nil;
 static b32 g_running = true;

 static GameMemory g_game_memory = {};
 static GameInput g_game_input = {};
 static RenderCommands g_render_commands = {};
 static PlatformDLL g_game_dll = {};
 static GameCode g_game_code = {};
 static Renderer* g_renderer = nullptr;

 static i32 g_window_width = 800;
 static i32 g_window_height = 600;

 Critical Files Reference

 - src/platform/linux_main.cpp - Pattern reference
 - src/platform/win32_main.cpp - Pattern reference
 - src/platform/opengl_loader.h - GL function declarations
 - src/platform/dll_loader.h - Hot-reload (already cross-platform)
 - src/renderer.h - Renderer interface

 Notes

 - macOS deprecated OpenGL but it still works (suppress warnings with
 -Wno-deprecated)
 - Need -fobjc-arc for automatic reference counting
 - The .dylib extension is already handled in dll_loader.h via DLL_EXT macro (need to
  add macOS case)
 - Window resize handled via NSWindowDelegate or view's reshape method

