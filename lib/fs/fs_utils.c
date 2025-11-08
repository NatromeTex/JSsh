#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include "quickjs.h"
#include "quickjs-libc.h"
#include "../../src/utils.h"

#define JS_SUPPRESS "\x1B[JSSH_SUPPRESS"

// Print tree symbols
static void print_indent(int depth, int is_last) {
    for (int i = 0; i < depth - 1; i++)
        printf("│   ");
    if (depth > 0)
        printf("%s", is_last ? "└── " : "├── ");
}

// Pretty printer for tree
static void print_tree(JSContext *ctx, const char *path, int depth) {
    DIR *dir = opendir(path);
    if (!dir) return;

    char *entries[1024];
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        entries[count++] = strdup(entry->d_name);
    }
    closedir(dir);
    // Sort entries alphabetically
    qsort(entries, count, sizeof(char *), (int (*)(const void *, const void *))strcmp);

    for (int i = 0; i < count; i++) {
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entries[i]);

        struct stat st;
        if (stat(fullpath, &st) != 0) {
            free(entries[i]);
            continue;
        }

        int is_last = (i == count - 1);
        print_indent(depth, is_last);
        print_name(entries[i], st.st_mode);
        printf("\n");

        if (S_ISDIR(st.st_mode)) {
            print_tree(ctx, fullpath, depth + 1);
        }
        free(entries[i]);
    }
}

// Fast find for find
static void find_fast(JSContext *ctx, JSValue result, const char *path, int depth, const char *namePat, char type, off_t minSize, int maxDepth) {
    if (maxDepth >= 0 && depth > maxDepth) return;
    DIR *dir = opendir(path);
    if (!dir) return;
    int dfd = dirfd(dir);
    struct dirent *entry;
    char buf[PATH_MAX];
    int baseLen = snprintf(buf, sizeof(buf), "%s/", path);
    if (baseLen < 0 || baseLen >= (int)sizeof(buf)) {
        closedir(dir);
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' &&
           (entry->d_name[1] == '\0' ||
           (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;
        struct stat st;
        if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0)
            continue;
        
        if (snprintf(buf + baseLen, sizeof(buf) - baseLen, "%s", entry->d_name) >= (int)(sizeof(buf) - baseLen))
            continue;
        
        int matches = 1;
        if (type == 'f' && !S_ISREG(st.st_mode)) matches = 0;
        if (type == 'd' && !S_ISDIR(st.st_mode)) matches = 0;
        if (minSize > 0 && st.st_size < minSize) matches = 0;
        if (namePat && fnmatch(namePat, entry->d_name, 0) != 0) matches = 0;
        if (matches) {
            uint32_t len = 0;
            JSValue lenVal = JS_GetPropertyStr(ctx, result, "length");
            JS_ToUint32(ctx, &len, lenVal);
            JS_FreeValue(ctx, lenVal);
            JS_SetPropertyUint32(ctx, result, len, JS_NewString(ctx, buf));
        }
        
        if (S_ISDIR(st.st_mode))
            find_fast(ctx, result, buf, depth + 1, namePat, type, minSize, maxDepth);
    }
    closedir(dir);
}



// ---------------------------------------------------------

// fs.tree()
JSValue js_tree(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *path = ".";
    if (argc > 0 && JS_IsString(argv[0])) {
        path = JS_ToCString(ctx, argv[0]);
        if (!path) return JS_EXCEPTION;
    }

    printf("%s\n", path);
    print_tree(ctx, path, 0);

    if (argc > 0) JS_FreeCString(ctx, path);
    return JS_NewString(ctx, JS_SUPPRESS);
}

// fs.find()
JSValue js_find(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *path = ".";
    const char *namePat = NULL;
    char type = 0;
    off_t minSize = 0;
    int maxDepth = -1;

    if (argc > 0 && JS_IsString(argv[0])) {
        path = JS_ToCString(ctx, argv[0]);
    }

    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue val;

        val = JS_GetPropertyStr(ctx, argv[1], "name");
        if (!JS_IsUndefined(val)) namePat = JS_ToCString(ctx, val);

        val = JS_GetPropertyStr(ctx, argv[1], "type");
        if (JS_IsString(val)) {
            const char *t = JS_ToCString(ctx, val);
            if (t && t[0]) type = t[0];
            JS_FreeCString(ctx, t);
        }

        val = JS_GetPropertyStr(ctx, argv[1], "minSize");
        if (JS_IsNumber(val)) JS_ToInt64(ctx, &minSize, val);

        val = JS_GetPropertyStr(ctx, argv[1], "maxDepth");
        if (JS_IsNumber(val)) JS_ToInt32(ctx, &maxDepth, val);
    }

    JSValue result = JS_NewArray(ctx);
    find_fast(ctx, result, path, 0, namePat, type, minSize, maxDepth);

    if (argc > 0) JS_FreeCString(ctx, path);
    if (namePat) JS_FreeCString(ctx, namePat);
    return result;
}
