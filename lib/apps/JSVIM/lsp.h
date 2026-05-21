// lsp.h - Language Server Protocol client
#ifndef LSP_H
#define LSP_H

#include "buffer.h"
#include "language.h"

// Skip full-file semantic-token requests for files larger than this.
// clangd's response scales with token count and the JSON parse dominates
// per-frame cost — letting it fire on large headers (think quickjs.h)
// freezes the editor for seconds while the response is parsed.
#define LSP_SEMTOK_MAX_LINES 3000

// LSP process commands
typedef struct {
    const char *argv[8];  // NULL-terminated
} LspCmd;

// Spawn LSP server process
struct LSPProcess spawn_lsp(FileType *ft);

// Stop LSP server process
void stop_lsp(struct LSPProcess *p);

// Send JSON message to LSP
void lsp_send(struct LSPProcess *p, const char *json);

// Append data to LSP accumulation buffer
void lsp_append_data(struct LSPProcess *p, const char *data, size_t n);

// Try to parse a complete LSP message from accumulation buffer
int try_parse_lsp_message(Buffer *buf);

// LSP notifications and requests
void lsp_initialize(Buffer *buf);
void lsp_notify_did_open(Buffer *buf);
void lsp_notify_did_change(Buffer *buf);
void lsp_request_semantic_tokens(Buffer *buf);

// LSP configuration from ~/.jsvimrc
void lsp_load_config(void);
void lsp_config_cleanup(void);

#endif
