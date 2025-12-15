// render.c - ncurses rendering functions
#include "render.h"
#include "highlight.h"
#include <string.h>
#include <time.h>

void render_init(void) {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(1);
    start_color();
    use_default_colors();
    set_escdelay(100);
    timeout(200); // update screen
}

void render_cleanup(void) {
    endwin();
}

void render_init_colors(void) {
    init_pair(COLOR_PAIR_TEXT, -1, -1);
    init_pair(COLOR_PAIR_GUTTER, 8, -1);                // gutter (gray on black)
    init_pair(COLOR_PAIR_STATUS, COLOR_BLACK, 7); // status bar
    init_pair(COLOR_PAIR_ERROR, 196, -1);               // errors
    init_pair(COLOR_PAIR_WARNING, 226, -1);             // warnings
    init_pair(SY_KEYWORD,  COLOR_BLUE,    -1);          // keywords 
    init_pair(SY_TYPE,     COLOR_CYAN,    -1);          // types
    init_pair(SY_FUNCTION, COLOR_YELLOW,  -1);          // functions
    init_pair(SY_STRING,   127,   -1);                  // strings
    init_pair(SY_NUMBER,   14, -1);                    // numbers
    init_pair(SY_COMMENT,  34,   -1);                   // comments
    init_pair(SY_OPERATOR,  COLOR_WHITE,  -1);   // operators stand out, neutral
    init_pair(SY_MACRO,     COLOR_MAGENTA,-1);   // annotations
    init_pair(SY_CLASS,     COLOR_GREEN,  -1);   // class / enum names
    init_pair(SY_ENUM,      COLOR_GREEN,  -1);   // same as class
    init_pair(SY_NAMESPACE, 66,            -1);  // packages (muted blue)
    init_pair(SY_VARIABLE, COLOR_WHITE,   -1);   // default identifiers
    init_pair(SY_PARAMETER, 180,           -1);  // subtle yellow
    init_pair(SY_PROPERTY,  110,           -1);  // muted cyan
}

int compute_gutter_width(size_t line_count) {
    int gutter_width = 1;
    for (size_t n = line_count; n >= 10; n /= 10) gutter_width++;
    return gutter_width;
}

