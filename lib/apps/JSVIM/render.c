// render.c - ncurses rendering functions
#include "render.h"
#include "highlight.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define JSVIM_CONFIG_FILE ".jsvimrc"

// Semantic color configuration loaded from ~/.jsvimrc
typedef struct {
    const char *name;  // token type name (e.g. "keyword")
    int value;         // ncurses color index
    int has_value;     // 1 if explicitly set in config
} SemanticColorConfig;

static SemanticColorConfig semantic_color_config[] = {
    {"keyword",   0, 0},
    {"type",      0, 0},
    {"function",  0, 0},
    {"string",    0, 0},
    {"number",    0, 0},
    {"comment",   0, 0},
    {"operator",  0, 0},
    {"macro",     0, 0},
    {"class",     0, 0},
    {"enum",      0, 0},
    {"namespace", 0, 0},
    {"variable",  0, 0},
    {"parameter", 0, 0},
    {"property",  0, 0},
};

static int semantic_colors_loaded = 0;

static void load_semantic_colors_from_config(void) {
    if (semantic_colors_loaded)
        return;
    semantic_colors_loaded = 1;

    const char *home = getenv("HOME");
    if (!home)
        return;

    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/%s", home, JSVIM_CONFIG_FILE);

    FILE *fp = fopen(config_path, "r");
    if (!fp)
        return;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;

        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p == '#' || *p == '\0' || *p == '\n')
            continue;

        char *eq = strchr(p, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = p;
        char *value = eq + 1;

        char *end = eq - 1;
        while (end > key && isspace((unsigned char)*end))
            *end-- = '\0';

        while (*value && isspace((unsigned char)*value))
            value++;
        end = value + strlen(value) - 1;
        while (end > value && isspace((unsigned char)*end))
            *end-- = '\0';

        const char prefix[] = "editor.color.";
        size_t prefix_len = sizeof(prefix) - 1;
        if (strncmp(key, prefix, prefix_len) != 0)
            continue;

        const char *token_name = key + prefix_len;
        if (*token_name == '\0')
            continue;

        char *endptr = NULL;
        long parsed = strtol(value, &endptr, 10);
        if (endptr == value)
            continue;

        for (size_t idx = 0; idx < sizeof(semantic_color_config) / sizeof(semantic_color_config[0]); idx++) {
            if (strcmp(token_name, semantic_color_config[idx].name) == 0) {
                semantic_color_config[idx].value = (int)parsed;
                semantic_color_config[idx].has_value = 1;
                break;
            }
        }
    }

    fclose(fp);
}

static int get_semantic_color(const char *name, int default_color) {
    load_semantic_colors_from_config();

    for (size_t idx = 0; idx < sizeof(semantic_color_config) / sizeof(semantic_color_config[0]); idx++) {
        if (strcmp(name, semantic_color_config[idx].name) == 0 && semantic_color_config[idx].has_value) {
            return semantic_color_config[idx].value;
        }
    }
    return default_color;
}

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

    // Semantic token colors (foreground only, background stays default)
    init_pair(SY_KEYWORD,   get_semantic_color("keyword",   COLOR_BLUE),    -1);
    init_pair(SY_TYPE,      get_semantic_color("type",      COLOR_CYAN),    -1);
    init_pair(SY_FUNCTION,  get_semantic_color("function",  COLOR_YELLOW),  -1);
    init_pair(SY_STRING,    get_semantic_color("string",    127),           -1);
    init_pair(SY_NUMBER,    get_semantic_color("number",    14),            -1);
    init_pair(SY_COMMENT,   get_semantic_color("comment",   34),            -1);
    init_pair(SY_OPERATOR,  get_semantic_color("operator",  COLOR_WHITE),   -1);
    init_pair(SY_MACRO,     get_semantic_color("macro",     COLOR_MAGENTA), -1);
    init_pair(SY_CLASS,     get_semantic_color("class",     COLOR_GREEN),   -1);
    init_pair(SY_ENUM,      get_semantic_color("enum",      COLOR_GREEN),   -1);
    init_pair(SY_NAMESPACE, get_semantic_color("namespace", 66),            -1);
    init_pair(SY_VARIABLE,  get_semantic_color("variable",  COLOR_WHITE),   -1);
    init_pair(SY_PARAMETER, get_semantic_color("parameter", 180),           -1);
    init_pair(SY_PROPERTY,  get_semantic_color("property",  110),           -1);
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
