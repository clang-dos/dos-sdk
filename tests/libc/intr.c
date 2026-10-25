/* __interrupt functions, _dos_getvect, _dos_setvect and _chain_intr:
 * handlers called with INT, with a different DS, through a pointer, by
 * chaining, and from the timer interrupt while the program computes. */
#include <dos.h>
#include <stdio.h>

static int failures;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL line %d: %s\n", __LINE__, #c);                              \
      failures++;                                                              \
    }                                                                          \
  } while (0)

typedef void (__interrupt __far *handler_t)();

static volatile unsigned n60, n62, ticks;
static volatile unsigned seen_ax, seen_dx, seen_ds;
static handler_t old60, old61, old62, old08, next62;

/* Open Watcom style: the registers as union INTPACK, which the handler
 * changes: AX = AX + BX, BX = 1234h, CL = 56h, and the carry set when the
 * sum is 0. */
void __interrupt __far on60(union INTPACK r) {
  n60++;
  r.w.ax += r.w.bx;
  r.w.bx = 0x1234;
  r.h.cl = 0x56;
  if (r.w.ax == 0)
    r.w.flags |= INTR_CF;
  else
    r.w.flags &= ~INTR_CF;
}

/* Microsoft C style register parameters; no __far, which __interrupt
 * implies. */
void __interrupt on61(unsigned es, unsigned ds, unsigned di, unsigned si,
                      unsigned bp, unsigned sp, unsigned bx, unsigned dx,
                      unsigned cx, unsigned ax) {
  seen_ax = ax;
  seen_dx = dx;
  seen_ds = ds;
}

/* Adds 1 to BX and goes on to next62 (on60). */
void __interrupt __far on62(union INTPACK r) {
  n62++;
  r.w.bx++;
  _chain_intr(next62);
}

void __interrupt __far on08(void) {
  ticks++;
  _chain_intr(old08);
}

/* INT 60h or 62h with AX and BX; returns AX, with BX, CX and the carry. */
static unsigned call(int n, unsigned ax, unsigned *bx, unsigned *cx,
                     unsigned *cf) {
  unsigned b = *bx, c = 0, f;
  if (n == 0x60)
    __asm__ volatile("int $0x60\n\tpushfw\n\tpopw %3"
                     : "+a"(ax), "+b"(b), "+c"(c), "=r"(f)
                     :
                     : "dx", "memory");
  else
    __asm__ volatile("int $0x62\n\tpushfw\n\tpopw %3"
                     : "+a"(ax), "+b"(b), "+c"(c), "=r"(f)
                     :
                     : "dx", "memory");
  *bx = b;
  *cx = c;
  *cf = f & INTR_CF;
  return ax;
}

/* A sum that every register takes part in, to see that the timer
 * interrupt leaves them alone. */
static unsigned long work(unsigned n) {
  unsigned long s = 0;
  for (unsigned i = 1; i <= n; i++)
    s += (unsigned long)i * i ^ (i >> 3);
  return s;
}

int main(void) {
  unsigned bx, cx, cf, ax;

  old60 = _dos_getvect(0x60);
  old61 = _dos_getvect(0x61);
  old62 = _dos_getvect(0x62);
  old08 = _dos_getvect(0x08);

  _dos_setvect(0x60, on60);
  CHECK(_dos_getvect(0x60) == (handler_t)on60);

  bx = 5;
  ax = call(0x60, 10, &bx, &cx, &cf);
  CHECK(ax == 15 && bx == 0x1234 && (cx & 0xff) == 0x56 && !cf);
  CHECK(n60 == 1);
  bx = 1;
  ax = call(0x60, 0xffff, &bx, &cx, &cf);
  CHECK(ax == 0 && cf);

  /* The handler loads DS itself. */
  unsigned ds;
  __asm__("movw %%ds, %0" : "=r"(ds));
  __asm__ volatile("pushw %%ds\n\tmovw %1, %%ds\n\tint $0x60\n\tpopw %%ds"
                   : "+a"(ax)
                   : "r"(0x40), "b"(0)
                   : "cx", "dx", "memory");
  CHECK(n60 == 3);

  /* Microsoft C register parameters. */
  _dos_setvect(0x61, on61);
  __asm__ volatile("int $0x61"
                   :
                   : "a"(0x1111), "d"(0x2222)
                   : "bx", "cx", "memory");
  CHECK(seen_ax == 0x1111 && seen_dx == 0x2222 && seen_ds == ds);

  /* Chaining: on62 adds 1 to BX, then on60 runs as if called directly. */
  next62 = _dos_getvect(0x60);
  _dos_setvect(0x62, on62);
  bx = 2;
  ax = call(0x62, 40, &bx, &cx, &cf);
  CHECK(ax == 43 && bx == 0x1234 && (cx & 0xff) == 0x56 && !cf);
  CHECK(n62 == 1 && n60 == 4);

  /* A call through a pointer to an interrupt function. */
  handler_t p = _dos_getvect(0x60);
  p();
  CHECK(n60 == 5);

  /* The timer interrupt, chained to the previous handler, comes in the
   * middle of the computation. */
  unsigned long expect = work(2000);
  _dos_setvect(0x08, on08);
  unsigned rounds = 0;
  while (ticks < 3 && rounds < 2000) {
    CHECK(work(2000) == expect);
    rounds++;
  }
  _dos_setvect(0x08, old08);
  CHECK(ticks >= 3);
  printf("ticks=%u rounds=%u\n", ticks, rounds);

  _dos_setvect(0x60, old60);
  _dos_setvect(0x61, old61);
  _dos_setvect(0x62, old62);
  CHECK(_dos_getvect(0x60) == old60);

  if (failures == 0)
    printf("ALL PASSED\n");
  return failures != 0;
}
