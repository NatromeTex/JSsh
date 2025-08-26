#include <limits.h>
#include <errno.h>
#include "quickjs.h"
#include "quickjs-libc.h"

JSValue js_cat(JSContext *ctx, JSValueConst this_val,
               int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "cat(\"path\") expected, specify a path");

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path)
        return JS_EXCEPTION;

    FILE *f = fopen(path, "rb");
    JS_FreeCString(ctx, path);
    if (!f)
        return JS_ThrowTypeError(ctx, "fopen failed: %s", strerror(errno));

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    fclose(f);
    fputc('\n', stdout);
    return JS_NewString(ctx, "\x1B[JSSH_SUPPRESS");
}

JSValue js_echo(JSContext *ctx, JSValueConst this_val,
                int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "echo(\"<string>\") expected");
    }

    for (int i = 0; i < argc; i++) {
        const char *arg = JS_ToCString(ctx, argv[i]);
        if (!arg)
            return JS_EXCEPTION;

        if (i > 0)
            fputc(' ', stdout);

        fputs(arg, stdout);
        JS_FreeCString(ctx, arg);
    }

    fputc('\n', stdout);
    
    return JS_NewString(ctx, "\x1B[JSSH_SUPPRESS");
}

