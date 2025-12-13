// buffer.h - Text buffer data structures and operations
#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>
#include <sys/types.h>
#include "semantic.h"
#include "language.h"

#define MAX_LSP_TOKEN_TYPES 64

// Diagnostics
typedef struct Diagnostic {
    int line;
    int col;
    int severity;
    char *msg;
} Diagnostic;

// LSP process handles
struct LSPProcess {
    pid_t pid;
    int stdin_fd;   // send to lsp
    int stdout_fd;  // read from lsp
    char *lsp_accum;
    size_t lsp_accum_len;
};

// Simple dynamic array of lines
typedef struct {
    char **lines;
    size_t count;
    size_t cap;
    FileType ft;

    struct LSPProcess lsp;

    Diagnostic *diagnostics;
    size_t diag_count;
    size_t diag_cap;

    // LSP document tracking
    int lsp_version;        // last version sent to LSP
    int lsp_opened;         // didOpen was sent successfully
    int lsp_dirty;          // buffer changed since last sync
    char lsp_uri[4096];     // URI used in LSP textDocument
    char filepath[1024];    // filename as opened in jsvim

    // Syntax highlighting
    SemanticToken *tokens;
    size_t token_count;
    size_t token_cap;

    SemanticKind lsp_token_map[MAX_LSP_TOKEN_TYPES];
    size_t lsp_token_map_len;
} Buffer;

// Buffer initialization and cleanup
void buf_init(Buffer *b);
void buf_free(Buffer *b);

// Buffer capacity management
void buf_ensure(Buffer *b, size_t need);

// Line operations
void buf_push(Buffer *b, char *line);
void buf_insert(Buffer *b, size_t idx, char *line);

// Diagnostics operations
void buf_clear_diagnostics(Buffer *buf);
void buf_add_diagnostic(Buffer *buf, int line, int col, int severity, const char *msg);

// File operations
int load_file(Buffer *b, const char *fname);
int save_file(Buffer *b, const char *fname);

#endif
