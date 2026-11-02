/* PDCurses on DOS (-lcurses): draw in a box, check what reached video
 * memory, and print the result after endwin(). */
#include <curses.h>
#include <i86.h>
#include <stdio.h>

static unsigned cell(int y, int x) {
  unsigned char mode = *(volatile unsigned char __far *)MK_FP(0x40, 0x49);
  unsigned short __far *v = MK_FP(mode == 7 ? 0xb000 : 0xb800, 0);
  return ((volatile unsigned short __far *)v)[y * COLS + x];
}

int main(void) {
  initscr();
  start_color();
  init_pair(1, COLOR_WHITE, COLOR_BLUE);
  WINDOW *w = newwin(5, 30, 2, 4);
  wbkgd(w, COLOR_PAIR(1));
  box(w, 0, 0);
  mvwprintw(w, 2, 2, "curses %d", 42);
  wrefresh(w);
  int ok = cell(2, 4) == 0x17da && cell(4, 6) == 0x1763 && cell(4, 13) == 0x1734;
  endwin();
  printf("curses: %s %s\n", ok ? "ok" : "wrong screen", PDC_VERDOT);
  return !ok;
}
