// repl.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "quickjs.h"
#include "quickjs-libc.h"
#include "utils.h"
#include "func.h"

int main(int argc, char **argv) {
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    js_std_add_helpers(ctx, argc, argv);

    JSValue global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "cat", JS_NewCFunction(ctx, js_cat, "cat", 1));
    JS_SetPropertyStr(ctx, global_obj, "echo", JS_NewCFunction(ctx, js_echo, "echo", 1));
    JS_SetPropertyStr(ctx, global_obj, "ls", JS_NewCFunction(ctx, js_ls, "ls", 2));
    JS_SetPropertyStr(ctx, global_obj, "cd", JS_NewCFunction(ctx, js_cd, "cd", 1));

    JS_FreeValue(ctx, global_obj);

    detect_color_mode();

    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : "unknown";

    char host[256];
    if (gethostname(host, sizeof(host)) != 0)
        strcpy(host, "unknown");

    char cwd[PATH_MAX];

    while (1) {
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            strcpy(cwd, "?");

        // prompt with readline-safe escapes
        char prompt[PATH_MAX + 512];
        snprintf(prompt, sizeof(prompt),
                 "\001\033[38;2;85;255;85m\002%s@%s\001\033[0m\002:"
                 "\001\033[38;2;85;85;255m\002%s\001\033[0m\002$ ",
                 username, host, cwd);

        char *line = readline(prompt);
        if (!line) { // EOF (Ctrl-D)
            printf("\n");
            break;
        }

        if (*line)
            add_history(line);

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
