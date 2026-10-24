/* A DLL. It runs on the stack of the program that calls it, so SS is not
 * DS: locals reached through pointers have to use SS. In the small and
 * medium models, the compiler (-mwindows-dll, which -shared implies) keeps
 * the locals whose address escapes in DGROUP, so plain near pointers to
 * them work, the C library's included. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int cmp(const void *a, const void *b) {
  return *(const int *)a - *(const int *)b;
}

__attribute__((noinline)) static void fill(int *p, int n, int seed) {
  for (int i = 0; i < n; i++)
    p[i] = (i * 7 + seed) % n;
}

__attribute__((noinline)) static int depth(int n, int *acc) {
  int local = n;
  *acc += local;
  return n ? depth(n - 1, acc) + local : 0;
}

/* Near pointers to locals, handed to the C library and to other functions.
 * Returns 0 when all of them work. */
int API NearLocals(int n) {
  int bad = 0;
  char buf[40];
  sprintf(buf, "n=%d %s", n, "dll");
  if (strcmp(buf, "n=7 dll") != 0)
    bad |= 1;
  if (strlen(buf) != 7)
    bad |= 2;
  int arr[10];
  fill(arr, 10, n);
  qsort(arr, 10, sizeof(int), cmp);
  for (int i = 0; i < 10; i++)
    if (arr[i] != i)
      bad |= 4;
  int acc = 0;
  if (depth(10, &acc) != 55 || acc != 55)
    bad |= 8;
  char copy[40];
  memcpy(copy, buf, sizeof(buf));
  if (sscanf(copy, "n=%d", &acc) != 1 || acc != n)
    bad |= 16;
  return bad;
}
