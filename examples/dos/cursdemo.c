/* A tour of PDCurses (-lcurses) on DOS: a menu, the 16 colors, panels that
 * move over each other, and the keys and mouse clicks curses reports.
 *
 *     clang --target=i8086-pc-msdos -Os cursdemo.c -lcurses -o cursdemo.exe
 *
 * Up and Down (or the mouse) choose a page, Enter or Tab goes into it, Esc
 * or F10 quits. */
#include <curses.h>
#include <panel.h>
#include <string.h>

static const char *pages[] = { "Colors", "Panels", "Keys and mouse",
                               "Borders", "Quit" };
#define NPAGES (int)(sizeof pages / sizeof pages[0])

static WINDOW *menu, *body;

static void frame(WINDOW *w, const char *title) {
  box(w, 0, 0);
  mvwprintw(w, 0, 2, " %s ", title);
}

static void draw_menu(int sel) {
  werase(menu);
  frame(menu, "Menu");
  for (int i = 0; i < NPAGES; i++) {
    if (i == sel)
      wattron(menu, A_REVERSE);
    mvwprintw(menu, i + 2, 2, " %-16s", pages[i]);
    wattroff(menu, A_REVERSE);
  }
  wnoutrefresh(menu);
}

static void status(const char *s) {
  attron(COLOR_PAIR(1));
  mvhline(LINES - 1, 0, ' ', COLS);
  mvaddstr(LINES - 1, 1, s);
  attroff(COLOR_PAIR(1));
  wnoutrefresh(stdscr);
}

/* Each foreground on each background, with the color numbers. */
static void page_colors(void) {
  werase(body);
  frame(body, "Colors");
  for (int bg = 0; bg < 8; bg++)
    for (int fg = 0; fg < 8; fg++)
      init_pair(16 + bg * 8 + fg, fg, bg);
  for (int bg = 0; bg < 8; bg++) {
    for (int fg = 0; fg < 8; fg++) {
      wattrset(body, COLOR_PAIR(16 + bg * 8 + fg));
      mvwprintw(body, 2 + bg * 2, 2 + fg * 6, " %d/%d ", fg, bg);
      wattron(body, A_BOLD);
      mvwprintw(body, 3 + bg * 2, 2 + fg * 6, " %d/%d ", fg + 8, bg);
    }
  }
  wattrset(body, A_NORMAL);
  mvwaddstr(body, 18, 2, "Bold gives the bright foreground colors.");
  wnoutrefresh(body);
}

/* Three panels that slide across each other until a key is pressed. */
static void page_panels(void) {
  werase(body);
  frame(body, "Panels");
  mvwaddstr(body, 18, 2, "Panels keep their stacking order. Press a key.");
  wnoutrefresh(body);
  int by, bx, h, w;
  getbegyx(body, by, bx);
  getmaxyx(body, h, w);
  WINDOW *win[3];
  PANEL *pan[3];
  int y[3] = { 2, 5, 8 }, x[3] = { 2, 14, 26 }, dy[3] = { 1, -1, 1 },
      dx[3] = { 2, 1, -1 };
  static const char *names[3] = { "First", "Second", "Third" };
  for (int i = 0; i < 3; i++) {
    win[i] = newwin(7, 18, by + y[i], bx + x[i]);
    wbkgd(win[i], COLOR_PAIR(2 + i));
    frame(win[i], names[i]);
    mvwprintw(win[i], 3, 3, "panel %d", i + 1);
    pan[i] = new_panel(win[i]);
  }
  nodelay(stdscr, TRUE);
  for (int t = 0; getch() == ERR; t++) {
    for (int i = 0; i < 3; i++) {
      if (y[i] + dy[i] < 1 || y[i] + dy[i] + 7 > h - 3)
        dy[i] = -dy[i];
      if (x[i] + dx[i] < 1 || x[i] + dx[i] + 18 > w - 1)
        dx[i] = -dx[i];
      y[i] += dy[i];
      x[i] += dx[i];
      move_panel(pan[i], by + y[i], bx + x[i]);
    }
    if (t % 8 == 0)
      top_panel(pan[(t / 8) % 3]);
    update_panels();
    doupdate();
    napms(80);
  }
  nodelay(stdscr, FALSE);
  for (int i = 0; i < 3; i++) {
    del_panel(pan[i]);
    delwin(win[i]);
  }
  touchwin(stdscr);
  touchwin(menu);
}

/* Show the name of each key and where the mouse was clicked. */
static void page_keys(void) {
  werase(body);
  frame(body, "Keys and mouse");
  mvwaddstr(body, 2, 2, "Press keys, or click with the mouse. Esc goes back.");
  wnoutrefresh(body);
  doupdate();
  int line = 4;
  for (;;) {
    int c = getch();
    if (c == 27)
      break;
    if (line > 17) {
      wmove(body, 4, 1);
      wclrtobot(body);
      frame(body, "Keys and mouse");
      line = 4;
    }
    if (c == KEY_MOUSE) {
      MEVENT ev;
      if (nc_getmouse(&ev) == OK)
        mvwprintw(body, line++, 2, "mouse: %s at row %2d, column %2d",
                  ev.bstate & BUTTON1_DOUBLE_CLICKED ? "double click"
                  : ev.bstate & BUTTON3_CLICKED      ? "right click"
                                                     : "click",
                  ev.y, ev.x);
    } else {
      mvwprintw(body, line++, 2, "%-12s code %4d (0x%03x)", keyname(c), c, c);
    }
    wrefresh(body);
  }
}

