// editor.c - Editor state and key handling
#include "editor.h"
#include "buffer.h"
#include "lsp.h"
#include "language.h"
#include "highlight.h"
#include "util.h"
#include "render.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void editor_init(EditorState *ed) {
    buf_init(&ed->buf);
    ed->filename[0] = '\0';
    ed->have_filename = 0;
    ed->existing_file = 0;
    ed->cursor_line = 0;
    ed->cursor_col = 0;
    ed->scroll_y = 0;
    ed->mode_insert = 1;
    ed->modified = 0;
    ed->cmdbuf[0] = '\0';
    ed->cmdlen = 0;
    ed->quit = 0;
    ed->force_quit = 0;
    ed->line_number_relative = 0;
}

void editor_cleanup(EditorState *ed) {
    stop_lsp(&ed->buf.lsp);
    buf_free(&ed->buf);
}

void editor_handle_insert_mode(EditorState *ed, int ch, int visible_rows) {
    Buffer *buf = &ed->buf;
    
    if (ch == 27) {
        // ESC -> command mode
        ed->mode_insert = 0;
        ed->cmdlen = 0;
        ed->cmdbuf[0] = '\0';
        return;
    }
    
    switch (ch) {
    case ERR:
        // timeout for clock update; continue
        break;
    case KEY_UP:
        if (ed->cursor_line > 0) {
            ed->cursor_line--;
            size_t len = strlen(buf->lines[ed->cursor_line]);
            if (ed->cursor_col > len) ed->cursor_col = len;
            if (ed->cursor_line < ed->scroll_y) ed->scroll_y = ed->cursor_line;
        }
        break;
    case KEY_DOWN:
        if (ed->cursor_line + 1 < buf->count) {
            ed->cursor_line++;
            size_t len = strlen(buf->lines[ed->cursor_line]);
            if (ed->cursor_col > len) ed->cursor_col = len;
            if (ed->cursor_line >= ed->scroll_y + (size_t)visible_rows) {
                if (ed->cursor_line >= (size_t)visible_rows)
                    ed->scroll_y = ed->cursor_line - (visible_rows - 1);
                else
                    ed->scroll_y = 0;
            }
        }
        break;
    case KEY_LEFT:
        if (ed->cursor_col > 0) ed->cursor_col--;
        else if (ed->cursor_line > 0) {
            ed->cursor_line--;
            ed->cursor_col = strlen(buf->lines[ed->cursor_line]);
            if (ed->cursor_line < ed->scroll_y) ed->scroll_y = ed->cursor_line;
        }
        break;
    case KEY_RIGHT:
        {
            size_t len = strlen(buf->lines[ed->cursor_line]);
            if (ed->cursor_col < len) ed->cursor_col++;
            else if (ed->cursor_line + 1 < buf->count) {
                ed->cursor_line++;
                ed->cursor_col = 0;
                if (ed->cursor_line >= ed->scroll_y + (size_t)visible_rows) {
                    if (ed->cursor_line >= (size_t)visible_rows)
                        ed->scroll_y = ed->cursor_line - (visible_rows - 1);
                    else ed->scroll_y = 0;
                }
            }
        }
        break;
    case KEY_BACKSPACE:
    case 127:
    case '\b':
        if (ed->cursor_col > 0) {
            // delete previous character
            char *line = buf->lines[ed->cursor_line];
            size_t len = strlen(line);
            memmove(line + ed->cursor_col - 1, line + ed->cursor_col, len - ed->cursor_col + 1);
            ed->cursor_col--;
            ed->modified = 1;
            buf->lsp_dirty = 1;
        } else if (ed->cursor_line > 0) {
            // join with previous line
            size_t prevlen = strlen(buf->lines[ed->cursor_line - 1]);
            size_t curlen = strlen(buf->lines[ed->cursor_line]);
            buf->lines[ed->cursor_line - 1] = realloc(buf->lines[ed->cursor_line - 1], prevlen + curlen + 1);
            memcpy(buf->lines[ed->cursor_line - 1] + prevlen, buf->lines[ed->cursor_line], curlen + 1);
            free(buf->lines[ed->cursor_line]);
            // shift lines up
            for (size_t i = ed->cursor_line; i + 1 < buf->count; i++) buf->lines[i] = buf->lines[i+1];
            buf->count--;
            ed->cursor_line--;
            ed->cursor_col = prevlen;
            ed->modified = 1;
            buf->lsp_dirty = 1;
        }
        break;
    case '\n':
    case '\r':
        {
            // split line at cursor
            char *line = buf->lines[ed->cursor_line];
            char *newline = dupstr(line + ed->cursor_col);
            line[ed->cursor_col] = '\0';
            // insert new line after current
            buf_insert(buf, ed->cursor_line + 1, newline);
            ed->cursor_line++;
            ed->cursor_col = 0;
            ed->modified = 1;
            buf->lsp_dirty = 1;
        }
        break;
    default:
        if (ch >= 32 && ch <= 126) {
            // printable
            char *line = buf->lines[ed->cursor_line];
            size_t len = strlen(line);
            line = realloc(line, len + 2);
            memmove(line + ed->cursor_col + 1, line + ed->cursor_col, len - ed->cursor_col + 1);
            line[ed->cursor_col] = (char)ch;
            buf->lines[ed->cursor_line] = line;
            ed->cursor_col++;
            ed->modified = 1;
            buf->lsp_dirty = 1;
        }
        break;
    }

    // If the buffer changed, re-highlight
    if (buf->lsp_dirty) {
        // Always do regex highlighting first (basic syntax)
        highlight_buffer(buf);
        
        // If LSP is active, also request semantic tokens (layered on top)
        if ((buf->ft == FT_C || buf->ft == FT_CPP) && buf->lsp.pid > 0) {
            lsp_notify_did_change(buf);
            lsp_request_semantic_tokens(buf);
        }
        buf->lsp_dirty = 0;
    }
}

