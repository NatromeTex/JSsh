// lsp.c - Language Server Protocol client
#include "lsp.h"
#include "buffer.h"
#include "highlight.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

void lsp_send(struct LSPProcess *p, const char *json) {
    if (p->stdin_fd == -1) return;

    size_t len = strlen(json);
    char header[128];
    int hlen = snprintf(header, sizeof(header), "Content-Length: %zu\r\n\r\n", len);

    // Write in two pieces: header, then body
    write(p->stdin_fd, header, hlen);
    write(p->stdin_fd, json, len);
}

void lsp_append_data(struct LSPProcess *p, const char *data, size_t n) {
    p->lsp_accum = realloc(p->lsp_accum, p->lsp_accum_len + n);
    memcpy(p->lsp_accum + p->lsp_accum_len, data, n);
    p->lsp_accum_len += n;
}

void lsp_notify_did_open(Buffer *buf) {
    if (buf->lsp.stdin_fd == -1) return;

    // Reconstruct full content
    size_t total = 0;
    for (size_t i = 0; i < buf->count; i++)
        total += strlen(buf->lines[i]) + 1;

    char *text = malloc(total + 1);
    if (!text) return;

    size_t pos = 0;
    for (size_t i = 0; i < buf->count; i++) {
        size_t n = strlen(buf->lines[i]);
        memcpy(text + pos, buf->lines[i], n);
        pos += n;
        text[pos++] = '\n';
    }
    text[pos] = '\0';

    // IMPORTANT: clangd wants a valid URI.
    // Prefer the actual file path when available.
    if (buf->lsp_uri[0] == '\0') {
        if (buf->filepath[0] == '/') {
            snprintf(buf->lsp_uri, sizeof(buf->lsp_uri), "file://%s", buf->filepath);
        } else if (buf->filepath[0] != '\0') {
            char cwd[1024];
            getcwd(cwd, sizeof(cwd));
            snprintf(buf->lsp_uri, sizeof(buf->lsp_uri), "file://%s/%s", cwd, buf->filepath);
        } else {
            char cwd[1024];
            getcwd(cwd, sizeof(cwd));
            snprintf(buf->lsp_uri, sizeof(buf->lsp_uri), "file://%s", cwd);
        }
    }

    // Build didOpen JSON using cJSON so text is properly escaped.
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        free(text);
        return;
    }

    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", "textDocument/didOpen");

    cJSON *params = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "params", params);

    cJSON *td = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "textDocument", td);

    cJSON_AddStringToObject(td, "uri", buf->lsp_uri);
    cJSON_AddStringToObject(td, "languageId", "c");
    buf->lsp_version = 1;
    cJSON_AddNumberToObject(td, "version", buf->lsp_version);
    cJSON_AddStringToObject(td, "text", text);

    char *json = cJSON_PrintUnformatted(root);
    if (json) {
        lsp_send(&buf->lsp, json);
        free(json);
    }

    cJSON_Delete(root);
    free(text);

    buf->lsp_opened = 1;
    buf->lsp_dirty = 0;
}

void lsp_initialize(Buffer *buf) {
    if (buf->lsp.stdin_fd == -1) return;

    char json[512];
    snprintf(json, sizeof(json),
        "{"\
            "\"jsonrpc\":\"2.0\","\
            "\"id\":1,"\
            "\"method\":\"initialize\","\
            "\"params\":{"\
                "\"processId\":%d,"\
                "\"rootUri\":null,"\
                "\"capabilities\":{" \
                    "\"textDocument\":{"
                        "\"semanticTokens\":{"
                            "\"requests\":{"
                                "\"full\":true"
                            "},"\
                            "\"tokenTypes\":[],"
                            "\"tokenModifiers\":[]"
                        "}"
                    "}"
                "}"\
            "}"\
        "}",
        getpid()
    );

    lsp_send(&buf->lsp, json);
}

