/* The C library on 16-bit Windows: main(), arguments, the heap, files and
 * the clock. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int failures;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL line %d: %s\n", __LINE__, #c);                              \
      failures++;                                                              \
    }                                                                          \
  } while (0)

int main(int argc, char **argv) {
  CHECK(argc == 3);
  CHECK(argc == 3 && strcmp(argv[1], "one") == 0 && strcmp(argv[2], "two") == 0);
  CHECK(argv[0] && strlen(argv[0]) > 0);

  char *p = malloc(1000);
  CHECK(p != NULL);
  memset(p, 'x', 1000);
  char *q = malloc(8000);
  CHECK(q != NULL);
  free(q);
  free(p);

  FILE *f = fopen("w16test.txt", "w");
  CHECK(f != NULL);
  if (f) {
    fprintf(f, "line %d\n", 42);
    fclose(f);
  }
  char buf[32] = "";
  f = fopen("w16test.txt", "r");
  CHECK(f != NULL);
  if (f) {
    CHECK(fgets(buf, sizeof buf, f) != NULL);
    fclose(f);
  }
  CHECK(strcmp(buf, "line 42\n") == 0);
  remove("w16test.txt");

  CHECK(time(NULL) > 1000000000);

  printf(failures ? "FAILED\n" : "ALL PASSED\n");
  return failures != 0;
}
