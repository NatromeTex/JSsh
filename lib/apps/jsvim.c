// jsvim.c
#include <ncurses.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#define INITIAL_CAP 128

// Simple dynamic array of lines
typedef struct {
    char **lines;
    size_t count;
    size_t cap;
} Buffer;

static void buf_init(Buffer *b) {
    b->cap = 16;
    b->count = 0;
    b->lines = malloc(sizeof(char*) * b->cap);
}

static void buf_free(Buffer *b) {
    for (size_t i = 0; i < b->count; i++) free(b->lines[i]);
    free(b->lines);
    b->lines = NULL;
    b->count = b->cap = 0;
}

static void buf_ensure(Buffer *b, size_t need) {
    if (need <= b->cap) return;
    while (b->cap < need) b->cap *= 2;
    b->lines = realloc(b->lines, sizeof(char*) * b->cap);
}

static void buf_push(Buffer *b, char *line) {
    buf_ensure(b, b->count + 1);
    b->lines[b->count++] = line;
}

static void buf_insert(Buffer *b, size_t idx, char *line) {
    if (idx > b->count) idx = b->count;
    buf_ensure(b, b->count + 1);
    for (size_t i = b->count; i > idx; --i) b->lines[i] = b->lines[i-1];
    b->lines[idx] = line;
    b->count++;
}

static char *dupstr(const char *s) {
    size_t n = strlen(s);
    char *p = malloc(n+1);
    memcpy(p, s, n+1);
    return p;
}

// Load file into buffer; returns 0 if success, 1 if file missing (but sets filename)
static int load_file(Buffer *b, const char *fname) {
    FILE *fp = fopen(fname, "r");
    if (!fp) return 1;
    char *line = NULL;
    size_t lncap = 0;
    ssize_t lnlen;
    while ((lnlen = getline(&line, &lncap, fp)) != -1) {
        // strip trailing newline
        if (lnlen > 0 && line[lnlen-1] == '\n') line[--lnlen] = '\0';
        buf_push(b, dupstr(line));
    }
    free(line);
    fclose(fp);
    // Ensure at least one line
    if (b->count == 0) buf_push(b, dupstr(""));
    return 0;
}

static int file_exists(const char *fname) {
    struct stat st;
    return stat(fname, &st) == 0;
}

