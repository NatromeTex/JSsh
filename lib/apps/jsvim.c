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

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_BLACK, 8);   //status
    init_pair(3, 8, COLOR_BLACK);   //gutter

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

    // Count logical lines
    size_t line_count = 1;
    if (filebuf) {
        for (size_t i = 0; i < filelen; i++)
            if (filebuf[i] == '\n')
                line_count++;
    }

    // Compute fixed gutter width
    int gutter_width = 1;
    for (size_t n = line_count; n >= 10; n /= 10)
        gutter_width++;

    int ch;
    int maxy, maxx;
    const char *title = "JSVIM";

    int cy = 1;
    int cx = gutter_width + 2;

    while (1) {
        getmaxyx(stdscr, maxy, maxx);
        erase();

        // Draw box, remove left border
        box(stdscr, 0, 0);
        mvaddch(0, 0, ' ');
        for (int i = 1; i < maxy - 1; i++)
            mvaddch(i, 0, ' ');
        mvaddch(maxy - 1, 0, ' ');

        attron(COLOR_PAIR(1));
        mvprintw(0, 2, "%s", title);
        attroff(COLOR_PAIR(1));

        // Render file with wrap-aware line numbers
        if (filebuf) {
            const char *p = filebuf;
            size_t lineno = 1;

            int row = 1;
            int col_offset = gutter_width + 2;

            while (*p && row < maxy - 1) {

                // Start of a logical line → print its number
                attron(COLOR_PAIR(3));
                mvprintw(row, 1, "%*zu", gutter_width, lineno);
                attroff(COLOR_PAIR(3));

                int col = col_offset;

                // Render logical line until newline OR screen row ends
                while (*p && *p != '\n') {

                    if (col >= maxx - 1) {
                        // Hard wrap → next screen row without incrementing lineno
                        row++;
                        if (row >= maxy - 1)
                            break;

                        // Wrapped row → blank gutter
                        attron(COLOR_PAIR(3));
                        mvprintw(row, 1, "%*s", gutter_width, "");
                        attroff(COLOR_PAIR(3));

                        col = col_offset;
                    }

                    if (row >= maxy - 1)
                        break;

                    mvaddch(row, col++, *p++);
                }

                // If newline, consume it and increment logical line number
                if (*p == '\n') {
                    p++;
                    lineno++;
                }

                row++;
            }
        }

        // Status bar background
        attron(COLOR_PAIR(2));
        for (int i = 1; i < maxx - 1; i++)
            mvaddch(maxy - 1, i, ' ');
        attroff(COLOR_PAIR(2));

        //Filename
        attron(COLOR_PAIR(2));
        mvprintw(maxy - 1, 2, "%s", filename);
        attroff(COLOR_PAIR(2));

        //Time
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char tbuf[6];
        strftime(tbuf, sizeof(tbuf), "%H:%M", tm_info);

        attron(COLOR_PAIR(2));
        mvprintw(maxy - 1, maxx - 1 - (int)strlen(tbuf) - 1, "%s", tbuf);
        attroff(COLOR_PAIR(2));

        // Keep cursor outside gutter
        int col_offset = gutter_width + 2;

        if (cy < 1) cy = 1;
        if (cy > maxy - 2) cy = maxy - 2;
        if (cx < col_offset) cx = col_offset;
        if (cx > maxx - 2) cx = maxx - 2;

        move(cy, cx);
        refresh();

        ch = getch();
        if (ch == 27) break;

        switch (ch) {
            case KEY_UP:    cy--; break;
            case KEY_DOWN:  cy++; break;
            case KEY_LEFT:  cx--; break;
            case KEY_RIGHT: cx++; break;
            case KEY_RESIZE:
                break;
        }
    }

    if (filebuf) free(filebuf);

    endwin();
    return 0;
}
