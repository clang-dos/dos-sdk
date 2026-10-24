/* The near and far functions of DOS compilers: _fmemcpy, _fstrlen and the
 * rest, the far heap (_fmalloc) and the near heap (_nmalloc), and the
 * standard functions called with near or far pointers. It builds as C and
 * as C++. */
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL line %d: %s\n", __LINE__, #c);                              \
      failures++;                                                              \
    }                                                                          \
  } while (0)

typedef char __far *fstr;

static void far_strings(void) {
  fstr a = (fstr)_fmalloc(64);
  fstr b = (fstr)_fmalloc(64);
  CHECK(a && b);
  if (!a || !b)
    return;
  _fstrcpy(a, "Hello");
  CHECK(_fstrlen(a) == 5);
  _fstrcat(a, ", World");
  CHECK(_fstrcmp(a, "Hello, World") == 0);
  CHECK(_fstricmp(a, "hELLO, wORLD") == 0);
  CHECK(_fstrncmp(a, "Help", 3) == 0 && _fstrncmp(a, "Help", 4) < 0);
  CHECK(_fstrnicmp(a, "HELP", 3) == 0);
  CHECK(_fstrchr(a, 'o') == a + 4 && _fstrrchr(a, 'o') == a + 8);
  CHECK(_fstrchr(a, 'z') == NULL && _fstrchr(a, 0) == a + 12);
  CHECK(_fstrstr(a, "World") == a + 7 && _fstrstr(a, "xyz") == NULL);
  CHECK(_fstrspn(a, "leH") == 4 && _fstrcspn(a, " ,") == 5);
  CHECK(_fstrpbrk(a, "dW") == a + 7);
  _fstrncpy(b, a, 3);
  CHECK(b[0] == 'H' && b[2] == 'l');
  _fstrncpy(b, "ab", 5);
  CHECK(b[1] == 'b' && b[2] == 0 && b[4] == 0);
  _fstrncat(b, "cdef", 2);
  CHECK(_fstrcmp(b, "abcd") == 0);
  _fstrupr(b);
  CHECK(_fstrcmp(b, "ABCD") == 0);
  _fstrlwr(b);
  _fstrrev(b);
  CHECK(_fstrcmp(b, "dcba") == 0);
  _fstrnset(b, 'x', 2);
  CHECK(_fstrcmp(b, "xxba") == 0);
  _fstrset(b, 'y');
  CHECK(_fstrcmp(b, "yyyy") == 0);

  /* Copies and fills, odd and even lengths, and overlapping moves. */
  for (int i = 0; i < 64; i++)
    a[i] = (char)i;
  _fmemmove(a + 1, a, 20);
  CHECK(a[1] == 0 && a[20] == 19 && a[21] == 21);
  _fmemmove(a, a + 1, 20);
  CHECK(a[0] == 0 && a[19] == 19 && a[20] == 19);
  _fmemset(b, 0x5a, 33);
  CHECK(b[0] == 0x5a && b[32] == 0x5a && b[33] != 0x5a);
  _fmemcpy(b, a, 31);
  CHECK(_fmemcmp(b, a, 31) == 0 && b[31] == 0x5a);
  CHECK(_fmemchr(a, 10, 64) == a + 10 && _fmemchr(a, 100, 64) == NULL);
  _fstrcpy(a, "abc:def");
  CHECK(_fmemccpy(b, a, ':', 10) == b + 4 && b[3] == ':');
  CHECK(_fmemicmp("ABC", a, 3) == 0);

  _fstrcpy(a, " one, two  three");
  char __far *t = _fstrtok(a, " ,");
  CHECK(t && _fstrcmp(t, "one") == 0);
  t = _fstrtok(NULL, " ,");
  CHECK(t && _fstrcmp(t, "two") == 0);
  t = _fstrtok(NULL, " ,");
  CHECK(t && _fstrcmp(t, "three") == 0);
  CHECK(_fstrtok(NULL, " ,") == NULL);

  char __far *d = _fstrdup(a + 1); /* strtok ended "one" */
  CHECK(d && _fstrcmp(d, "one") == 0);
  _ffree(d);
  _ffree(a);
  _ffree(b);
}

#if defined(__TINY__) || defined(__SMALL__) || defined(__MEDIUM__)
#define NEAR_DATA 1
#endif

