// util.h - Common utility functions
#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

// Version macros
#ifndef JSVIM_VERSION
#define JSVIM_VERSION "0.1.0"
#endif
#ifndef JSSH_VERSION
#define JSSH_VERSION "unknown"
#endif

// Initial buffer capacity
#define INITIAL_CAP 128

// Duplicate a string (caller must free)
char *dupstr(const char *s);

// Check if a file exists
int file_exists(const char *fname);

#endif
