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

    // Stop at first newline
    char *newline = strchr(buf, '\n');
    if (newline) *newline = '\0';

    // Skip missing/invalid output
    if (strncmp(buf, "sh:", 3) == 0 || strstr(buf, "not found") || strstr(buf, "command not found"))
        return NULL;

    return strdup(buf);
}

JSValue js_compiler_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char result[4096];
    result[0] = '\0';

    for (int i = 0; compilers[i]; i++) {
        char *out = get_version_output(compilers[i]);
        if (out) {
            snprintf(result + strlen(result),
                     sizeof(result) - strlen(result),
                     "%s: %s\n", compilers[i], out);
            free(out);
        }
    }

    if (strlen(result) == 0)
        strcpy(result, "No compilers found.\n");

    return JS_NewString(ctx, result);
}
