/* Compiler helper functions on PowerPC Mac OS X: 64-bit division and
 * shifts, conversions between floating point and 64-bit integers, bit
 * counting, and (on 10.4 and later) 128-bit long double arithmetic. clang
 * calls these in libclang_rt.ppc_osx.a, libcc_dynamic.a or libgcc_s.1.dylib.
 * volatile keeps clang from working the answers out at compile time. */
#include <assert.h>
#include <stdio.h>

static int fails;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL line %d: %s\n", __LINE__, #c);                              \
      fails++;                                                                 \
    }                                                                          \
  } while (0)

int main(void) {
  volatile long long a = 0x123456789abcdefLL, b = -1000003LL;
  volatile unsigned long long ua = 0xfedcba9876543210ULL, ub = 77777ULL;
  volatile int sh = 37;
  volatile double d = -12345678901.75, big = 1.5e19;
  volatile float f = 3.0e9f;

  CHECK(a / b == -81985283260LL);          /* __divdi3 */
  CHECK(a % b == 637115LL);                /* __moddi3 */
  CHECK(ua / ub == 236120685350335ULL);    /* __udivdi3 */
  CHECK(ua % ub == 59425ULL);              /* __umoddi3 */
  CHECK((long long)((unsigned long long)a << sh) ==
        (long long)(0x123456789abcdefULL << 37)); /* __ashldi3 */
  CHECK((b >> sh) == -1);                  /* __ashrdi3 */
  CHECK((ua >> sh) == 0x7f6e5d4ULL);       /* __lshrdi3 */
  CHECK((long long)d == -12345678901LL);   /* __fixdfdi */
  CHECK((unsigned long long)big == 15000000000000000000ULL); /* __fixunsdfdi */
  CHECK((unsigned long long)f == 3000000000ULL); /* __fixunssfdi */
  CHECK((double)a == 81985529216486896.0); /* __floatdidf */
  CHECK((double)ua == 18364758544493064192.0); /* __floatundidf */
  CHECK((float)b == -1000003.0f);          /* __floatdisf */
  CHECK(__builtin_popcountll(ua) == 32);
  CHECK(__builtin_clzll(ua >> sh) == 37);
  CHECK(__builtin_ctzll(ua) == 4);

#if __LDBL_MANT_DIG__ == 106
  {
    /* IBM double-double: __gcc_qadd, __gcc_qmul, __gcc_qdiv, __fixtfdi. */
    /* eps is a power of two, so it is exact as a double-double too. */
    volatile long double x = 1.0L, y = 3.0L, eps = 0x1p-100L;
    long double s = x + eps;
    CHECK(s != x && s - x == eps);
    CHECK(9.0L / y == y);
    CHECK((long long)(y * 1e15L) == 3000000000000000LL);
    CHECK((long double)a == 81985529216486895.0L); /* __floatditf */
    printf("long double: 128-bit\n");
  }
#else
  printf("long double: 64-bit\n");
#endif
  /* Before 10.4, assert() calls __eprintf, from the compiler's helpers. */
  assert(fails >= 0);
  printf(fails ? "%d FAILED\n" : "ALL PASSED\n", fails);
  return fails != 0;
}
