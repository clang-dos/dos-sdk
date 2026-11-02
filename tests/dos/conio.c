/* <conio.h> and the Turbo C parts of <dos.h>: the text window, cursor,
 * colors, screen copies and port I/O helpers, int86 and intdos, and
 * delay. Prints ALL PASSED. */
#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void) {
  struct text_info ti;
  gettextinfo(&ti);
  CHECK(ti.screenwidth == 80);
  CHECK(ti.screenheight >= 25);

  /* A window: coordinates count from its corner, and output stays in it. */
  window(11, 6, 30, 10);
  textattr(0x1e);
  clrscr();
  CHECK(wherex() == 1 && wherey() == 1);
  cputs("abc");
  CHECK(wherex() == 4 && wherey() == 1);
  gotoxy(5, 3);
  CHECK(wherex() == 5 && wherey() == 3);
  CHECK(cprintf("%d-%s", 42, "x") == 4);
  CHECK(wherex() == 9);
  gettextinfo(&ti);
  CHECK(ti.winleft == 11 && ti.wintop == 6 && ti.winright == 30 && ti.winbottom == 10);
  CHECK(ti.attribute == 0x1e);
  /* Wrapping at the right edge. */
  gotoxy(19, 1);
  cputs("WXYZ");
  CHECK(wherex() == 3 && wherey() == 2);

  /* What was written is in video memory, in the color. */
  unsigned char cells[8];
  CHECK(gettext(11, 6, 13, 6, cells));
  CHECK(cells[0] == 'a' && cells[1] == 0x1e && cells[2] == 'b' && cells[4] == 'c');
  CHECK(gettext(15, 8, 18, 8, cells));
  CHECK(memcmp(cells, "4\x1e" "2\x1e-\x1ex\x1e", 8) == 0);

  /* puttext and movetext. */
  static const unsigned char put[4] = { 'Q', 0x4f, 'R', 0x4f };
  CHECK(puttext(1, 20, 2, 20, put));
  CHECK(movetext(1, 20, 2, 20, 41, 21));
  CHECK(gettext(41, 21, 42, 21, cells));
  CHECK(memcmp(cells, put, 4) == 0);
  CHECK(!gettext(0, 1, 5, 1, cells));
  CHECK(!puttext(79, 1, 81, 1, put));

  /* Colors. */
  textcolor(LIGHTRED);
  textbackground(BLUE);
  gettextinfo(&ti);
  CHECK(ti.attribute == 0x1c);
  lowvideo();
  gettextinfo(&ti);
  CHECK(ti.attribute == 0x14);
  highvideo();
  gettextinfo(&ti);
  CHECK(ti.attribute == 0x1c);
  textcolor(WHITE + BLINK);
  gettextinfo(&ti);
  CHECK(ti.attribute == 0x9f);

  /* Scrolling: the last line moves up. */
  window(1, 1, 80, ti.screenheight);
  textattr(0x07);
  clrscr();
  window(1, 1, 10, 2);
  cputs("line1\r\nline2\r\nline3");
  CHECK(wherey() == 2);
  unsigned char row[10];
  CHECK(gettext(1, 1, 5, 1, row) && memcmp(row, "l\7i\7n\7e\0072\7", 10) == 0);
  window(1, 1, 80, 25);

  /* ungetch and kbhit. */
  CHECK(ungetch('z') == 'z');
  CHECK(kbhit());
  CHECK(getch() == 'z');

  /* int86: INT 10h function 0Fh gives the mode and columns. */
  union REGS r;
  r.h.ah = 0x0f;
  int86(0x10, &r, &r);
  CHECK(r.h.ah == 80);
  /* intdos: the DOS version, and the carry flag of a failing call. */
  r.x.ax = 0x3000;
  intdos(&r, &r);
  CHECK(r.h.al >= 2);
  r.x.ax = 0x3d00;  /* open */
  r.x.dx = (unsigned)(unsigned long)"NOSUCH.FIL";
  struct SREGS sr;
  segread(&sr);
  char __far *name = "NOSUCH.FIL";
  sr.ds = FP_SEG(name);
  r.x.dx = FP_OFF(name);
  intdosx(&r, &r, &sr);
  CHECK(r.x.cflag);
  CHECK(r.x.ax == 2);
  /* segread: CS and DS are what the program runs with. */
  unsigned short ds;
  __asm__("movw %%ds, %0" : "=r"(ds));
  segread(&sr);
  CHECK(sr.ds == ds);
  CHECK(peekb(0x40, 0x4a) == 80);

  delay(1);

  textattr(0x07);
  clrscr();
  if (!fails)
    printf("ALL PASSED\n");
  return fails;
}
