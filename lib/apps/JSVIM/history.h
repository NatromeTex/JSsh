// history.h - Per-file local history (undo/redo, disk snapshots)
#ifndef HISTORY_H
#define HISTORY_H

#include <time.h>

struct Buffer;  // forward declaration

// History storage location
typedef enum {
    HISTORY_LOCATION_DISK   = 0,
    HISTORY_LOCATION_MEMORY = 1
} HistoryLocation;

// Tree node representing a single version of the buffer
typedef struct HistoryNode {
    struct HistoryNode *parent;
    struct HistoryNode *first_child;
    struct HistoryNode *next_sibling;

    // Stable integer identifier for this node (used in on-disk tree)
    int id;

    // Snapshot of buffer text at this node
    char  **lines;
    size_t line_count;

    // Cursor position associated with this snapshot
    size_t cursor_line;
    size_t cursor_col;

    // Timestamp when this node was created
    time_t timestamp;

    // True if this node corresponds to the last saved-on-disk state
    int is_clean;
} HistoryNode;

// Per-buffer history state
typedef struct {
    HistoryNode *root;      // initial version
    HistoryNode *current;   // current version (where the editor is)

    HistoryLocation location; // disk or memory

    // Interval in minutes for disk snapshots:
    // -1 = disable disk snapshots
    //  0 = follow autosave events
    // >0 = snapshot every N minutes when modified
    int interval_minutes;

    // Last time we persisted a snapshot to disk (if enabled)
    time_t last_persist_time;

    // Absolute path of the file this history belongs to
    char filepath[1024];

    // Simple cap on in-memory nodes to avoid unbounded growth
    size_t node_count;

    // Next node id to assign when creating history nodes
    int next_id;
} HistoryState;

// Initialize history state for a buffer / file path.
void history_init(HistoryState *hs, const char *filepath,
                  HistoryLocation location, int interval_minutes);

// Free all history nodes and reset the state.
void history_free(HistoryState *hs);

// Record a new in-memory history node after an edit.
// Always updates the tree for undo/redo, regardless of disk settings.
void history_record_edit(HistoryState *hs, struct Buffer *buf,
                         size_t cursor_line, size_t cursor_col, time_t now);

// Mark the current node as corresponding to the last saved state.
void history_mark_clean(HistoryState *hs);

// Clear all history for this file (used by :clearhistory).
// After clearing, a new root node for the current buffer state is created.
void history_clear(HistoryState *hs, struct Buffer *buf,
                   size_t cursor_line, size_t cursor_col, time_t now);

// Prune older history while keeping the current branch (used by :prunehistory).
void history_prune(HistoryState *hs);

// Undo/redo operations. Return 1 on success, 0 if no further step.
int history_can_undo(const HistoryState *hs);
int history_can_redo(const HistoryState *hs);
int history_undo(HistoryState *hs, struct Buffer *buf,
                 size_t *cursor_line, size_t *cursor_col);
int history_redo(HistoryState *hs, struct Buffer *buf,
                 size_t *cursor_line, size_t *cursor_col);

#endif
