// render.h - ncurses rendering functions
#ifndef RENDER_H
#define RENDER_H

#include <ncurses.h>
#include "buffer.h"

// Color pairs
#define COLOR_PAIR_TEXT     1
#define COLOR_PAIR_STATUS   2
#define COLOR_PAIR_GUTTER   3
#define COLOR_PAIR_ERROR    4
#define COLOR_PAIR_WARNING  5
#define COLOR_PAIR_STATUS_MID    6   // middle section of status bar
#define COLOR_PAIR_ARROW_LEFT    7   // left arrow transition
#define COLOR_PAIR_ARROW_RIGHT   8   // right arrow transition

// Initialize ncurses and colors
void render_init(void);

// Cleanup ncurses
void render_cleanup(void);

// Initialize color pairs
void render_init_colors(void);

// Render the main editor window
void render_main_window(WINDOW *main_win, Buffer *buf, 
                        int maxy, int maxx,
                        size_t scroll_y, size_t cursor_line, size_t cursor_col,
                        int gutter_width,
                        const char *title, const char *filename, int have_filename,
                        int modified, int mode_insert, int line_number_relative);

// Render the command window
void render_command_window(WINDOW *cmd_win, Buffer *buf,
                          int maxx, int mode_insert,
                          const char *cmdbuf, size_t cursor_line,
                          int pending_create_prompt, const char *filename);

// Compute cursor screen position
void compute_cursor_position(Buffer *buf, size_t cursor_line, size_t cursor_col,
                            int col_offset, int maxx, int visible_rows,
                            size_t *scroll_y, int *cy, int *cx);

// Compute gutter width based on line count
int compute_gutter_width(size_t line_count);

#endif
