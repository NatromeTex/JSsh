// language.c - File type detection and language settings
#include "language.h"
#include <string.h>
#include <stddef.h>

const char *get_file_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return NULL;
    return dot + 1;
}

FileType detect_filetype(const char *filename) {
    const char *ext = get_file_ext(filename);
    if (!ext) return FT_NONE;

    if (strcmp(ext, "c") == 0)
        return FT_C;

    if (strcmp(ext, "h") == 0)
        return FT_C;

    if (strcmp(ext, "cpp") == 0 || strcmp(ext, "cc") == 0 || strcmp(ext, "cxx") == 0)
        return FT_CPP;

    if (strcmp(ext, "hpp") == 0)
        return FT_CPP;

    return FT_NONE;
}
