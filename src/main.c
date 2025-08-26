// repl.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
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
    JS_FreeValue(ctx, global_obj);

    detect_color_mode();

    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : "unknown";

    char host[256];
    if (gethostname(host, sizeof(host)) != 0)
        strcpy(host, "unknown");

    char cwd[PATH_MAX];

    char line[1024];
    while (1) {
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            strcpy(cwd, "?");

        printR("{rgb:85,255,85}%s@%s{reset}:{rgb:85,85,255}%s{reset}$ ", username, host, cwd);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n')
            line[len-1] = 0;

        if (strcmp(line, ":quit") == 0)
            break;

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
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
