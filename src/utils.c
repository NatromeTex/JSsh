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
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "sys.h"
#include "env.h"
#include "utils.h"

static int g_color_mode = 8; // 8, 256, or 16777216 (truecolor)
int js_lib_count = 0; // Pure JS libs counter
static int g_previous_lines = 1; // Lines drawn by readline

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

int get_terminal_dimensions(int *width, int *height) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    }
    *width = ws.ws_col;
    *height = ws.ws_row;
    return 0;
}

int visual_width(const char *str) {
    int width = 0;
    if (!str) return 0;

    while (*str) {
        if (*str == '\033') {
            str++;
            if (*str == '[') {
                str++;
                while ((*str >= '0' && *str <= '9') || *str == ';') {
                    str++;
                }
                if (*str) str++;
            }
        } else {
            width++;
            str++;
        }
    }
    return width;
}

char* rl_line_buffer_slice(int start, int end) {
    int len = end - start;
    if (len < 0) len = 0;
    char *slice = malloc(len + 1);
    if (slice) {
        strncpy(slice, rl_line_buffer + start, len);
        slice[len] = '\0';
    }
    return slice;
}

// Run make via fork+execvp; returns 0 on success, -1 on failure.
static int run_make(char **make_argv) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        execvp("make", make_argv);
        perror("execvp make");
        _exit(1);
    }
    int status;
    pid_t r;
    do { r = waitpid(pid, &status, 0); } while (r == -1 && errno == EINTR);
    if (r == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return -1;
    return 0;
}

// update
JSValue js_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    write_history(history_file);

    if (argc == 0) {
        // No user input — safe literal command
        char *clean_argv[] = { "make", "clean", NULL };
        if (run_make(clean_argv) != 0) {
            fprintf(stderr, "update: make clean failed\n");
            return JS_UNDEFINED;
        }
        char *build_argv[] = { "make", NULL };
        if (run_make(build_argv) != 0) {
            fprintf(stderr, "update: make failed\n");
            return JS_UNDEFINED;
        }
    } else {
        // Build modules string with length tracking (no shell expansion risk
        // because we pass args directly to execvp, not through a shell).
        char modules[1024];
        int modules_len = 0;
        modules[0] = '\0';
        int do_install = 0;

        for (int i = 0; i < argc; i++) {
            const char *arg = JS_ToCString(ctx, argv[i]);
            if (!arg) continue;

            if (strcmp(arg, "install") == 0) {
                do_install = 1;
                JS_FreeCString(ctx, arg);
                continue;
            }

            int arg_len = (int)strlen(arg);
            if (modules_len + arg_len + 2 <= (int)sizeof(modules) - 1) {
                if (modules_len > 0) modules[modules_len++] = ' ';
                memcpy(modules + modules_len, arg, arg_len);
                modules_len += arg_len;
                modules[modules_len] = '\0';
            }
            JS_FreeCString(ctx, arg);
        }

        // Build argv for make — no shell, so metacharacters in modules are safe
        char modules_kv[1200];
        char *make_argv[8];
        int ai = 0;
        make_argv[ai++] = "make";
        if (modules[0]) {
            snprintf(modules_kv, sizeof(modules_kv), "MODULES=%s", modules);
            make_argv[ai++] = modules_kv;
        }
        if (do_install)
            make_argv[ai++] = "install";
        make_argv[ai] = NULL;

        if (run_make(make_argv) != 0) {
            fprintf(stderr, "update: make failed\n");
            return JS_UNDEFINED;
        }
    }

    char *argv0 = "./bin/jssh";
    char *exec_args[] = { argv0, NULL };
    execv(argv0, exec_args);
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
        perror("opendir lib/js");
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
                js_lib_count++;
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
static const char *autoquote_cmds[] = { "ls", "cat", "chmod", "mkdir", "cd", "touch", "echo", "rm", "js", "tac", "help", NULL};

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

// Syntax Highlighting
#define CLR_RESET    "\033[0m"
#define CLR_KEYWORD  "\033[38;2;85;130;231m"
#define CLR_OBJECT   "\033[38;2;116;180;214m"
#define CLR_STRING   "\033[38;2;206;145;120m"
#define CLR_NUMBER   "\033[38;2;148;206;110m"
#define CLR_FUNCTION "\033[38;2;220;220;110m"
#define CLR_COMMENT  "\033[38;2;106;153;85m"