void render_main_window(WINDOW *main_win, Buffer *buf, 
                        int maxy, int maxx,
                        size_t scroll_y, size_t cursor_line, size_t cursor_col,
                        int gutter_width,
                        const char *title, const char *filename, int have_filename,
                        int modified, int mode_insert, int line_number_relative) {
    (void)cursor_col;
    
    werase(main_win);
    
    // draw border on main window only
    box(main_win, 0, 0);
    // clear left edge for gutter 
    mvwaddch(main_win, 0, 0, ' ');
    for (int i = 1; i < maxy - 2; i++) mvwaddch(main_win, i, 0, ' ');
    mvwaddch(main_win, maxy - 2, 0, ' ');

    wattron(main_win, COLOR_PAIR(COLOR_PAIR_TEXT));
    mvwprintw(main_win, 0, 2, "%s", title);
    wattroff(main_win, COLOR_PAIR(COLOR_PAIR_TEXT));

    int col_offset = gutter_width + 2;

    // render file starting from scroll_y logical line
    int row = 1;
    for (size_t lineno = scroll_y; lineno < buf->count && row < maxy - 2; lineno++) {
        // Check for diagnostics on this line
        int diag_severity = 0;
        for (size_t d = 0; d < buf->diag_count; d++) {
            if (buf->diagnostics[d].line == (int)lineno) {
                diag_severity = buf->diagnostics[d].severity;
                break;
            }
        }

        // Calculate line number to display (absolute or relative)
        size_t display_num;
        if (line_number_relative) {
            if (lineno == cursor_line) {
                display_num = lineno + 1;  // current line shows absolute number
            } else if (lineno > cursor_line) {
                display_num = lineno - cursor_line;
            } else {
                display_num = cursor_line - lineno;
            }
        } else {
            display_num = lineno + 1;
        }

        // highlight line number when diagnostics exist on this line
        if (diag_severity == 1) {
            wattron(main_win, COLOR_PAIR(COLOR_PAIR_ERROR));
            mvwprintw(main_win, row, 1, "%*zu", gutter_width, display_num);
            wattroff(main_win, COLOR_PAIR(COLOR_PAIR_ERROR));
        } else if (diag_severity == 2) {
            wattron(main_win, COLOR_PAIR(COLOR_PAIR_WARNING));
            mvwprintw(main_win, row, 1, "%*zu", gutter_width, display_num);
            wattroff(main_win, COLOR_PAIR(COLOR_PAIR_WARNING));
        } else {
            wattron(main_win, COLOR_PAIR(COLOR_PAIR_GUTTER));
            mvwprintw(main_win, row, 1, "%*zu", gutter_width, display_num);
            wattroff(main_win, COLOR_PAIR(COLOR_PAIR_GUTTER));
        }

        int col = col_offset;
        const char *p = buf->lines[lineno];
        size_t ip = 0;
        while (p[ip] && row < maxy - 2) {
            if (col >= maxx - 1) {
                // wrap
                row++;
                if (row >= maxy - 2) break;
                if (diag_severity == 1) {
                    wattron(main_win, COLOR_PAIR(COLOR_PAIR_ERROR));
                    mvwprintw(main_win, row, 1, "%*zu", gutter_width, display_num);
                    wattroff(main_win, COLOR_PAIR(COLOR_PAIR_ERROR));
                } else if (diag_severity == 2) {
                    wattron(main_win, COLOR_PAIR(COLOR_PAIR_WARNING));
                    mvwprintw(main_win, row, 1, "%*zu", gutter_width, display_num);
                    wattroff(main_win, COLOR_PAIR(COLOR_PAIR_WARNING));
                } else {
                    wattron(main_win, COLOR_PAIR(COLOR_PAIR_GUTTER));
                    mvwprintw(main_win, row, 1, "%*zu", gutter_width, display_num);
                    wattroff(main_win, COLOR_PAIR(COLOR_PAIR_GUTTER));
                }
                col = col_offset;
            }
            if (row >= maxy - 2) break;
            SemanticKind sk = semantic_kind_at(buf, (int)lineno, (int)ip);
            int sy = color_for_semantic_kind(sk);
            if (sy)
                wattron(main_win, COLOR_PAIR(sy));
            mvwaddch(main_win, row, col++, p[ip++]);
            if (sy)
                wattroff(main_win, COLOR_PAIR(sy));
        }
        row++;
    }

    // status bar background -> now at maxy-2
    wattron(main_win, COLOR_PAIR(COLOR_PAIR_STATUS));
    for (int i = 1; i < maxx - 1; i++) mvwaddch(main_win, maxy - 2, i, ' ');
    wattroff(main_win, COLOR_PAIR(COLOR_PAIR_STATUS));

    // status content: filename (truncated), mode, modified, clock
    wattron(main_win, COLOR_PAIR(COLOR_PAIR_STATUS));
    char status_left[256];
    if (have_filename) {
        char fname_short[240];
        strncpy(fname_short, filename, sizeof(fname_short) - 1);
        fname_short[sizeof(fname_short) - 1] = '\0';
        const char *mod_suffix = modified ? " [+]" : "";
        snprintf(status_left, sizeof(status_left), "%s%s", fname_short, mod_suffix);
    } else {
        const char *mod_suffix = modified ? " [+]" : "";
        snprintf(status_left, sizeof(status_left), "[No Name]%s", mod_suffix);
    }
    const char *mode_str = mode_insert ? "-- INSERT --" : "-- COMMAND --";
    mvwprintw(main_win, maxy - 2, 2, "%s %s", status_left, mode_str);

    // clock
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char tbuf[6];
    strftime(tbuf, sizeof(tbuf), "%H:%M", tm_info);
    mvwprintw(main_win, maxy - 2, maxx - 1 - (int)strlen(tbuf) - 1, "%s", tbuf);
    wattroff(main_win, COLOR_PAIR(COLOR_PAIR_STATUS));
}

