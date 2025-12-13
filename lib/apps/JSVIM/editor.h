// editor.h - Editor state and key handling
#ifndef EDITOR_H
#define EDITOR_H

#include <ncurses.h>
#include "buffer.h"

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
} EditorState;

// Initialize editor state
void editor_init(EditorState *ed);

// Cleanup editor state
void editor_cleanup(EditorState *ed);

// Handle input in insert mode
void editor_handle_insert_mode(EditorState *ed, int ch, int visible_rows);

// Handle input in command mode
void editor_handle_command_mode(EditorState *ed, int ch, 
                                WINDOW *cmd_win, int maxx);

// Process LSP input (non-blocking read)
void editor_process_lsp(EditorState *ed);

#endif
