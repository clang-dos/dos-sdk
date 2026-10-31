/* Calls into Mac OS X's own libSystem, which Apple built with gcc, to check
 * that our PowerPC Darwin calling convention, struct layout and stubs agree
 * with it: varargs with doubles and long longs, struct returns, callbacks,
 * setjmp, stdio, constructors and atexit. Prints ALL PASSED. */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL line %d: %s\n", __LINE__, #c);                              \
      fails++;                                                                 \
    }                                                                          \
  } while (0)

static int ctor_ran;
__attribute__((constructor)) static void ctor(void) { ctor_ran = 1; }
static void at_exit(void) { printf("atexit ran\n"); }

static int cmp_int(const void *a, const void *b) {
  return *(const int *)a - *(const int *)b;
}
static int cmp_double(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : x > y;
}

/* Our own varargs function, passed on to libSystem's vsnprintf. */
static int fmt(char *buf, size_t n, const char *f, ...) {
  va_list ap;
  va_start(ap, f);
  int r = vsnprintf(buf, n, f, ap);
  va_end(ap);
  return r;
}

/* gcc's "power" alignment: a double that is not the first member is only
 * word aligned. */
struct after_int { int i; double d; };
struct first_double { double d; int i; };

static jmp_buf jb;
static void jump(int v) { longjmp(jb, v); }

int main(int argc, char **argv) {
  char buf[256];
  CHECK(ctor_ran);
  atexit(at_exit);

  /* Doubles go in FPRs and also take up GPRs; long long takes two GPRs.
   * Enough arguments to spill onto the stack. */
  snprintf(buf, sizeof buf, "%d %lld %.3f %s %d %.1f %lld %d %.2f %c %u %x",
           1, 1234567890123LL, 2.5, "str", -7, 0.5, -9LL, 42, 3.25, 'z', 9u,
           255);
  CHECK(strcmp(buf, "1 1234567890123 2.500 str -7 0.5 -9 42 3.25 z 9 ff") == 0);
  printf("snprintf: %s\n", buf);

  /* va_list passed through our own varargs function. */
  CHECK(fmt(buf, sizeof buf, "%g/%lld/%s", 1e10, 5LL << 40, "x") > 0);
  CHECK(strcmp(buf, "1e+10/5497558138880/x") == 0);
  printf("vsnprintf: %s\n", buf);

  /* Doubles returned in f1, and through pointers. */
  CHECK(strtod("6.125", 0) == 6.125);
  CHECK(sqrt(2.25) == 1.5);
  CHECK(pow(2.0, 10.0) == 1024.0);
  int e;
  CHECK(frexp(48.0, &e) == 0.75 && e == 6);
  double ip;
  CHECK(modf(3.75, &ip) == 0.75 && ip == 3.0);
  CHECK(fabs(-0.5) == 0.5 && floor(-1.5) == -2.0);

  /* Parsing into our variables. */
  long long ll = 0;
  double d = 0;
  int i = 0;
  CHECK(sscanf("77 -123456789012 2.75", "%d %lld %lf", &i, &ll, &d) == 3);
  CHECK(i == 77 && ll == -123456789012LL && d == 2.75);
  CHECK(strtoll("-9000000000", 0, 10) == -9000000000LL);

  /* Struct returns. */
  div_t q = div(17, 5);
  CHECK(q.quot == 3 && q.rem == 2);
  ldiv_t lq = ldiv(-17L, 5L);
  CHECK(lq.quot == -3 && lq.rem == -2);

  /* Layout matches gcc's. */
  CHECK(offsetof(struct after_int, d) == 4 && sizeof(struct after_int) == 12);
  CHECK(_Alignof(struct first_double) == 8 && sizeof(struct first_double) == 16);
  CHECK(sizeof(long double) == 16 || sizeof(long double) == 8);
  CHECK(sizeof(_Bool) == 4);

  /* Callbacks from libSystem into our code. */
  int ints[] = {5, 3, 9, 1, 7};
  qsort(ints, 5, sizeof ints[0], cmp_int);
  CHECK(ints[0] == 1 && ints[2] == 5 && ints[4] == 9);
  double ds[] = {2.5, -1.0, 8.0, 0.25};
  qsort(ds, 4, sizeof ds[0], cmp_double);
  CHECK(ds[0] == -1.0 && ds[1] == 0.25 && ds[3] == 8.0);
  int key = 7;
  CHECK(bsearch(&key, ints, 5, sizeof ints[0], cmp_int) == &ints[3]);

  /* setjmp and longjmp. */
  volatile int jumped = 0;
  int v = setjmp(jb);
  if (v == 0) jump(3);
  else jumped = v;
  CHECK(jumped == 3);

  /* errno through __error(). */
  errno = 0;
  CHECK(strtol("99999999999999999999", 0, 10) == LONG_MAX && errno == ERANGE);

  /* Heap, files and time. */
  char *p = malloc(100);
  CHECK(p != 0);
  strcpy(p, "heap");
  p = realloc(p, 100000);
  CHECK(p && strcmp(p, "heap") == 0);
  free(p);
  snprintf(buf, sizeof buf, "/tmp/clangdos-abi-%d.txt", (int)getpid());
  FILE *f = fopen(buf, "w");
  CHECK(f != 0);
  if (f) {
    fprintf(f, "line %d %.2f\n", 1, 0.5);
    fclose(f);
    f = fopen(buf, "r");
    char line[64] = "";
    CHECK(f && fgets(line, sizeof line, f) && strcmp(line, "line 1 0.50\n") == 0);
    if (f) fclose(f);
    CHECK(unlink(buf) == 0);
  }
  time_t t = 86400 * 365;
  struct tm tm;
  CHECK(gmtime_r(&t, &tm) && tm.tm_year == 71 && tm.tm_yday == 0);
  CHECK(time(0) > 0);
  CHECK(getenv("PATH") != 0);
  CHECK(argc >= 1 && argv[0] != 0);

  printf(fails ? "%d FAILED\n" : "ALL PASSED\n", fails);
  return fails != 0;
}
