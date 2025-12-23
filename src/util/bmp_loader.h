#pragma once

#include "lib/def.h"
#include "lib/memory_arena.h"
#include <cstdio>

// Simple BMP loader for 24-bit and 32-bit BMPs
// Allocates pixel data from the provided MemoryArena

#pragma pack(push, 1)
struct BMPFileHeader {
    u16 type; // Must be 'BM' (0x4D42)
    u32 size; // File size
    u16 reserved1;
    u16 reserved2;
    u32 offset; // Offset to pixel data
};

struct BMPInfoHeader {
    u32 size; // Header size (40+ for various formats)
    i32 width;
    i32 height; // Positive = bottom-up, negative = top-down
    u16 planes;
    u16 bits_per_pixel; // 24 or 32
    u32 compression;    // 0 = uncompressed, 3 = BI_BITFIELDS
    u32 image_size;
    i32 x_pixels_per_m;
    i32 y_pixels_per_m;
    u32 colors_used;
    u32 colors_important;
};
#pragma pack(pop)

struct BMPImage {
    u8* pixels; // RGBA format, allocated from arena
    i32 width;
    i32 height;
    b32 valid;
};

// Load BMP file, allocating pixel data from the provided arena
// The row_buffer for temporary decoding also comes from the arena
inline BMPImage bmp_load(const char* filepath, MemoryArena* arena) {
    BMPImage result = {};

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return result;
    }

    BMPFileHeader file_header;
    if (fread(&file_header, sizeof(file_header), 1, file) != 1) {
        fclose(file);
        return result;
    }

    // Check magic number 'BM'
    if (file_header.type != 0x4D42) {
        fclose(file);
        return result;
    }

    // Read just the header size first to determine format
    u32 header_size;
    if (fread(&header_size, sizeof(header_size), 1, file) != 1) {
        fclose(file);
        return result;
    }

    // Seek back and read the standard info header portion
    fseek(file, sizeof(BMPFileHeader), SEEK_SET);

    BMPInfoHeader info_header = {};
    u32 bytes_to_read = (header_size < sizeof(BMPInfoHeader))
                            ? header_size
                            : sizeof(BMPInfoHeader);
    if (fread(&info_header, bytes_to_read, 1, file) != 1) {
        fclose(file);
        return result;
    }
    info_header.size = header_size;

    // Support uncompressed (0) and BI_BITFIELDS (3) for 32-bit BMPs
    b32 valid_compression =
        (info_header.compression == 0) ||
        (info_header.compression == 3 && info_header.bits_per_pixel == 32);
    if (!valid_compression || (info_header.bits_per_pixel != 24 &&
                               info_header.bits_per_pixel != 32)) {
        fclose(file);
        return result;
    }

    i32 width = info_header.width;
    i32 height = info_header.height;
    b32 top_down = height < 0;
    if (height < 0) height = -height;

    i32 bytes_per_pixel = info_header.bits_per_pixel / 8;
    i32 row_stride = ((width * bytes_per_pixel + 3) / 4) * 4;

    // Allocate from arena: pixels + temporary row buffer
    u32 pixel_count = width * height;
    u8* pixels = arena->push_array<u8>(pixel_count * 4);
    u8* row_buffer = arena->push_array<u8>(row_stride);

    // Seek to pixel data
    fseek(file, file_header.offset, SEEK_SET);

    for (i32 y = 0; y < height; y++) {
        i32 dest_y = top_down ? y : (height - 1 - y);

        if (fread(row_buffer, row_stride, 1, file) != 1) {
            fclose(file);
            return result;
        }

        u8* dest_row = pixels + dest_y * width * 4;

        for (i32 x = 0; x < width; x++) {
            u8* src = row_buffer + x * bytes_per_pixel;
            u8* dst = dest_row + x * 4;

            // BMP stores BGR(A), we want RGBA
            dst[0] = src[2];                                // R
            dst[1] = src[1];                                // G
            dst[2] = src[0];                                // B
            dst[3] = (bytes_per_pixel == 4) ? src[3] : 255; // A
        }
    }

    fclose(file);

    result.pixels = pixels;
    result.width = width;
    result.height = height;
    result.valid = true;

    return result;
}
