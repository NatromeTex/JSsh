/* This file contains code for implementing the prototypes in func.h
*  You can add your own native functions to work through JS from here
*  This file is used mostly to support native Unix primitive commands 
*   as a stepping stone for pure JS commands
*/

#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "quickjs.h"
#include "quickjs-libc.h"
#include "utils.h"


// return this in a JS_String to suppress the undefined after functions which print directly to console
#define JS_SUPPRESS "\x1B[JSSH_SUPPRESS" 

// cat
JSValue js_cat(JSContext *ctx, JSValueConst this_val,
               int argc, JSValueConst *argv) {

    const char *path;
    if (argc < 1)
        path = "./";

    path = JS_ToCString(ctx, argv[0]);
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
    return JS_NewString(ctx, JS_SUPPRESS);
}

// tac
JSValue js_tac(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "tac(path)");

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path)
        return JS_EXCEPTION;

    FILE *f = fopen(path, "rb");
    if (!f) {
        JS_FreeCString(ctx, path);
        return JS_ThrowTypeError(ctx, "fopen failed: %s", strerror(errno));
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        JS_FreeCString(ctx, path);
        return JS_ThrowInternalError(ctx, "fseek failed: %s", strerror(errno));
    }

    long pos = ftell(f);
    if (pos < 0) {
        fclose(f);
        JS_FreeCString(ctx, path);
        return JS_ThrowInternalError(ctx, "ftell failed: %s", strerror(errno));
    }

    const size_t bufsize = 4096;
    char buf[bufsize];
    long offset = pos;
    long line_end = pos;

    while (offset > 0) {
        size_t chunk = (offset >= bufsize) ? bufsize : offset;
        offset -= chunk;
        if (fseek(f, offset, SEEK_SET) != 0)
            break;

        size_t n = fread(buf, 1, chunk, f);
        if (n == 0) break; // EOF or error

        for (long i = n - 1; i >= 0; i--) {
            if (buf[i] == '\n') {
                long line_start = offset + i + 1;
                long line_len = line_end - line_start;
                if (line_len > 0) {
                    char *line = malloc(line_len + 1);
                    if (!line) continue;
                    if (fseek(f, line_start, SEEK_SET) == 0) {
                        size_t got = fread(line, 1, line_len, f);
                        if (got == (size_t)line_len) {
                            line[line_len] = '\0';
                            fwrite(line, 1, line_len, stdout);
                            fputc('\n', stdout);
                        }
                    }
                    free(line);
                }
                line_end = offset + i;
            }
        }
    }

    // First line (if not newline terminated)
    if (line_end > 0) {
        char *line = malloc(line_end + 1);
        if (line) {
            if (fseek(f, 0, SEEK_SET) == 0) {
                size_t got = fread(line, 1, line_end, f);
                if (got == (size_t)line_end) {
                    line[line_end] = '\0';
                    fwrite(line, 1, line_end, stdout);
                    fputc('\n', stdout);
                }
            }
            free(line);
        }
    }

    fclose(f);
    JS_FreeCString(ctx, path);
    return JS_NewString(ctx, JS_SUPPRESS);
}

// echo
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
    
    return JS_NewString(ctx, JS_SUPPRESS);
}

// ls command
static void print_perms(mode_t mode) {
    char buf[11];
    buf[0] = S_ISDIR(mode) ? 'd' : '-';
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
    printf("%s ", buf);
}

JSValue js_ls(JSContext *ctx, JSValueConst this_val,
              int argc, JSValueConst *argv) {
    const char *path = ".";
    const char *flag = NULL;

    if (argc > 0) {
        path = JS_ToCString(ctx, argv[0]);
        if (!path) return JS_EXCEPTION;
    }
    if (argc > 1) {
        flag = JS_ToCString(ctx, argv[1]);
        if (!flag) {
            if (argc > 0) JS_FreeCString(ctx, path);
            return JS_EXCEPTION;
        }
    }

    DIR *dir = opendir(path);
    if (!dir) {
        if (argc > 0) JS_FreeCString(ctx, path);
        if (argc > 1) JS_FreeCString(ctx, flag);
        return JS_ThrowTypeError(ctx, "cannot open directory");
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0)
            continue;

        if (flag && strcmp(flag, "l") == 0) {
            print_perms(st.st_mode);
            printf("%5ld ", (long)st.st_size);

            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "%b %d %H:%M",
                     localtime(&st.st_mtime));
            printf("%s ", timebuf);

            // colored name
            print_name(entry->d_name, st.st_mode);
            printf("\n");
        } else {
            // colored name
            print_name(entry->d_name, st.st_mode);
            printf("  ");
        }
    }

    if (!(flag && strcmp(flag, "l") == 0))
        printf("\n");

    closedir(dir);
    if (argc > 0) JS_FreeCString(ctx, path);
    if (argc > 1) JS_FreeCString(ctx, flag);

    return JS_NewString(ctx, JS_SUPPRESS);
}

