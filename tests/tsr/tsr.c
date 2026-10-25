/* A resident program: INT 60h answers AX = 5453h ("TS") with BX = the
 * number of calls and CX = the timer ticks it has counted on INT 1Ch, which
 * it chains to the previous handler. With an argument it keeps that many
 * paragraphs; without, _dos_keep(0, 0) keeps the program up to its heap. */
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned calls;
static volatile unsigned ticks;
static void (__interrupt __far *old1c)();

void __interrupt __far on60(union INTPACK r) {
  if (r.w.ax != 0x5453)
    return;
  calls++;
  r.w.bx = calls;
  r.w.cx = ticks;
  r.w.ax = 0;
}

void __interrupt __far on1c(void) {
  ticks++;
  _chain_intr(old1c);
}

int main(int argc, char **argv) {
  /* A buffer on the heap, which stays resident too. */
  char *buf = malloc(1000);
  if (!buf)
    return 1;
  old1c = _dos_getvect(0x1c);
  _dos_setvect(0x1c, on1c);
  _dos_setvect(0x60, on60);
  printf("resident\n");
  fflush(stdout);
  _dos_keep(0, argc > 1 ? (unsigned)atoi(argv[1]) : 0);
}
