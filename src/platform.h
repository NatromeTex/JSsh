#pragma once
#include <limits.h>
#include <unistd.h>

#ifdef MACOS
#include <mach-o/dyld.h>
static inline int get_self_exe(char *buf, size_t size) {
    uint32_t len = (uint32_t)size;
    if (_NSGetExecutablePath(buf, &len) != 0) return -1;
    return 0;
}
#else
static inline int get_self_exe(char *buf, size_t size) {
    ssize_t r = readlink("/proc/self/exe", buf, size - 1);
    if (r < 0) return -1;
    buf[r] = '\0';
    return 0;
}
#endif
