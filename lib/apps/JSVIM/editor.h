// editor.h - Editor state and key handling
#ifndef EDITOR_H
#define EDITOR_H

#include <ncurses.h>
#include <time.h>
#include "buffer.h"

typedef struct {
    size_t line;
    size_t col;
} CursorPos;

typedef struct {
    // Range in buffer coordinates BEFORE the edit
    size_t pre_start_line;
    size_t pre_start_col;
    size_t pre_end_line;
    size_t pre_end_col;
    // Range in buffer coordinates AFTER the edit
    size_t post_start_line;
    size_t post_start_col;
    size_t post_end_line;
    size_t post_end_col;
    // Text replaced: old_text (pre range contents) -> new_text (post range contents)
    char *old_text;
    char *new_text;
    CursorPos cursor_before;
    CursorPos cursor_after;
} UndoDelta;

typedef struct {
    UndoDelta *deltas;
    size_t count;
    size_t cap;
} UndoEntry;

typedef struct {
    UndoEntry *entries;
    size_t size;   // number of valid entries
    size_t index;  // current history position (0..size)
} UndoHistory;

// Editor state structure
typedef struct {
    Buffer buf;
    char filename[1024];
    int have_filename;
    int existing_file;
    
    size_t cursor_line;
    size_t cursor_col;
    size_t scroll_y;
    
    int mode_insert;  // 1 = insert, 0 = command
    int modified;
    
    char cmdbuf[1024];
    size_t cmdlen;
    
    int quit;
    int force_quit;
    int line_number_relative;  // 0 = absolute, 1 = relative
    int file_created;          // 1 if file exists or user confirmed creation
    int pending_create_prompt; // 1 if waiting for user to confirm file creation
    
    int tab_width;  // -1 = use \t, >0 = number of spaces per tab

    int autosave_enabled;  // 1 = autosave on, 0 = off
    time_t last_input_time; // last time we received user input

    // Undo/redo configuration
    int edit_group_timeout; // milliseconds to group rapid edits
    UndoHistory history;
    long long last_edit_time_ms;
    int has_last_edit_time;
} EditorState;

// Load editor configuration from ~/.jsvimrc
void editor_load_config(EditorState *ed);

// Get the indentation string based on tab_width setting
const char *editor_get_indent_str(EditorState *ed);

// Get the current line's indentation
char *editor_get_line_indent(const char *line);

// Get auto-indent for new line based on language and previous line
char *editor_auto_indent(EditorState *ed, const char *prev_line);

// Initialize editor state
void editor_init(EditorState *ed);

// Cleanup editor state
void editor_cleanup(EditorState *ed);

// Undo/redo operations
void editor_undo(EditorState *ed);
void editor_redo(EditorState *ed);

// Handle input in insert mode
void editor_handle_insert_mode(EditorState *ed, int ch, int visible_rows);

// Handle input in command mode
void editor_handle_command_mode(EditorState *ed, int ch, 
                                WINDOW *cmd_win, int maxx);

// Process LSP input (non-blocking read)
void editor_process_lsp(EditorState *ed);

#endif
