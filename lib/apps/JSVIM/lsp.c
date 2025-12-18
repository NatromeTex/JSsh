// lsp.c - Language Server Protocol client
#include "lsp.h"
#include "buffer.h"
#include "highlight.h"
#include "cJSON.h"
#include "language.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>
#include <linux/limits.h>

#define JSVIM_CONFIG_FILE ".jsvimrc"
#define MAX_LSP_ARGS 8

// Default LSP commands (used as fallback)
static const LspCmd LSP_CMDS_DEFAULT[] = {
    [FT_NONE] = { { NULL } },
    [FT_C] = {
        { "clangd", NULL }
    },
    [FT_CPP] = {
        { "clangd", NULL }
    },
    [FT_PYTHON] = {
        { "pyright-langserver", "--stdio", NULL }
    },
    [FT_TS] = {
        { "typescript-language-server", "--stdio", NULL }
    },
    [FT_JS] = {
        { "typescript-language-server", "--stdio", NULL }
    },
};

// Custom LSP commands from config file
typedef struct {
    char *argv[MAX_LSP_ARGS];  // dynamically allocated, NULL-terminated
} LspCmdConfig;

static LspCmdConfig lsp_config[FT_MARKDOWN + 1] = {0};
static int lsp_config_loaded = 0;

// Map config key to FileType
static FileType config_key_to_filetype(const char *key) {
    if (strcmp(key, "lsp.c") == 0) return FT_C;
    if (strcmp(key, "lsp.cpp") == 0) return FT_CPP;
    if (strcmp(key, "lsp.python") == 0) return FT_PYTHON;
    if (strcmp(key, "lsp.typescript") == 0) return FT_TS;
    if (strcmp(key, "lsp.javascript") == 0) return FT_JS;
    if (strcmp(key, "lsp.rust") == 0) return FT_RUST;
    if (strcmp(key, "lsp.go") == 0) return FT_GO;
    if (strcmp(key, "lsp.java") == 0) return FT_JAVA;
    if (strcmp(key, "lsp.sh") == 0) return FT_SH;
    if (strcmp(key, "lsp.json") == 0) return FT_JSON;
    if (strcmp(key, "lsp.markdown") == 0) return FT_MARKDOWN;
    return FT_NONE;
}

// Free a single LspCmdConfig entry
static void free_lsp_cmd_config(LspCmdConfig *cfg) {
    for (int i = 0; i < MAX_LSP_ARGS && cfg->argv[i]; i++) {
        free(cfg->argv[i]);
        cfg->argv[i] = NULL;
    }
}

// Parse a command string into argv array (space-separated)
static void parse_lsp_command(const char *cmd, LspCmdConfig *cfg) {
    free_lsp_cmd_config(cfg);
    
    char *cmdcopy = strdup(cmd);
    if (!cmdcopy) return;
    
    int argc = 0;
    char *token = strtok(cmdcopy, " \t");
    while (token && argc < MAX_LSP_ARGS - 1) {
        cfg->argv[argc++] = strdup(token);
        token = strtok(NULL, " \t");
    }
    cfg->argv[argc] = NULL;
    
    free(cmdcopy);
}

// Load LSP configuration from ~/.jsvimrc
void lsp_load_config(void) {
    if (lsp_config_loaded) return;
    lsp_config_loaded = 1;
    
    const char *home = getenv("HOME");
    if (!home) return;
    
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/%s", home, JSVIM_CONFIG_FILE);
    
    FILE *fp = fopen(config_path, "r");
    if (!fp) return;
    
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        
        // Remove trailing newline
        size_t len = strlen(p);
        if (len > 0 && p[len-1] == '\n') p[len-1] = '\0';
        
        // Parse key=value
        char *eq = strchr(p, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = p;
        char *value = eq + 1;
        
        // Trim whitespace from key
        char *key_end = eq - 1;
        while (key_end > key && (*key_end == ' ' || *key_end == '\t')) *key_end-- = '\0';
        
        // Trim leading whitespace from value
        while (*value == ' ' || *value == '\t') value++;
        
        // Check if this is an LSP config key
        FileType ft = config_key_to_filetype(key);
        if (ft != FT_NONE) {
            parse_lsp_command(value, &lsp_config[ft]);
        }
    }
    
    fclose(fp);
}

// Get LSP command for a filetype (checks config first, then defaults)
static const char *const *get_lsp_cmd(FileType ft) {
    // Ensure config is loaded
    if (!lsp_config_loaded) {
        lsp_load_config();
    }
    
    // Check if we have a custom config for this filetype
    if (ft > FT_NONE && ft <= FT_MARKDOWN && lsp_config[ft].argv[0]) {
        return (const char *const *)lsp_config[ft].argv;
    }
    
    // Fall back to defaults
    return LSP_CMDS_DEFAULT[ft].argv;
}

// Cleanup LSP config (call on exit)
void lsp_config_cleanup(void) {
    for (int i = 0; i <= FT_MARKDOWN; i++) {
        free_lsp_cmd_config(&lsp_config[i]);
    }
}

