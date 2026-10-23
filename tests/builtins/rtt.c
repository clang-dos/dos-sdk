/* Checks compiler-rt builtins on i8086 against values computed on the host. */
#include "vec.h"
#include <string.h>
#include <unistd.h>
/* Output goes straight to write(): with stdio, the tables and the soft
 * float code do not fit in a .COM program. */
static void out(const char *s) { write(1, s, strlen(s)); }
static void putnum(unsigned n) {
  char b[6], *p = b + sizeof b;
  *--p = 0;
  do *--p = '0' + n % 10; while (n /= 10);
  out(p);
}
typedef unsigned long u32;
typedef long s32;
typedef unsigned long long u64;
typedef long long s64;
static int bad;
#define N(a) (sizeof(a) / sizeof(a[0]))
static void fail(const char *what, int row, int col) {
  if (bad++ < 20) {
    out("FAIL "); out(what);
    out(" row "); putnum(row);
    out(" col "); putnum(col);
    out("\n");
  }
}
static u32 fbits(float f) { union { float f; u32 u; } x; x.f = f; return x.u; }
static float bitsf(u32 u) { union { float f; u32 u; } x; x.u = u; return x.f; }
static u64 dbits(double f) { union { double f; u64 u; } x; x.f = f; return x.u; }
static double bitsd(u64 u) { union { double f; u64 u; } x; x.u = u; return x.f; }
int main(void) {
  for (unsigned i = 0; i < N(d32); i++) {
    volatile s32 a = (s32)d32[i][0], b = (s32)d32[i][1];
    if ((u32)(a / b) != d32[i][2]) fail("sdiv32", i, 2);
    if ((u32)(a % b) != d32[i][3]) fail("srem32", i, 3);
    if ((u32)a / (u32)b != d32[i][4]) fail("udiv32", i, 4);
    if ((u32)a % (u32)b != d32[i][5]) fail("urem32", i, 5);
    if ((u32)a * (u32)b != d32[i][6]) fail("mul32", i, 6);
  }
  for (unsigned i = 0; i < N(d64); i++) {
    volatile s64 a = (s64)d64[i][0], b = (s64)d64[i][1];
    volatile int sh = (int)d64[i][7];
    if ((u64)(a / b) != d64[i][2]) fail("sdiv64", i, 2);
    if ((u64)(a % b) != d64[i][3]) fail("srem64", i, 3);
    if ((u64)a / (u64)b != d64[i][4]) fail("udiv64", i, 4);
    if ((u64)a % (u64)b != d64[i][5]) fail("urem64", i, 5);
    if ((u64)a * (u64)b != d64[i][6]) fail("mul64", i, 6);
    if ((u64)a << sh != d64[i][8]) fail("shl64", i, 8);
    if ((u64)a >> sh != d64[i][9]) fail("lshr64", i, 9);
    if ((u64)(a >> sh) != d64[i][10]) fail("ashr64", i, 10);
  }
  for (unsigned i = 0; i < N(ff); i++) {
    volatile float a = bitsf(ff[i][0]), b = bitsf(ff[i][1]);
    if (fbits(a + b) != ff[i][2]) fail("fadd", i, 2);
    if (fbits(a - b) != ff[i][3]) fail("fsub", i, 3);
    if (fbits(a * b) != ff[i][4]) fail("fmul", i, 4);
    if (b != 0 && fbits(a / b) != ff[i][5]) fail("fdiv", i, 5);
    if ((u32)(a < b) != ff[i][6]) fail("flt", i, 6);
    if (a > -2147483648.0f && a < 2147483648.0f && (u32)(s32)a != ff[i][7])
      fail("fptosi", i, 7);
  }
  for (unsigned i = 0; i < N(dd); i++) {
    volatile double a = bitsd(dd[i][0]), b = bitsd(dd[i][1]);
    if (dbits(a + b) != dd[i][2]) fail("dadd", i, 2);
    if (dbits(a - b) != dd[i][3]) fail("dsub", i, 3);
    if (dbits(a * b) != dd[i][4]) fail("dmul", i, 4);
    if (b != 0 && dbits(a / b) != dd[i][5]) fail("ddiv", i, 5);
    if ((u64)(a < b) != dd[i][6]) fail("dlt", i, 6);
    if (a > -9.2e18 && a < 9.2e18 && (u64)(s64)a != dd[i][7]) fail("dtosi", i, 7);
    if (a > -3e38 && a < 3e38 && dbits((double)(float)a) != dd[i][8])
      fail("dtof", i, 8);
  }
  if (bad) { putnum(bad); out(" FAILED\n"); } else out("ALL PASSED\n");
  return bad != 0;
}
