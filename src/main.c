// repl.c
#include <pwd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "env.h"
#include "sys.h"
#include "func.h"
#include "utils.h"
#include "quickjs.h"
#include "quickjs-libc.h"
#include "utils.h"
#include "func.h"
#include "sys.h"
#include "env.h"

// Network package
#ifdef ENABLE_NETWORK
extern void js_init_network(JSContext *ctx);
#endif
#ifndef ENABLE_NETWORK
static void js_init_network(JSContext *ctx) {}
#endif
// Compiler package
#ifdef ENABLE_COMPILER
extern void js_init_compiler(JSContext *ctx);
#endif
#ifndef ENABLE_COMPILER
static void js_init_compiler(JSContext *ctx) {}
#endif
// FileSystem package
#ifdef ENABLE_FS
extern void js_init_fs(JSContext *ctx);
#endif
#ifndef ENABLE_FS
static void js_init_fs(JSContext *ctx) {}
#endif
// Git package
#ifdef ENABLE_GIT
extern void js_init_git(JSContext *ctx);
#endif
#ifndef ENABLE_GIT
static void js_init_git(JSContext *ctx) {}
#endif

extern const char *history_file;

int main(int argc, char **argv) {
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    js_std_add_helpers(ctx, argc, argv);

    JSValue global_obj = JS_GetGlobalObject(ctx);

    // Init all os primitives in JS
    JS_SetPropertyStr(ctx, global_obj, "printR", JS_NewCFunction(ctx, js_printR, "printR", 1));
    JS_SetPropertyStr(ctx, global_obj, "cat", JS_NewCFunction(ctx, js_cat, "cat", 1));
    JS_SetPropertyStr(ctx, global_obj, "tac", JS_NewCFunction(ctx, js_tac, "tac", 1));
    JS_SetPropertyStr(ctx, global_obj, "echo", JS_NewCFunction(ctx, js_echo, "echo", 1));
    JS_SetPropertyStr(ctx, global_obj, "ls", JS_NewCFunction(ctx, js_ls, "ls", 2));
    JS_SetPropertyStr(ctx, global_obj, "cd", JS_NewCFunction(ctx, js_cd, "cd", 1));
    JS_SetPropertyStr(ctx, global_obj, "mkdir", JS_NewCFunction(ctx, js_mkdir, "mkdir", 2));
    JS_SetPropertyStr(ctx, global_obj, "touch", JS_NewCFunction(ctx, js_touch, "touch", 1));
    JS_SetPropertyStr(ctx, global_obj, "rm", JS_NewCFunction(ctx, js_rm, "rm", 10));
    JS_SetPropertyStr(ctx, global_obj, "chmod", JS_NewCFunction(ctx, js_chmod, "chmod", 2));
    JS_SetPropertyStr(ctx, global_obj, "js", JS_NewCFunction(ctx, js_runfile, "js", 1));
    JS_SetPropertyStr(ctx, global_obj, "show_env", JS_NewCFunction(ctx, js_show_env, "show_env", 0));
    JS_SetPropertyStr(ctx, global_obj, "env_get", JS_NewCFunction(ctx, js_env_get, "env_get", 1));
    JS_SetPropertyStr(ctx, global_obj, "env_add", JS_NewCFunction(ctx, js_env_add, "env_add", 2));
    JS_SetPropertyStr(ctx, global_obj, "clear", JS_NewCFunction(ctx, js_clear, "clear", 0));
    JS_SetPropertyStr(ctx, global_obj, "version", JS_NewCFunction(ctx, js_version, "version", 0));
    JS_SetPropertyStr(ctx, global_obj, "update", JS_NewCFunction(ctx, js_update, "update", 0));


    // Init syscalls for pure JS commands
    js_init_sys(ctx);

    JS_FreeValue(ctx, global_obj);

    // Load the pure JS commands
    load_js_libs(ctx, "./lib/js");

    // Get color mode of terminal
    detect_color_mode();

    // Module library conditional imports
    js_init_network(ctx);
    js_init_compiler(ctx);
    js_init_fs(ctx);
    js_init_git(ctx);


    // Init keybindings for autocomplete
    init_qol_bindings();

    // Init highlighting
    rl_redisplay_function = jssh_redisplay;

    // Detect root
    int is_root = (geteuid() == 0);

    struct passwd *pw = getpwuid(getuid());
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : ".";
    }
    const char *username = pw ? pw->pw_name : "unknown";

    init_history_file(); // sets history path to ~/.jssh_history
    read_history(history_file);  // loads ~/.jssh_history if exists

    // Load env file for settings
    char envpath[512];
    snprintf(envpath, sizeof(envpath), "%s/.jssh_env", home);
    env_load(envpath);

    char host[256];
    if (gethostname(host, sizeof(host)) != 0)
        strcpy(host, "unknown");

    char cwd[PATH_MAX];

    while (1) {
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            strcpy(cwd, "?");

        // Prompt with readline-safe escapes
        char prompt[PATH_MAX + 512];
        if (is_root) {
            // White prompt for root
            snprintf(prompt, sizeof(prompt),
                    "\001\033[38;2;255;255;255m\002%s@%s\001\033[0m\002:"
                    "\001\033[38;2;255;255;255m\002%s\001\033[0m\002# ", 
                    username, host, cwd);
        } else {
            // Green/blue prompt for normal user
            snprintf(prompt, sizeof(prompt), 
                    "\001\033[38;2;85;255;85m\002%s@%s\001\033[0m\002:"
                    "\001\033[38;2;85;85;255m\002%s\001\033[0m\002$ ", 
                    username, host, cwd);
        }

        char *line = readline(prompt);
        if (!line) { // EOF (Ctrl-D)
            printf("\n");
            break;
        }
        printf("\n");

        if (*line){
            add_history(line);
            write_history(history_file);
        }

        if (strcmp(line, ":quit") == 0) {
            free(line);
            break;
        }

        JSValue val = JS_Eval(ctx, line, strlen(line), "<repl>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(val)) {
            js_std_dump_error(ctx);
        } else {
            const char *str = JS_ToCString(ctx, val);
            if (str) {
                if (strcmp(str, "undefined") != 0 &&
                    strcmp(str, "\x1B[JSSH_SUPPRESS") != 0) { 
                    // Remove the undefined after certain functions
                    printf("%s\n", str);
                }
                JS_FreeCString(ctx, str);
            }
        }
        JS_FreeValue(ctx, val);
        free(line);
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
