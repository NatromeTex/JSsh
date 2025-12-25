// history.c - Per-file local history (undo/redo, optional disk snapshots)
#include "history.h"
#include "buffer.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

// Maximum number of in-memory history nodes per buffer
#define HISTORY_MAX_NODES 256

// Base directory for on-disk history: $HOME/.config/JSVIM/history
static int history_get_base_dir(char *out, size_t out_len) {
    const char *home = getenv("HOME");
    if (!home) return 0;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0700);
    snprintf(path, sizeof(path), "%s/.config/JSVIM", home);
    mkdir(path, 0700);
    snprintf(path, sizeof(path), "%s/.config/JSVIM/history", home);
    mkdir(path, 0700);

    // Copy full path safely to caller buffer
    size_t len = strlen(path);
    if (len >= out_len) len = out_len - 1;
    memcpy(out, path, len);
    out[len] = '\0';
    return 1;
}

// Simple hash of the file path for history filename
static unsigned long history_hash_path(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++) != 0) {
        h = ((h << 5) + h) + (unsigned long)c;
    }
    return h;
}

static int history_get_file_path(const HistoryState *hs, char *out, size_t out_len) {
    if (!hs || !hs->filepath[0]) return 0;
    char dir[1024];
    if (!history_get_base_dir(dir, sizeof(dir))) return 0;
    unsigned long h = history_hash_path(hs->filepath);
    size_t need = strlen(dir) + 1 /* slash */ + 8 /* hash */ + 5 /* .hist */ + 1; /* NUL */
    if (need > out_len) return 0;
    snprintf(out, out_len, "%s/%08lx.hist", dir, h);
    return 1;
}

static void history_free_node_recursive(HistoryNode *node) {
    if (!node) return;
    HistoryNode *child = node->first_child;
    while (child) {
        HistoryNode *next = child->next_sibling;
        history_free_node_recursive(child);
        child = next;
    }
    if (node->lines) {
        for (size_t i = 0; i < node->line_count; i++) {
            free(node->lines[i]);
        }
        free(node->lines);
    }
    free(node);
}

static HistoryNode *history_clone_from_buffer(struct Buffer *buf,
                                              size_t cursor_line,
                                              size_t cursor_col,
                                              time_t now) {
    HistoryNode *node = calloc(1, sizeof(HistoryNode));
    if (!node) return NULL;

    node->timestamp = now;
    node->cursor_line = cursor_line;
    node->cursor_col = cursor_col;

    if (buf->count == 0) {
        node->lines = NULL;
        node->line_count = 0;
        return node;
    }

    node->line_count = buf->count;
    node->lines = malloc(sizeof(char*) * node->line_count);
    if (!node->lines) {
        free(node);
        return NULL;
    }

    for (size_t i = 0; i < buf->count; i++) {
        node->lines[i] = dupstr(buf->lines[i]);
        if (!node->lines[i]) {
            for (size_t j = 0; j < i; j++) free(node->lines[j]);
            free(node->lines);
            free(node);
            return NULL;
        }
    }

    return node;
}

static void history_restore_to_buffer(HistoryNode *node, struct Buffer *buf) {
    // Replace buffer contents with the snapshot stored in node.
    if (!node) return;

    // Free current buffer lines
    for (size_t i = 0; i < buf->count; i++) {
        free(buf->lines[i]);
    }
    buf->count = 0;

    if (!node->lines || node->line_count == 0) {
        // Ensure at least one empty line
        if (buf->cap == 0) {
            buf->cap = 16;
            buf->lines = malloc(sizeof(char*) * buf->cap);
        }
        buf->lines[0] = dupstr("");
        buf->count = 1;
        return;
    }

    if (buf->cap < node->line_count) {
        buf->cap = node->line_count;
        buf->lines = realloc(buf->lines, sizeof(char*) * buf->cap);
    }

    for (size_t i = 0; i < node->line_count; i++) {
        buf->lines[i] = dupstr(node->lines[i]);
    }
    buf->count = node->line_count;
}

void history_init(HistoryState *hs, const char *filepath,
                  HistoryLocation location, int interval_minutes) {
    if (!hs) return;
    memset(hs, 0, sizeof(*hs));

    hs->location = location;
    hs->interval_minutes = interval_minutes;
    hs->last_persist_time = 0;
    hs->next_id = 1;
    if (filepath) {
        strncpy(hs->filepath, filepath, sizeof(hs->filepath) - 1);
        hs->filepath[sizeof(hs->filepath) - 1] = '\0';
    } else {
        hs->filepath[0] = '\0';
    }
}

void history_free(HistoryState *hs) {
    if (!hs) return;
    history_free_node_recursive(hs->root);
    hs->root = NULL;
    hs->current = NULL;
    hs->node_count = 0;
}

// Persist a single node snapshot to the per-file on-disk history.
// Each record encodes: NODE <id> <parent-id> <timestamp>
// followed by LINES <n> and n lines of text, then END.
static void history_persist_node(HistoryState *hs, HistoryNode *node) {
    if (!hs || !node) return;
    if (hs->location != HISTORY_LOCATION_DISK) return;
    if (hs->interval_minutes == -1) return; // history disabled

    char path[1024];
    if (!history_get_file_path(hs, path, sizeof(path))) return;

    FILE *fp = fopen(path, "a");
    if (!fp) return;

    int parent_id = node->parent ? node->parent->id : -1;
    fprintf(fp, "NODE %d %d %ld\n", node->id, parent_id, (long)node->timestamp);
    fprintf(fp, "LINES %zu\n", node->line_count);
    for (size_t i = 0; i < node->line_count; i++) {
        const char *line = node->lines[i] ? node->lines[i] : "";
        fputs(line, fp);
        fputc('\n', fp);
    }
    fputs("END\n", fp);
    fclose(fp);
}