void editor_handle_command_mode(EditorState *ed, int ch,
                                WINDOW *cmd_win, int maxx) {
    Buffer *buf = &ed->buf;
    
    if (ch == ERR) {
        // refresh/clock; continue
        return;
    }
    
    if (ch == 27) {
        // ESC in command mode -> back to insert
        ed->mode_insert = 1;
        return;
    }
    
    if (ch == '\n' || ch == '\r') {
        // execute command in cmdbuf
        if (ed->cmdlen > 0) {
            // common commands: q, q!, w, wq, x
            if (ed->cmdbuf[0] == 'q' && ed->cmdbuf[1] == '\0') {
                ed->quit = 1;
            } else if (strcmp(ed->cmdbuf, "q!") == 0) {
                // force quit (ignore modified)
                ed->force_quit = 1;
                ed->quit = 1;
            } else if (strcmp(ed->cmdbuf, "w") == 0) {
                if (!ed->have_filename || strlen(ed->filename) == 0) {
                    // prompt for filename in command window
                    const char *pr = "Enter filename: ";
                    echo();
                    curs_set(1);
                    wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                    for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                    mvwprintw(cmd_win, 0, 1, "%s", pr);
                    wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                    wrefresh(cmd_win);
                    char fnamebuf[1024];
                    memset(fnamebuf, 0, sizeof(fnamebuf));
                    mvwgetnstr(cmd_win, 0, 1 + (int)strlen(pr), fnamebuf, sizeof(fnamebuf)-1);
                    noecho();
                    if (strlen(fnamebuf) > 0) {
                        strncpy(ed->filename, fnamebuf, sizeof(ed->filename)-1);
                        ed->have_filename = 1;
                    }
                }
                if (ed->have_filename) {
                    if (!file_exists(ed->filename)) {
                        // ask create?
                        char qbuf[128];
                        char fname_short[64];
                        strncpy(fname_short, ed->filename, sizeof(fname_short)-1);
                        fname_short[sizeof(fname_short)-1] = '\0';
                        snprintf(qbuf, sizeof(qbuf), "Create %s and write to it? (Y/n): ", fname_short);
                        wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                        for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                        mvwprintw(cmd_win, 0, 1, "%s", qbuf);
                        wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                        wrefresh(cmd_win);
                        int r = getch();
                        if (r == 'n' || r == 'N') {
                            // abort save
                        } else {
                            // proceed to save
                            if (save_file(buf, ed->filename) == 0) {
                                ed->modified = 0;
                                ed->existing_file = 1;
                            } else {
                                // show error briefly
                                wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                                for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                                mvwprintw(cmd_win, 0, 1, "Error writing %s", ed->filename);
                                wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                                wrefresh(cmd_win);
                            }
                        }
                    } else {
                        if (save_file(buf, ed->filename) == 0) ed->modified = 0;
                        else {
                            wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                            for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                            mvwprintw(cmd_win, 0, 1, "Error writing %s", ed->filename);
                            wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                            wrefresh(cmd_win);
                        }
                    }
                }
            } else if (strcmp(ed->cmdbuf, "wq") == 0 || strcmp(ed->cmdbuf, "x") == 0) {
                if (!ed->have_filename || strlen(ed->filename) == 0) {
                    // prompt for filename
                    const char *pr = "Enter filename: ";
                    echo();
                    curs_set(1);
                    wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                    for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                    mvwprintw(cmd_win, 0, 1, "%s", pr);
                    wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                    wrefresh(cmd_win);
                    char fnamebuf[1024];
                    memset(fnamebuf, 0, sizeof(fnamebuf));
                    mvwgetnstr(cmd_win, 0, 1 + (int)strlen(pr), fnamebuf, sizeof(fnamebuf)-1);
                    noecho();
                    if (strlen(fnamebuf) > 0) {
                        strncpy(ed->filename, fnamebuf, sizeof(ed->filename)-1);
                        ed->have_filename = 1;
                    }
                }
                if (ed->have_filename) {
                    if (!file_exists(ed->filename)) {
                        // ask create?
                        char qbuf[128];
                        char fname_short[64];
                        strncpy(fname_short, ed->filename, sizeof(fname_short)-1);
                        fname_short[sizeof(fname_short)-1] = '\0';
                        snprintf(qbuf, sizeof(qbuf), "Create %s and write to it? (Y/n): ", fname_short);
                        wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                        for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                        mvwprintw(cmd_win, 0, 1, "%s", qbuf);
                        wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                        wrefresh(cmd_win);
                        int r = getch();
                        if (r == 'n' || r == 'N') {
                            // abort save+quit
                        } else {
                            if (save_file(buf, ed->filename) == 0) {
                                ed->modified = 0;
                                ed->existing_file = 1;
                                ed->quit = 1;
                            } else {
                                wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                                for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                                mvwprintw(cmd_win, 0, 1, "Error writing %s", ed->filename);
                                wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                                wrefresh(cmd_win);
                            }
                        }
                    } else {
                        if (save_file(buf, ed->filename) == 0) {
                            ed->modified = 0;
                            ed->quit = 1;
                        } else {
                            wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                            for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                            mvwprintw(cmd_win, 0, 1, "Error writing %s", ed->filename);
                            wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                            wrefresh(cmd_win);
                        }
                    }
                }
            } else if (strcmp(ed->cmdbuf, "set rel") == 0) {
                // set relative line numbers
                ed->line_number_relative = 1;
            } else if (strcmp(ed->cmdbuf, "set nu") == 0) {
                // set absolute line numbers
                ed->line_number_relative = 0;
            } else {
                // unknown command; show it briefly
                wattron(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                for (int i = 0; i < maxx; i++) mvwaddch(cmd_win, 0, i, ' ');
                mvwprintw(cmd_win, 0, 1, "Unknown command: %s", ed->cmdbuf);
                wattroff(cmd_win, COLOR_PAIR(COLOR_PAIR_STATUS));
                wrefresh(cmd_win);
            }
        }
        // clear command buffer and go back to insert mode (unless quit)
        ed->cmdlen = 0;
        ed->cmdbuf[0] = '\0';
        ed->mode_insert = 1;
        return;
    }
    
    if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
        if (ed->cmdlen > 0) {
            ed->cmdlen--;
            ed->cmdbuf[ed->cmdlen] = '\0';
        } else {
            // if empty, ESC to insert mode
            ed->mode_insert = 1;
        }
        return;
    }
    
    if (ch >= 32 && ch <= 126) {
        if (ed->cmdlen + 1 < sizeof(ed->cmdbuf)) {
            ed->cmdbuf[ed->cmdlen++] = (char)ch;
            ed->cmdbuf[ed->cmdlen] = '\0';
        }
        return;
    }
    
    // ignore arrow keys in command mode
}

void editor_process_lsp(EditorState *ed) {
    Buffer *buf = &ed->buf;
    
    if (buf->lsp.stdout_fd != -1) {
        char temp[4096];
        ssize_t n = read(buf->lsp.stdout_fd, temp, sizeof(temp));
        if (n > 0) {
            lsp_append_data(&buf->lsp, temp, (size_t)n);

            int r;
            while ((r = try_parse_lsp_message(buf)) == 1) {
                // keep parsing messages
            }
        }
    }
}
