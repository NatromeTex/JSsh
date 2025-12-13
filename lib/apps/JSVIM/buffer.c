// buffer.c - Text buffer operations
#include "buffer.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

void buf_init(Buffer *b) {
    // Initialize line storage
    b->cap = 16;
    b->count = 0;
    b->lines = malloc(sizeof(char*) * b->cap);

    // Filetype not known yet
    b->ft = FT_NONE;

    // Initialize LSP state
    b->lsp.pid = 0;
    b->lsp.stdin_fd = -1;
    b->lsp.stdout_fd = -1;
    b->lsp.lsp_accum = NULL;
    b->lsp.lsp_accum_len = 0;

    // Initialize diagnostics
    b->diagnostics = NULL;
    b->diag_count = 0;
    b->diag_cap = 0;

    // Initialize LSP document state
    b->lsp_version = 0;
    b->lsp_opened = 0;
    b->lsp_dirty = 0;
    b->lsp_uri[0] = '\0';
    b->filepath[0] = '\0';

    // Initialize semantic tokens
    b->tokens = NULL;
    b->token_count = 0;
    b->token_cap = 0;

    // Initialize LSP token map
    b->lsp_token_map_len = 0;
}

void buf_free(Buffer *b) {
    // Free text lines
    for (size_t i = 0; i < b->count; i++) {
        free(b->lines[i]);
    }
    free(b->lines);
    b->lines = NULL;
    b->count = 0;
    b->cap = 0;

    // Shut down LSP process if active
    if (b->lsp.pid > 0) {
        // Kill clangd process
        kill(b->lsp.pid, SIGTERM);

        // Reap it (avoids zombies)
        waitpid(b->lsp.pid, NULL, 0);
    }

    // Close FD handles if open
    if (b->lsp.stdin_fd  != -1) close(b->lsp.stdin_fd);
    if (b->lsp.stdout_fd != -1) close(b->lsp.stdout_fd);

    b->lsp.stdin_fd  = -1;
    b->lsp.stdout_fd = -1;
    b->lsp.pid       = 0;

    // Free LSP accumulation buffer
    if (b->lsp.lsp_accum) {
        free(b->lsp.lsp_accum);
        b->lsp.lsp_accum = NULL;
    }
    b->lsp.lsp_accum_len = 0;

    // Free diagnostics
    for (size_t i = 0; i < b->diag_count; i++) {
        free(b->diagnostics[i].msg);
    }
    free(b->diagnostics);

    // Free semantic tokens
    free(b->tokens);
    b->tokens = NULL;
    b->token_count = 0;
    b->token_cap = 0;
}

void buf_ensure(Buffer *b, size_t need) {
    if (need <= b->cap) return;
    while (b->cap < need) b->cap *= 2;
    b->lines = realloc(b->lines, sizeof(char*) * b->cap);
}

void buf_push(Buffer *b, char *line) {
    buf_ensure(b, b->count + 1);
    b->lines[b->count++] = line;
}

void buf_insert(Buffer *b, size_t idx, char *line) {
    if (idx > b->count) idx = b->count;
    buf_ensure(b, b->count + 1);
    for (size_t i = b->count; i > idx; --i) b->lines[i] = b->lines[i-1];
    b->lines[idx] = line;
    b->count++;
}

void buf_clear_diagnostics(Buffer *buf) {
    for (size_t i = 0; i < buf->diag_count; i++) {
        free(buf->diagnostics[i].msg);
    }
    buf->diag_count = 0;
}

void buf_add_diagnostic(Buffer *buf, int line, int col, int severity, const char *msg) {
    if (buf->diag_count == buf->diag_cap) {
        buf->diag_cap = buf->diag_cap ? buf->diag_cap * 2 : 8;
        buf->diagnostics = realloc(buf->diagnostics, buf->diag_cap * sizeof(Diagnostic));
    }

    buf->diagnostics[buf->diag_count].line = line;
    buf->diagnostics[buf->diag_count].col  = col;
    buf->diagnostics[buf->diag_count].severity = severity;
    buf->diagnostics[buf->diag_count].msg  = dupstr(msg);
    buf->diag_count++;
}

int load_file(Buffer *b, const char *fname) {
    FILE *fp = fopen(fname, "r");
    if (!fp) return 1;
    char *line = NULL;
    size_t lncap = 0;
    ssize_t lnlen;
    while ((lnlen = getline(&line, &lncap, fp)) != -1) {
        // strip trailing newline
        if (lnlen > 0 && line[lnlen-1] == '\n') line[--lnlen] = '\0';
        buf_push(b, dupstr(line));
    }
    free(line);
    fclose(fp);
    // Ensure at least one line
    if (b->count == 0) buf_push(b, dupstr(""));
    return 0;
}

int save_file(Buffer *b, const char *fname) {
    FILE *fp = fopen(fname, "w");
    if (!fp) return 1;
    for (size_t i = 0; i < b->count; i++) {
        fputs(b->lines[i], fp);
        if (i + 1 < b->count) fputc('\n', fp);
    }
    fclose(fp);
    return 0;
}
