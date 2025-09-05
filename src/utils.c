// contains utilities for terminal working such as size, color

#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <stdarg.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <readline/history.h>
#include <readline/readline.h>
<<<<<<< HEAD
=======
#include "sys.h"
>>>>>>> bea3155 (Moved env logic to env.ch/env.h)
#include "env.h"
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

// import JS command files
void load_js_file(JSContext *ctx, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        printR("{red}Failed to open %s{reset}\n", path);
        return;
    }

    struct stat st;
    if (fstat(fileno(f), &st) < 0) {
        printR("{red}fstat failed on %s{reset}\n", path);
        fclose(f);
        return;
    }

    char *buf = malloc(st.st_size + 1);
    if (!buf) {
        printR("{red}Out of memory while loading %s{reset}\n", path);
        fclose(f);
        return;
    }

    size_t nread = fread(buf, 1, st.st_size, f);
    fclose(f);
    if (nread != (size_t)st.st_size) {
        printR("{red}Short read while loading %s{reset}\n", path);
        free(buf);
        return;
    }
    buf[st.st_size] = '\0';

    JSValue val = JS_Eval(ctx, buf, st.st_size, path, JS_EVAL_TYPE_GLOBAL);
    free(buf);

    if (JS_IsException(val)) {
        printR("{red}JS error in %s{reset}\n", path);
        JSValue exc = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, exc);
        if (msg) {
            printR("{yellow}%s{reset}\n", msg);
            JS_FreeCString(ctx, msg);
        }
        JS_FreeValue(ctx, exc);
        JS_FreeValue(ctx, val);
        return;
    }

    JS_FreeValue(ctx, val);
}

void load_js_libs(JSContext *ctx, const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        perror("opendir js/lib");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            const char *name = entry->d_name;
            size_t len = strlen(name);
            if (len > 3 && strcmp(name + len - 3, ".js") == 0) {
                char path[PATH_MAX];
                snprintf(path, sizeof(path), "%s/%s", dirpath, name);
                printf("[JSsh] Loading %s\n", path);
                load_js_file(ctx, path);
            }
        }
    }
    closedir(dir);
}

// env_get
JSValue js_env_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_EXCEPTION;

    const char *val = env_get(key, NULL);
    JS_FreeCString(ctx, key);

    if (!val) return JS_UNDEFINED;
    return JS_NewString(ctx, val);
}

// env_add
JSValue js_env_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *key = JS_ToCString(ctx, argv[0]);
    const char *val = JS_ToCString(ctx, argv[1]);
    if (!key || !val) {
        if (key) JS_FreeCString(ctx, key);
        if (val) JS_FreeCString(ctx, val);
        return JS_EXCEPTION;
    }

    env_add(key, val);

    JS_FreeCString(ctx, key);
    JS_FreeCString(ctx, val);

    return JS_UNDEFINED;
}

<<<<<<< HEAD
// QOL updates

// ("") autocomplete
static const char *autoquote_cmds[] = { "ls", "cat", "chmod", "mkdir", "cd", "touch", "echo", "rm", "js", NULL };
=======
// printR
JSValue js_printR(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "printR(string)");

    const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_EXCEPTION;

    printR("%s", str);
    JS_FreeCString(ctx, str);

    return JS_UNDEFINED;
}

// QOL updates

// ("") autocomplete
static const char *autoquote_cmds[] = { "ls", "cat", "chmod", "mkdir", "cd", "touch", "echo", "rm", "js", "tac",NULL };
>>>>>>> bea3155 (Moved env logic to env.ch/env.h)

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
<<<<<<< HEAD
}
=======
}

// Syntax Highliting
#define CLR_RESET   "\033[0m"
#define CLR_KEYWORD "\033[38;2;85;154;211m"  // blue
#define CLR_STRING  "\033[38;2;206;145;120m"  // green
#define CLR_NUMBER  "\033[38;2;148;206;168m"  // magenta
#define CLR_FUNCTION "\033[38;2;220;220;170m" // yelow

static const char *js_keywords[] = {
    "function","return","if","else","while","for","var","let","const",
    "true","false","null","undefined","new","class","import","export",NULL
};

static char *highlight_line(const char *line) {
    size_t cap = strlen(line) * 10 + 64;
    char *out = malloc(cap);
    if (!out) return NULL;

    char *dst = out;
    const char *p = line;

    while (*p) {
        int matched = 0;

        // keywords (must be full word)
        for (int i = 0; js_keywords[i]; i++) {
            size_t len = strlen(js_keywords[i]);
            if (strncmp(p, js_keywords[i], len) == 0) {
                char next = p[len];
                char prev = (p > line) ? p[-1] : '\0';
                if ((next == '\0' || isspace((unsigned char)next) || next == '(') &&
                    (prev == '\0' || (!isalnum((unsigned char)prev) && prev != '_'))) {
                    dst += sprintf(dst, "%s%.*s%s", CLR_KEYWORD, (int)len, p, CLR_RESET);
                    p += len;
                    matched = 1;
                    break;
                }
            }
        }
        if (matched) continue;

        // strings
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            const char *start = p - 1;
            while (*p && *p != quote) p++;
            if (*p == quote) p++;
            size_t len = p - start;
            dst += sprintf(dst, "%s%.*s%s", CLR_STRING, (int)len, start, CLR_RESET);
            continue;
        }

        // numbers
        if (isdigit((unsigned char)*p)) {
            const char *start = p;
            while (isdigit((unsigned char)*p)) p++;
            size_t len = p - start;
            dst += sprintf(dst, "%s%.*s%s", CLR_NUMBER, (int)len, start, CLR_RESET);
            continue;
        }

        // identifiers → check for function name (followed by '(')
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            size_t len = p - start;
            if (*p == '(') {
                dst += sprintf(dst, "%s%.*s%s", CLR_FUNCTION, (int)len, start, CLR_RESET);
            } else {
                dst += sprintf(dst, "%.*s", (int)len, start);
            }
            continue;
        }

        // default single char
        *dst++ = *p++;
    }

    *dst = '\0';
    return out;
}

void jssh_redisplay(void) {
    rl_on_new_line();
    rl_clear_visible_line();

    char *hl = highlight_line(rl_line_buffer);
    const char *line = hl ? hl : rl_line_buffer;

    fputs(rl_display_prompt, stdout);
    fputs(line, stdout);

    // Move cursor back if not at end
    int cur = rl_point;
    int end = strlen(rl_line_buffer);
    if (end > cur) {
        // Move left (end - cur) times
        fprintf(stdout, "\033[%dD", end - cur);
    }

    fflush(stdout);
    if (hl) free(hl);
}


