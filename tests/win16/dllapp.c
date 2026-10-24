/* Calls TESTDLL.DLL. */
#include <stdio.h>
#include <string.h>

int __far __pascal AddNumbers(int, int);
int __far __pascal NextCounter(void);
void __far __pascal FillBuf(char __far *, int);
int __far __pascal StackWork(int);

static int failures;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL line %d: %s\n", __LINE__, #c);                              \
      failures++;                                                              \
    }                                                                          \
  } while (0)

int main(void) {
  char b[16];
  CHECK(AddNumbers(2, 3) == 6);
  CHECK(NextCounter() == 101);
  CHECK(NextCounter() == 102);
  FillBuf(b, 5);
  CHECK(strcmp(b, "abcde") == 0);
  int r = StackWork(3);
  if (r)
    printf("StackWork: %#x\n", r);
  CHECK(r == 0);
  printf(failures ? "FAILED\n" : "ALL PASSED\n");
  return failures != 0;
}
