// main.c - JSVIM entry point
/* When I was in college, our first UNIX Class had us coding in bash, awk and all those god-awful
*  Scripting languages. One thing was common in all those scripts though, vi, the simple text editor
*  That runs in the terminal with such unintuitive controls I swear to god Tax codes are more clearer
*  on the matter. While I am sure you can rebind the controls but First impressions last...
*/

#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "util.h"
#include "buffer.h"
#include "editor.h"
#include "render.h"
#include "language.h"
#include "lsp.h"
#include "highlight.h"

#ifndef JSVIM_VERSION
#define JSVIM_VERSION "0.2.7"
#endif
#ifndef JSSH_VERSION
#define JSSH_VERSION "unknown"
#endif

#define JSVIM_CONFIG_FILE ".jsvimrc"

// Open or create the config file at ~/.jsvimrc
// Returns: 0 on success, -1 on failure
static int init_config_file(void) {
    const char *home = getenv("HOME");
    if (!home) {
        return -1;
    }

    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/%s", home, JSVIM_CONFIG_FILE);

    // Check if file exists
    struct stat st;
    if (stat(config_path, &st) == 0) {
        // File exists, nothing to do for now
        return 0;
    }

    // File doesn't exist, create it with default settings
    FILE *fp = fopen(config_path, "w");
    if (!fp) {
        return -1;
    }
    // Write default config with comments
    fprintf(fp, "# JSVIM Configuration File\n");
    fprintf(fp, "# \n");
    fprintf(fp, "# Tab width setting:\n");
    fprintf(fp, "#   -1 = use actual tab character (\\t)\n");
    fprintf(fp, "#   >0 = number of spaces per tab (e.g., 2, 4, 8)\n");
    fprintf(fp, "editor.tab=4\n");
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    EditorState ed;
    editor_init(&ed);

    // Initialize config file and load settings
    init_config_file();
    editor_load_config(&ed);
    
    if (argc > 1) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("JSVIM - A Text Editor for JSSH %s\n", JSVIM_VERSION);
            printf("Packaged with JSSH %s\n", JSSH_VERSION);
            return 0;
        }
        else {
            strncpy(ed.filename, argv[1], sizeof(ed.filename)-1);
            ed.have_filename = 1;
            ed.existing_file = !load_file(&ed.buf, ed.filename);
            if (!ed.existing_file) {
                // file didn't exist; start with empty buffer, prompt user
                buf_push(&ed.buf, dupstr(""));
                ed.file_created = 0;
                ed.mode_insert = 0;  // Stay in command mode until file is created
                ed.pending_create_prompt = 1;  // Show create prompt
            } else {
                ed.file_created = 1;
            }
        }
    } else {
        // start with an empty buffer
        buf_push(&ed.buf, dupstr(""));
    }

    // Initialize ncurses after args have been processed
    render_init();
    render_init_colors();

    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // If no filename at startup, prompt user before main loop
    if (!ed.have_filename) {
        const char *prompt = "Enter filename: ";
        echo();
        curs_set(1);
        attron(COLOR_PAIR(COLOR_PAIR_STATUS));
        for (int i = 0; i < maxx; i++) mvaddch(maxy - 1, i, ' ');
        mvprintw(maxy - 1, 0, "%s", prompt);
        attroff(COLOR_PAIR(COLOR_PAIR_STATUS));
        refresh();

        char fnamebuf[1024];
        memset(fnamebuf, 0, sizeof(fnamebuf));
        mvgetnstr(maxy - 1, (int)strlen(prompt), fnamebuf, sizeof(fnamebuf)-1);

        noecho();
        if (strlen(fnamebuf) > 0) {
            snprintf(ed.filename, sizeof(ed.filename), "%s", fnamebuf);
            ed.have_filename = 1;
            ed.existing_file = !load_file(&ed.buf, ed.filename);
            if (!ed.existing_file) {
                // file didn't exist; start with empty buffer, prompt user
                buf_free(&ed.buf);
                buf_init(&ed.buf);
                buf_push(&ed.buf, dupstr(""));
                ed.file_created = 0;
                ed.mode_insert = 0;  // Stay in command mode until file is created
                ed.pending_create_prompt = 1;  // Show create prompt
            } else {
                ed.file_created = 1;
            }
        }
    }
    
    ed.buf.ft = detect_filetype(ed.filename);
    // Remember the path for LSP URI construction
    strncpy(ed.buf.filepath, ed.filename, sizeof(ed.buf.filepath) - 1);
    ed.buf.filepath[sizeof(ed.buf.filepath) - 1] = '\0';

    // Always do regex highlighting first (basic syntax highlighting)
    highlight_buffer(&ed.buf);

    // Then try to start LSP for deep semantic highlighting (layered on top)
    if (ed.buf.ft != FT_NONE) {
        ed.buf.lsp = spawn_lsp(&ed.buf.ft);
        if (ed.buf.lsp.pid > 0) {
            lsp_initialize(&ed.buf);
        } else {
            // failed to start LSP
        }
    }

    // Create two windows: main window and command window
    WINDOW *main_win = NULL;
    WINDOW *cmd_win = NULL;
    const char *title = "JSVIM";
    int ch;

    while (!ed.quit) {
        // Process LSP messages first to get diagnostics before rendering
        editor_process_lsp(&ed);
        
        getmaxyx(stdscr, maxy, maxx);
        
        // Delete old windows if they exist
        if (main_win) delwin(main_win);
        if (cmd_win) delwin(cmd_win);
        
        // Create main window (everything except last row)
        main_win = newwin(maxy - 1, maxx, 0, 0);
        keypad(main_win, TRUE);
        wtimeout(main_win, 200);  // Set timeout for LSP message processing
        
        // Create command window (last row only)
        cmd_win = newwin(1, maxx, maxy - 1, 0);
        keypad(cmd_win, TRUE);
        wtimeout(cmd_win, 200);  // Set timeout for LSP message processing

        int gutter_width = compute_gutter_width(ed.buf.count);
        int col_offset = gutter_width + 2;
        int visible_rows = maxy - 3;

        // Compute cursor position
        int cy, cx;
        compute_cursor_position(&ed.buf, ed.cursor_line, ed.cursor_col,
                               col_offset, maxx, visible_rows,
                               &ed.scroll_y, &cy, &cx);

        // Render windows
        render_main_window(main_win, &ed.buf, maxy, maxx,
                          ed.scroll_y, ed.cursor_line, ed.cursor_col,
                          gutter_width, title, ed.filename, ed.have_filename,
                          ed.modified, ed.mode_insert, ed.line_number_relative);

        render_command_window(cmd_win, &ed.buf, maxx, ed.mode_insert,
                             ed.cmdbuf, ed.cursor_line,
                             ed.pending_create_prompt, ed.filename);

        // Position cursor and get input
        if (ed.mode_insert) {
            // clamp cy to visible text area bounds
            if (cy < 1) cy = 1;
            if (cy > visible_rows) cy = visible_rows;
            wmove(main_win, cy, cx);
            wrefresh(cmd_win);
            wrefresh(main_win);
            ch = wgetch(main_win);
            editor_handle_insert_mode(&ed, ch, visible_rows);
        } else {
            // place cursor in command window
            if (ed.pending_create_prompt) {
                wmove(cmd_win, 0, 1 + 8 + (int)strlen(ed.filename) + 8);
            } else {
                wmove(cmd_win, 0, (int)ed.cmdlen + 2);
            }
            wrefresh(main_win);
            wrefresh(cmd_win);
            ch = wgetch(cmd_win);
            editor_handle_command_mode(&ed, ch, cmd_win, maxx);
        }
    }

    // Cleanup
    if (main_win) delwin(main_win);
    if (cmd_win) delwin(cmd_win);
    editor_cleanup(&ed);
    highlight_cleanup();
    lsp_config_cleanup();
    render_cleanup();

    return 0;
}
