/* halloc and hfree: blocks over 64 KiB through huge pointers, next to
 * malloc. */
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

int main(void) {
  char *small = malloc(1000);
  CHECK(small != NULL);
  memset(small, 'a', 1000);

  /* 200000 bytes: over three segments. */
  long n = 50000;
  long __huge *a = halloc(n, sizeof(long));
  CHECK(a != NULL);
  if (a) {
    long bad = 0;
    for (long i = 0; i < n; i++)
      bad += a[i] != 0;
    CHECK(bad == 0);
    for (long i = 0; i < n; i++)
      a[i] = i * 7;
    bad = 0;
    for (long i = 0; i < n; i++)
      bad += a[i] != i * 7;
    CHECK(bad == 0);
    /* Bytes across the 64 KiB boundary. */
    char __huge *b = (char __huge *)a;
    b[65535] = 1;
    b[65536] = 2;
    CHECK(b[65535] == 1 && b[65536] == 2);
  }

  /* A second block, and a block of odd-sized elements. */
  char __huge *c = halloc(3, 30000);
  CHECK(c != NULL);
  if (c) {
    c[89999] = 'z';
    CHECK(c[0] == 0 && c[89999] == 'z');
  }

  /* malloc still works, and its memory was left alone. */
  char *more = malloc(2000);
  CHECK(more != NULL);
  int bad = 0;
  for (int i = 0; i < 1000; i++)
    bad += small[i] != 'a';
  CHECK(bad == 0);

  hfree(a);
  hfree(c);
  /* The memory comes back. */
  a = halloc(n, sizeof(long));
  CHECK(a != NULL);
  hfree(a);

  /* Too much fails. */
  CHECK(halloc(1, 0) == NULL);
  CHECK(halloc(0x100000L, 4) == NULL);
  CHECK(halloc(-1, 4) == NULL);
  hfree(NULL);

  free(more);
  free(small);
  printf(failures ? "FAILED\n" : "ALL PASSED\n");
  return failures != 0;
}
