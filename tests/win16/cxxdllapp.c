/* Calls CXXDLL.DLL. */
#include <stdio.h>

int __far __pascal CxxWork(int);

int main(void) {
  int r = CxxWork(4);
  if (r)
    printf("CxxWork: %#x\n", r);
  printf(r ? "FAILED\n" : "ALL PASSED\n");
  return r != 0;
}
