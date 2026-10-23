/* The __near, __far and __huge qualifiers: far variables in any model, and
 * pointers of each kind with the conversions between them. */
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c)                                                              \
  do {                                                                        \
    if (!(c)) {                                                               \
      printf("FAIL line %d: %s\n", __LINE__, #c);                             \
      failures++;                                                             \
    }                                                                         \
  } while (0)

/* Far variables, outside DGROUP even in the small model: together more
 * than DGROUP could hold. */
int __far table[20000] = {1, 2, 3};
char __far zeros[30000];
const char __far message[] = "far message";
char __near near_buf[16] = "near";

static size_t __attribute__((noinline)) far_strlen(const char __far *s) {
  size_t n = 0;
  while (s[n])
    n++;
  return n;
}

static void __attribute__((noinline)) far_fill(char __far *p, char c,
                                                unsigned n) {
  while (n--)
    *p++ = c;
}

int main(void) {
  CHECK(sizeof(char __near *) == 2);
  CHECK(sizeof(char __far *) == 4);
  CHECK(sizeof(char __huge *) == 4);
  CHECK(table[0] == 1 && table[2] == 3 && table[19999] == 0);
  for (unsigned i = 0; i < 20000; i++)
    table[i] = i * 3;
  CHECK(table[19999] == (int)(19999u * 3));
  CHECK(zeros[0] == 0 && zeros[29999] == 0);
  far_fill(zeros, 'z', sizeof zeros);
  CHECK(zeros[0] == 'z' && zeros[29999] == 'z');
  CHECK(far_strlen(message) == 11);
  /* A near pointer converts to far implicitly. */
  CHECK(far_strlen(near_buf) == 4);
  char __far *fp = near_buf;
  fp[0] = 'N';
  CHECK(near_buf[0] == 'N');
  /* Far to near needs a cast; it keeps the offset. */
  char __near *np = (char __near *)fp;
  CHECK(np == near_buf);
  /* Far and huge pointers convert to each other. */
  char __huge *hp = zeros;
  hp += 29999;
  CHECK(*hp == 'z');
  char __far *back = hp;
  CHECK(*back == 'z');
  CHECK(table[19999] == (int)(19999u * 3));
  if (failures == 0)
    printf("ALL PASSED\n");
  return failures != 0;
}
