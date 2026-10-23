/* picolibc on DOS: memory, stdio, files, directories, setjmp, constructors,
 * atexit, environment and time. Prints ALL PASSED. */
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int ctor_ran;
__attribute__((constructor)) static void ctor(void) { ctor_ran = 1; }
__attribute__((destructor)) static void dtor(void) { printf("destructor ran\n"); }
static void at_exit(void) { printf("atexit ran\n"); }

static jmp_buf jb;
static void jump(int v) { longjmp(jb, v); }

static int cmp(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

int main(void) {
  CHECK(ctor_ran);
  atexit(at_exit);

  /* malloc (small sizes: an unoptimized .COM test leaves about 10 KB of
   * heap) */
  char *p = malloc(1000), *q = malloc(4000);
  CHECK(p && q);
  memset(q, 0x5a, 4000);
  free(p);
  p = realloc(q, 6000);
  CHECK(p && p[3999] == 0x5a);
  free(p);
#if defined(__HUGE__) && __SIZEOF_SIZE_T__ == 4
  /* Huge pointers and a 32-bit size_t: one object over 64 KiB. */
  CHECK(sizeof(size_t) == 4 && sizeof(char[100000]) == 100000);
  volatile size_t big = 200000;
  unsigned char *h = malloc(big);
  CHECK(h != NULL);
  if (h) {
    for (size_t i = 0; i < big; i++)
      h[i] = (unsigned char)(i * 7 + (i >> 16));
    size_t bad = 0;
    for (size_t i = 0; i < big; i++)
      bad += h[i] != (unsigned char)(i * 7 + (i >> 16));
    CHECK(bad == 0);
    unsigned char *e = h + big;
    CHECK(e - h == 200000 && e > h + 70000 && h + 70000 > h + 65535);
    memset(h + 60000, 0xa5, 20000);
    CHECK(h[59999] != 0xa5 && h[60000] == 0xa5 && h[79999] == 0xa5 && h[80000] != 0xa5);
    memcpy(h + 100000, h + 60000, 20000);
    CHECK(h[100000] == 0xa5 && h[119999] == 0xa5 && memcmp(h + 60000, h + 100000, 20000) == 0);
    free(h);
  }
#elif defined(__HUGE__)
  /* huge16: a 16-bit size_t and a 32-bit ptrdiff_t. Objects stay under
   * 64 KiB, but the heap is all of DOS memory. */
  CHECK(sizeof(size_t) == 2 && sizeof(ptrdiff_t) == 4);
  volatile size_t big = 60000;
  unsigned char *h1 = malloc(big), *h2 = malloc(big);
  CHECK(h1 != NULL && h2 != NULL);
  if (h1 && h2) {
    memset(h1, 1, big);
    memset(h2, 2, big);
    CHECK(h1[0] == 1 && h1[59999] == 1 && h2[0] == 2 && h2[59999] == 2);
    volatile ptrdiff_t d = h2 - h1;
    CHECK(d >= 60000 || d <= -60000);
    memcpy(h1 + 1000, h2, 3000);
    CHECK(h1[999] == 1 && h1[1000] == 2 && h1[3999] == 2 && h1[4000] == 1);
  }
  free(h2);
  free(h1);
#else
  volatile size_t big = 60000;
  void *volatile toobig = malloc(big);
  CHECK(toobig == NULL && errno == ENOMEM);
#endif

  /* stdio formatting */
  char buf[100];
  snprintf(buf, sizeof buf, "%d %u %ld %lld %x %s %c %5.2f %g", -12, 65535u, -100000L, 123456789012LL, 0xbeef, "str", 'z', 3.14159, 1e10);
  CHECK(strcmp(buf, "-12 65535 -100000 123456789012 beef str z  3.14 1e+10") == 0);
  printf("%s\n", buf);
  CHECK(strtod("2.5e3", NULL) == 2500.0);
  CHECK(fabs(sqrt(2.0) - 1.41421356) < 1e-6);
  CHECK(atol("-70000") == -70000L);

  /* qsort */
  int a[] = {5, 3, 9, 1, 7};
  qsort(a, 5, sizeof a[0], cmp);
  CHECK(a[0] == 1 && a[4] == 9);

  /* setjmp */
  volatile int n = 0;
  int r = setjmp(jb);
  if (r == 0) { n = 1; jump(42); }
  CHECK(r == 42 && n == 1);
  if ((r = setjmp(jb)) == 0) longjmp(jb, 0);
  CHECK(r == 1);

  /* files (text mode: each line ends in CR LF on disk) */
  FILE *f = fopen("TEST.TXT", "w");
  CHECK(f != NULL);
  fprintf(f, "line one\nline two\n");
  fclose(f);
  struct stat st;
  CHECK(stat("TEST.TXT", &st) == 0 && st.st_size == 20 && S_ISREG(st.st_mode));
  f = fopen("TEST.TXT", "a");
  fputs("line three\n", f);
  fclose(f);
  f = fopen("TEST.TXT", "r");
  CHECK(fgets(buf, sizeof buf, f) && strcmp(buf, "line one\n") == 0);
  CHECK(fseek(f, -12, SEEK_END) == 0 && fgets(buf, sizeof buf, f) && strcmp(buf, "line three\n") == 0);
  CHECK(ftell(f) == 32);
  fclose(f);
  CHECK(rename("TEST.TXT", "TEST2.TXT") == 0);
  CHECK(open("TEST.TXT", O_RDONLY) == -1 && errno == ENOENT);
  int fd = open("TEST2.TXT", O_RDWR);
  CHECK(fd >= 0);
  CHECK(fstat(fd, &st) == 0 && st.st_size == 32);
  CHECK(lseek(fd, 5, SEEK_SET) == 5 && read(fd, buf, 3) == 3 && memcmp(buf, "one", 3) == 0);
  close(fd);
  CHECK(open("TEST2.TXT", O_CREAT | O_EXCL | O_WRONLY, 0666) == -1 && (errno == EEXIST || errno == EACCES)); /* emu2 reports access denied */
  CHECK(unlink("TEST2.TXT") == 0);
  CHECK(stat("TEST2.TXT", &st) == -1 && errno == ENOENT);
  CHECK(fopen("NOFILE.XYZ", "r") == NULL && errno == ENOENT);

  /* directories */
  CHECK(mkdir("TDIR", 0777) == 0);
  CHECK(stat("TDIR", &st) == 0 && S_ISDIR(st.st_mode));
  CHECK(rmdir("TDIR") == 0);
  CHECK(getcwd(buf, sizeof buf) != NULL && buf[1] == ':');

  /* environment and time */
  CHECK(getenv("PATH") != NULL || getenv("COMSPEC") != NULL);
  time_t t = time(NULL);
  CHECK(t > 1700000000LL);
  struct tm *tm = gmtime(&t);
  CHECK(tm && tm->tm_year >= 123);
  printf("year %d, isatty(1)=%d\n", tm->tm_year + 1900, isatty(1));

  printf(fails ? "%d FAILED\n" : "ALL PASSED\n", fails);
  return fails;
}
