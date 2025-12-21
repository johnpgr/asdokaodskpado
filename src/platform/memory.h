#pragma once

#include "lib/def.h"

// Platform-agnostic memory allocation using virtual memory
// - Memory is committed and ready to use (read/write)
// - Memory is zero-initialized by the OS
// - Returns nullptr on failure

#ifdef _WIN32
#include <windows.h>

inline void* platform_alloc(usize size) {
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

inline void platform_free(void* ptr, usize size) {
    (void)size; // Windows doesn't need size for VirtualFree
    VirtualFree(ptr, 0, MEM_RELEASE);
}

#else
// POSIX (macOS, Linux, BSD, etc.)
#include <sys/mman.h>

inline void* platform_alloc(usize size) {
    void* ptr = mmap(
        nullptr,
        size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    return (ptr == MAP_FAILED) ? nullptr : ptr;
}

inline void platform_free(void* ptr, usize size) { munmap(ptr, size); }

#endif
