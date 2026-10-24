/* Calls to KERNEL and USER through windows.h, from WinMain. */
#include <stdio.h>
#include <string.h>
#include <windows.h>

static int failures;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL line %d: %s\n", __LINE__, #c);                              \
      failures++;                                                              \
    }                                                                          \
  } while (0)

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
  CHECK(inst != 0);
  CHECK(prev == 0);
  CHECK(lstrcmp(cmd, "abc") == 0);

  DWORD v = GetVersion();
  CHECK(LOBYTE(LOWORD(v)) == 3);
  CHECK(GetWinFlags() & WF_PMODE);

  char buf[64];
  int n = wsprintf(buf, "%d-%s-%lx", 42, (LPSTR) "x", 0x12345678L);
  CHECK(n == 13 && strcmp(buf, "42-x-12345678") == 0);
  CHECK(lstrlen(buf) == 13);
  lstrcpy(buf, "abc");
  lstrcat(buf, "def");
  CHECK(lstrcmpi(buf, "ABCDEF") == 0);

  HGLOBAL h = GlobalAlloc(GHND, 40000);
  CHECK(h != 0);
  char __far *p = GlobalLock(h);
  CHECK(p != 0 && p[39999] == 0);
  p[39999] = 7;
  CHECK(p[39999] == 7);
  GlobalUnlock(h);
  GlobalFree(h);

  HLOCAL l = LocalAlloc(LPTR, 100);
  CHECK(l != 0);
  LocalFree(l);

  char name[128];
  CHECK(GetModuleFileName(inst, name, sizeof name) > 0);

  printf(failures ? "FAILED\n" : "ALL PASSED\n");
  return failures != 0;
}