void lsp_notify_did_change(Buffer *buf) {
    if (buf->lsp.stdin_fd == -1) return;
    if (!buf->lsp_opened) return;

    // Reconstruct full content
    size_t total = 0;
    for (size_t i = 0; i < buf->count; i++)
        total += strlen(buf->lines[i]) + 1;

    char *text = malloc(total + 1);
    if (!text) return;

    size_t pos = 0;
    for (size_t i = 0; i < buf->count; i++) {
        size_t n = strlen(buf->lines[i]);
        memcpy(text + pos, buf->lines[i], n);
        pos += n;
        text[pos++] = '\n';
    }
    text[pos] = '\0';

    // Increment document version for LSP.
    buf->lsp_version++;

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        free(text);
        return;
    }

    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", "textDocument/didChange");

    cJSON *params = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "params", params);

    cJSON *td = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "textDocument", td);
    cJSON_AddStringToObject(td, "uri", buf->lsp_uri);
    cJSON_AddNumberToObject(td, "version", buf->lsp_version);

    cJSON *changes = cJSON_CreateArray();
    cJSON_AddItemToObject(params, "contentChanges", changes);

    cJSON *change = cJSON_CreateObject();
    cJSON_AddItemToArray(changes, change);
    // Send full text as a single change.
    cJSON_AddStringToObject(change, "text", text);

    char *json = cJSON_PrintUnformatted(root);
    if (json) {
        lsp_send(&buf->lsp, json);
        free(json);
    }

    cJSON_Delete(root);
    free(text);

    buf->lsp_dirty = 0;
}

void lsp_request_semantic_tokens(Buffer *buf) {
    if (!buf) return;
    if (buf->lsp.stdin_fd == -1) return;
    if (!buf->lsp_opened) return;
    if (buf->lsp_uri[0] == '\0') return;

    cJSON *root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddStringToObject(root, "jsonrpc", "2.0");

    // Use a fixed ID for now
    cJSON_AddNumberToObject(root, "id", 100);

    cJSON_AddStringToObject(
        root,
        "method",
        "textDocument/semanticTokens/full"
    );

    cJSON *params = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "params", params);

    cJSON *td = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "textDocument", td);

    cJSON_AddStringToObject(td, "uri", buf->lsp_uri);

    char *json = cJSON_PrintUnformatted(root);
    if (json) {
        lsp_send(&buf->lsp, json);
        free(json);
    }

    cJSON_Delete(root);
}

