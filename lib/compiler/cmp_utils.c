#include <pty.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <quickjs.h>
#include <sys/wait.h>
#include "sys/select.h"

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

typedef struct {
    const char *ext;
    const char *compiler;
} AutoMap;

static const AutoMap auto_map[] = {
    { ".c",   "gcc" },
    { ".cpp", "g++" },
    { ".cc",  "g++" },
    { ".py",  "python3" },
    { ".js",  "node" },
    { ".java","javac" },
    { ".rs",  "rustc" },
    { ".go",  "go" },
    { NULL,   NULL }
};


CompilerInfo detected[20];
int detected_count = 0;
static volatile pid_t current_child_pid = 0;

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

static void sigint_handler(int sig) {
    (void)sig;
    if (current_child_pid > 0) {
        kill(current_child_pid, SIGINT);
    }
}

static const char *detect_compiler_by_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return NULL;

    for (int i = 0; auto_map[i].ext; i++) {
        if (strcmp(dot, auto_map[i].ext) == 0)
            return auto_map[i].compiler;
    }
    return NULL;
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

JSValue js_run_compiler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValueConst *func_data) {
    (void)magic;
    const char *compiler = JS_ToCString(ctx, func_data[0]);
    const char *file = JS_ToCString(ctx, argv[0]);
    if (!compiler || !file)
        return JS_ThrowTypeError(ctx, "Compiler and filename required");
    
    // Build argv for execvp
    const char *args[3];
    args[0] = compiler;
    args[1] = file;
    args[2] = NULL;
    
    int master_fd;
    pid_t pid = forkpty(&master_fd, NULL, NULL, NULL);
    if (pid < 0) {
        JS_FreeCString(ctx, compiler);
        JS_FreeCString(ctx, file);
        return JS_ThrowInternalError(ctx, "forkpty failed: %s", strerror(errno));
    }
    
    if (pid == 0) {
        // Child: replace with compiler process
        execvp(compiler, (char *const *)args);
        _exit(127);
    }
    
    // Parent: Setup signal handler
    current_child_pid = pid;
    struct sigaction sa, old_sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &old_sa);
    
    // Set up terminal for raw mode
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    
    // Relay between stdin/stdout and PTY
    fd_set readfds;
    char buf[1024];
    ssize_t n;
    int child_running = 1;
    
    while (child_running) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(master_fd, &readfds);
        
        int max_fd = (master_fd > STDIN_FILENO) ? master_fd : STDIN_FILENO;
        
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        // Read from stdin and write to PTY
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n > 0) {
                ssize_t written = 0;
                while (written < n) {
                    ssize_t w = write(master_fd, buf + written, n - written);
                    if (w < 0) {
                        if (errno == EINTR) continue;
                        child_running = 0;
                        break;
                    }
                    written += w;
                }
            }
        }
        
        // Read from PTY and write to stdout
        if (FD_ISSET(master_fd, &readfds)) {
            n = read(master_fd, buf, sizeof(buf));
            if (n > 0) {
                ssize_t written = 0;
                while (written < n) {
                    ssize_t w = write(STDOUT_FILENO, buf + written, n - written);
                    if (w < 0) {
                        if (errno == EINTR) continue;
                        child_running = 0;
                        break;
                    }
                    written += w;
                }
            } else if (n == 0) {
                // EOF from PTY
                child_running = 0;
            }
        }
        
        // Check if child has exited
        int status;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            child_running = 0;
        }
    }
    
    // Restore signal handler
    sigaction(SIGINT, &old_sa, NULL);
    current_child_pid = 0;
    
    // Restore terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    close(master_fd);
    
    JS_FreeCString(ctx, compiler);
    JS_FreeCString(ctx, file);
    
    return JS_UNDEFINED;
}

JSValue js_auto_compile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "filename required");
    const char *file = JS_ToCString(ctx, argv[0]);
    const char *compiler = detect_compiler_by_ext(file);
    if (!compiler) {
        JS_FreeCString(ctx, file);
        return JS_ThrowTypeError(ctx, "unknown or unsupported file extension");
    }
    // Check if that compiler was detected
    int found = 0;
    for (int i = 0; i < detected_count; i++) {
        if (strcmp(detected[i].name, compiler) == 0) {
            found = 1;
            break;
        }
    }
    if (!found) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Compiler '%s' is required for '%s' but was not found on this system.", compiler, file);
        JS_FreeCString(ctx, file);
        return JS_ThrowReferenceError(ctx, "%s", msg);
    }
    
    // Build argv for execvp
    const char *args[3];
    args[0] = compiler;
    args[1] = file;
    args[2] = NULL;
    
    int master_fd;
    pid_t pid = forkpty(&master_fd, NULL, NULL, NULL);
    if (pid < 0) {
        JS_FreeCString(ctx, file);
        return JS_ThrowInternalError(ctx, "forkpty failed: %s", strerror(errno));
    }
    
    if (pid == 0) {
        // Child: replace with compiler process
        execvp(compiler, (char *const *)args);
        _exit(127);
    }
    
    // Parent: Setup signal handler
    current_child_pid = pid;
    struct sigaction sa, old_sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &old_sa);
    
    // Set up terminal for raw mode
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    
    // Relay between stdin/stdout and PTY
    fd_set readfds;
    char buf[1024];
    ssize_t n;
    int child_running = 1;
    
    while (child_running) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(master_fd, &readfds);
        
        int max_fd = (master_fd > STDIN_FILENO) ? master_fd : STDIN_FILENO;
        
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        // Read from stdin and write to PTY
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n > 0) {
                ssize_t written = 0;
                while (written < n) {
                    ssize_t w = write(master_fd, buf + written, n - written);
                    if (w < 0) {
                        if (errno == EINTR) continue;
                        child_running = 0;
                        break;
                    }
                    written += w;
                }
            }
        }
        
        // Read from PTY and write to stdout
        if (FD_ISSET(master_fd, &readfds)) {
            n = read(master_fd, buf, sizeof(buf));
            if (n > 0) {
                ssize_t written = 0;
                while (written < n) {
                    ssize_t w = write(STDOUT_FILENO, buf + written, n - written);
                    if (w < 0) {
                        if (errno == EINTR) continue;
                        child_running = 0;
                        break;
                    }
                    written += w;
                }
            } else if (n == 0) {
                // EOF from PTY
                child_running = 0;
            }
        }
        
        // Check if child has exited
        int status;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            child_running = 0;
        }
    }
    
    // Restore signal handler
    sigaction(SIGINT, &old_sa, NULL);
    current_child_pid = 0;
    
    // Restore terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    close(master_fd);
    
    JS_FreeCString(ctx, file);
    
    return JS_UNDEFINED;
}