/* __near and __far functions: far functions in the near-code models and
 * near functions in the far-code models, called directly and through
 * pointers. */
#include <stdio.h>

static int failures;
#define CHECK(c)                                                              \
  do {                                                                        \
    if (!(c)) {                                                               \
      printf("FAIL line %d: %s\n", __LINE__, #c);                             \
      failures++;                                                             \
    }                                                                         \
  } while (0)

/* Defined in another file in a real program; here, a far function in every
 * model, with arguments, so the frame offsets of a far return matter. */
long __far far_add(long a, int b, char c) { return a + b + c; }
int __far far_square(int x) { return x * x; }
/* A near function: called with a near call, so only from this file's code
 * segment in the far-code models. */
static int __near near_twice(int x) { return 2 * x; }
/* Watcom places the qualifier before the name too. */
int * __far far_ptr(int *p) { return p + 1; }

int __far (*far_fp)(int) = far_square;
int (__far *far_fp2)(int) = far_square;
static int (__near *near_fp)(int) = near_twice;

static int __attribute__((noinline)) call_far(int (__far *f)(int), int x) {
  return f(x) + 1;
}

int main(void) {
  CHECK(sizeof(far_fp) == 4);
  CHECK(sizeof(near_fp) == 2);
  CHECK(far_add(100000L, 20, 3) == 100023L);
  CHECK(far_square(12) == 144);
  CHECK(near_twice(21) == 42);
  CHECK(far_fp(5) == 25 && far_fp2(6) == 36);
  CHECK(near_fp(4) == 8);
  CHECK(call_far(far_square, 3) == 10);
  int a[2] = {1, 2};
  CHECK(*far_ptr(a) == 2);
  if (failures == 0)
    printf("ALL PASSED\n");
  return failures != 0;
}
