#pragma once

#include "lib/def.h"
#include <sys/stat.h>

struct FileTime {
    u64 value;
};

inline FileTime platform_get_file_write_time(const char *filename) {
    FileTime result = {};
    struct stat file_stat;
    if (stat(filename, &file_stat) == 0) {
        result.value = file_stat.st_mtime;
    }
    return result;
}

inline b32 platform_file_time_changed(FileTime old_time, FileTime new_time) {
    return old_time.value != new_time.value;
}
