// util.c - Common utility functions
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

char *dupstr(const char *s) {
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

int file_exists(const char *fname) {
    struct stat st;
    return stat(fname, &st) == 0;
}
