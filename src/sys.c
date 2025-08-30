#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sys.h"
#include "quickjs.h"

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


// ENV file configuration
typedef struct {
    char *key;
    char *value;
} EnvEntry;

static EnvEntry *g_env = NULL;
static size_t g_env_count = 0;

// default settings
static const char *default_env[] = {
    "color_dir={blue}",
    "color_exe={green}",
    "color_link={cyan}",
    "color_fifo={yellow}",
    "color_sock={magenta}",
    "color_chr={red}",
    "color_blk={red}",
    "color_reg={white}",
    NULL
};

// add to env file
static void env_add(const char *key, const char *value) {
    g_env = realloc(g_env, (g_env_count + 1) * sizeof(EnvEntry));
    g_env[g_env_count].key = strdup(key);
    g_env[g_env_count].value = strdup(value);
    g_env_count++;
}

// read setting from env file
const char *env_get(const char *key, const char *def) {
    for (size_t i = 0; i < g_env_count; i++) {
        if (strcmp(g_env[i].key, key) == 0)
            return g_env[i].value;
    }
    return def;
}

// call at startup to load the env file
void env_load(const char *filename) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        // file missing → create with defaults
        FILE *f = fopen(filename, "w");
        if (!f) return;
        for (const char **p = default_env; *p; p++) {
            fprintf(f, "%s\n", *p);
            char key[64], val[128];
            if (sscanf(*p, "%63[^=]=%127[^\n]", key, val) == 2) {
                env_add(key, val);
            }
        }
        fclose(f);
        return;
    }

    // load existing file
    FILE *f = fopen(filename, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char key[64], val[128];
        if (sscanf(line, "%63[^=]=%127[^\n]", key, val) == 2) {
            env_add(key, val);
        }
    }
    fclose(f);
}

// show contents of env file
void env_show(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("cannot open %s: %s\n", filename, strerror(errno));
        return;
    }
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        fputs(buf, stdout);
    }
    fclose(f);
}

// QOL updates

// ("") autocomplete
static const char *autoquote_cmds[] = { "ls", "cat", "chmod", "mkdir", "cd", "touch", "echo", "rm", "js", NULL };

static int is_autoquote_cmd(const char *tok) {
    for (int i = 0; autoquote_cmds[i]; i++)
        if (strcmp(tok, autoquote_cmds[i]) == 0) return 1;
    return 0;
}

static int jssh_tab_handler(int count, int key) {
    (void)count; (void)key;

    char *buf = rl_line_buffer;
    int p = rl_point, len = rl_end;

    // find first token
    int i = 0; while (i < len && isspace((unsigned char)buf[i])) i++;
    int start = i;
    while (i < len && !isspace((unsigned char)buf[i]) && buf[i] != '(') i++;
    int tok_end = i;

    // token string
    char tok[64] = {0};
    int tlen = tok_end - start;
    if (tlen <= 0 || tlen >= (int)sizeof(tok)) { rl_complete(0, 0); return 0; }
    memcpy(tok, buf + start, tlen); tok[tlen] = 0;

    // only when cursor is at end of token (ignoring trailing spaces)
    int j = tok_end; while (j < len && isspace((unsigned char)buf[j])) j++;
    if (!is_autoquote_cmd(tok) || p != tok_end || (j < len && buf[j] == '(')) {
        // fallback to normal completion
        rl_complete(0, 0);
        return 0;
    }

    // remove trailing spaces after token
    if (tok_end < len && j > tok_end) {
        rl_point = tok_end;
        rl_delete_text(tok_end, j);
        rl_point = tok_end;
    }

    // insert ("")
    rl_insert_text("(\"\")");
    rl_point -= 2; // place cursor between the quotes
    return 0;
}

void init_qol_bindings(void) {
    rl_bind_key('\t', jssh_tab_handler);
}