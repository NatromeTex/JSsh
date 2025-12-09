#include <ncurses.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(1);
    start_color();
    use_default_colors();
    set_escdelay(100);
    timeout(1000); // update clock every second 

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_BLACK, 248);   // status bar 
    init_pair(3, 8, COLOR_BLACK);     // gutter (gray on black) 

    char *filebuf = NULL;
    size_t filelen = 0;

    const char *filename = (argc > 1 ? argv[1] : "");

    if (argc > 1) {
        FILE *fp = fopen(argv[1], "r");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            filelen = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            filebuf = malloc(filelen + 1);
            if (filebuf) {
                fread(filebuf, 1, filelen, fp);
                filebuf[filelen] = '\0';
            }
            fclose(fp);
        }
    }

    // count logical lines in the loaded buffer 
    size_t line_count = 1;
    if (filebuf && filelen > 0) {
        for (size_t i = 0; i < filelen; i++)
            if (filebuf[i] == '\n')
                line_count++;
    }

    // compute gutter width from number of logical lines 
    int gutter_width = 1;
    for (size_t n = line_count; n >= 10; n /= 10)
        gutter_width++;

    // logical cursor and scroll positions (0-based) 
    size_t scroll_y = 0;       // first logical line displayed 
    size_t cursor_line = 0;    // current logical line 
    size_t cursor_col = 0;     // column within logical line 

    int ch;
    int maxy, maxx;
    int cx = gutter_width + 2; // screen X position for cursor 
    const char *title = "JSVIM";

    while (1) {
        getmaxyx(stdscr, maxy, maxx);
        erase();

        // draw outer frame but leave left edge clear for gutter 
        box(stdscr, 0, 0);
        mvaddch(0, 0, ' ');
        for (int i = 1; i < maxy - 1; i++)
            mvaddch(i, 0, ' ');
        mvaddch(maxy - 1, 0, ' ');

        attron(COLOR_PAIR(1));
        mvprintw(0, 2, "%s", title);
        attroff(COLOR_PAIR(1));

        int col_offset = gutter_width + 2;
        int text_width = (maxx - 1) - col_offset; // usable columns for text 
        if (text_width < 1) text_width = 1;

        // render file starting from scroll_y logical line 
        if (filebuf) {
            const char *p = filebuf;
            size_t lineno = 0;

            // skip to first visible logical line 
            while (*p && lineno < scroll_y) {
                if (*p == '\n') lineno++;
                p++;
            }

            lineno++; // display numbers are 1-based
            int row = 1;

            while (*p && row < maxy - 1) {
                // print logical line number 
                attron(COLOR_PAIR(3));
                mvprintw(row, 1, "%*zu", gutter_width, lineno);
                attroff(COLOR_PAIR(3));

                int col = col_offset;

                // print characters until newline or wrap 
                while (*p && *p != '\n') {
                    if (col >= maxx - 1) {
                        // wrapped screen row 
                        row++;
                        if (row >= maxy - 1) break;

                        // blank gutter on wrapped rows
                        attron(COLOR_PAIR(3));
                        mvprintw(row, 1, "%*s", gutter_width, "");
                        attroff(COLOR_PAIR(3));

                        col = col_offset;
                    }

                    if (row >= maxy - 1) break;

                    mvaddch(row, col++, *p++);
                }

                // consume newline and advance logical line 
                if (*p == '\n') {
                    p++;
                    lineno++;
                }

                row++;
            }
        }

        // status bar background 
        attron(COLOR_PAIR(2));
        for (int i = 1; i < maxx - 1; i++)
            mvaddch(maxy - 1, i, ' ');
        attroff(COLOR_PAIR(2));

        // filename in status bar 
        attron(COLOR_PAIR(2));
        mvprintw(maxy - 1, 2, "%s", filename);
        attroff(COLOR_PAIR(2));

        // clock in status bar (HH:MM) 
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char tbuf[6];
        strftime(tbuf, sizeof(tbuf), "%H:%M", tm_info);
        attron(COLOR_PAIR(2));
        mvprintw(maxy - 1, maxx - 1 - (int)strlen(tbuf) - 1, "%s", tbuf);
        attroff(COLOR_PAIR(2));

        /*
         * Compute screen-row indices (absolute from top of file) for:
         *   - scroll_y (first visible logical line)
         *   - cursor position (logical line and column)
         */
        // rows_before_scroll: rows consumed by logical lines [0 .. scroll_y-1] 
        int rows_before_scroll = 0;
        if (filebuf) {
            const char *q = filebuf;
            size_t ln = 0;
            while (*q && ln < scroll_y) {
                // count this logical line's screen rows 
                int col = col_offset;
                while (*q && *q != '\n') {
                    if (col >= maxx - 1) {
                        rows_before_scroll++;
                        col = col_offset;
                    }
                    col++;
                    q++;
                }
                // account for the last partial row of this logical line 
                rows_before_scroll++;
                if (*q == '\n') q++;
                ln++;
            }
        }

        /*
         * rows_before_cursor: rows consumed by logical lines [0 .. cursor_line-1]
         * plus wrapped rows within cursor_line up to cursor_col.
         */
        int rows_before_cursor = 0;
        int cursor_screen_col = col_offset; // final cursor X position 
        if (filebuf) {
            const char *q = filebuf;
            size_t ln = 0;

            // advance to cursor_line 
            while (*q && ln < cursor_line) {
                int col = col_offset;
                while (*q && *q != '\n') {
                    if (col >= maxx - 1) {
                        rows_before_cursor++;
                        col = col_offset;
                    }
                    col++;
                    q++;
                }
                rows_before_cursor++;
                if (*q == '\n') q++;
                ln++;
            }

            // q now points to the start of cursor_line 
            int col = col_offset;
            size_t cc = 0;
            while (*q && *q != '\n' && cc < cursor_col) {
                if (col >= maxx - 1) {
                    rows_before_cursor++;
                    col = col_offset;
                }
                col++;
                q++;
                cc++;
            }
            // cursor is on current (possibly wrapped) screen row 
            cursor_screen_col = col;
        } else {
            rows_before_cursor = 0;
            cursor_screen_col = col_offset;
        }

        // visible cursor row (1-based inside the text area) 
        int cy = rows_before_cursor - rows_before_scroll + 1;

        // adjust scroll_y to keep cursor within the visible text area 
        int visible_rows = maxy - 2;
        if (cy < 1) {
            // scroll up so cursor becomes the first visible row 
            size_t new_scroll = 0;
            int accum = 0;
            if (filebuf) {
                const char *q = filebuf;
                size_t ln = 0;
                while (*q) {
                    // compute rows for this logical line 
                    int col = col_offset;
                    const char *r = q;
                    while (*r && *r != '\n') {
                        if (col >= maxx - 1) {
                            accum++;
                            col = col_offset;
                        }
                        col++;
                        r++;
                    }
                    accum++;
                    if (accum > rows_before_cursor) break;
                    // advance q to next logical line 
                    while (*q && *q != '\n') q++;
                    if (*q == '\n') q++;
                    ln++;
                    new_scroll = ln;
                }
            }
            scroll_y = new_scroll;
            cy = 1;
        } else if (cy > visible_rows) {
            // scroll down so cursor becomes the last visible row 
            int target_rows_before_scroll = rows_before_cursor - visible_rows + 1;
            // compute scroll_y that yields target_rows_before_scroll 
            size_t new_scroll = 0;
            int accum = 0;
            if (filebuf) {
                const char *q = filebuf;
                size_t ln = 0;
                while (*q) {
                    // compute rows for this logical line 
                    int col = col_offset;
                    const char *r = q;
                    while (*r && *r != '\n') {
                        if (col >= maxx - 1) {
                            accum++;
                            col = col_offset;
                        }
                        col++;
                        r++;
                    }
                    accum++;
                    if (accum >= target_rows_before_scroll) break;
                    // advance to next logical line 
                    while (*q && *q != '\n') q++;
                    if (*q == '\n') q++;
                    ln++;
                    new_scroll = ln;
                }
            }
            scroll_y = new_scroll;
            cy = visible_rows;
        }

        // compute screen X (cx) from cursor_screen_col and clamp 
        cx = cursor_screen_col;
        if (cx < col_offset) cx = col_offset;
        if (cx > maxx - 2) cx = maxx - 2;

        // keep cursor out of the gutter 
        if (cx < gutter_width + 2) cx = gutter_width + 2;

        move(cy, cx);
        refresh();

        ch = getch();
        if (ch == 27) break;

        switch (ch) {
            case KEY_UP:
                if (cursor_line > 0) {
                    cursor_line--;
                    // clamp cursor_col to new line length 
                    size_t len = 0;
                    if (filebuf) {
                        const char *q = filebuf;
                        size_t ln = 0;
                        // find start of cursor_line 
                        while (*q && ln < cursor_line) {
                            if (*q == '\n') ln++;
                            q++;
                        }
                        // count chars until newline 
                        while (*q && *q != '\n') {
                            len++; q++;
                        }
                    }
                    if (cursor_col > len) cursor_col = len;
                }
                // scroll if needed (enforced on next render) 
                if (cursor_line < scroll_y) scroll_y = cursor_line;
                break;

            case KEY_DOWN:
                if (cursor_line < line_count - 1) {
                    cursor_line++;
                    // clamp cursor_col to new line length 
                    size_t len = 0;
                    if (filebuf) {
                        const char *q = filebuf;
                        size_t ln = 0;
                        while (*q && ln < cursor_line) {
                            if (*q == '\n') ln++;
                            q++;
                        }
                        while (*q && *q != '\n') {
                            len++; q++;
                        }
                    }
                    if (cursor_col > len) cursor_col = len;
                }
                if (cursor_line >= scroll_y + visible_rows) {
                    // scroll down to keep cursor visible 
                    if (cursor_line >= visible_rows)
                        scroll_y = cursor_line - (visible_rows - 1);
                    else
                        scroll_y = 0;
                }
                break;

            case KEY_LEFT:
                if (cursor_col > 0) {
                    cursor_col--;
                } else {
                    // move to end of previous line if possible 
                    if (cursor_line > 0) {
                        cursor_line--;
                        // compute length of new line 
                        size_t len = 0;
                        if (filebuf) {
                            const char *q = filebuf;
                            size_t ln = 0;
                            while (*q && ln < cursor_line) {
                                if (*q == '\n') ln++;
                                q++;
                            }
                            while (*q && *q != '\n') {
                                len++; q++;
                            }
                        }
                        cursor_col = len;
                        if (cursor_line < scroll_y) scroll_y = cursor_line;
                    }
                }
                break;

            case KEY_RIGHT:
                {
                    // compute length of current logical line 
                    size_t len = 0;
                    if (filebuf) {
                        const char *q = filebuf;
                        size_t ln = 0;
                        while (*q && ln < cursor_line) {
                            if (*q == '\n') ln++;
                            q++;
                        }
                        while (*q && *q != '\n') {
                            len++; q++;
                        }
                    }
                    if (cursor_col < len) {
                        cursor_col++;
                    } else {
                        // move to start of next line if it exists 
                        if (cursor_line < line_count - 1) {
                            cursor_line++;
                            cursor_col = 0;
                            if (cursor_line >= scroll_y + visible_rows) {
                                if (cursor_line >= visible_rows)
                                    scroll_y = cursor_line - (visible_rows - 1);
                                else
                                    scroll_y = 0;
                            }
                        }
                    }
                }
                break;

            case KEY_RESIZE:
                // no special handling; next render adapts 
                break;

            default:
                // no-op for other keys (placeholder for inserts) 
                break;
        }
    }

    if (filebuf) free(filebuf);

    endwin();
    return 0;
}
