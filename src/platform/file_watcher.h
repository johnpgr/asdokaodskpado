#pragma once

#include "lib/def.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <sys/stat.h>
#endif

struct FileTime {
    u64 value;
};

inline FileTime platform_get_file_write_time(const char* filename) {
    FileTime result = {};
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(filename, GetFileExInfoStandard, &data)) {
        ULARGE_INTEGER time;
        time.LowPart = data.ftLastWriteTime.dwLowDateTime;
        time.HighPart = data.ftLastWriteTime.dwHighDateTime;
        result.value = time.QuadPart;
    }
#else
    struct stat file_stat;
    if (stat(filename, &file_stat) == 0) {
        result.value = file_stat.st_mtime;
    }
#endif
    return result;
}

inline b32 platform_file_time_changed(FileTime old_time, FileTime new_time) {
    return old_time.value != new_time.value;
}