// Lengths precomputed via sizeof to avoid strlen() in the hot loop
#define KW(s) { s, (int)(sizeof(s)-1) }
static const struct { const char *word; int len; } js_keywords[] = {
    KW("function"), KW("return"), KW("if"),    KW("else"),   KW("while"),
    KW("for"),      KW("var"),    KW("let"),   KW("const"),  KW("true"),
    KW("false"),    KW("null"),   KW("undefined"), KW("new"), KW("class"),
    KW("import"),   KW("export"), { NULL, 0 }
};
static const struct { const char *word; int len; } js_objects[] = {
    KW("fs"), KW("cmp"), KW("net"), KW("crypt"), KW("sys"), KW("git"),
    { NULL, 0 }
};
#undef KW

// Append a compile-time-known color code string using sizeof instead of strlen
#define APPEND_CLR(dst, clr) do { \
    memcpy((dst), (clr), sizeof(clr)-1); (dst) += sizeof(clr)-1; \
} while (0)

static char *highlight_line(const char *line) {
    size_t in_len = strlen(line);
    // 26 bytes per input char covers worst case: single char token wrapped in
    // longest color code (20 bytes) + reset (4 bytes) = 25 bytes overhead.
    char *out = malloc(in_len * 26 + 64);
    if (!out) return NULL;

    char *dst = out;
    const char *p = line;

    while (*p) {
        // Single-line comment: rest of line gets comment color
        if (p[0] == '/' && p[1] == '/') {
            size_t rest = strlen(p);
            APPEND_CLR(dst, CLR_COMMENT);
            memcpy(dst, p, rest); dst += rest;
            APPEND_CLR(dst, CLR_RESET);
            p += rest;
            continue;
        }

        // Block comment
        if (p[0] == '/' && p[1] == '*') {
            const char *end = strstr(p + 2, "*/");
            size_t len = end ? (size_t)(end - p + 2) : strlen(p);
            APPEND_CLR(dst, CLR_COMMENT);
            memcpy(dst, p, len); dst += len;
            APPEND_CLR(dst, CLR_RESET);
            p += len;
            continue;
        }

        // Strings: " ' ` with escape sequence handling
        if (*p == '"' || *p == '\'' || *p == '`') {
            char quote = *p;
            const char *start = p++;
            while (*p && *p != quote) {
                if (*p == '\\' && p[1]) p++; // skip escaped char
                p++;
            }
            if (*p == quote) p++;
            size_t len = (size_t)(p - start);
            APPEND_CLR(dst, CLR_STRING);
            memcpy(dst, start, len); dst += len;
            APPEND_CLR(dst, CLR_RESET);
            continue;
        }

        // Keywords and identifiers (both start with alpha or _)
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            int len = (int)(p - start);

            char prev = (start > line) ? start[-1] : '\0';
            char next = *p;

            // Property access (foo.bar): the identifier after '.' is never a keyword
            int is_prop = (prev == '.');
            int word_start = !is_prop &&
                (prev == '\0' || (!isalnum((unsigned char)prev) && prev != '_'));
            int word_end = (next == '\0' || (!isalnum((unsigned char)next) && next != '_'));

            int colored = 0;

            // Keyword: must be a full word and not after '.'
            if (word_start && word_end) {
                for (int i = 0; js_keywords[i].word; i++) {
                    if (len == js_keywords[i].len &&
                        memcmp(start, js_keywords[i].word, len) == 0) {
                        APPEND_CLR(dst, CLR_KEYWORD);
                        memcpy(dst, start, len); dst += len;
                        APPEND_CLR(dst, CLR_RESET);
                        colored = 1;
                        break;
                    }
                }
            }

            if (!colored) {
                const char *look = p;
                while (isspace((unsigned char)*look)) look++;

                if (*look == '.') {
                    // Known object?
                    for (int i = 0; js_objects[i].word; i++) {
                        if (len == js_objects[i].len &&
                            memcmp(start, js_objects[i].word, len) == 0) {
                            APPEND_CLR(dst, CLR_OBJECT);
                            memcpy(dst, start, len); dst += len;
                            APPEND_CLR(dst, CLR_RESET);
                            colored = 1;
                            break;
                        }
                    }
                } else if (*look == '(') {
                    // Function call
                    APPEND_CLR(dst, CLR_FUNCTION);
                    memcpy(dst, start, len); dst += len;
                    APPEND_CLR(dst, CLR_RESET);
                    colored = 1;
                }
            }

            if (!colored) {
                memcpy(dst, start, len); dst += len;
            }
            continue;
        }

        // Numbers: integer, float (3.14), hex (0xFF), scientific (1e10)
        if (isdigit((unsigned char)*p) ||
            (*p == '.' && isdigit((unsigned char)p[1]))) {
            const char *start = p;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
                p += 2;
                while (isxdigit((unsigned char)*p)) p++;
            } else {
                while (isdigit((unsigned char)*p)) p++;
                if (*p == '.') { p++; while (isdigit((unsigned char)*p)) p++; }
                if (*p == 'e' || *p == 'E') {
                    p++;
                    if (*p == '+' || *p == '-') p++;
                    while (isdigit((unsigned char)*p)) p++;
                }
            }
            size_t len = (size_t)(p - start);
            APPEND_CLR(dst, CLR_NUMBER);
            memcpy(dst, start, len); dst += len;
            APPEND_CLR(dst, CLR_RESET);
            continue;
        }

        // Default: pass character through unchanged
        *dst++ = *p++;
    }

    *dst = '\0';
    return out;
}

