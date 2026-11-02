/* PDCurses (-lcurses): windows, colors and attributes, the line drawing
 * characters, scrolling, panels and input with no key pressed. Checks
 * what refresh() puts in video memory, then prints ALL PASSED after
 * endwin(). */
#include <curses.h>
#include <panel.h>
#include <i86.h>
#include <stdio.h>
#include <string.h>

static int fails;
static char msgs[16][64];
#define CHECK(c)                                                          \
  do {                                                                    \
    if (!(c)) {                                                           \
      if (fails < 16)                                                     \
        snprintf(msgs[fails], sizeof msgs[0], "FAIL %d: %s", __LINE__, #c); \
      fails++;                                                            \
    }                                                                     \
  } while (0)

/* The character and attribute bytes at a screen position. */
static unsigned cell(int y, int x) {
  unsigned char mode = *(volatile unsigned char __far *)MK_FP(0x40, 0x49);
  unsigned seg = mode == 7 ? 0xb000 : 0xb800;
  return ((volatile unsigned short __far *)MK_FP(seg, 0))[y * COLS + x];
}
#define CH(y, x) (cell(y, x) & 0xff)
#define AT(y, x) (cell(y, x) >> 8)

static int row_is(int y, int x, const char *s) {
  for (; *s; s++, x++)
    if (CH(y, x) != (unsigned char)*s)
      return 0;
  return 1;
}

int main(void) {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  CHECK(LINES >= 25 && COLS == 80);
  CHECK(has_colors());
  CHECK(start_color() == OK);
  CHECK(COLORS >= 8 && COLOR_PAIRS >= 64);

  /* Text and attributes on stdscr. */
  CHECK(init_pair(1, COLOR_WHITE, COLOR_BLUE) == OK);
  CHECK(init_pair(2, COLOR_YELLOW, COLOR_RED) == OK);
  mvaddstr(0, 0, "Hello");
  attron(COLOR_PAIR(1));
  mvaddstr(1, 0, "blue");
  attron(A_BOLD);
  mvaddstr(2, 0, "bold");
  attrset(A_REVERSE);
  mvaddstr(3, 0, "rev");
  attrset(A_NORMAL);
  mvprintw(4, 0, "%d-%s-%05lu", 42, "x", 123UL);
  mvaddch(5, 0, ACS_ULCORNER);
  addch(ACS_HLINE);
  addch(ACS_URCORNER);
  refresh();
  CHECK(row_is(0, 0, "Hello") && AT(0, 0) == 0x07);
  CHECK(row_is(1, 0, "blue") && AT(1, 0) == 0x17);
  CHECK(row_is(2, 0, "bold") && AT(2, 3) == 0x1f);
  CHECK(row_is(3, 0, "rev") && AT(3, 0) == 0x70);
  CHECK(row_is(4, 0, "42-x-00123"));
  CHECK(CH(5, 0) == 0xda && CH(5, 1) == 0xc4 && CH(5, 2) == 0xbf);
  CHECK((mvinch(1, 1) & A_CHARTEXT) == 'l');
  CHECK(PAIR_NUMBER(mvinch(1, 1)) == 1);
  char buf[16];
  CHECK(mvinnstr(0, 0, buf, 5) == 5 && strcmp(buf, "Hello") == 0);

  /* A window with a border and a background. */
  WINDOW *w = newwin(6, 20, 8, 30);
  CHECK(w != NULL);
  wbkgd(w, COLOR_PAIR(2));
  box(w, 0, 0);
  mvwaddstr(w, 1, 1, "in a box");
  wrefresh(w);
  CHECK(CH(8, 30) == 0xda && CH(8, 49) == 0xbf && CH(13, 30) == 0xc0 &&
        CH(13, 49) == 0xd9 && CH(9, 30) == 0xb3);
  CHECK(row_is(9, 31, "in a box") && AT(9, 31) == 0x46);
  CHECK(AT(12, 48) == 0x46);
  int y, x;
  getyx(w, y, x);
  CHECK(y == 1 && x == 9);
  getbegyx(w, y, x);
  CHECK(y == 8 && x == 30);

  /* Scrolling inside a window. */
  WINDOW *s = newwin(3, 10, 16, 0);
  scrollok(s, TRUE);
  waddstr(s, "one\ntwo\nthree\nfour");
  wrefresh(s);
  CHECK(row_is(16, 0, "two") && row_is(17, 0, "three") && row_is(18, 0, "four"));

  /* Panels: the top one covers the other where they overlap. */
  WINDOW *pw1 = newwin(3, 10, 20, 50);
  WINDOW *pw2 = newwin(3, 10, 21, 55);
  wbkgd(pw1, '1');
  wbkgd(pw2, '2');
  PANEL *p1 = new_panel(pw1);
  PANEL *p2 = new_panel(pw2);
  update_panels();
  doupdate();
  CHECK(CH(20, 50) == '1' && CH(21, 54) == '1' && CH(21, 55) == '2' &&
        CH(23, 64) == '2');
  top_panel(p1);
  update_panels();
  doupdate();
  CHECK(CH(21, 55) == '1' && CH(22, 59) == '1' && CH(22, 60) == '2');
  CHECK(panel_below(p1) == p2);
  hide_panel(p1);
  update_panels();
  doupdate();
  CHECK(CH(21, 55) == '2' && CH(20, 50) == ' ');
  del_panel(p1);
  del_panel(p2);
  delwin(pw1);
  delwin(pw2);

  /* Input: no key is waiting, ungetch puts one back. */
  nodelay(stdscr, TRUE);
  CHECK(getch() == ERR);
  CHECK(ungetch(KEY_F(2)) == OK);
  CHECK(getch() == KEY_F(2));
  CHECK(strcmp(keyname(KEY_F(2)), "KEY_F(2)") == 0);
  CHECK(strcmp(keyname('a'), "a") == 0);
  napms(60);

  delwin(s);
  delwin(w);
  endwin();
  CHECK(isendwin());

  for (int i = 0; i < fails && i < 16; i++)
    printf("%s\n", msgs[i]);
  if (fails)
    printf("%d FAILED\n", fails);
  else
    printf("ALL PASSED\n");
  return fails != 0;
}
