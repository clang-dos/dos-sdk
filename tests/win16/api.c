/* Calls to KERNEL and USER through windows.h, from WinMain. Built for
 * Windows 1.x or 2.x (WINVER below 0x0300), it uses only their functions,
 * with AnsiUpper and lstrcmp from KERNEL. */
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

  /* The version Wine reports for the expected version: 2.0 for 1.x and
   * 2.x, 3.0 or 3.1 for those. */
  DWORD v = GetVersion();
  CHECK(LOBYTE(LOWORD(v)) >= (WINVER < 0x0300 ? 2 : 3));

  char buf[64];
#if WINVER >= 0x0300
  CHECK(GetWinFlags() & WF_PMODE);
  int n = wsprintf(buf, "%d-%s-%lx", 42, (LPSTR) "x", 0x12345678L);
  CHECK(n == 13 && strcmp(buf, "42-x-12345678") == 0);
  CHECK(lstrlen(buf) == 13);
#endif
  lstrcpy(buf, "abc");
  lstrcat(buf, "def");
  CHECK(lstrlen(buf) == 6);
#if WINVER >= 0x0300
  CHECK(lstrcmpi(buf, "ABCDEF") == 0);
#endif
  AnsiUpper(buf);
  CHECK(lstrcmp(buf, "ABCDEF") == 0);
  CHECK(AnsiNext(buf) == (LPSTR)buf + 1);

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