// cd
JSValue js_cd(JSContext *ctx, JSValueConst this_val,
              int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "cd: missing path");
    }

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path)
        return JS_EXCEPTION;

    if (chdir(path) != 0) {
        JS_FreeCString(ctx, path);
        return JS_ThrowTypeError(ctx, "cd: cannot change to '%s'", path);
    }

    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
}

// mkdir
JSValue js_mkdir(JSContext *ctx, JSValueConst this_val,
                 int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "mkdir: missing path");
    }

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path)
        return JS_EXCEPTION;

    int mode = 0755; // default permissions
    if (argc > 1) {
        int m;
        if (JS_ToInt32(ctx, &m, argv[1]) == 0) {
            mode = m & 0777;
        }
    }

    int ret = mkdir(path, mode);
    if (ret != 0) {
        JS_FreeCString(ctx, path);
        return JS_ThrowTypeError(ctx, "mkdir: cannot create '%s': %s",
                                 path, strerror(errno));
    }

    JS_FreeCString(ctx, path);
    return JS_NewString(ctx, JS_SUPPRESS);
}

// touch
JSValue js_touch(JSContext *ctx, JSValueConst this_val,
                 int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "touch: missing path");
    }

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path)
        return JS_EXCEPTION;

    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        JS_FreeCString(ctx, path);
        return JS_ThrowTypeError(ctx, "touch: cannot create '%s': %s",
                                 path, strerror(errno));
    }
    close(fd);

    // update mtime to now
    struct timespec times[2];
    times[0].tv_nsec = UTIME_NOW; // access time
    times[1].tv_nsec = UTIME_NOW; // modification time
    if (utimensat(AT_FDCWD, path, times, 0) != 0) {
        JS_FreeCString(ctx, path);
        return JS_ThrowTypeError(ctx, "touch: cannot update time '%s': %s", path, strerror(errno));
    }

    JS_FreeCString(ctx, path);
    return JS_NewString(ctx, JS_SUPPRESS);
}

// rm
static int rm_path(const char *path, int recursive, int force) {
    struct stat st;
    if (lstat(path, &st) != 0) {
        if (!force)
            perror(path);
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (!recursive) {
            if (!force)
                fprintf(stderr, "rm: cannot remove '%s': Is a directory\n", path);
            return -1;
        }
        DIR *dir = opendir(path);
        if (!dir) {
            if (!force)
                perror(path);
            return -1;
        }
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;
            char child[PATH_MAX];
            snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
            rm_path(child, recursive, force);
        }
        closedir(dir);
        if (rmdir(path) != 0 && !force)
            perror(path);
    } else {
        if (unlink(path) != 0 && !force)
            perror(path);
    }
    return 0;
}

JSValue js_rm(JSContext *ctx, JSValueConst this_val,
              int argc, JSValueConst *argv) {
    int recursive = 0;
    int force = 0;
    int start = 0;

    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "rm: missing path");
    }

    // check for options
    const char *first = JS_ToCString(ctx, argv[0]);
    if (!first) return JS_EXCEPTION;

    if (first[0] == '-') {
        for (size_t i = 1; first[i]; i++) {
            if (first[i] == 'r') recursive = 1;
            if (first[i] == 'f') force = 1;
        }
        start = 1;
    }
    JS_FreeCString(ctx, first);

    if (start >= argc) return JS_UNDEFINED; // nothing to remove

    for (int i = start; i < argc; i++) {
        const char *path = JS_ToCString(ctx, argv[i]);
        if (!path) continue;
        rm_path(path, recursive, force);
        JS_FreeCString(ctx, path);
    }

    return JS_NewString(ctx, JS_SUPPRESS);
}


// chmod
JSValue js_chmod(JSContext *ctx, JSValueConst this_val,
                 int argc, JSValueConst *argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "chmod requires path and mode");
    }

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }

    int mode;
    if (JS_ToInt32(ctx, &mode, argv[1])) {
        JS_FreeCString(ctx, path);
        return JS_ThrowTypeError(ctx, "invalid mode");
    }

    int ret = chmod(path, (mode_t)mode);
    JS_FreeCString(ctx, path);

    if (ret != 0) {
        return JS_ThrowInternalError(ctx, "chmod failed: %s", strerror(errno));
    }

    return JS_NewString(ctx, JS_SUPPRESS);
}

// js
JSValue js_runfile(JSContext *ctx, JSValueConst this_val,
                   int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "js(path) requires a filename");
    }

    const char *filename = JS_ToCString(ctx, argv[0]);
    if (!filename) {
        return JS_EXCEPTION;
    }

    size_t buf_len;
    uint8_t *buf = js_load_file(ctx, &buf_len, filename);
    if (!buf) {
        JS_FreeCString(ctx, filename);
        return JS_ThrowReferenceError(ctx, "cannot open '%s'", filename);
    }

    JSValue result = JS_Eval(ctx, (const char *)buf, buf_len, filename,
                             JS_EVAL_TYPE_GLOBAL);

    js_free(ctx, buf);
    JS_FreeCString(ctx, filename);
    return result;
}

// clear
JSValue js_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
    return JS_NewString(ctx, JS_SUPPRESS);
}