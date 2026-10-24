/* The 16-bit Windows entry sequence (-mwindows-prologue=exported) and
 * __loadds. Windows patches the first bytes of an exported function and
 * calls it with AX = the program's data segment and DS = the caller's.
 * The test does the same: it patches "push ds; pop ax" to NOPs and calls
 * through a thunk with a wrong DS, and the function must still see its own
 * data. */
#include <stdio.h>

static int failures;
#define CHECK(c)                                                              \
  do {                                                                        \
    if (!(c)) {                                                               \
      printf("FAIL line %d: %s\n", __LINE__, #c);                             \
      failures++;                                                             \
    }                                                                         \
  } while (0)

/* 16 bytes past g_value[0] is g_value[8], 0: what a function reads as
 * g_value[0] when DS is one paragraph off. */
volatile int g_value[16] = {0x1234};

int __export __far patched(int a) { return a + g_value[0]; }
int __export __far unpatched(int a) { return a + g_value[0]; }
__attribute__((noinline)) int __export __far odd_frame(void) {
  /* The saved BP is odd, marking a far frame. Not inlined, so that it has
   * a frame of its own. */
  return *(volatile unsigned *)__builtin_frame_address(0) & 1;
}
int __far __loadds loadds(int a) { return a + g_value[0]; }
int __export __far __loadds exported_loadds(int a) { return a + g_value[0]; }
__attribute__((noinline)) int __far plain(void) {
  return *(volatile unsigned *)__builtin_frame_address(0) & 1;
}
long __export __far __pascal exported_pascal(int a, long b) {
  return a + b + g_value[0];
}

typedef int (__far *farfn)(int);
union code {
  farfn f;
  unsigned char __far *bytes;
  struct {
    unsigned off, seg;
  } p;
};

/* Call f(arg) with AX and DS set, as Windows calls an exported function. */
static int call_with(farfn f, int arg, unsigned ax, unsigned ds) {
  union code u;
  u.f = f;
  unsigned seg = u.p.seg, off = u.p.off;
  int r;
  __asm__ volatile("pushw %%ds\n\t"
                   "pushw %%cx\n\t"
                   "pushw %%dx\n\t"
                   "pushw %%si\n\t"
                   "movw %%di, %%ds\n\t"
                   "movw %%sp, %%bx\n\t"
                   "lcallw *%%ss:2(%%bx)\n\t"
                   "addw $6, %%sp\n\t"
                   "popw %%ds"
                   : "=a"(r), "+c"(seg), "+d"(off)
                   : "S"(arg), "D"(ds), "a"(ax)
                   : "bx", "memory", "cc");
  return r;
}

int main(void) {
  unsigned dgroup;
  __asm__("movw %%ds, %0" : "=r"(dgroup));
  unsigned wrong = dgroup + 1;

  CHECK(patched(1) == 0x1235);
  CHECK(odd_frame() == 1);
  CHECK(plain() == 0);
  CHECK(loadds(2) == 0x1236);
  CHECK(exported_loadds(3) == 0x1237);
  CHECK(exported_pascal(1, 0x10000L) == 0x11235L);

  /* Unpatched, the function takes the caller's DS. */
  CHECK(call_with(unpatched, 5, 0, wrong) == 5);
  /* Patched as Windows does for a program, it takes AX. */
  union code u;
  u.f = patched;
  CHECK(u.bytes[0] == 0x1e && u.bytes[1] == 0x58 && u.bytes[2] == 0x90);
  u.bytes[0] = u.bytes[1] = 0x90;
  CHECK(call_with(patched, 6, dgroup, wrong) == 0x123a);
  /* __loadds loads DGROUP itself, whatever AX and DS are. */
  CHECK(call_with(loadds, 7, wrong, wrong) == 0x123b);
  CHECK(call_with(exported_loadds, 8, wrong, wrong) == 0x123c);
  u.f = exported_loadds;
  CHECK(u.bytes[0] == 0xb8); /* mov ax, DGROUP */

  if (failures == 0)
    printf("ALL PASSED\n");
  return failures != 0;
}