/* The line drawing characters of the alternate character set. */
static void page_borders(void) {
  werase(body);
  frame(body, "Borders");
  WINDOW *a = derwin(body, 6, 20, 2, 3);
  wborder(a, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE, ACS_ULCORNER,
          ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
  mvwaddstr(a, 2, 4, "single line");
  WINDOW *b = derwin(body, 6, 20, 2, 28);
  wattron(b, COLOR_PAIR(3));
  box(b, '*', '-');
  mvwaddstr(b, 2, 4, "any character");
  mvwaddch(body, 10, 3, ACS_LTEE);
  whline(body, ACS_HLINE, 10);
  mvwaddch(body, 10, 14, ACS_PLUS);
  mvwaddch(body, 10, 26, ACS_RTEE);
  mvwhline(body, 10, 15, ACS_HLINE, 11);
  mvwaddstr(body, 12, 3, "Blocks:");
  waddch(body, ' ');
  waddch(body, ACS_BLOCK);
  waddch(body, ACS_CKBOARD);
  waddch(body, ACS_BOARD);
  waddch(body, ' ');
  waddch(body, ACS_DIAMOND);
  waddch(body, ACS_BULLET);
  waddch(body, ACS_DEGREE);
  waddch(body, ACS_PLMINUS);
  waddch(body, ' ');
  waddch(body, ACS_LARROW);
  waddch(body, ACS_RARROW);
  waddch(body, ACS_UARROW);
  waddch(body, ACS_DARROW);
  delwin(a);
  delwin(b);
  wnoutrefresh(body);
}

/* The pages that wait for keys start when Enter is pressed. */
static void placeholder(int page) {
  werase(body);
  frame(body, pages[page]);
  mvwaddstr(body, 2, 2,
            page == NPAGES - 1 ? "Press Enter to leave the demo."
                               : "Press Enter to start this page.");
  wnoutrefresh(body);
}

static void show(int page) {
  switch (page) {
  case 0: page_colors(); break;
  case 3: page_borders(); break;
  default: placeholder(page); break;
  }
}

int main(void) {
  initscr();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON3_CLICKED, NULL);
  start_color();
  init_pair(1, COLOR_BLACK, COLOR_CYAN);
  init_pair(2, COLOR_WHITE, COLOR_BLUE);
  init_pair(3, COLOR_YELLOW, COLOR_RED);
  init_pair(4, COLOR_BLACK, COLOR_GREEN);
  init_pair(5, COLOR_WHITE, COLOR_BLACK);

  bkgd(COLOR_PAIR(5));
  attron(COLOR_PAIR(1));
  mvhline(0, 0, ' ', COLS);
  mvprintw(0, 1, "PDCurses %s on DOS, built with clang-dos", PDC_VERDOT);
  attroff(COLOR_PAIR(1));
  status("Up/Down choose  Enter/Tab open  Esc/F10 quit");

  menu = newwin(LINES - 2, 22, 1, 0);
  body = newwin(LINES - 2, COLS - 22, 1, 22);
  wbkgd(menu, COLOR_PAIR(2));
  wbkgd(body, COLOR_PAIR(5));
  /* The menu and the page are panels too, so that the moving panels
     restore them as they go by. */
  new_panel(menu);
  new_panel(body);
  int sel = 0;
  draw_menu(sel);
  show(sel);
  doupdate();

  for (;;) {
    int c = getch();
    int old = sel;
    if (c == KEY_UP && sel > 0)
      sel--;
    else if (c == KEY_DOWN && sel < NPAGES - 1)
      sel++;
    else if (c == KEY_HOME)
      sel = 0;
    else if (c == KEY_END)
      sel = NPAGES - 1;
    else if (c == 27 || c == KEY_F(10))
      break;
    else if (c == KEY_MOUSE) {
      MEVENT ev;
      if (nc_getmouse(&ev) == OK && (ev.bstate & BUTTON1_CLICKED) &&
          wenclose(menu, ev.y, ev.x)) {
        int i = ev.y - getbegy(menu) - 2;
        if (i >= 0 && i < NPAGES)
          sel = i;
        c = '\n';
      }
    }
    if (c == '\n' || c == '\r' || c == '\t' || c == KEY_ENTER) {
      if (sel == NPAGES - 1)
        break;
      if (sel == 1 || sel == 2) {
        status(sel == 1 ? "Any key stops the panels"
                        : "Esc goes back to the menu");
        if (sel == 1)
          page_panels();
        else
          page_keys();
        status("Up/Down choose  Enter/Tab open  Esc/F10 quit");
        draw_menu(sel);
        placeholder(sel);
      }
    }
    if (sel != old) {
      draw_menu(sel);
      show(sel);
    }
    doupdate();
  }
  endwin();
  return 0;
}
