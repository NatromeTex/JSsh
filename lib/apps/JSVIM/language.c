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
    if (!filename) return FT_NONE;
    
    // Check for extensionless files by basename
    const char *basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;
    
    if (strcmp(basename, "Makefile") == 0 || strcmp(basename, "makefile") == 0 ||
        strcmp(basename, "GNUmakefile") == 0)
        return FT_MAKEFILE;
    
    const char *ext = get_file_ext(filename);
    if (!ext) return FT_NONE;

    // C
    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0)
        return FT_C;

    // C++
    if (strcmp(ext, "cpp") == 0 || strcmp(ext, "cc") == 0 || strcmp(ext, "cxx") == 0 ||
        strcmp(ext, "hpp") == 0 || strcmp(ext, "hxx") == 0 || strcmp(ext, "hh") == 0)
        return FT_CPP;

    // JavaScript/TypeScript
    if (strcmp(ext, "js") == 0 || strcmp(ext, "mjs") == 0 || strcmp(ext, "cjs") == 0 ||
        strcmp(ext, "ts") == 0 || strcmp(ext, "tsx") == 0 || strcmp(ext, "jsx") == 0)
        return FT_JS;

    // Python
    if (strcmp(ext, "py") == 0 || strcmp(ext, "pyw") == 0 || strcmp(ext, "pyi") == 0)
        return FT_PYTHON;

    // Rust
    if (strcmp(ext, "rs") == 0)
        return FT_RUST;

    // Go
    if (strcmp(ext, "go") == 0)
        return FT_GO;

    // Java
    if (strcmp(ext, "java") == 0)
        return FT_JAVA;

    // Shell
    if (strcmp(ext, "sh") == 0 || strcmp(ext, "bash") == 0 || strcmp(ext, "zsh") == 0)
        return FT_SH;

    // Makefile
    if (strcmp(ext, "mk") == 0)
        return FT_MAKEFILE;

    // JSON
    if (strcmp(ext, "json") == 0)
        return FT_JSON;

    // Markdown
    if (strcmp(ext, "md") == 0 || strcmp(ext, "markdown") == 0)
        return FT_MARKDOWN;

    return FT_NONE;
}
