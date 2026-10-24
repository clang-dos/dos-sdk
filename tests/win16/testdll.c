/* A DLL. It runs on the stack of the program that calls it, so SS is not
 * DS: locals reached through pointers have to use SS. In the small and
 * medium models, pointers to locals have to be far (__far) for that. */
#include <stdarg.h>
#include <stdio.h>

#define API __export __far __pascal
#define FARLOCAL __far /* a pointer that can point to a local */

static int counter = 100;
static int inited;
static volatile int k = 5;

int __far __pascal LibMain(unsigned inst, unsigned ds, unsigned heap,
                           char __far *cmd) {
  inited = inst != 0 && ds != 0;
  return 1;
}

int API AddNumbers(int a, int b) { return a + b + inited; }
int API NextCounter(void) { return ++counter; }

void API FillBuf(char __far *buf, int n) {
  for (int i = 0; i < n; i++)
    buf[i] = 'a' + i;
  buf[n] = 0;
}

struct big {
  int a[40];
};

__attribute__((noinline)) static struct big make(int n) {
  struct big b;
  for (int i = 0; i < 40; i++)
    b.a[i] = i * n;
  return b;
}

__attribute__((noinline)) static int sum(int FARLOCAL *p, int n) {
  int s = 0;
  while (n--)
    s += *p++;
  return s;
}

__attribute__((noinline)) static int vsum(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int s = 0;
  while (n--)
    s += va_arg(ap, int);
  va_end(ap);
  return s;
}

/* Locals through registers, far pointers to locals, a returned structure
 * and variable arguments. Returns 0 when all of them work. */
int API StackWork(int n) {
  int bad = 0;
  int arr[16];
  for (int i = 0; i < 16; i++)
    arr[i] = i + n;
  if (arr[k] != 5 + n)
    bad |= 1;
  if (sum(arr, 16) != 120 + 16 * n)
    bad |= 2;
  struct big x = make(n), y;
  y = x;
  x.a[3] = 0;
  if (y.a[k + 1] != 6 * n || x.a[3] != 0)
    bad |= 4;
  if (vsum(3, n, 2 * n, 3 * n) != 6 * n)
    bad |= 8;
  char buf[8];
  char FARLOCAL *s = buf;
  for (int i = 0; i < 6; i++)
    s[i] = "stack"[i];
  for (int i = 0; i < 6; i++)
    if (s[i] != "stack"[i])
      bad |= 16;
  return bad;
}
