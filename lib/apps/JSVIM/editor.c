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
#include <stdio.h>
#include <ctype.h>

#define JSVIM_CONFIG_FILE ".jsvimrc"
#define DEFAULT_TAB_WIDTH 4

// Static indent string buffer for tab/spaces
static char s_indent_str[32] = "    ";  // default 4 spaces

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
    ed->file_created = 0;
    ed->pending_create_prompt = 0;
    ed->tab_width = DEFAULT_TAB_WIDTH;  // default: 4 spaces
}

void editor_load_config(EditorState *ed) {
    const char *home = getenv("HOME");
    if (!home) return;

    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/%s", home, JSVIM_CONFIG_FILE);

    FILE *fp = fopen(config_path, "r");
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0' || *p == '\n') continue;

        // Parse key=value
        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = p;
        char *value = eq + 1;

        // Trim whitespace from key
        char *end = eq - 1;
        while (end > key && isspace((unsigned char)*end)) *end-- = '\0';

        // Trim whitespace from value
        while (*value && isspace((unsigned char)*value)) value++;
        end = value + strlen(value) - 1;
        while (end > value && isspace((unsigned char)*end)) *end-- = '\0';

        // Process known keys
        if (strcmp(key, "editor.tab") == 0) {
            int tab_val = atoi(value);
            if (tab_val == -1 || tab_val > 0) {
                ed->tab_width = tab_val;
            }
        }
    }

    fclose(fp);

    // Update the static indent string based on loaded config
    if (ed->tab_width == -1) {
        s_indent_str[0] = '\t';
        s_indent_str[1] = '\0';
    } else {
        int width = ed->tab_width;
        if (width > 16) width = 16;  // cap at 16 spaces
        for (int i = 0; i < width; i++) {
            s_indent_str[i] = ' ';
        }
        s_indent_str[width] = '\0';
    }
}

const char *editor_get_indent_str(EditorState *ed) {
    (void)ed;  // Use static buffer
    return s_indent_str;
}

char *editor_get_line_indent(const char *line) {
    // Extract leading whitespace from a line
    size_t i = 0;
    while (line[i] && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }
    char *indent = malloc(i + 1);
    if (indent) {
        memcpy(indent, line, i);
        indent[i] = '\0';
    }
    return indent;
}

// Check if line ends with a block opener for C-like languages
static int line_ends_with_brace(const char *line) {
    size_t len = strlen(line);
    if (len == 0) return 0;
    // Find last non-whitespace char
    size_t i = len - 1;
    while (i > 0 && isspace((unsigned char)line[i])) i--;
    return line[i] == '{';
}

// Check if line ends with colon for Python
static int line_ends_with_colon(const char *line) {
    size_t len = strlen(line);
    if (len == 0) return 0;
    // Find last non-whitespace char
    size_t i = len - 1;
    while (i > 0 && isspace((unsigned char)line[i])) i--;
    return line[i] == ':';
}