static void handle_lsp_json_message(Buffer *buf, const char *json_text) {
    cJSON *root = cJSON_Parse(json_text);
    if (!root) {
        fprintf(stderr, "LSP: invalid JSON\n");
        return;
    }

    // Common LSP fields
    cJSON *id      = cJSON_GetObjectItem(root, "id");
    cJSON *method  = cJSON_GetObjectItem(root, "method");

    // initialize response
    if (!method && id && cJSON_IsNumber(id)) {
        int msg_id = id->valueint;

        if (msg_id == 1) {
            // initialize response (LSP spec)
            cJSON *result = cJSON_GetObjectItem(root, "result");
            if (result) {
                cJSON *caps = cJSON_GetObjectItem(result, "capabilities");
                if (caps) {
                    cJSON *stp = cJSON_GetObjectItem(caps, "semanticTokensProvider");
                    if (stp) {
                        cJSON *legend = cJSON_GetObjectItem(stp, "legend");
                        if (legend) {
                            cJSON *tokenTypes = cJSON_GetObjectItem(legend, "tokenTypes");
                            if (cJSON_IsArray(tokenTypes)) {
                                int count = cJSON_GetArraySize(tokenTypes);
                                if (count > MAX_LSP_TOKEN_TYPES)
                                    count = MAX_LSP_TOKEN_TYPES;

                                for (int i = 0; i < count; i++) {
                                    cJSON *item = cJSON_GetArrayItem(tokenTypes, i);
                                    if (item && cJSON_IsString(item)) {
                                        buf->lsp_token_map[i] =
                                            semantic_kind_from_lsp(item->valuestring);
                                    } else {
                                        buf->lsp_token_map[i] = SEM_NONE;
                                    }
                                }
                                buf->lsp_token_map_len = (size_t)count;
                            }
                        }
                    }
                }

                lsp_send(&buf->lsp,
                    "{"
                        "\"jsonrpc\":\"2.0\","
                        "\"method\":\"initialized\","
                        "\"params\":{}"
                    "}"
                );

                // Send didOpen now that connection is established.
                lsp_notify_did_open(buf);
                lsp_request_semantic_tokens(buf);
            }
        } else if (msg_id == 100) {
            cJSON *result = cJSON_GetObjectItem(root, "result");
            cJSON *data   = cJSON_GetObjectItem(result, "data");
            if (!cJSON_IsArray(data)) {
                cJSON_Delete(root);
                return;
            }
        
            semantic_tokens_clear(buf);
        
            int line = 0;
            int col  = 0;
            int n = cJSON_GetArraySize(data);
        
            for (int i = 0; i + 5 <= n; i += 5) {
                cJSON *dL = cJSON_GetArrayItem(data, i);
                cJSON *dS = cJSON_GetArrayItem(data, i + 1);
                cJSON *dLen = cJSON_GetArrayItem(data, i + 2);
                cJSON *dType = cJSON_GetArrayItem(data, i + 3);
                cJSON *dMod = cJSON_GetArrayItem(data, i + 4);
                
                if (!dL || !dS || !dLen || !dType || !dMod) continue;
                
                int deltaLine  = dL->valueint;
                int deltaStart = dS->valueint;
                int length     = dLen->valueint;
                int type_index = dType->valueint;
                int modifiers  = dMod->valueint;
            
                line += deltaLine;
                col = (deltaLine == 0) ? col + deltaStart : deltaStart;
            
                SemanticToken token;
                token.line = line;
                token.col  = col;
                token.len  = length;
                token.modifiers = modifiers;
            
                SemanticKind kind = SEM_NONE;
                if (type_index >= 0 && (size_t)type_index < buf->lsp_token_map_len)
                    kind = buf->lsp_token_map[type_index];
                token.kind = kind;
            
                semantic_token_push(buf, &token);
            }
        }

        cJSON_Delete(root);
        return;
    }

    // method present
    if (method && cJSON_IsString(method)) {
        const char *m = method->valuestring;

        // publishDiagnostics (clangd sends errors/warnings)
        if (strcmp(m, "textDocument/publishDiagnostics") == 0) {
            cJSON *params = cJSON_GetObjectItem(root, "params");
            if (!params) goto done;

            // Ignore errors from other files
            cJSON *uri = cJSON_GetObjectItem(params, "uri");
            if (!uri || strcmp(uri->valuestring, buf->lsp_uri) != 0)
                goto done;

            cJSON *diagnostics = cJSON_GetObjectItem(params, "diagnostics");
            if (!diagnostics || !cJSON_IsArray(diagnostics)) goto done;

            // Clear old diagnostics for this buffer
            buf_clear_diagnostics(buf);

            int count = cJSON_GetArraySize(diagnostics);
            for (int i = 0; i < count; i++) {
                cJSON *diag = cJSON_GetArrayItem(diagnostics, i);
                if (!diag) continue;

                cJSON *range = cJSON_GetObjectItem(diag, "range");
                cJSON *msg   = cJSON_GetObjectItem(diag, "message");

                if (!range || !msg) continue;

                cJSON *start = cJSON_GetObjectItem(range, "start");
                cJSON *line_j  = cJSON_GetObjectItem(start, "line");
                cJSON *col_j   = cJSON_GetObjectItem(start, "character");

                if (!line_j || !col_j || !msg) continue;

                int l = line_j->valueint;
                int c = col_j->valueint;
                const char *t = msg->valuestring;

                cJSON *sev = cJSON_GetObjectItem(diag, "severity");
                int severity = (sev && cJSON_IsNumber(sev)) ? sev->valueint : 3;

                if (severity > 2)
                    continue;

                buf_add_diagnostic(buf, l, c, severity, t);
            }

            goto done;
        }
        
        // window/logMessage — used for debugging
        if (strcmp(m, "window/logMessage") == 0) {
            cJSON *params = cJSON_GetObjectItem(root, "params");
            if (params) {
                cJSON *msg = cJSON_GetObjectItem(params, "message");
                if (msg && cJSON_IsString(msg)) {
                    fprintf(stderr, "LSP log: %s\n", msg->valuestring);
                }
            }
            goto done;
        }
        
        // window/showMessage        
        if (strcmp(m, "window/showMessage") == 0) {
            cJSON *params = cJSON_GetObjectItem(root, "params");
            if (params) {
                cJSON *msg = cJSON_GetObjectItem(params, "message");
                if (msg && cJSON_IsString(msg)) {
                    fprintf(stderr, "LSP showMessage: %s\n", msg->valuestring);
                }
            }
            goto done;
        }

        // clangd sometimes sends telemetry/event notifications
        if (strcmp(m, "$/progress") == 0 ||
            strcmp(m, "telemetry/event") == 0) {
            goto done; // safely ignore
        }
    }

done:
    cJSON_Delete(root);
}

