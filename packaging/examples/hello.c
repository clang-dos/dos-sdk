/* Hello world for every clang-dos target. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  char *p = malloc(100);
  if (!p)
    return 1;
  strcpy(p, "Hello, world");
  printf("%s from %s! %d %ld %.2f\n", p, argc > 1 ? argv[1] : "?", 42, 123456789L, 3.25);
  free(p);
  return 0;
}