void render_command_window(WINDOW *cmd_win, Buffer *buf,
                          int maxx, int mode_insert,
                          const char *cmdbuf, size_t cursor_line,
                          int pending_create_prompt, const char *filename) {
    werase(cmd_win);
    
    // command row background
    wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_TEXT));
    for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
    wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_TEXT));

    // Show create prompt if pending
    if (pending_create_prompt) {
        wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_TEXT));
        mvwprintw(cmd_win, 0, 1, "Create %s? (Y/n): ", filename);
        wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_TEXT));
        return;
    }

    if (!mode_insert) {
        wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_TEXT));
        mvwprintw(cmd_win, 0, 1, ":%s", cmdbuf);
        wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_TEXT));
    } else {
        // INSERT MODE: automatically show diagnostic for current line
        for (size_t d = 0; d < buf->diag_count; d++) {
            if (buf->diagnostics[d].line == (int)cursor_line) {
                const char *stype =
                    (buf->diagnostics[d].severity == 1 ? "error" :
                     buf->diagnostics[d].severity == 2 ? "warning" : "info");
                    
                wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_TEXT));
                mvwprintw(cmd_win, 0, 1, "[%s] %s", stype, buf->diagnostics[d].msg);
                wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_TEXT));
                break;  // show first diag on that line
            }
        }
    }
}

void compute_cursor_position(Buffer *buf, size_t cursor_line, size_t cursor_col,
                            int col_offset, int maxx, int visible_rows,
                            size_t *scroll_y, int *cy, int *cx) {
    // Compute cursor screen position: need to compute wrapped rows up to cursor_line and cursor_col
    int rows_before_cursor = 0;
    int cursor_screen_col = col_offset;
    
    for (size_t ln = 0; ln < cursor_line; ln++) {
        const char *s = buf->lines[ln];
        int col = col_offset;
        size_t i = 0;
        while (s[i]) {
            if (col >= maxx - 1) { rows_before_cursor++; col = col_offset; }
            col++; i++;
        }
        rows_before_cursor++; // last partial row of the line
    }
    
    // now on cursor_line
    {
        const char *s = buf->lines[cursor_line];
        int col = col_offset;
        size_t i = 0;
        while (s[i] && i < cursor_col) {
            if (col >= maxx - 1) { rows_before_cursor++; col = col_offset; }
            col++; i++;
        }
        cursor_screen_col = col;
    }

    *cy = rows_before_cursor - (int)*scroll_y + 1;
    
    if (*cy < 1) {
        // scroll up so cursor becomes the first visible row
        int accum = 0;
        size_t new_scroll = 0;
        for (size_t ln = 0; ln < buf->count; ln++) {
            const char *s = buf->lines[ln];
            int col = col_offset;
            size_t i = 0;
            while (s[i]) {
                if (col >= maxx - 1) { accum++; col = col_offset; }
                col++; i++;
            }
            accum++;
            if (accum > rows_before_cursor) break;
            new_scroll = ln + 1;
        }
        *scroll_y = new_scroll;
        *cy = 1;
    } else if (*cy > visible_rows) {
        // scroll down so cursor becomes the last visible row
        int target = rows_before_cursor - visible_rows + 1;
        int accum = 0;
        size_t new_scroll = 0;
        for (size_t ln = 0; ln < buf->count; ln++) {
            const char *s = buf->lines[ln];
            int col = col_offset;
            size_t i = 0;
            while (s[i]) {
                if (col >= maxx - 1) { accum++; col = col_offset; }
                col++; i++;
            }
            accum++;
            if (accum >= target) { new_scroll = ln; break; }
            new_scroll = ln + 1;
        }
        *scroll_y = new_scroll;
        *cy = visible_rows;
    }

    *cx = cursor_screen_col;
    if (*cx < col_offset) *cx = col_offset;
    if (*cx > maxx - 2) *cx = maxx - 2;
    
    int gutter_width = compute_gutter_width(buf->count);
    if (*cx < gutter_width + 2) *cx = gutter_width + 2;
}
