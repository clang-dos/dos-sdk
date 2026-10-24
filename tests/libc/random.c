/* getentropy and arc4random. The program prints its first arc4random
 * values; the test scripts run it twice and check that they differ. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL line %d: %s\n", __LINE__, #c);                              \
      failures++;                                                              \
    }                                                                          \
  } while (0)

static unsigned char big[256];

int main(void) {
  /* The seed: different on each run. */
  uint32_t r0 = arc4random(), r1 = arc4random();
  printf("seed %08lx%08lx\n", (unsigned long)r0, (unsigned long)r1);
  CHECK(r0 != r1);

  unsigned char a[40], b[40], zero[40] = {0};
  CHECK(getentropy(a, sizeof a) == 0);
  CHECK(getentropy(b, sizeof b) == 0);
  CHECK(memcmp(a, zero, sizeof a) != 0);
  CHECK(memcmp(a, b, sizeof a) != 0);
  CHECK(getentropy(a, 0) == 0);

  /* At most 256 bytes at a time. */
  errno = 0;
  CHECK(getentropy(big, 257) == -1 && errno == EIO);

  /* 2048 bits, about half of them set (7 standard deviations). */
  CHECK(getentropy(big, sizeof big) == 0);
  int ones = 0;
  for (unsigned i = 0; i < sizeof big; i++)
    for (unsigned v = big[i]; v; v >>= 1)
      ones += v & 1;
  CHECK(ones > 1024 - 160 && ones < 1024 + 160);

  /* Each value of arc4random_uniform(8) in 800 draws. */
  int count[8] = {0};
  for (int i = 0; i < 800; i++) {
    uint32_t v = arc4random_uniform(8);
    CHECK(v < 8);
    if (v < 8)
      count[v]++;
  }
  for (int i = 0; i < 8; i++)
    CHECK(count[i] > 50 && count[i] < 150);

  arc4random_buf(big, sizeof big);
  CHECK(memcmp(big, zero, sizeof zero) != 0);

  if (failures == 0)
    printf("ALL PASSED\n");
  return failures != 0;
}