char *editor_auto_indent(EditorState *ed, const char *prev_line) {
    Buffer *buf = &ed->buf;
    char *base_indent = editor_get_line_indent(prev_line);
    if (!base_indent) return dupstr("");

    const char *indent_str = editor_get_indent_str(ed);
    size_t base_len = strlen(base_indent);
    size_t indent_len = strlen(indent_str);

    int should_increase = 0;

    switch (buf->ft) {
        case FT_C:
        case FT_CPP:
        case FT_JAVA:
        case FT_JS:
        case FT_TS:
        case FT_RUST:
        case FT_GO:
        case FT_JSON:
            // C-like: indent after {
            should_increase = line_ends_with_brace(prev_line);
            break;
        case FT_PYTHON:
            // Python: indent after :
            should_increase = line_ends_with_colon(prev_line);
            break;
        case FT_SH:
            // Shell: indent after then, do, else, {
            should_increase = line_ends_with_brace(prev_line);
            // Also check for then, do, else at end
            {
                size_t len = strlen(prev_line);
                if (len >= 4) {
                    const char *end = prev_line + len;
                    // Skip trailing whitespace
                    while (end > prev_line && isspace((unsigned char)*(end-1))) end--;
                    size_t trimmed_len = end - prev_line;
                    if (trimmed_len >= 4 && strncmp(end - 4, "then", 4) == 0) should_increase = 1;
                    if (trimmed_len >= 2 && strncmp(end - 2, "do", 2) == 0) should_increase = 1;
                    if (trimmed_len >= 4 && strncmp(end - 4, "else", 4) == 0) should_increase = 1;
                }
            }
            break;
        default:
            break;
    }

    if (should_increase) {
        char *result = malloc(base_len + indent_len + 1);
        if (result) {
            memcpy(result, base_indent, base_len);
            memcpy(result + base_len, indent_str, indent_len);
            result[base_len + indent_len] = '\0';
        }
        free(base_indent);
        return result ? result : dupstr("");
    }

    return base_indent;
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
    case KEY_HOME:
        // Move cursor to beginning of line
        ed->cursor_col = 0;
        break;
    case KEY_END:
        // Move cursor to end of line
        ed->cursor_col = strlen(buf->lines[ed->cursor_line]);
        break;
    case KEY_DC:  // Delete key
        {
            char *line = buf->lines[ed->cursor_line];
            size_t len = strlen(line);
            if (ed->cursor_col < len) {
                // delete character at cursor
                memmove(line + ed->cursor_col, line + ed->cursor_col + 1, len - ed->cursor_col);
                ed->modified = 1;
                buf->lsp_dirty = 1;
            } else if (ed->cursor_line + 1 < buf->count) {
                // at end of line, join with next line
                size_t nextlen = strlen(buf->lines[ed->cursor_line + 1]);
                line = realloc(line, len + nextlen + 1);
                memcpy(line + len, buf->lines[ed->cursor_line + 1], nextlen + 1);
                buf->lines[ed->cursor_line] = line;
                free(buf->lines[ed->cursor_line + 1]);
                // shift lines up
                for (size_t i = ed->cursor_line + 1; i + 1 < buf->count; i++) 
                    buf->lines[i] = buf->lines[i + 1];
                buf->count--;
                ed->modified = 1;
                buf->lsp_dirty = 1;
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
    case '\t':
        {
            // Insert tab/spaces based on config
            const char *indent = editor_get_indent_str(ed);
            size_t indent_len = strlen(indent);
            char *line = buf->lines[ed->cursor_line];
            size_t len = strlen(line);
            line = realloc(line, len + indent_len + 1);
            memmove(line + ed->cursor_col + indent_len, line + ed->cursor_col, len - ed->cursor_col + 1);
            memcpy(line + ed->cursor_col, indent, indent_len);
            buf->lines[ed->cursor_line] = line;
            ed->cursor_col += indent_len;
            ed->modified = 1;
            buf->lsp_dirty = 1;
            
            // Ensure scroll is correct after tab insertion
            if (ed->cursor_line >= ed->scroll_y + (size_t)visible_rows) {
                if (ed->cursor_line >= (size_t)visible_rows)
                    ed->scroll_y = ed->cursor_line - (visible_rows - 1);
                else
                    ed->scroll_y = 0;
            }
        }
        break;
    case '\n':
    case '\r':
        {
            // split line at cursor with auto-indent
            char *line = buf->lines[ed->cursor_line];
            char *after_cursor = dupstr(line + ed->cursor_col);
            line[ed->cursor_col] = '\0';
            
            // Skip leading whitespace in after_cursor
            char *after_trimmed = after_cursor;
            while (*after_trimmed == ' ' || *after_trimmed == '\t') after_trimmed++;
            size_t trimmed_len = strlen(after_trimmed);
            
            // Get the base indent of the current line
            char *base_indent = editor_get_line_indent(line);
            size_t base_indent_len = strlen(base_indent);
            
            // Calculate auto-indent for the new line (may be increased)
            char *auto_ind = editor_auto_indent(ed, line);
            size_t auto_ind_len = strlen(auto_ind);
            
            // Check for vim-style bracket handling: cursor between { and }
            // If line ends with '{' and after_trimmed starts with '}'
            int between_braces = 0;
            if (trimmed_len > 0 && after_trimmed[0] == '}') {
                // Check if line (before cursor) ends with '{'
                size_t line_len = strlen(line);
                size_t i = line_len;
                while (i > 0 && isspace((unsigned char)line[i-1])) i--;
                if (i > 0 && line[i-1] == '{') {
                    between_braces = 1;
                }
            }
            
            if (between_braces) {
                // Vim-style: create two lines
                // Line 1: just the indented cursor line (empty with indent)
                // Line 2: closing brace with base indent
                
                // First new line: just the auto-indent (cursor goes here)
                char *newline1 = malloc(auto_ind_len + 1);
                memcpy(newline1, auto_ind, auto_ind_len);
                newline1[auto_ind_len] = '\0';
                
                // Second new line: base indent + the closing brace and rest
                char *newline2 = malloc(base_indent_len + trimmed_len + 1);
                memcpy(newline2, base_indent, base_indent_len);
                memcpy(newline2 + base_indent_len, after_trimmed, trimmed_len + 1);
                
                free(after_cursor);
                free(auto_ind);
                free(base_indent);
                
                // Insert both lines
                buf_insert(buf, ed->cursor_line + 1, newline1);
                buf_insert(buf, ed->cursor_line + 2, newline2);
                ed->cursor_line++;
                ed->cursor_col = auto_ind_len;
            } else {
                // Normal case: single new line with auto-indent
                char *newline = malloc(auto_ind_len + trimmed_len + 1);
                memcpy(newline, auto_ind, auto_ind_len);
                memcpy(newline + auto_ind_len, after_trimmed, trimmed_len + 1);
                
                free(after_cursor);
                free(auto_ind);
                free(base_indent);
                
                buf_insert(buf, ed->cursor_line + 1, newline);
                ed->cursor_line++;
                ed->cursor_col = auto_ind_len;
            }
            
            ed->modified = 1;
            buf->lsp_dirty = 1;
            
            // Adjust scroll if cursor moved past visible area
            if (ed->cursor_line >= ed->scroll_y + (size_t)visible_rows) {
                if (ed->cursor_line >= (size_t)visible_rows)
                    ed->scroll_y = ed->cursor_line - (visible_rows - 1);
                else
                    ed->scroll_y = 0;
            }
        }
        break;
    default:
        if (ch >= 32 && ch <= 126) {
            // Check for auto-closing brackets
            char closing = 0;
            switch (ch) {
                case '(': closing = ')'; break;
                case '[': closing = ']'; break;
                case '{': closing = '}'; break;
                case '"': closing = '"'; break;
                case '\'': closing = '\''; break;
                case '`': closing = '`'; break;
            }
            
            // For quotes, don't auto-close if next char is same quote (user closing it)
            // or if previous char is alphanumeric (likely a contraction like "don't")
            if (closing == '"' || closing == '\'' || closing == '`') {
                char *line = buf->lines[ed->cursor_line];
                size_t len = strlen(line);
                // Skip if next char is same (user is closing)
                if (ed->cursor_col < len && line[ed->cursor_col] == ch) {
                    // Just move cursor past the existing quote
                    ed->cursor_col++;
                    break;
                }
                // Skip auto-close for apostrophe if prev char is alphanumeric
                if (ch == '\'' && ed->cursor_col > 0 && isalnum((unsigned char)line[ed->cursor_col - 1])) {
                    closing = 0;
                }
            }
            
            // For regular brackets, skip if next char is the closing bracket
            if (closing == ')' || closing == ']' || closing == '}') {
                char *line = buf->lines[ed->cursor_line];
                size_t len = strlen(line);
                if (ed->cursor_col < len && line[ed->cursor_col] == closing) {
                    // User typed opening bracket but closing already exists next
                    // Just insert normally without auto-close
                    closing = 0;
                }
            }
            
            if (closing) {
                // Insert both opening and closing, cursor between them
                char *line = buf->lines[ed->cursor_line];
                size_t len = strlen(line);
                line = realloc(line, len + 3);  // +2 for both chars, +1 for null
                memmove(line + ed->cursor_col + 2, line + ed->cursor_col, len - ed->cursor_col + 1);
                line[ed->cursor_col] = (char)ch;
                line[ed->cursor_col + 1] = closing;
                buf->lines[ed->cursor_line] = line;
                ed->cursor_col++;  // Position cursor between the brackets
                ed->modified = 1;
                buf->lsp_dirty = 1;
            } else {
                // Normal character insertion
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

void editor_handle_command_mode(EditorState *ed, int ch, WINDOW *cmd_win, int maxx) {
    Buffer *buf = &ed->buf;
    
    if (ch == ERR) {
        // refresh/clock; continue
        return;
    }
    
    // Handle create file prompt
    if (ed->pending_create_prompt) {
        if (ch == 'n' || ch == 'N') {
            ed->quit = 1;
        } else if (ch == 'y' || ch == 'Y' || ch == '\n' || ch == '\r') {
            ed->pending_create_prompt = 0;
            ed->file_created = 1;
            ed->mode_insert = 1;  // Now allow insert mode
        }
        return;
    }
    
    if (ch == 27) {
        // ESC in command mode -> back to insert (only if file is created)
        if (ed->file_created) {
            ed->mode_insert = 1;
        }
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
                        snprintf(ed->filename, sizeof(ed->filename), "%s", fnamebuf);
                        ed->have_filename = 1;
                    }
                }
                if (ed->have_filename) {
                    if (!file_exists(ed->filename)) {
                        // ask create?
                        char qbuf[128];
                        char fname_short[64];
                        snprintf(fname_short, sizeof(fname_short), "%.63s", ed->filename);
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
                                ed->file_created = 1;
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
                        if (save_file(buf, ed->filename) == 0) {
                            ed->modified = 0;
                            ed->file_created = 1;
                        } else {
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
                        snprintf(ed->filename, sizeof(ed->filename), "%s", fnamebuf);
                        ed->have_filename = 1;
                    }
                }
                if (ed->have_filename) {
                    if (!file_exists(ed->filename)) {
                        // ask create?
                        char qbuf[128];
                        char fname_short[64];
                        snprintf(fname_short, sizeof(fname_short), "%.63s", ed->filename);
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
                                ed->file_created = 1;
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
                            ed->file_created = 1;
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
            } else if (strncmp(ed->cmdbuf, "go ", 3) == 0) {
                // go to line number: "go 100"
                int line_num = atoi(ed->cmdbuf + 3);
                if (line_num > 0) {
                    // Convert to 0-based index
                    size_t target = (size_t)(line_num - 1);
                    if (target >= buf->count && buf->count > 0)
                        target = buf->count - 1;
                    ed->cursor_line = target;
                    ed->cursor_col = 0;
                    // Adjust scroll to show cursor
                    if (ed->cursor_line < ed->scroll_y)
                        ed->scroll_y = ed->cursor_line;
                }
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
    
    if (ch == KEY_UP){
        // Find first digit in cmdbuf to parse amount
        int amount = 0;
        for (size_t i = 0; i < ed->cmdlen; i++) {
            if (ed->cmdbuf[i] >= '0' && ed->cmdbuf[i] <= '9') {
                amount = atoi(&ed->cmdbuf[i]);
                break;
            }
        }
        if (amount <= 0) amount = 1;  // default to 1 if empty/invalid
        if (ed->cursor_line >= (size_t)amount)
            ed->cursor_line -= amount;
        else
            ed->cursor_line = 0;
        size_t len = strlen(buf->lines[ed->cursor_line]);
        if (ed->cursor_col > len) ed->cursor_col = len;
        if (ed->cursor_line < ed->scroll_y)
            ed->scroll_y = ed->cursor_line;
        // Clear cmdbuf after navigation
        ed->cmdlen = 0;
        ed->cmdbuf[0] = '\0';
        ed->mode_insert = 1;
        return;
    } else if (ch == KEY_DOWN){
        // Find first digit in cmdbuf to parse amount
        int amount = 0;
        for (size_t i = 0; i < ed->cmdlen; i++) {
            if (ed->cmdbuf[i] >= '0' && ed->cmdbuf[i] <= '9') {
                amount = atoi(&ed->cmdbuf[i]);
                break;
            }
        }
        if (amount <= 0) amount = 1;
        if (ed->cursor_line + amount < buf->count)
            ed->cursor_line += amount;
        else if (buf->count > 0)
            ed->cursor_line = buf->count - 1;
        size_t len = strlen(buf->lines[ed->cursor_line]);
        if (ed->cursor_col > len) ed->cursor_col = len;
        // Clear cmdbuf after navigation
        ed->cmdlen = 0;
        ed->cmdbuf[0] = '\0';
        ed->mode_insert = 1;
        return;
    }
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
