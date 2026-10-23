/* Reads lines from stdin. run-tests.sh pipes "first\nsecond line\n" in. */
#include <stdio.h>
#include <string.h>
int main(void) {
  char line[80];
  int n = 0;
  while (fgets(line, sizeof line, stdin)) { n++; printf("[%s]", strtok(line, "\r\n")); }
  printf("\nlines=%d\n", n);
  return 0;
}
