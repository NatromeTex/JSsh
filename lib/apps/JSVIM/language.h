// language.h - File type detection and language settings
#ifndef LANGUAGE_H
#define LANGUAGE_H

// File Types
typedef enum {
    FT_NONE = 0,
    FT_C,
    FT_CPP,
    FT_JS,
    FT_PYTHON,
    FT_RUST,
    FT_GO,
    FT_JAVA,
    FT_SH,
    FT_MAKEFILE,
    FT_JSON,
    FT_MARKDOWN,
} FileType;

// Get file extension from filename
const char *get_file_ext(const char *filename);

// Detect filetype from filename
FileType detect_filetype(const char *filename);

#endif
