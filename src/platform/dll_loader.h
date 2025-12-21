#pragma once

#include "game_interface.h"
#include "lib/def.h"
#include "platform/file_watcher.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief Global counter used to generate unique temp dylib filenames.
 * @details Incremented each load to avoid macOS code signing issues when
 * overwriting a temp file that's still being validated.
 */
static u32 g_dll_load_counter = 0;

/**
 * @struct PlatformDLL
 * @brief Handle + metadata for a hot-reloadable dynamic library (game code).
 *
 * @details
 * Uses POSIX `dlopen`/`dlsym`/`dlclose` (macOS/Linux). The compiled library at
 * @ref PlatformDLL::dll_path is copied to a unique temp path and loaded from
 * there to avoid interfering with rebuilds of the original output and to
 * prevent macOS code signing invalidation.
 * A lock file at @ref PlatformDLL::lock_path is used to skip loading while a
 * build/link is in progress.
 */
struct PlatformDLL {
    void* handle; /**< @brief `dlopen()` handle for the loaded temp library. */
    const char*
        dll_path; /**< @brief Source/original library path (build output). */
    const char* temp_dll_base; /**< @brief Base path for temp copies (e.g.,
                                  "out/game_temp"). */
    char temp_dll_path[256];   /**< @brief Actual temp copy path with unique
                                  suffix. */
    const char* lock_path;    /**< @brief "Build in progress" lock file path. */
    FileTime last_write_time; /**< @brief Last known write time of @ref
                                 PlatformDLL::dll_path. */
    b32 is_valid; /**< @brief True when the library is loaded and usable. */
};

/**
 * @brief Copy a file from @p source to @p dest using buffered stdio.
 *
 * @details
 * Intended for hot-reload workflows: copy the freshly built dynamic library to
 * a temporary filename before calling `dlopen`, so the build system can
 * overwrite the original output file without issues.
 *
 * @param source Path to the source file.
 * @param dest   Path to the destination file (overwritten/created).
 *
 * @note Failures (open/create) are silent; metadata (permissions/timestamps) is
 * not preserved.
 */
inline void platform_copy_file(const char* source, const char* dest) {
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
}

/**
 * @brief Check whether a filesystem path exists.
 *
 * @param path Path to test.
 * @return Non-zero if the path exists; otherwise zero.
 */
inline b32 platform_file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

/**
 * @brief Load the game dynamic library for hot-reloading.
 *
 * @details
 * Loading is skipped if @p lock_path exists (assumed "build in progress").
 * Otherwise, the library at @p source_dll_path is copied to a uniquely-named
 * temp file (based on @p temp_dll_base and an incrementing counter) and loaded
 * from there. This prevents macOS code signing invalidation that occurs when
 * overwriting a dylib that's still being validated by the OS.
 *
 * @param source_dll_path Path to the built library (original output).
 * @param temp_dll_base   Base path for temp copies (e.g., "out/game_temp").
 * @param lock_path       Path to a lock file used to prevent loading mid-build.
 * @return A @ref PlatformDLL with `is_valid` set to non-zero on success;
 * otherwise zero.
 *
 * @note On `dlopen` failure, prints the loader error via `dlerror()`.
 */
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

    // Generate unique temp filename to avoid macOS code signing issues
    snprintf(
        result.temp_dll_path,
        sizeof(result.temp_dll_path),
        "%s_%u.dylib",
        temp_dll_base,
        g_dll_load_counter++
    );

    platform_copy_file(source_dll_path, result.temp_dll_path);

    result.handle = dlopen(result.temp_dll_path, RTLD_NOW);
    if (!result.handle) {
        printf("dlopen failed: %s\n", dlerror());
        result.is_valid = false;
        return result;
    }

    result.last_write_time = platform_get_file_write_time(source_dll_path);
    result.is_valid = true;
    return result;
}

/**
 * @brief Unload a previously loaded dynamic library.
 *
 * @param dll PlatformDLL to unload.
 *
 * @details
 * Calls `dlclose` on a non-null handle, deletes the temp file to avoid
 * accumulating old copies, clears the handle, and marks the DLL as invalid.
 */
inline void platform_unload_game_code(PlatformDLL* dll) {
    if (dll->handle) {
        dlclose(dll->handle);
        dll->handle = nullptr;
    }
    // Delete the old temp file to avoid accumulating files
    if (dll->temp_dll_path[0] != '\0') {
        unlink(dll->temp_dll_path);
        dll->temp_dll_path[0] = '\0';
    }
    dll->is_valid = false;
}

/**
 * @brief Resolve game entry points from a loaded dynamic library.
 *
 * @details
 * Required symbol:
 * - `game_update_and_render`
 *
 * Optional symbol:
 * - `game_get_version` (used to fill `GameCode::version`; otherwise set to 0)
 *
 * @param dll Loaded DLL container.
 * @return A populated @ref GameCode. `GameCode::is_valid` is non-zero when
 * required symbols exist.
 *
 * @note Prints a message if required symbols are missing.
 */
inline GameCode platform_get_game_code(PlatformDLL* dll) {
    GameCode result = {};
    if (!dll->is_valid) {
        return result;
    }

    result.update_and_render = (game_update_and_render_func*)
        dlsym(dll->handle, "game_update_and_render");

    auto get_version = (u32(*)())dlsym(dll->handle, "game_get_version");

    result.is_valid = (result.update_and_render != nullptr);
    result.version = get_version ? get_version() : 0;

    if (!result.is_valid) {
        printf("Failed to load game functions\n");
    }

    return result;
}

/**
 * @brief Hot-reload helper: reload the dynamic library if the source file
 * changed.
 *
 * @details
 * Compares the current write time of @ref PlatformDLL::dll_path with
 * @ref PlatformDLL::last_write_time. If changed, unloads the current library,
 * loads a fresh copy, and resolves symbols into @p game_code.
 *
 * @param dll       In/out DLL state; replaced when a reload occurs.
 * @param game_code Out resolved function pointers/version for the active DLL.
 *
 * @note If loading fails (e.g., lock file present or `dlopen` failure), the
 * resulting @p dll and @p game_code may become invalid.
 */
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
