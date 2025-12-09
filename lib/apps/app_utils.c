#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>
#include "quickjs.h"

#define JSSH_SUPPRESS "\x1B[JSSH_SUPPRESS"

JSValue jsvim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char **exec_argv = calloc(argc + 2, sizeof(char *));
    if (!exec_argv)
        return JS_ThrowInternalError(ctx, "Memory allocation failed");

    exec_argv[0] = "jsvim"; 

    for (int i = 0; i < argc; i++) {
        exec_argv[i + 1] = (char *)JS_ToCString(ctx, argv[i]);
    }
    exec_argv[argc + 1] = NULL;

    int shell_terminal = STDIN_FILENO;
    struct termios shell_tmodes;
    pid_t shell_pgid = getpgrp();
    int is_interactive = isatty(shell_terminal);

    if (is_interactive) {
        while (tcgetattr(shell_terminal, &shell_tmodes) == -1) {
            if (errno != EINTR) break;
        }
    }

    pid_t pid = fork();

    if (pid < 0) {
        for (int i = 0; i < argc; i++) JS_FreeCString(ctx, exec_argv[i + 1]);
        free(exec_argv);
        return JS_ThrowInternalError(ctx, "Failed to fork process");
    }

    if (pid == 0) {
        if (is_interactive) {
            pid_t child_pid = getpid();
            setpgid(child_pid, child_pid);
            tcsetpgrp(shell_terminal, child_pid);

            unsetenv("LINES");
            unsetenv("COLUMNS");
            unsetenv("TERMCAP");

            sigset_t set;
            sigemptyset(&set);
            sigprocmask(SIG_SETMASK, &set, NULL);

            struct sigaction sa;
            sa.sa_handler = SIG_DFL;
            sa.sa_flags = 0;
            sigemptyset(&sa.sa_mask);

            sigaction(SIGINT, &sa, NULL);
            sigaction(SIGQUIT, &sa, NULL);
            sigaction(SIGTSTP, &sa, NULL);
            sigaction(SIGTTIN, &sa, NULL);
            sigaction(SIGTTOU, &sa, NULL);
            sigaction(SIGWINCH, &sa, NULL);
        }

        execvp("jsvim", exec_argv);
        
        perror("execvp");
        _exit(127);
    }

    if (is_interactive) {
        setpgid(pid, pid);
        signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(shell_terminal, pid);
    }

    int status;
    waitpid(pid, &status, WUNTRACED);

    if (is_interactive) {
        tcsetpgrp(shell_terminal, shell_pgid);
        tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
        signal(SIGTTOU, SIG_DFL);
    }

    for (int i = 0; i < argc; i++) {
        JS_FreeCString(ctx, exec_argv[i + 1]);
    }
    free(exec_argv);

    return JS_NewString(ctx, JSSH_SUPPRESS);
}