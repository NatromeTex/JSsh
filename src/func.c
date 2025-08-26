/* This file contains code for implementing the prototypes in func.h
*  You can add your own native functions to work through JS from here
*  This file is used mostly to support native Unix primitive commands 
*   as a stepping stone for pure JS commands
*/

#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <errno.h>
#include "quickjs.h"
#include "quickjs-libc.h"

// return this in a JS_String to suppress the undefined after functions which print directly to console
#define JS_SUPPRESS "\x1B[JSSH_SUPPRESS" 

// cat command
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

// echo command
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

        if (flag && strcmp(flag, "l") == 0) {
            char fullpath[PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
            struct stat st;
            if (stat(fullpath, &st) == 0) {
                print_perms(st.st_mode);
                printf("%5ld ", (long)st.st_size);

                char timebuf[64];
                strftime(timebuf, sizeof(timebuf), "%b %d %H:%M",
                         localtime(&st.st_mtime));
                printf("%s %s\n", timebuf, entry->d_name);
            }
        } else {
            printf("%s  ", entry->d_name);
        }
    }
    if (!(flag && strcmp(flag, "l") == 0))
        printf("\n");

    closedir(dir);
    if (argc > 0) JS_FreeCString(ctx, path);
    if (argc > 1) JS_FreeCString(ctx, flag);

    return JS_NewString(ctx, JS_SUPPRESS);
}

// cd command

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