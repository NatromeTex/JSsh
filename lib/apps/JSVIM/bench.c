// bench.c - JSVIM performance benchmark
// Measures load_file + highlight_buffer on a large C source file.
//
// Build:
//   cc -O2 -o bench bench.c buffer.c highlight.c language.c semantic.c util.c \
//      -lncurses -lm
//
// Run:
//   ./bench [path/to/file] [iterations]
//
// Defaults: quickjs.c (59k lines), 5 iterations

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "buffer.h"
#include "highlight.h"
#include "language.h"

// Fallback: resolved at runtime relative to this binary's directory
#define DEFAULT_TARGET_REL "../../src/quickjs/quickjs.c"
#define DEFAULT_ITERS  5

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void print_bar(const char *label, double ms, double ref_ms) {
    int bar_width = 40;
    int filled = ref_ms > 0 ? (int)(ms / ref_ms * bar_width) : bar_width;
    if (filled > bar_width) filled = bar_width;
    printf("  %-14s %8.2f ms  [", label, ms);
    for (int i = 0; i < filled;  i++) putchar('#');
    for (int i = filled; i < bar_width; i++) putchar('-');
    puts("]");
}

int main(int argc, char **argv) {
    // Resolve default target: walk up 3 dirs from the binary, then append the relative path.
    // Done with explicit slash-stripping so it works on WSL DrvFs (which rejects ".." in paths).
    char default_target[4096] = "";
    if (argc < 2) {
        char self[4096];
        ssize_t len = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (len > 0) {
            self[len] = '\0';
            // Strip filename + 3 directory levels (lib/apps/JSVIM)
            for (int ups = 0; ups < 4; ups++) {
                char *sl = strrchr(self, '/');
                if (!sl) break;
                *sl = '\0';
            }
            snprintf(default_target, sizeof(default_target),
                     "%s/src/quickjs/quickjs.c", self);
        }
    }
    if (!default_target[0])
        strncpy(default_target, DEFAULT_TARGET_REL, sizeof(default_target) - 1);
    const char *target = argc >= 2 ? argv[1] : default_target;
    int iters = argc >= 3 ? atoi(argv[2]) : DEFAULT_ITERS;
    if (iters <= 0) iters = DEFAULT_ITERS;

    printf("JSVIM Benchmark\n");
    printf("  File       : %s\n", target);
    printf("  Iterations : %d\n\n", iters);

    double load_total   = 0.0;
    double hl_total     = 0.0;
    double load_best    = 1e18;
    double hl_best      = 1e18;
    size_t line_count   = 0;
    size_t token_count  = 0;

    for (int i = 0; i < iters; i++) {
        Buffer buf;
        buf_init(&buf);

        // --- load ---
        double t0 = now_ms();
        int ok = load_file(&buf, target);
        double load_ms = now_ms() - t0;

        if (ok != 0) {
            fprintf(stderr, "error: could not load '%s'\n", target);
            buf_free(&buf);
            return 1;
        }

        if (i == 0) line_count = buf.count;

        buf.ft = detect_filetype(target);
        strncpy(buf.filepath, target, sizeof(buf.filepath) - 1);

        // --- highlight ---
        double t1 = now_ms();
        highlight_buffer(&buf);
        double hl_ms = now_ms() - t1;

        if (i == 0) token_count = buf.token_count;

        printf("  run %2d  load=%7.2f ms  highlight=%7.2f ms\n",
               i + 1, load_ms, hl_ms);

        load_total += load_ms;
        hl_total   += hl_ms;
        if (load_ms < load_best) load_best = load_ms;
        if (hl_ms   < hl_best)   hl_best   = hl_ms;

        buf_free(&buf);
        highlight_cleanup();
    }

    double load_avg = load_total / iters;
    double hl_avg   = hl_total   / iters;
    double total_avg = load_avg + hl_avg;

    printf("\n--- Results (%d iterations) ---\n", iters);
    printf("  Lines loaded  : %zu\n", line_count);
    printf("  Tokens found  : %zu\n\n", token_count);

    double ref = load_avg > hl_avg ? load_avg : hl_avg;
    print_bar("load  avg",  load_avg,  ref);
    print_bar("load  best", load_best, ref);
    print_bar("hl    avg",  hl_avg,    ref);
    print_bar("hl    best", hl_best,   ref);

    printf("\n  Total avg (load + hl) : %.2f ms\n", total_avg);
    printf("  Throughput            : %.0f lines/ms\n",
           line_count / (total_avg > 0 ? total_avg : 1));

    return 0;
}