int try_parse_lsp_message(Buffer *buf) {
    struct LSPProcess *p = &buf->lsp;

    char *acc = p->lsp_accum;
    size_t len = p->lsp_accum_len;

    if (len < 4) {
        return 0; // not enough data for even "\r\n\r\n"
    }

    // Find header terminator: "\r\n\r\n"
    char *header_end = NULL;
    for (size_t i = 0; i + 3 < len; i++) {
        if (acc[i] == '\r' && acc[i+1] == '\n' && acc[i+2] == '\r' && acc[i+3] == '\n') {
            header_end = acc + i + 4; // start of JSON payload
            break;
        }
    }

    if (!header_end) {
        return 0; // incomplete header
    }

    // HEADER PARSING
    size_t header_len = header_end - acc;
    size_t content_length = 0;

    // Safely isolate the header for parsing
    char saved = acc[header_len];
    acc[header_len] = '\0';

    // Find the "Content-Length:" field strictly inside the header
    const char *needle = "Content-Length:";
    char *cl = strstr(acc, needle);

    if (!cl || cl >= acc + header_len) {
        acc[header_len] = saved;
        return -1; // malformed header
    }

    // Move pointer past "Content-Length:"
    cl += strlen(needle);

    // Skip whitespace
    while (*cl == ' ' || *cl == '\t') cl++;

    // Parse length
    content_length = strtoul(cl, NULL, 10);

    // Restore buffer
    acc[header_len] = saved;

    // TOTAL SIZE CHECK
    size_t total_needed = header_len + content_length;

    if (len < total_needed) {
        return 0; // incomplete message
    }

    // complete message available
    char *json = malloc(content_length + 1);
    if (!json) {
        return -1; // allocation failure
    }

    memcpy(json, header_end, content_length);
    json[content_length] = '\0';

    // Handle the finished JSON message
    handle_lsp_json_message(buf, json);
    free(json);

    // REMOVE THE PROCESSED MESSAGE FROM THE BUFFER
    size_t remaining = len - total_needed;

    if (remaining > 0) {
        memmove(acc, acc + total_needed, remaining);
    }

    p->lsp_accum_len = remaining;

    // Shrink to exact size (if zero, free fully)
    if (remaining == 0) {
        free(p->lsp_accum);
        p->lsp_accum = NULL;
    } else {
        char *newbuf = realloc(p->lsp_accum, remaining);
        if (newbuf) {
            p->lsp_accum = newbuf;
        }
    }

    return 1; // message processed
}

struct LSPProcess spawn_lsp(void) {
    struct LSPProcess proc = {0};

    int in_pipe[2];   // parent writes → child reads (stdin)
    int out_pipe[2];  // child writes → parent reads (stdout)

    if (pipe(in_pipe) < 0) {
        perror("pipe in_pipe");
        return proc;
    }

    if (pipe(out_pipe) < 0) {
        perror("pipe out_pipe");
        close(in_pipe[0]);
        close(in_pipe[1]);
        return proc;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        return proc;
    }

    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);

        close(in_pipe[1]);
        close(out_pipe[0]);

        int logfd = open("clangd.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }

        setpgid(0, 0);
        // testing with clangd, will later allow more options
        execlp("clangd", "clangd", NULL);
        perror("execlp clangd");
        _exit(1);
    }

    // Close ends not used by jsvim
    close(in_pipe[0]);
    close(out_pipe[1]);

    // Set stdout_fd non-blocking
    int flags = fcntl(out_pipe[0], F_GETFL, 0);
    fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);

    proc.pid = pid;
    proc.stdin_fd = in_pipe[1];
    proc.stdout_fd = out_pipe[0];

    return proc;
}

void stop_lsp(struct LSPProcess *p) {
    if (p->pid > 0) {
        kill(p->pid, SIGTERM);
        waitpid(p->pid, NULL, 0);
    }
    if (p->stdin_fd > 0) close(p->stdin_fd);
    if (p->stdout_fd > 0) close(p->stdout_fd);
}