typedef struct {
    const char *text;  // suggestion string
    int active;        // 1 if should show, 0 otherwise
} prediction_t;

static prediction_t jssh_predict(const char *buf, int point) {
    prediction_t out = { .text = NULL, .active = 0 };

    // --- simple tokenizer: get first word ---
    int i = 0;
    while (buf[i] && isspace((unsigned char)buf[i])) i++;
    int start = i;
    while (buf[i] && !isspace((unsigned char)buf[i]) && buf[i] != '(') i++;
    int end = i;

    int tok_len = end - start;
    if (tok_len <= 0 || tok_len >= 64) return out;

    char tok[64];
    memcpy(tok, buf + start, tok_len);
    tok[tok_len] = 0;

    // --- only trigger if cursor is at end of token ---
    int j = end;
    while (buf[j] && isspace((unsigned char)buf[j])) j++;
    if (point != end || buf[j] == '(') return out;

    if (is_autoquote_cmd(tok)) {
        out.text = "(\"\")";
        out.active = 1;
    }

    return out;
}

void jssh_redisplay(void) {
    fputs("\r", stdout);

    if(g_previous_lines > 1) {
        fprintf(stdout, "\033[%dA", g_previous_lines - 1);
    }
    fputs("\033[J", stdout);

    int w, h;
    if (get_terminal_dimensions(&w, &h) == -1) {
        w = 80;
    }

    char *hl = highlight_line(rl_line_buffer);
    const char *line = hl ? hl : rl_line_buffer;
    prediction_t pred = jssh_predict(rl_line_buffer, rl_point);

    fputs(rl_display_prompt, stdout);
    fputs(line, stdout);

    if (pred.active && pred.text) {
        fputs("\033[90m", stdout);
        fputs(pred.text, stdout);
        fputs("\033[0m", stdout);
    }

    int prompt_len = visual_width(rl_display_prompt);
    int line_len = visual_width(line); // same as visual_width(rl_line_buffer) — ANSI codes ignored
    int pred_len = pred.active && pred.text ? (int)strlen(pred.text) : 0;

    int total_visual_len = prompt_len + line_len + pred_len;

    if (total_visual_len == 0) {
        g_previous_lines = 1;
    } else {
        g_previous_lines = (total_visual_len - 1) / w + 1;
    }
    char *buffer_slice = rl_line_buffer_slice(0, rl_point);
    int cur_visual_pos = visual_width(buffer_slice);
    if (buffer_slice) free(buffer_slice);
    int line_visual_len = line_len; // reuse already-computed value

    if (rl_point < rl_end || (pred.active && pred.text)) {
        int cols_to_end_of_line = line_visual_len - cur_visual_pos;
        int back = cols_to_end_of_line + pred_len;

        if (back > 0) {
            fprintf(stdout, "\033[%dD", back);
        }
    }

    fflush(stdout);
    if (hl) free(hl);
}