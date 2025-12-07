#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>

#define JSSH_SUPPRESS "\x1B[JSSH_SUPPRESS"

JSValue jsvim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    pid_t pid = fork();

    if (pid < 0) {
        return JS_ThrowInternalError(ctx, "Failed to fork process");
    }

    if (pid == 0) {
        execl("/usr/bin/jsvim", "jsvim", filepath, NULL);
        perror("execl");
        return JS_ThrowInternalError(ctx, "Failed to execute jsvim");
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return JS_NewString(ctx, JSSH_SUPPRESS);
    }
    if (WIFSIGNALED(status)) {
        return JS_NewString(ctx, JSSH_SUPPRESS);
    }

    return JS_NewString(ctx, JSSH_SUPPRESS);
}
