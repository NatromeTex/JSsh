#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sys.h"
#include "quickjs.h"

#define REGISTER_APP(ctx, global, name, func) JS_SetPropertyStr(ctx, global, name, JS_NewCFunction(ctx, func, name, 0));

static struct termios orig_termios;
static int rawmode_enabled = 0;

typedef void (*app_register_func)(JSContext *ctx);

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

// sys.raw_mode(1)
JSValue js_sys_enableRawMode(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    if (rawmode_enabled) return JS_UNDEFINED;

    if (!isatty(STDIN_FILENO))
        return JS_ThrowTypeError(ctx, "stdin is not a tty");

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        return JS_ThrowTypeError(ctx, "tcgetattr failed");

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        return JS_ThrowTypeError(ctx, "tcsetattr failed");

    rawmode_enabled = 1;
    return JS_UNDEFINED;
}

// sys.raw_mode(0)
JSValue js_sys_disableRawMode(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    if (!rawmode_enabled) return JS_UNDEFINED;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        return JS_ThrowTypeError(ctx, "tcsetattr restore failed");
    rawmode_enabled = 0;
    return JS_UNDEFINED;
}

// sys.readkey()
JSValue js_sys_readKey(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) {
        return JS_NULL;
    }
    char buf[2] = {c, '\0'};
    return JS_NewString(ctx, buf);
}

// Get terminal window size
JSValue js_sys_getWinSize(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return JS_ThrowTypeError(ctx, "ioctl TIOCGWINSZ failed");
    }
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "rows", JS_NewInt32(ctx, ws.ws_row));
    JS_SetPropertyStr(ctx, obj, "cols", JS_NewInt32(ctx, ws.ws_col));
    return obj;
}

// sys.isatty(fd)
static JSValue js_sys_isatty(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "isatty(fd)");
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    return JS_NewBool(ctx, isatty(fd));
}

// sys.ttyname(fd)
static JSValue js_sys_ttyname(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "ttyname(fd)");
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    char *name = ttyname(fd);
    if (!name) return JS_NULL;
    return JS_NewString(ctx, name);
}

// sys.registerApp(name, func)
static JSValue js_sys_registerApp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "registerApp(name, func)");

    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;

    JSValue func = JS_DupValue(ctx, argv[1]); // keep ref
    JSValue global_obj = JS_GetGlobalObject(ctx);

    JS_SetPropertyStr(ctx, global_obj, name, func);

    JS_FreeValue(ctx, global_obj);
    JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
}

void js_init_apps(JSContext *ctx) {
    const char *apps_root = "/lib/apps";
    DIR *dir = opendir(apps_root);
    if (!dir) {
        perror("opendir apps_root failed");
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_DIR) continue;
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s/entry.so", apps_root, ent->d_name);

        if (access(path, F_OK) != 0) continue;

        void *handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
        if (!handle) {
            fprintf(stderr, "dlopen failed for %s: %s\n", path, dlerror());
            continue;
        }

        dlerror(); // clear old errors
        app_register_func reg = (app_register_func)dlsym(handle, "register_app");
        const char *err = dlerror();
        if (err != NULL) {
            fprintf(stderr, "dlsym(register_app) failed for %s: %s\n", path, err);
            dlclose(handle);
            continue;
        }

        // call the app’s registration
        reg(ctx);
    }
    closedir(dir);
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
    JS_SetPropertyStr(ctx, sys, "enableRawMode", JS_NewCFunction(ctx, js_sys_enableRawMode, "enableRawMode", 0));
    JS_SetPropertyStr(ctx, sys, "disableRawMode", JS_NewCFunction(ctx, js_sys_disableRawMode, "disableRawMode", 0));
    JS_SetPropertyStr(ctx, sys, "readKey", JS_NewCFunction(ctx, js_sys_readKey, "readKey", 0));
    JS_SetPropertyStr(ctx, sys, "getWinSize", JS_NewCFunction(ctx, js_sys_getWinSize, "getWinSize", 0));
    JS_SetPropertyStr(ctx, sys, "isatty", JS_NewCFunction(ctx, js_sys_isatty, "isatty", 1));
    JS_SetPropertyStr(ctx, sys, "ttyname", JS_NewCFunction(ctx, js_sys_ttyname, "ttyname", 1));
    JS_SetPropertyStr(ctx, sys, "registerApp", JS_NewCFunction(ctx, js_sys_registerApp, "registerApp", 2));

    JS_SetPropertyStr(ctx, global_obj, "sys", sys);
    JS_FreeValue(ctx, global_obj);
}