// Write buffer to file; returns 0 on success, nonzero on error
static int save_file(Buffer *b, const char *fname) {
    FILE *fp = fopen(fname, "w");
    if (!fp) return 1;
    for (size_t i = 0; i < b->count; i++) {
        fputs(b->lines[i], fp);
        if (i + 1 < b->count) fputc('\n', fp);
    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(1);
    start_color();
    use_default_colors();
    set_escdelay(100);
    timeout(200); // update screen

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_BLACK, 248);   // status bar 
    init_pair(3, 8, COLOR_BLACK);     // gutter (gray on black) 

    Buffer buf;
    buf_init(&buf);

    char filename[1024] = "";
    int have_filename = 0;
    int existing_file = 0;
    if (argc > 1) {
        strncpy(filename, argv[1], sizeof(filename)-1);
        have_filename = 1;
        existing_file = !load_file(&buf, filename);
    } else {
        // start with an empty buffer
        buf_push(&buf, dupstr(""));
    }

    // if no filename at startup, prompt user before main loop
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    if (!have_filename) {
        // Prompt in the command row (last row)
        const char *prompt = "Enter filename: ";
        echo();
        curs_set(1);
        // draw minimal prompt
        attron(COLOR_PAIR(2));
        for (int i = 0; i < maxx; i++) mvaddch(maxy - 1, i, ' ');
        mvprintw(maxy - 1, 0, "%s", prompt);
        attroff(COLOR_PAIR(2));
        refresh();

        char fnamebuf[1024];
        memset(fnamebuf, 0, sizeof(fnamebuf));
        mvgetnstr(maxy - 1, (int)strlen(prompt), fnamebuf, sizeof(fnamebuf)-1);

        noecho();
        if (strlen(fnamebuf) > 0) {
            strncpy(filename, fnamebuf, sizeof(filename)-1);
            have_filename = 1;
            existing_file = !load_file(&buf, filename);
            if (!existing_file) {
                // file didn't exist; start with empty buffer but filename set
                buf_free(&buf);
                buf_init(&buf);
                buf_push(&buf, dupstr(""));
            }
        } else {
            // leave unnamed; user can :w with filename later
        }
    }

    // Create two windows: main window (includes status bar) and command window
    WINDOW *main_win = NULL;
    WINDOW *cmd_win = NULL;

    // editor state
    size_t cursor_line = 0, cursor_col = 0;
    size_t scroll_y = 0;
    int gutter_width = 1;
    int ch;
    const char *title = "JSVIM";
    int mode_insert = 1; // 1 = insert, 0 = command
    int modified = 0;

    // command-line buffer (displayed in last row when in command mode)
    char cmdbuf[1024];
    size_t cmdlen = 0;
    int quit = 0;
    int force_quit = 0;

    while (!quit) {
        getmaxyx(stdscr, maxy, maxx);
        
        // Delete old windows if they exist
        if (main_win) delwin(main_win);
        if (cmd_win) delwin(cmd_win);
        
        // Create main window (everything except last row)
        main_win = newwin(maxy - 1, maxx, 0, 0);
        keypad(main_win, TRUE);
        
        // Create command window (last row only)
        cmd_win = newwin(1, maxx, maxy - 1, 0);
        keypad(cmd_win, TRUE);
        
        werase(main_win);
        werase(cmd_win);

        // recompute gutter width from number of logical lines
        size_t line_count = buf.count;
        gutter_width = 1;
        for (size_t n = line_count; n >= 10; n /= 10) gutter_width++;

        // draw border on main window only
        box(main_win, 0, 0);
        // clear left edge for gutter 
        mvwaddch(main_win, 0, 0, ' ');
        for (int i = 1; i < maxy - 2; i++) mvwaddch(main_win, i, 0, ' ');
        mvwaddch(main_win, maxy - 2, 0, ' ');

        wattron(main_win, COLOR_PAIR(1));
        mvwprintw(main_win, 0, 2, "%s", title);
        wattroff(main_win, COLOR_PAIR(1));

        int col_offset = gutter_width + 2;
        int text_width = (maxx - 1) - col_offset;
        if (text_width < 1) text_width = 1;

        // render file starting from scroll_y logical line
        int row = 1;
        for (size_t lineno = scroll_y; lineno < buf.count && row < maxy - 2; lineno++) {
            // print logical line number 
            wattron(main_win, COLOR_PAIR(3));
            mvwprintw(main_win, row, 1, "%*zu", gutter_width, lineno + 1);
            wattroff(main_win, COLOR_PAIR(3));

            int col = col_offset;
            const char *p = buf.lines[lineno];
            size_t ip = 0;
            while (p[ip] && row < maxy - 2) {
                if (col >= maxx - 1) {
                    // wrap
                    row++;
                    if (row >= maxy - 2) break;
                    wattron(main_win, COLOR_PAIR(3));
                    mvwprintw(main_win, row, 1, "%*s", gutter_width, "");
                    wattroff(main_win, COLOR_PAIR(3));
                    col = col_offset;
                }
                if (row >= maxy - 2) break;
                mvwaddch(main_win, row, col++, p[ip++]);
            }
            row++;
        }

        // status bar background -> now at maxy-2
        wattron(main_win, COLOR_PAIR(2));
        for (int i = 1; i < maxx - 1; i++) mvwaddch(main_win, maxy - 2, i, ' ');
        wattroff(main_win, COLOR_PAIR(2));

        // status content: filename (truncated), mode, modified, clock
        wattron(main_win, COLOR_PAIR(2));
        char status_left[256];
        if (have_filename) {
            // Copy at most 239 chars of filename; leaves space for " [+]".
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
        wattroff(main_win, COLOR_PAIR(2));

        // command row in command window
        wattron(cmd_win, COLOR_PAIR(1));
        for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
        wattroff(cmd_win, COLOR_PAIR(1));
        if (!mode_insert) {
            // show typed command
            wattron(cmd_win, COLOR_PAIR(1));
            mvwprintw(cmd_win, 0, 1, ":%s", cmdbuf);
            wattroff(cmd_win, COLOR_PAIR(1));
        }

        // Compute cursor screen position (approx): need to compute wrapped rows up to cursor_line and cursor_col
        int rows_before_cursor = 0;
        int cursor_screen_col = col_offset;
        for (size_t ln = 0; ln < cursor_line; ln++) {
            const char *s = buf.lines[ln];
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
            const char *s = buf.lines[cursor_line];
            int col = col_offset;
            size_t i = 0;
            while (s[i] && i < cursor_col) {
                if (col >= maxx - 1) { rows_before_cursor++; col = col_offset; }
                col++; i++;
            }
            cursor_screen_col = col;
        }

        int cy = rows_before_cursor - (int)scroll_y + 1;
        int visible_rows = maxy - 3; // text area rows
        if (cy < 1) {
            // scroll up so cursor becomes the first visible row
            int accum = 0;
            size_t new_scroll = 0;
            for (size_t ln = 0; ln < buf.count; ln++) {
                const char *s = buf.lines[ln];
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
            scroll_y = new_scroll;
            cy = 1;
        } else if (cy > visible_rows) {
            // scroll down so cursor becomes the last visible row
            int target = rows_before_cursor - visible_rows + 1;
            int accum = 0;
            size_t new_scroll = 0;
            for (size_t ln = 0; ln < buf.count; ln++) {
                const char *s = buf.lines[ln];
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
            scroll_y = new_scroll;
            cy = visible_rows;
        }

        int cx = cursor_screen_col;
        if (cx < col_offset) cx = col_offset;
        if (cx > maxx - 2) cx = maxx - 2;
        if (cx < gutter_width + 2) cx = gutter_width + 2;

        // if in insert mode, place cursor in text area; if in command mode, place cursor in command window
        if (mode_insert) {
            // clamp cy to visible text area bounds
            if (cy < 1) cy = 1;
            if (cy > visible_rows) cy = visible_rows;
            wmove(main_win, cy, cx);
            wrefresh(cmd_win);
            wrefresh(main_win);
            // get input from main window so cursor appears there
            ch = wgetch(main_win);
        } else {
            // place cursor in command window
            wmove(cmd_win, 0, (int)cmdlen + 2);
            wrefresh(main_win);
            wrefresh(cmd_win);
            // get input from command window so cursor appears there
            ch = wgetch(cmd_win);
        }
        if (mode_insert) {
            if (ch == 27) {
                // ESC -> command mode
                mode_insert = 0;
                cmdlen = 0;
                cmdbuf[0] = '\0';
                continue;
            }
            switch (ch) {
                case ERR:
                    // timeout for clock update; continue
                    break;
                case KEY_UP:
                    if (cursor_line > 0) {
                        cursor_line--;
                        // clamp cursor_col
                        size_t len = strlen(buf.lines[cursor_line]);
                        if (cursor_col > len) cursor_col = len;
                        if (cursor_line < scroll_y) scroll_y = cursor_line;
                    }
                    break;
                case KEY_DOWN:
                    if (cursor_line + 1 < buf.count) {
                        cursor_line++;
                        size_t len = strlen(buf.lines[cursor_line]);
                        if (cursor_col > len) cursor_col = len;
                        if (cursor_line >= scroll_y + visible_rows) {
                            if (cursor_line >= visible_rows)
                                scroll_y = cursor_line - (visible_rows - 1);
                            else
                                scroll_y = 0;
                        }
                    }
                    break;
                case KEY_LEFT:
                    if (cursor_col > 0) cursor_col--;
                    else if (cursor_line > 0) {
                        cursor_line--;
                        cursor_col = strlen(buf.lines[cursor_line]);
                        if (cursor_line < scroll_y) scroll_y = cursor_line;
                    }
                    break;
                case KEY_RIGHT:
                    {
                        size_t len = strlen(buf.lines[cursor_line]);
                        if (cursor_col < len) cursor_col++;
                        else if (cursor_line + 1 < buf.count) {
                            cursor_line++;
                            cursor_col = 0;
                            if (cursor_line >= scroll_y + visible_rows) {
                                if (cursor_line >= visible_rows)
                                    scroll_y = cursor_line - (visible_rows - 1);
                                else scroll_y = 0;
                            }
                        }
                    }
                    break;
                case KEY_BACKSPACE:
                case 127:
                case '\b':
                    if (cursor_col > 0) {
                        // delete previous character
                        char *line = buf.lines[cursor_line];
                        size_t len = strlen(line);
                        memmove(line + cursor_col - 1, line + cursor_col, len - cursor_col + 1);
                        cursor_col--;
                        modified = 1;
                    } else if (cursor_line > 0) {
                        // join with previous line
                        size_t prevlen = strlen(buf.lines[cursor_line - 1]);
                        size_t curlen = strlen(buf.lines[cursor_line]);
                        buf.lines[cursor_line - 1] = realloc(buf.lines[cursor_line - 1], prevlen + curlen + 1);
                        memcpy(buf.lines[cursor_line - 1] + prevlen, buf.lines[cursor_line], curlen + 1);
                        free(buf.lines[cursor_line]);
                        // shift lines up
                        for (size_t i = cursor_line; i + 1 < buf.count; i++) buf.lines[i] = buf.lines[i+1];
                        buf.count--;
                        cursor_line--;
                        cursor_col = prevlen;
                        modified = 1;
                    }
                    break;
                case '\n':
                case '\r':
                    {
                        // split line at cursor
                        char *line = buf.lines[cursor_line];
                        size_t len = strlen(line);
                        char *newline = dupstr(line + cursor_col);
                        line[cursor_col] = '\0';
                        // insert new line after current
                        buf_insert(&buf, cursor_line + 1, newline);
                        cursor_line++;
                        cursor_col = 0;
                        modified = 1;
                    }
                    break;
                default:
                    if (ch >= 32 && ch <= 126) {
                        // printable
                        char *line = buf.lines[cursor_line];
                        size_t len = strlen(line);
                        line = realloc(line, len + 2); // +1 char + NUL
                        // shift tail
                        memmove(line + cursor_col + 1, line + cursor_col, len - cursor_col + 1);
                        line[cursor_col] = (char)ch;
                        buf.lines[cursor_line] = line;
                        cursor_col++;
                        modified = 1;
                    }
                    break;
            }
        } else {
            // Command mode: capture characters into cmdbuf; show on last row. Backspace, Enter triggers command.
            if (ch == ERR) {
                // refresh/clock; continue
            } else if (ch == 27) {
                // ESC in command mode -> back to insert
                mode_insert = 1;
            } else if (ch == '\n' || ch == '\r') {
                // execute command in cmdbuf
                if (cmdlen > 0) {
                    // common commands: q, q!, w, wq, x
                    if (cmdbuf[0] == 'q' && cmdbuf[1] == '\0') {
                        quit = 1;
                    } else if (strcmp(cmdbuf, "q!") == 0) {
                        // force quit (ignore modified)
                        force_quit = 1;
                        quit = 1;
                    } else if (strcmp(cmdbuf, "w") == 0) {
                        if (!have_filename || strlen(filename) == 0) {
                            // prompt for filename in command window
                            const char *pr = "Enter filename: ";
                            echo();
                            curs_set(1);
                            wattron(cmd_win, COLOR_PAIR(2));
                            for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                            mvwprintw(cmd_win, 0, 1, "%s", pr);
                            wattroff(cmd_win, COLOR_PAIR(2));
                            wrefresh(cmd_win);
                            char fnamebuf[1024];
                            memset(fnamebuf,0,sizeof(fnamebuf));
                            mvwgetnstr(cmd_win, 0, 1 + (int)strlen(pr), fnamebuf, sizeof(fnamebuf)-1);
                            noecho();
                            if (strlen(fnamebuf) > 0) {
                                strncpy(filename, fnamebuf, sizeof(filename)-1);
                                have_filename = 1;
                            }
                        }
                        if (have_filename) {
                            if (!file_exists(filename)) {
                                // ask create?
                                char qbuf[128];
                                char fname_short[64];
                                strncpy(fname_short, filename, sizeof(fname_short)-1);
                                fname_short[sizeof(fname_short)-1] = '\0';
                                snprintf(qbuf, sizeof(qbuf), "Create %s and write to it? (Y/n): ", fname_short);
                                wattron(cmd_win, COLOR_PAIR(2));
                                for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                                mvwprintw(cmd_win, 0, 1, "%s", qbuf);
                                wattroff(cmd_win, COLOR_PAIR(2));
                                wrefresh(cmd_win);
                                int r = getch();
                                if (r == 'n' || r == 'N') {
                                    // abort save
                                } else {
                                    // proceed to save
                                    if (save_file(&buf, filename) == 0) {
                                        modified = 0;
                                        existing_file = 1;
                                    } else {
                                        // show error briefly
                                        wattron(cmd_win, COLOR_PAIR(2));
                                        for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                                        mvwprintw(cmd_win, 0, 1, "Error writing %s", filename);
                                        wattroff(cmd_win, COLOR_PAIR(2));
                                        wrefresh(cmd_win);
                                    }
                                }
                            } else {
                                if (save_file(&buf, filename) == 0) modified = 0;
                                else {
                                    wattron(cmd_win, COLOR_PAIR(2));
                                    for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                                    mvwprintw(cmd_win, 0, 1, "Error writing %s", filename);
                                    wattroff(cmd_win, COLOR_PAIR(2));
                                    wrefresh(cmd_win);
                                }
                            }
                        }
                    } else if (strcmp(cmdbuf, "wq") == 0 || strcmp(cmdbuf, "x") == 0) {
                        if (!have_filename || strlen(filename) == 0) {
                            // prompt for filename
                            const char *pr = "Enter filename: ";
                            echo();
                            curs_set(1);
                            wattron(cmd_win, COLOR_PAIR(2));
                            for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                            mvwprintw(cmd_win, 0, 1, "%s", pr);
                            wattroff(cmd_win, COLOR_PAIR(2));
                            wrefresh(cmd_win);
                            char fnamebuf[1024];
                            memset(fnamebuf,0,sizeof(fnamebuf));
                            mvwgetnstr(cmd_win, 0, 1 + (int)strlen(pr), fnamebuf, sizeof(fnamebuf)-1);
                            noecho();
                            if (strlen(fnamebuf) > 0) {
                                strncpy(filename, fnamebuf, sizeof(filename)-1);
                                have_filename = 1;
                            }
                        }
                        if (have_filename) {
                            if (!file_exists(filename)) {
                                // ask create?
                                char qbuf[128];
                                char fname_short[64];
                                strncpy(fname_short, filename, sizeof(fname_short)-1);
                                fname_short[sizeof(fname_short)-1] = '\0';
                                snprintf(qbuf, sizeof(qbuf), "Create %s and write to it? (Y/n): ", fname_short);
                                wattron(cmd_win, COLOR_PAIR(2));
                                for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                                mvwprintw(cmd_win, 0, 1, "%s", qbuf);
                                wattroff(cmd_win, COLOR_PAIR(2));
                                wrefresh(cmd_win);
                                int r = getch();
                                if (r == 'n' || r == 'N') {
                                    // abort save+quit
                                } else {
                                    if (save_file(&buf, filename) == 0) {
                                        modified = 0;
                                        existing_file = 1;
                                        quit = 1;
                                    } else {
                                        wattron(cmd_win, COLOR_PAIR(2));
                                        for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                                        mvwprintw(cmd_win, 0, 1, "Error writing %s", filename);
                                        wattroff(cmd_win, COLOR_PAIR(2));
                                        wrefresh(cmd_win);
                                    }
                                }
                            } else {
                                if (save_file(&buf, filename) == 0) modified = 0, quit = 1;
                                else {
                                    wattron(cmd_win, COLOR_PAIR(2));
                                    for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                                    mvwprintw(cmd_win, 0, 1, "Error writing %s", filename);
                                    wattroff(cmd_win, COLOR_PAIR(2));
                                    wrefresh(cmd_win);
                                }
                            }
                        }
                    } else {
                        // unknown command; show it briefly
                        wattron(cmd_win, COLOR_PAIR(2));
                        for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                        mvwprintw(cmd_win, 0, 1, "Unknown command: %s", cmdbuf);
                        wattroff(cmd_win, COLOR_PAIR(2));
                        wrefresh(cmd_win);
                    }
                }
                // clear command buffer and go back to insert mode (unless quit)
                cmdlen = 0;
                cmdbuf[0] = '\0';
                mode_insert = 1;
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
                if (cmdlen > 0) {
                    cmdlen--;
                    cmdbuf[cmdlen] = '\0';
                } else {
                    // if empty, ESC to insert mode
                    mode_insert = 1;
                }
            } else if (ch >= 32 && ch <= 126) {
                if (cmdlen + 1 < sizeof(cmdbuf)) {
                    cmdbuf[cmdlen++] = (char)ch;
                    cmdbuf[cmdlen] = '\0';
                }
            } else if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT) {
                // ignore in command mode
            }
        }
    } // main loop

    // cleanup
    if (main_win) delwin(main_win);
    if (cmd_win) delwin(cmd_win);
    buf_free(&buf);
    endwin();

    return 0;
}