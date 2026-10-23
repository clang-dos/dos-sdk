/* Variables larger than the data threshold (32767 bytes by default) go in
 * far segments of their own outside DGROUP, in the compact, large and huge
 * models. This checks that they work, that they don't take DGROUP space,
 * and in the huge models that an object over 64 KiB spans segments. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures;
#define CHECK(c)                                                              \
  do {                                                                        \
    if (!(c)) {                                                               \
      printf("FAIL line %d: %s\n", __LINE__, #c);                             \
      failures++;                                                             \
    }                                                                         \
  } while (0)

/* Three far variables: initialized, zeroed and read-only. */
static int big_init[20000] = {1, 2, 3, [19999] = 42};
int big_zero[20000];
const char big_ro[40000] = "read only";
/* Small variables stay in DGROUP and can point into far data. */
int *ptr_into_big = &big_init[19999];
const char *ptr_into_ro = &big_ro[5];
char small[100];

#if defined(__HUGE__) && __SIZEOF_SIZE_T__ == 4
/* Over 64 KiB: split into several far segments. An object that big needs
 * a 32-bit size_t, so not huge16. */
static unsigned char huge_init[150000] = {7, [70000] = 8, [149999] = 9};
unsigned char huge_zero[200000];
unsigned char *ptr_high = &huge_init[149999];
#endif

static void __attribute__((noinline)) fill(int *p, int n, int v) {
  for (int i = 0; i < n; i++)
    p[i] = v + i;
}

int main(void) {
  CHECK(big_init[0] == 1 && big_init[2] == 3 && big_init[19999] == 42);
  CHECK(*ptr_into_big == 42);
  CHECK(strcmp(big_ro, "read only") == 0);
  CHECK(strcmp(ptr_into_ro, "only") == 0);
  for (int i = 0; i < 20000; i++)
    if (big_zero[i] != 0) {
      CHECK(big_zero[i] == 0);
      break;
    }
  fill(big_zero, 20000, 5);
  CHECK(big_zero[0] == 5 && big_zero[19999] == 20004);
  memset(big_init, 0x11, sizeof big_init);
  CHECK(big_init[100] == 0x1111);
  /* The far variables don't overlap each other, the heap or the stack. */
  char *heap = malloc(1000);
  CHECK(heap != NULL);
  memset(heap, 0x55, 1000);
  memset(small, 0x66, sizeof small);
  CHECK(big_zero[0] == 5 && big_zero[19999] == 20004);
  CHECK(big_init[19999] == 0x1111);
  CHECK(strcmp(big_ro, "read only") == 0);
#if defined(__HUGE__) && __SIZEOF_SIZE_T__ == 4
  CHECK(huge_init[0] == 7 && huge_init[70000] == 8 && huge_init[149999] == 9);
  CHECK(*ptr_high == 9);
  CHECK(huge_zero[0] == 0 && huge_zero[65536] == 0 && huge_zero[199999] == 0);
  for (long i = 0; i < 200000; i++)
    huge_zero[i] = (unsigned char)(i * 7);
  long bad = 0;
  for (long i = 0; i < 200000; i++)
    bad += huge_zero[i] != (unsigned char)(i * 7);
  CHECK(bad == 0);
  CHECK(huge_init[70000] == 8 && huge_init[149999] == 9);
  CHECK(big_zero[19999] == 20004);
#endif
  if (failures == 0)
    printf("ALL PASSED\n");
  return failures != 0;
}
