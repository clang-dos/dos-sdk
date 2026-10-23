/* __pascal functions (arguments pushed left to right, the callee removes
 * them, upper-case names) called from C and through pointers, mixed with
 * __far and __export as in 16-bit Windows code. */
#include <stdio.h>

static int failures;
#define CHECK(c)                                                              \
  do {                                                                        \
    if (!(c)) {                                                               \
      printf("FAIL line %d: %s\n", __LINE__, #c);                             \
      failures++;                                                             \
    }                                                                         \
  } while (0)

long __pascal mix(int a, long b, char c, int d) {
  return a * 1000000L + b * 1000L + c * 10 + d;
}
int __far __pascal far_sub(int a, int b) { return a - b; }
int __export __far __pascal exported(int a) { return a + 1; }
struct pair { int x, y; };
struct pair __pascal make_pair(int x, int y) {
  struct pair p = {x, y};
  return p;
}
/* Written in assembly as a 16-bit Windows API function would be: it reads
 * its arguments in pascal order and removes them. The name is upper case
 * with no underscore. */
__asm__(".text\n"
        "\t.globl\tASMSUB\n"
        "ASMSUB:\n"
        "\tpushw\t%bp\n"
        "\tmovw\t%sp, %bp\n"
#if defined(__MEDIUM__) || defined(__LARGE__) || defined(__HUGE__)
        "\tmovw\t8(%bp), %ax\n" /* first argument */
        "\tsubw\t6(%bp), %ax\n" /* minus the second */
        "\tpopw\t%bp\n"
        "\tlretw\t$4\n"
#else
        "\tmovw\t6(%bp), %ax\n"
        "\tsubw\t4(%bp), %ax\n"
        "\tpopw\t%bp\n"
        "\tretw\t$4\n"
#endif
);
int __pascal asmsub(int a, int b);

int main(void) {
  long (__pascal *pm)(int, long, char, int) = mix;
  CHECK(mix(1, 2, 3, 4) == 1002034L);
  CHECK(pm(5, 6, 7, 8) == 5006078L);
  CHECK(far_sub(10, 3) == 7);
  CHECK(exported(41) == 42);
  CHECK(asmsub(10, 4) == 6);
  struct pair p = make_pair(3, 4);
  CHECK(p.x == 3 && p.y == 4);
  /* The stack is balanced after many calls. */
  long sum = 0;
  for (int i = 0; i < 1000; i++)
    sum += far_sub(i, 1) + asmsub(i, 0);
  CHECK(sum == 1000L * 999 - 1000);
  if (failures == 0)
    printf("ALL PASSED\n");
  return failures != 0;
}
