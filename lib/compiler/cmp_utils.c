#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *compilers[] = {
    "python",
    "python3",
    "gcc",
    "g++",
    "clang",
    "javac",
    "rustc",
    "go",
    "node",
    NULL
};

typedef struct {
    const char *name;
    char *version;
} CompilerInfo;

CompilerInfo detected[20];
int detected_count = 0;


static char *get_version_output(const char *cmd) {
    char command[256];
    snprintf(command, sizeof(command), "%s --version 2>&1", cmd);
    FILE *fp = popen(command, "r");
    if (!fp) return NULL;

    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    if (n == 0) return NULL;
    buf[n] = '\0';

    char *newline = strchr(buf, '\n');
    if (newline) *newline = '\0';

    if (strncmp(buf, "sh:", 3) == 0 || strstr(buf, "not found") || strstr(buf, "command not found"))
        return NULL;

    return strdup(buf);
}

void detect_compilers(void) {
    detected_count = 0;

    for (int i = 0; compilers[i]; i++) {
        char *out = get_version_output(compilers[i]);
        if (out) {
            detected[detected_count].name = compilers[i];
            detected[detected_count].version = out;
            detected_count++;
        }
    }
}

JSValue js_compiler_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (detected_count == 0)
        detect_compilers();

    if (detected_count == 0)
        return JS_NewString(ctx, "No compilers found.");

    char result[4096];
    result[0] = '\0';

    for (int i = 0; i < detected_count; i++) {
        if (i > 0)
            strncat(result, "\n", sizeof(result) - strlen(result) - 1);
        snprintf(result + strlen(result), sizeof(result) - strlen(result), "%s: %s", detected[i].name, detected[i].version);
    }

    return JS_NewString(ctx, result);
}

JSValue js_run_compiler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic ,JSValueConst *func_data) {
    // magic is required by QuickJS but not used here
    const char *compiler = JS_ToCString(ctx, func_data[0]);
    const char *file = JS_ToCString(ctx, argv[0]);

    if (!compiler || !file)
        return JS_ThrowTypeError(ctx, "compiler or file missing");

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s 2>&1", compiler, file);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return JS_ThrowInternalError(ctx, "failed to execute");

    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';

    JS_FreeCString(ctx, compiler);
    JS_FreeCString(ctx, file);

    return JS_NewString(ctx, buf);
}
