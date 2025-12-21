#pragma once

#include "def.h"

struct MemoryArena {
    u8* base;
    usize size;
    usize used;

    // Initialize an arena from a pre-allocated buffer
    static MemoryArena make(void* buffer, usize size_bytes) {
        MemoryArena result = {};
        result.base = (u8*)buffer;
        result.size = size_bytes;
        result.used = 0;
        return result;
    }

    // Push raw bytes, returns aligned pointer
    void* push_size(usize size_bytes, usize alignment = alignof(max_align_t)) {
        usize aligned_offset = (used + (alignment - 1)) & ~(alignment - 1);
        ASSERT((aligned_offset + size_bytes) <= size);
        void* result = base + aligned_offset;
        used = aligned_offset + size_bytes;
        return result;
    }

    // Push a single struct/type
    template <typename T> T* push_struct() {
        return (T*)push_size(sizeof(T), alignof(T));
    }

    // Push an array of structs/types
    template <typename T> T* push_array(usize count) {
        return (T*)push_size(sizeof(T) * count, alignof(T));
    }

    // Push and zero-initialize a single struct
    template <typename T> T* push_struct_zero() {
        T* result = push_struct<T>();
        *result = {};
        return result;
    }

    // Push and zero-initialize an array
    template <typename T> T* push_array_zero(usize count) {
        T* result = push_array<T>(count);
        for (usize i = 0; i < count; ++i) {
            result[i] = {};
        }
        return result;
    }

    // Reset arena to empty state
    void clear() { used = 0; }

    // Get remaining capacity
    usize remaining() { return size - used; }
};

struct TemporaryMemory {
    MemoryArena* arena;
    usize saved_used;

    TemporaryMemory make(MemoryArena* arena) {
        TemporaryMemory result;
        result.arena = arena;
        result.saved_used = arena->used;
        return result;
    }

    void end() {
        arena->used = saved_used;
    }
};
