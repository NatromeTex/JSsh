#include <ncurses.h>
#include <string.h>
#include <time.h>

int main() {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(1);
    start_color();
    use_default_colors();

    // Pair 1: white text on black background (JSVIM)
    init_pair(1, COLOR_WHITE, COLOR_BLACK);

    // Pair 2: black text on white background ("gray bar")
    init_pair(2, COLOR_BLACK, 8);

    int ch;
    int maxy, maxx;
    int cy = 1, cx = 1;
    const char *title = "JSVIM";

    while (1) {
        getmaxyx(stdscr, maxy, maxx);
        clear();

        // Border
        box(stdscr, 0, 0);

        // Draw JSVIM at (0,2)
        attron(COLOR_PAIR(1));
        mvprintw(0, 2, "%s", title);
        attroff(COLOR_PAIR(1));

        // Bottom "gray" bar (full-width)
        attron(COLOR_PAIR(2));
        for (int i = 1; i < maxx - 1; i++)
            mvaddch(maxy - 1, i, ' ');
        attroff(COLOR_PAIR(2));

        // Format current time (HH:MM)
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char tbuf[6];
        strftime(tbuf, sizeof(tbuf), "%H:%M", tm_info);

        // Print time right-aligned: last column - 2
        attron(COLOR_PAIR(2));
        mvprintw(maxy - 1, maxx - 1 - (int)strlen(tbuf) - 1, "%s", tbuf);
        attroff(COLOR_PAIR(2));

        // Keep cursor within the border
        if (cy < 1) cy = 1;
        if (cy > maxy - 2) cy = maxy - 2;
        if (cx < 1) cx = 1;
        if (cx > maxx - 2) cx = maxx - 2;

        move(cy, cx);
        refresh();

        ch = getch();
        if (ch == 27) break; // ESC exits

        switch (ch) {
            case KEY_UP:    cy--; break;
            case KEY_DOWN:  cy++; break;
            case KEY_LEFT:  cx--; break;
            case KEY_RIGHT: cx++; break;
            case KEY_RESIZE:
                // Loop redraws everything automatically
                break;
        }
    }

    endwin();
    return 0;
}