static void far_heap(void) {
  /* Blocks that need several segments. In the compact and large models
   * the far heap is the heap of malloc, of up to 64 KiB. */
#if defined(NEAR_DATA) || defined(__HUGE__)
  enum { N = 6, SIZE = 30000 };
#else
  enum { N = 4, SIZE = 10000 };
#endif
  char __far *p[N];
  for (int i = 0; i < N; i++) {
    p[i] = (char __far *)_fmalloc(SIZE);
    CHECK(p[i] != NULL);
    if (p[i])
      _fmemset(p[i], 'a' + i, SIZE);
  }
  for (int i = 0; i < N; i++)
    if (p[i])
      CHECK(p[i][0] == 'a' + i && p[i][SIZE - 1] == 'a' + i);
  for (int i = 0; i < N; i++)
    _ffree(p[i]);

  /* Small blocks, freed and reused. */
  char __far *q[40];
  for (int i = 0; i < 40; i++) {
    q[i] = (char __far *)_fmalloc(100 + i);
    CHECK(q[i] && _fmsize(q[i]) >= 100u + i);
    if (q[i])
      _fmemset(q[i], i, 100 + i);
  }
  for (int i = 0; i < 40; i += 2)
    _ffree(q[i]);
  for (int i = 1; i < 40; i += 2)
    CHECK(q[i][0] == i && q[i][99 + i] == i);
  for (int i = 0; i < 40; i += 2)
    q[i] = (char __far *)_fmalloc(50);
  for (int i = 0; i < 40; i++)
    _ffree(q[i]);

  /* realloc grows a block where it is when the space after it is free,
   * and otherwise moves it, keeping its contents. */
  char __far *r = (char __far *)_fmalloc(10);
  _fstrcpy(r, "realloc");
  r = (char __far *)_frealloc(r, 2000);
  CHECK(r && _fstrcmp(r, "realloc") == 0 && _fmsize(r) >= 2000);
  char __far *s = (char __far *)_fmalloc(10);
  r = (char __far *)_frealloc(r, 20000);
  CHECK(r && _fstrcmp(r, "realloc") == 0);
  _ffree(s);
  _ffree(r);

  char __far *z = (char __far *)_fcalloc(100, 3);
  CHECK(z != NULL);
  int nz = 0;
  for (int i = 0; z && i < 300; i++)
    nz += z[i] != 0;
  CHECK(nz == 0);
  _ffree(z);
#ifdef NEAR_DATA
  /* A block of the far heap is in one segment. */
  CHECK(_fmalloc(0xfff8) == NULL);
#endif
}

static void near_heap(void) {
  char __near *a = (char __near *)_nmalloc(100);
  CHECK(a != NULL);
  for (int i = 0; i < 100; i++)
    a[i] = (char)i;
  CHECK(_nmsize(a) >= 100);
  a = (char __near *)_nrealloc(a, 300);
  CHECK(a && a[99] == 99);
  char __near *z = (char __near *)_ncalloc(10, 10);
  CHECK(z && z[0] == 0 && z[99] == 0);
  _nfree(z);
  /* A near pointer reaches the block through DS, as a near pointer to a
   * variable would. */
  static char __near *keep;
  keep = a;
  CHECK(keep[50] == 50);
  _nfree(a);

  /* The near heap ends; all of it comes back after a free. */
  char __near *blocks[200];
  int n = 0;
  while (n < 200 && (blocks[n] = (char __near *)_nmalloc(256)) != NULL)
    n++;
  CHECK(n > 4);
  for (int i = 0; i < n; i++)
    _nfree(blocks[i]);
  int m = 0;
  while (m < 200 && (blocks[m] = (char __near *)_nmalloc(256)) != NULL)
    m++;
  CHECK(m == n);
  for (int i = 0; i < m; i++)
    _nfree(blocks[i]);
}

/* The standard functions take near and far pointers: with near data, the
 * header's overloads call the far functions for far pointers; with far
 * data, near pointers convert. */
static void standard_names(void) {
  char __far *f = (char __far *)_fmalloc(32);
  char __near *n = (char __near *)_nmalloc(32);
  char local[32];
  CHECK(f && n);
  if (!f || !n)
    return;
  memcpy(f, "far", 4);
  strcpy(n, "near");
  CHECK(strlen(f) == 3 && strlen(n) == 4);
  strcat(f, "-");
  strcat(f, n);
  CHECK(strcmp(f, "far-near") == 0);
  memcpy(local, f, strlen(f) + 1);
  CHECK(strcmp(local, "far-near") == 0);
  CHECK(strchr(f, '-') == f + 3 && strrchr(f, 'a') == f + 6);
  CHECK(strstr(f, "near") == f + 4);
  CHECK(memcmp(f, local, 8) == 0);
  memset(f, 'q', 3);
  CHECK(strncmp(f, "qqq-", 4) == 0);
  memmove(f + 1, f, 4);
  CHECK(f[4] == '-');
  char __far *d = strdup(f);
  CHECK(d && strcmp(d, f) == 0);
  free(d);
  /* free and realloc use the heap of the pointer. */
  f = (char __far *)realloc(f, 3000);
  CHECK(f && f[4] == '-' && _msize(f) >= 3000);
  n = (char __near *)realloc(n, 100);
  CHECK(n && strcmp(n, "near") == 0 && _msize(n) >= 100);
  free(f);
  free(n);
  free(NULL);
  void *m = malloc(20);
  CHECK(m && _msize(m) >= 20);
  free(m);
}

int main(void) {
  far_strings();
  far_heap();
  near_heap();
  standard_names();
  /* The heap of malloc still works. */
  char *big = (char *)malloc(1000);
  CHECK(big != NULL);
  free(big);
  if (failures)
    printf("%d FAILED\n", failures);
  else
    printf("ALL PASSED\n");
  return failures != 0;
}
