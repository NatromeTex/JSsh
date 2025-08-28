#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include "quickjs.h"
#include "sys.h"

// sys.open(path, flags, mode)
static JSValue js_sys_open(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "open(path, flags[, mode])");

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;

    int flags, mode = 0;
    if (JS_ToInt32(ctx, &flags, argv[1])) {
        JS_FreeCString(ctx, path);
        return JS_EXCEPTION;
    }
    if (argc > 2 && JS_ToInt32(ctx, &mode, argv[2])) {
        JS_FreeCString(ctx, path);
        return JS_EXCEPTION;
    }

    int fd = open(path, flags, mode);
    JS_FreeCString(ctx, path);
    if (fd < 0)
        return JS_ThrowInternalError(ctx, "open failed: %s", strerror(errno));

    return JS_NewInt32(ctx, fd);
}

// sys.read(fd, len) -> string
static JSValue js_sys_read(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "read(fd, len)");

    int fd, len;
    if (JS_ToInt32(ctx, &fd, argv[0]) || JS_ToInt32(ctx, &len, argv[1]))
        return JS_EXCEPTION;

    char *buf = malloc(len + 1);
    if (!buf) return JS_EXCEPTION;

    ssize_t n = read(fd, buf, len);
    if (n < 0) {
        free(buf);
        return JS_ThrowInternalError(ctx, "read failed: %s", strerror(errno));
    }
    buf[n] = '\0';

    JSValue ret = JS_NewStringLen(ctx, buf, n);
    free(buf);
    return ret;
}

// sys.write(fd, data)
static JSValue js_sys_write(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "write(fd, data)");

    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0]))
        return JS_EXCEPTION;

    size_t len;
    const char *data = JS_ToCStringLen(ctx, &len, argv[1]);
    if (!data) return JS_EXCEPTION;

    ssize_t n = write(fd, data, len);
    JS_FreeCString(ctx, data);

    if (n < 0)
        return JS_ThrowInternalError(ctx, "write failed: %s", strerror(errno));

    return JS_NewInt32(ctx, n);
}

// sys.close(fd)
static JSValue js_sys_close(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "close(fd)");

    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0]))
        return JS_EXCEPTION;

    if (close(fd) < 0)
        return JS_ThrowInternalError(ctx, "close failed: %s", strerror(errno));

    return JS_UNDEFINED;
}

// sys.getcwd()
static JSValue js_sys_getcwd(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    char buf[PATH_MAX];
    if (!getcwd(buf, sizeof(buf)))
        return JS_ThrowInternalError(ctx, "getcwd failed: %s", strerror(errno));
    return JS_NewString(ctx, buf);
}

// sys.chdir(path)
static JSValue js_sys_chdir(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "chdir(path)");

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;

    int ret = chdir(path);
    JS_FreeCString(ctx, path);
    if (ret != 0)
        return JS_ThrowInternalError(ctx, "chdir failed: %s", strerror(errno));

    return JS_UNDEFINED;
}

// sys.readdir(path) -> array of strings
static JSValue js_sys_readdir(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "readdir(path)");

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) return JS_EXCEPTION;

    DIR *dir = opendir(path);
    if (!dir) {
        JS_FreeCString(ctx, path);
        return JS_ThrowInternalError(ctx, "opendir failed: %s", strerror(errno));
    }

    JSValue arr = JS_NewArray(ctx);
    int idx = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        JS_SetPropertyUint32(ctx, arr, idx++, JS_NewString(ctx, ent->d_name));
    }
    closedir(dir);
    JS_FreeCString(ctx, path);
    return arr;
}

// Register sys.* functions
void js_init_sys(JSContext *ctx) {
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue sys = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, sys, "open", JS_NewCFunction(ctx, js_sys_open, "open", 3));
    JS_SetPropertyStr(ctx, sys, "read", JS_NewCFunction(ctx, js_sys_read, "read", 2));
    JS_SetPropertyStr(ctx, sys, "write", JS_NewCFunction(ctx, js_sys_write, "write", 2));
    JS_SetPropertyStr(ctx, sys, "close", JS_NewCFunction(ctx, js_sys_close, "close", 1));
    JS_SetPropertyStr(ctx, sys, "getcwd", JS_NewCFunction(ctx, js_sys_getcwd, "getcwd", 0));
    JS_SetPropertyStr(ctx, sys, "chdir", JS_NewCFunction(ctx, js_sys_chdir, "chdir", 1));
    JS_SetPropertyStr(ctx, sys, "readdir", JS_NewCFunction(ctx, js_sys_readdir, "readdir", 1));

    JS_SetPropertyStr(ctx, global_obj, "sys", sys);
    JS_FreeValue(ctx, global_obj);
}
