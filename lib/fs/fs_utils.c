#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <fnmatch.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "quickjs.h"
#include "quickjs-libc.h"
#include "../../src/utils.h"

#define JS_SUPPRESS "\x1B[JSSH_SUPPRESS"

// Detect if running under WSL
static int is_wsl(void) {
    static int cached = -1;
    if (cached >= 0) return cached;
    FILE *fp = fopen("/proc/version", "r");
    if (!fp) { cached = 0; return 0; }
    char buf[512];
    cached = 0;
    if (fgets(buf, sizeof(buf), fp)) {
        if (strstr(buf, "Microsoft") || strstr(buf, "microsoft") || strstr(buf, "WSL"))
            cached = 1;
    }
    fclose(fp);
    return cached;
}

// Decode octal escape sequences in /proc/mounts entries
// e.g. "C:\\134" (representing C:\) => "C:\\"
static void decode_mount_escapes(char *dst, const char *src, size_t dstlen) {
    size_t di = 0;
    for (const char *s = src; *s && di < dstlen - 1; ) {
        if (*s == '\\' && s[1] >= '0' && s[1] <= '7' &&
            s[2] >= '0' && s[2] <= '7' && s[3] >= '0' && s[3] <= '7') {
            dst[di++] = (char)((s[1] - '0') * 64 + (s[2] - '0') * 8 + (s[3] - '0'));
            s += 4;
        } else {
            dst[di++] = *s++;
        }
    }
    dst[di] = '\0';
}

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

// Convert bytes to human-readable unit
static const char *units(double bytes, double *out){
    const double KiB = 1024.0;
    const double MiB = KiB * 1024.0;
    const double GiB = MiB * 1024.0;
    const double TiB = GiB * 1024.0;

    if (bytes >= TiB) { *out = bytes / TiB; return "TiB"; }
    if (bytes >= GiB) { *out = bytes / GiB; return "GiB"; }
    if (bytes >= MiB) { *out = bytes / MiB; return "MiB"; }

    *out = bytes / KiB;
    return "KiB";
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

// fs.df()
JSValue js_df(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    FILE *fp = fopen("/proc/self/mounts", "r");
    if (!fp)
        return JS_ThrowInternalError(ctx, "Failed to open /proc/self/mounts");

    printf("Filesystem                Size        Used       Avail    Use%%   Mounted on\n");

    char fs[256], mountpoint[256], type[64], opts[256];
    int dump, pass;

    //Track already-seen mountpoints after normalization 
    char seen[256][256];
    int seen_count = 0;

    //pseudo FS types to ignore 
    static const char *skip_fs[] = {
        "proc", "sysfs", "tmpfs", "devtmpfs", "cgroup", "cgroup2",
        "debugfs", "tracefs", "mqueue", "autofs", "overlay",
        "squashfs", "ramfs", "rootfs", "devpts", "binfmt_misc",
        "hugetlbfs", "fusectl", "configfs", "securityfs", "pstore",
        NULL
    };

    int i;

    //helper: check if filesystem type is pseudo 
    int is_pseudo_fs(const char *t) {
        for (i = 0; skip_fs[i]; i++)
            if (strcmp(t, skip_fs[i]) == 0)
                return 1;
        return 0;
    }

    //helper: dedupe mountpoints 
    int seen_mount(const char *mp) {
        int k;
        for (k = 0; k < seen_count; k++)
            if (strcmp(mp, seen[k]) == 0)
                return 1;
        strncpy(seen[seen_count], mp, 255);
        seen[seen_count][255] = 0;
        seen_count++;
        return 0;
    }

    // Track already-seen device names for dedup (by underlying device)
    char seen_devs[256][256];
    int seen_dev_count = 0;

    int seen_device(const char *dev) {
        int k;
        for (k = 0; k < seen_dev_count; k++)
            if (strcmp(dev, seen_devs[k]) == 0)
                return 1;
        if (seen_dev_count < 256) {
            strncpy(seen_devs[seen_dev_count], dev, 255);
            seen_devs[seen_dev_count][255] = 0;
            seen_dev_count++;
        }
        return 0;
    }

    int wsl = is_wsl();

    while (fscanf(fp, "%255s %255s %63s %255s %d %d",
                  fs, mountpoint, type, opts, &dump, &pass) == 6) {

        //Skip pseudo filesystems 
        if (is_pseudo_fs(type))
            continue;

        // Decode octal escapes from /proc/mounts (e.g. C:\134 -> C:\)
        char decoded_fs[256], decoded_mp[256];
        decode_mount_escapes(decoded_fs, fs, sizeof(decoded_fs));
        decode_mount_escapes(decoded_mp, mountpoint, sizeof(decoded_mp));

        if (wsl) {
            // Skip WSL-internal mounts
            if (strncmp(decoded_mp, "/mnt/wslg", 9) == 0)
                continue;
            if (strcmp(decoded_mp, "/init") == 0)
                continue;
            // Skip WSL driver/lib pseudo mounts (9p type but not Windows drives)
            if (strcmp(type, "9p") == 0 &&
                strncmp(decoded_mp, "/mnt/", 5) != 0)
                continue;
        }

        // Deduplicate by mountpoint
        if (seen_mount(decoded_mp))
            continue;

        // Deduplicate by device name (e.g. same /dev/sdd at / and /mnt/wslg/distro)
        if (seen_device(decoded_fs))
            continue;

        struct statvfs buf;
        if (statvfs(decoded_mp, &buf) != 0)
            continue;

        unsigned long bs = buf.f_frsize ? buf.f_frsize : buf.f_bsize;
        double total = (double)buf.f_blocks * bs;
        double freeb = (double)buf.f_bfree  * bs;
        double avail = (double)buf.f_bavail * bs;
        double used  = total - freeb;

        double htotal, hused, havail;
        const char *utotal = units(total, &htotal);
        const char *uused  = units(used,  &hused);
        const char *uavail = units(avail, &havail);

        double pct = total > 0 ? (used / total) * 100.0 : 0.0;

        printf("%-18s  %6.1f %s  %6.1f %s  %6.1f %s  %5.1f%%  %s\n",
               decoded_fs,
               htotal, utotal,
               hused,  uused,
               havail, uavail,
               pct,
               decoded_mp);
    }

    fclose(fp);
    return JS_NewString(ctx, JS_SUPPRESS);
}