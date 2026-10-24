/* A stack larger than the 4 KiB default, from the linker's option stack
 * (linked with -Wl,option,stack=16384). The heap starts after it, so heap
 * blocks and deep recursion do not overlap. Prints ALL PASSED. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* About 11 KiB of stack in all. */
static int deep(int n) {
  volatile char buf[1024];
  memset((char *)buf, n, sizeof buf);
  return n ? deep(n - 1) + buf[5] : 0;
}

int main(void) {
  char *p = malloc(2000);
  CHECK(p != NULL);
  memset(p, 0x55, 2000);
  CHECK(deep(10) == 55);
  char *q = malloc(2000);
  CHECK(q != NULL);
  memset(q, 0x66, 2000);
  CHECK(deep(10) == 55);
  CHECK(p[0] == 0x55 && p[1999] == 0x55);
  CHECK(q[0] == 0x66 && q[1999] == 0x66);
  if (fails == 0)
    printf("ALL PASSED\n");
  return fails != 0;
}
