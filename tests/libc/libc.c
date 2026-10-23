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

  /* malloc */
  char *p = malloc(1000), *q = malloc(8000);
  CHECK(p && q);
  memset(q, 0x5a, 8000);
  free(p);
  p = realloc(q, 12000);
  CHECK(p && p[7999] == 0x5a);
  free(p);
  volatile size_t big = 60000;
  void *volatile toobig = malloc(big);
  CHECK(toobig == NULL && errno == ENOMEM);

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

  /* files */
  FILE *f = fopen("TEST.TXT", "w");
  CHECK(f != NULL);
  fprintf(f, "line one\nline two\n");
  fclose(f);
  struct stat st;
  CHECK(stat("TEST.TXT", &st) == 0 && st.st_size == 18 && S_ISREG(st.st_mode));
  f = fopen("TEST.TXT", "a");
  fputs("line three\n", f);
  fclose(f);
  f = fopen("TEST.TXT", "r");
  CHECK(fgets(buf, sizeof buf, f) && strcmp(buf, "line one\n") == 0);
  CHECK(fseek(f, -11, SEEK_END) == 0 && fgets(buf, sizeof buf, f) && strcmp(buf, "line three\n") == 0);
  CHECK(ftell(f) == 29);
  fclose(f);
  CHECK(rename("TEST.TXT", "TEST2.TXT") == 0);
  CHECK(open("TEST.TXT", O_RDONLY) == -1 && errno == ENOENT);
  int fd = open("TEST2.TXT", O_RDWR);
  CHECK(fd >= 0);
  CHECK(fstat(fd, &st) == 0 && st.st_size == 29);
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
