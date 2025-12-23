#pragma once

#include "game_interface.h"
#include "lib/def.h"
#include "platform/file_watcher.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #define DLL_EXT ".dll"
#else
    #include <dlfcn.h>
    #include <fcntl.h>
    #include <stdlib.h>
    #include <unistd.h>
    #define DLL_EXT ".so"
#endif

static u32 g_dll_load_counter = 0;

struct PlatformDLL {
#ifdef _WIN32
    HMODULE handle;
#else
    void* handle;
#endif
    const char* dll_path;
    const char* temp_dll_base;
    char temp_dll_path[256];
    const char* lock_path;
    FileTime last_write_time;
    b32 is_valid;
};

inline void platform_copy_file(const char* source, const char* dest) {
#ifdef _WIN32
    CopyFileA(source, dest, FALSE);
#else
    char buffer[8192];
    FILE* src = fopen(source, "rb");
    if (!src) return;

    FILE* dst = fopen(dest, "wb");
    if (!dst) {
        fclose(src);
        return;
    }

    usize bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes_read, dst);
    }

    fclose(src);
    fclose(dst);
#endif
}

inline b32 platform_file_exists(const char* path) {
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    return access(path, F_OK) == 0;
#endif
}

inline void platform_delete_file(const char* path) {
#ifdef _WIN32
    DeleteFileA(path);
#else
    unlink(path);
#endif
}

inline PlatformDLL platform_load_game_code(
    const char* source_dll_path,
    const char* temp_dll_base,
    const char* lock_path
) {
    PlatformDLL result = {};
    result.dll_path = source_dll_path;
    result.temp_dll_base = temp_dll_base;
    result.lock_path = lock_path;

    if (platform_file_exists(lock_path)) {
        result.is_valid = false;
        return result;
    }

    // Generate unique temp filename
    snprintf(
        result.temp_dll_path,
        sizeof(result.temp_dll_path),
        "%s_%u" DLL_EXT,
        temp_dll_base,
        g_dll_load_counter++
    );

    platform_copy_file(source_dll_path, result.temp_dll_path);

#ifdef _WIN32
    result.handle = LoadLibraryA(result.temp_dll_path);
    if (!result.handle) {
        printf("LoadLibrary failed: %lu\n", GetLastError());
        result.is_valid = false;
        return result;
    }
#else
    result.handle = dlopen(result.temp_dll_path, RTLD_NOW);
    if (!result.handle) {
        printf("dlopen failed: %s\n", dlerror());
        result.is_valid = false;
        return result;
    }
#endif

    result.last_write_time = platform_get_file_write_time(source_dll_path);
    result.is_valid = true;
    return result;
}

inline void platform_unload_game_code(PlatformDLL* dll) {
    if (dll->handle) {
#ifdef _WIN32
        FreeLibrary(dll->handle);
#else
        dlclose(dll->handle);
#endif
        dll->handle = nullptr;
    }
    if (dll->temp_dll_path[0] != '\0') {
        platform_delete_file(dll->temp_dll_path);
        dll->temp_dll_path[0] = '\0';
    }
    dll->is_valid = false;
}

inline GameCode platform_get_game_code(PlatformDLL* dll) {
    GameCode result = {};
    if (!dll->is_valid) {
        return result;
    }

#ifdef _WIN32
    result.update_and_render = (game_update_and_render_func*)
        GetProcAddress(dll->handle, "game_update_and_render");
    auto get_version = (u32(*)())GetProcAddress(dll->handle, "game_get_version");
#else
    result.update_and_render = (game_update_and_render_func*)
        dlsym(dll->handle, "game_update_and_render");
    auto get_version = (u32(*)())dlsym(dll->handle, "game_get_version");
#endif

    result.is_valid = (result.update_and_render != nullptr);
    result.version = get_version ? get_version() : 0;

    if (!result.is_valid) {
        printf("Failed to load game functions\n");
    }

    return result;
}

inline void
platform_reload_game_code_if_changed(PlatformDLL* dll, GameCode* game_code) {
    FileTime new_write_time = platform_get_file_write_time(dll->dll_path);
    if (platform_file_time_changed(dll->last_write_time, new_write_time)) {
        const char* dll_path = dll->dll_path;
        const char* temp_base = dll->temp_dll_base;
        const char* lock_path = dll->lock_path;
        platform_unload_game_code(dll);
        *dll = platform_load_game_code(dll_path, temp_base, lock_path);
        *game_code = platform_get_game_code(dll);
    }
}
