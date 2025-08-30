// contains utilities for terminal working such as size, color

#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "sys.h"
#include "utils.h"

static int g_color_mode = 8; // 8, 256, or 16777216 (truecolor)

// History path setup
// Call this at startup
char history_path[PATH_MAX];
const char *history_file = NULL;

void init_history_file(void) {
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw ? pw->pw_dir : getenv("HOME");
    if (!homedir) homedir = ".";

    snprintf(history_path, sizeof(history_path), "%s/.jssh_history", homedir);
    history_file = history_path;
}

// Call this once at startup
void detect_color_mode(void) {
    const char *colorterm = getenv("COLORTERM");
    const char *term = getenv("TERM");
    if (colorterm && strstr(colorterm, "truecolor")) {
        g_color_mode = 16777216;
    } else if (term && strstr(term, "256color")) {
        g_color_mode = 256;
    } else {
        g_color_mode = 8;
    }
}

// Nearest 8-color ANSI fallback
static const char *ansi8(const char *name) {
    if (strcmp(name, "reset") == 0) return "\033[0m";
    if (strcmp(name, "black") == 0) return "\033[30m";
    if (strcmp(name, "red") == 0) return "\033[31m";
    if (strcmp(name, "green") == 0) return "\033[32m";
    if (strcmp(name, "yellow") == 0) return "\033[33m";
    if (strcmp(name, "blue") == 0) return "\033[34m";
    if (strcmp(name, "magenta") == 0) return "\033[35m";
    if (strcmp(name, "cyan") == 0) return "\033[36m";
    if (strcmp(name, "white") == 0) return "\033[37m";
    return NULL;
}

// Render either named colors or {rgb:r,g,b}
static void render_colors(const char *input) {
    const char *p = input;
    while (*p) {
        if (*p == '{') {
            const char *end = strchr(p, '}');
            if (end) {
                char tag[64];
                size_t len = end - p - 1;
                if (len < sizeof(tag)) {
                    strncpy(tag, p + 1, len);
                    tag[len] = '\0';

                    // Check for rgb: prefix
                    if (strncmp(tag, "rgb:", 4) == 0) {
                        int r, g, b;
                        if (sscanf(tag + 4, "%d,%d,%d", &r, &g, &b) == 3) {
                            if (g_color_mode == 16777216) {
                                printf("\033[38;2;%d;%d;%dm", r, g, b);
                            } else if (g_color_mode == 256) {
                                // Approximate 256-color cube
                                int R = r / 51, G = g / 51, B = b / 51;
                                int idx = 16 + 36 * R + 6 * G + B;
                                printf("\033[38;5;%dm", idx);
                            } else {
                                // crude 8-color fallback
                                fputs("\033[37m", stdout); // white
                            }
                            p = end + 1;
                            continue;
                        }
                    } else {
                        // Named colors
                        const char *code = ansi8(tag);
                        if (code) {
                            fputs(code, stdout);
                            p = end + 1;
                            continue;
                        }
                    }
                }
            }
        }
        fputc(*p, stdout);
        p++;
    }
    fputs("\033[0m", stdout); // reset at the end
}

void printR(const char *fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    render_colors(buf);
}

void print_name(const char *name, mode_t mode) {
    const char *color = NULL;

    if (S_ISDIR(mode))       color = env_get("color_dir", "{blue}");
    else if (S_ISLNK(mode))  color = env_get("color_link", "{cyan}");
    else if (S_ISFIFO(mode)) color = env_get("color_fifo", "{yellow}");
    else if (S_ISSOCK(mode)) color = env_get("color_sock", "{magenta}");
    else if (S_ISCHR(mode))  color = env_get("color_chr", "{red}");
    else if (S_ISBLK(mode))  color = env_get("color_blk", "{red}");
    else if (mode & S_IXUSR) color = env_get("color_exe", "{green}");
    else                     color = env_get("color_reg", "{white}");

    printR("%s%s{reset}", color, name);
}

// update
JSValue js_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // Save current history
    write_history(history_file);

    // Run make in current directory
    int ret = system("make");
    if (ret != 0) {
        fprintf(stderr, "update: make failed\n");
        return JS_UNDEFINED;
    }

    // Replace current process with the newly built binary
    char *argv0 = "./bin/jssh";   // path to new binary
    char *args[] = { argv0, NULL };
    execv(argv0, args);           // replaces current REPL process

    // If execv fails
    perror("execv");
    return JS_UNDEFINED;
}

// show_env
JSValue js_show_env(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : ".";
    }
    char envpath[512];
    snprintf(envpath, sizeof(envpath), "%s/.jssh_env", home);

    env_show(envpath);
    return JS_UNDEFINED;
}