// Map FileType to LSP languageId string
static const char *lsp_language_id(FileType ft) {
    switch (ft) {
        case FT_C:        return "c";
        case FT_CPP:      return "cpp";
        case FT_JS:       return "javascript";
        case FT_TS:       return "typescript";
        case FT_PYTHON:   return "python";
        case FT_RUST:     return "rust";
        case FT_GO:       return "go";
        case FT_JAVA:     return "java";
        case FT_SH:       return "shellscript";
        case FT_MAKEFILE: return "makefile";
        case FT_JSON:     return "json";
        case FT_MARKDOWN: return "markdown";
        case FT_NONE:     return "plaintext";
        default:          return "plaintext";
    }
}

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
            // Use realpath to get canonical absolute path (handles ./path and ../path)
            char resolved[PATH_MAX];
            if (realpath(buf->filepath, resolved)) {
                snprintf(buf->lsp_uri, sizeof(buf->lsp_uri), "file://%s", resolved);
            } else {
                // Fallback: strip leading ./ and combine with cwd
                const char *rel_path = buf->filepath;
                if (rel_path[0] == '.' && rel_path[1] == '/') {
                    rel_path += 2;  // skip "./"
                }
                char cwd[1024];
                getcwd(cwd, sizeof(cwd));
                snprintf(buf->lsp_uri, sizeof(buf->lsp_uri), "file://%s/%s", cwd, rel_path);
            }
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
    cJSON_AddStringToObject(td, "languageId", lsp_language_id(buf->ft));
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

    // Build rootUri from file path - pyright and many other LSPs require this
    char root_uri[2048] = {0};
    if (buf->filepath[0] == '/') {
        // Find the directory containing the file
        char dirpath[1024];
        strncpy(dirpath, buf->filepath, sizeof(dirpath) - 1);
        char *last_slash = strrchr(dirpath, '/');
        if (last_slash && last_slash != dirpath) {
            *last_slash = '\0';
        }
        snprintf(root_uri, sizeof(root_uri), "file://%s", dirpath);
    } else if (buf->filepath[0] != '\0') {
        // Use realpath to get canonical absolute path (handles ./path and ../path)
        char resolved[PATH_MAX];
        if (realpath(buf->filepath, resolved)) {
            // Find directory from resolved path
            char *last_slash = strrchr(resolved, '/');
            if (last_slash && last_slash != resolved) {
                *last_slash = '\0';
            }
            snprintf(root_uri, sizeof(root_uri), "file://%s", resolved);
        } else {
            // Fallback to cwd
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd))) {
                snprintf(root_uri, sizeof(root_uri), "file://%s", cwd);
            }
        }
    } else {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            snprintf(root_uri, sizeof(root_uri), "file://%s", cwd);
        }
    }

    // Build initialize request using cJSON for proper escaping
    cJSON *req = cJSON_CreateObject();
    if (!req) return;

    cJSON_AddStringToObject(req, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(req, "id", 1);
    cJSON_AddStringToObject(req, "method", "initialize");

    cJSON *params = cJSON_CreateObject();
    cJSON_AddItemToObject(req, "params", params);

    cJSON_AddNumberToObject(params, "processId", getpid());
    
    // Add rootUri (required by pyright and many other LSPs)
    if (root_uri[0] != '\0') {
        cJSON_AddStringToObject(params, "rootUri", root_uri);
    } else {
        cJSON_AddNullToObject(params, "rootUri");
    }

    // Add capabilities
    cJSON *caps = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "capabilities", caps);

    cJSON *textDoc = cJSON_CreateObject();
    cJSON_AddItemToObject(caps, "textDocument", textDoc);

    cJSON *semTokens = cJSON_CreateObject();
    cJSON_AddItemToObject(textDoc, "semanticTokens", semTokens);

    cJSON *requests = cJSON_CreateObject();
    cJSON_AddItemToObject(semTokens, "requests", requests);
    cJSON_AddBoolToObject(requests, "full", 1);

    cJSON *tokenTypes = cJSON_CreateArray();
    cJSON_AddItemToObject(semTokens, "tokenTypes", tokenTypes);

    cJSON *tokenModifiers = cJSON_CreateArray();
    cJSON_AddItemToObject(semTokens, "tokenModifiers", tokenModifiers);

    // Add workspaceFolders capability (helps pyright)
    cJSON *workspace = cJSON_CreateObject();
    cJSON_AddItemToObject(caps, "workspace", workspace);

    cJSON *wsFolders = cJSON_CreateObject();
    cJSON_AddItemToObject(workspace, "workspaceFolders", wsFolders);
    cJSON_AddBoolToObject(wsFolders, "supported", 1);

    char *json = cJSON_PrintUnformatted(req);
    if (json) {
        lsp_send(&buf->lsp, json);
        free(json);
    }

    cJSON_Delete(req);
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
        return;  // silently ignore invalid JSON
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
        
            semantic_tokens_clear_lsp(buf);
        
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
                token.source = TOKEN_SOURCE_LSP;
            
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
        
        // window/logMessage — silently ignore (would corrupt ncurses display)
        if (strcmp(m, "window/logMessage") == 0) {
            goto done;
        }
        
        // window/showMessage — silently ignore (would corrupt ncurses display)
        if (strcmp(m, "window/showMessage") == 0) {
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

struct LSPProcess spawn_lsp(FileType *ft) {
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
        
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
    
        setpgid(0, 0);
    
        const char *const *cmd = get_lsp_cmd(*ft);
        execvp(cmd[0], (char *const *)cmd);
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