static void history_attach_child(HistoryState *hs, HistoryNode *parent, HistoryNode *child) {
    if (!parent || !child) return;
    child->parent = parent;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
    hs->node_count++;

    // Simple pruning: if we exceed max nodes, drop all siblings of root
    if (hs->node_count > HISTORY_MAX_NODES && hs->root) {
        HistoryNode *child_it = hs->root->first_child;
        HistoryNode *keep = NULL;
        // Keep the chain that leads to current, drop others
        while (child_it) {
            HistoryNode *next = child_it->next_sibling;
            if (child_it == hs->current || child_it == hs->current->parent) {
                keep = child_it;
            } else {
                history_free_node_recursive(child_it);
            }
            child_it = next;
        }
        hs->root->first_child = keep;
        // Node count is approximate after pruning; recompute conservatively
        hs->node_count = 1; // root
        HistoryNode *n = hs->root->first_child;
        while (n) {
            hs->node_count++;
            n = n->first_child; // linearize along main branch
        }
    }
}

void history_record_edit(HistoryState *hs, struct Buffer *buf,
                         size_t cursor_line, size_t cursor_col, time_t now) {
    if (!hs || !buf) return;

    // If no root yet, create one from the current buffer state
    if (!hs->root) {
        HistoryNode *root = history_clone_from_buffer(buf, cursor_line, cursor_col, now);
        if (!root) return;
        root->id = hs->next_id++;
        hs->root = root;
        hs->current = root;
        hs->node_count = 1;
        return;
    }

    // Create a child node representing the new state
    HistoryNode *node = history_clone_from_buffer(buf, cursor_line, cursor_col, now);
    if (!node) return;

    node->id = hs->next_id++;

    history_attach_child(hs, hs->current, node);
    hs->current = node;

    // For interval-based disk snapshots (>0), persist when sufficient time has passed
    if (hs->location == HISTORY_LOCATION_DISK && hs->interval_minutes > 0) {
        if (hs->last_persist_time == 0 ||
            now - hs->last_persist_time >= (time_t)(hs->interval_minutes * 60)) {
            history_persist_node(hs, node);
            hs->last_persist_time = now;
        }
    }
}

void history_mark_clean(HistoryState *hs) {
    if (!hs || !hs->current) return;
    // Clear previous clean flags
    HistoryNode *it = hs->root;
    while (it) {
        it->is_clean = 0;
        it = it->first_child; // follow main branch only
    }
    hs->current->is_clean = 1;

    // For disk history with interval == 0, follow autosave/save events
    if (hs->location == HISTORY_LOCATION_DISK && hs->interval_minutes == 0) {
        history_persist_node(hs, hs->current);
        hs->last_persist_time = hs->current->timestamp;
    }
}

void history_clear(HistoryState *hs, struct Buffer *buf,
                   size_t cursor_line, size_t cursor_col, time_t now) {
    if (!hs || !buf) return;
    // Remove on-disk history file as well, if any
    if (hs->location == HISTORY_LOCATION_DISK) {
        char path[1024];
        if (history_get_file_path(hs, path, sizeof(path))) {
            unlink(path);
        }
        hs->last_persist_time = 0;
    }
    history_free_node_recursive(hs->root);
    hs->root = NULL;
    hs->current = NULL;
    hs->node_count = 0;

    HistoryNode *root = history_clone_from_buffer(buf, cursor_line, cursor_col, now);
    if (!root) return;
    hs->root = root;
    hs->current = root;
    hs->node_count = 1;
}

void history_prune(HistoryState *hs) {
    if (!hs || !hs->root || !hs->current) return;

    // Keep only the chain from root to current; discard other branches.
    HistoryNode *node = hs->root;
    while (node) {
        HistoryNode *child = node->first_child;
        HistoryNode *on_path = NULL;
        while (child) {
            HistoryNode *next = child->next_sibling;
            // Determine if this child is on the path to current
            HistoryNode *p = hs->current;
            int on_chain = 0;
            while (p) {
                if (p == child) {
                    on_chain = 1;
                    break;
                }
                p = p->parent;
            }
            if (on_chain) {
                on_path = child;
            } else {
                history_free_node_recursive(child);
            }
            child = next;
        }
        node->first_child = on_path;
        node = on_path;
    }

    // Recompute node_count along the remaining chain
    hs->node_count = 0;
    HistoryNode *it = hs->root;
    while (it) {
        hs->node_count++;
        it = it->first_child;
    }
}

int history_can_undo(const HistoryState *hs) {
    return hs && hs->current && hs->current->parent != NULL;
}

int history_can_redo(const HistoryState *hs) {
    return hs && hs->current && hs->current->first_child != NULL;
}

int history_undo(HistoryState *hs, struct Buffer *buf,
                 size_t *cursor_line, size_t *cursor_col) {
    if (!history_can_undo(hs)) return 0;
    HistoryNode *target = hs->current->parent;
    history_restore_to_buffer(target, buf);
    hs->current = target;
    if (cursor_line) *cursor_line = target->cursor_line;
    if (cursor_col) *cursor_col = target->cursor_col;
    return 1;
}

int history_redo(HistoryState *hs, struct Buffer *buf,
                 size_t *cursor_line, size_t *cursor_col) {
    if (!history_can_redo(hs)) return 0;
    // By default, follow the first_child branch as the main redo chain
    HistoryNode *target = hs->current->first_child;
    history_restore_to_buffer(target, buf);
    hs->current = target;
    if (cursor_line) *cursor_line = target->cursor_line;
    if (cursor_col) *cursor_col = target->cursor_col;
    return 1;
}
