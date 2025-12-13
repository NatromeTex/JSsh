// language.h - File type detection and language settings
#ifndef LANGUAGE_H
#define LANGUAGE_H

// File Types
typedef enum {
    FT_NONE = 0,
    FT_C,
    FT_CPP,
} FileType;

// Get file extension from filename
const char *get_file_ext(const char *filename);

// Detect filetype from filename
FileType detect_filetype(const char *filename);

#endif
