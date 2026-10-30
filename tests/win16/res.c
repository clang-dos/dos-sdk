/* Resources from res.rc: strings, a menu, accelerators, a dialog, an icon,
 * raw data and version information. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "res.h"

static int failures;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL line %d: %s\n", __LINE__, #c);                              \
      failures++;                                                              \
    }                                                                          \
  } while (0)

BOOL CALLBACK __export AboutProc(HWND dlg, UINT msg, WPARAM wp,
                                        LPARAM lp) {
  (void)wp;
  (void)lp;
  return msg == WM_INITDIALOG;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
  (void)prev;
  (void)cmd;
  (void)show;
  char buf[64];

  CHECK(LoadString(inst, IDS_HELLO, buf, sizeof buf) == 21);
  CHECK(strcmp(buf, "Hello from a resource") == 0);
  CHECK(LoadString(inst, IDS_SECOND, buf, sizeof buf) == 13);
  CHECK(strcmp(buf, "Second bundle") == 0);

  HMENU menu = LoadMenu(inst, MAKEINTRESOURCE(IDM_MAIN));
  CHECK(menu != 0);
  if (menu) {
    HMENU file = GetSubMenu(menu, 0);
    CHECK(file != 0);
#if WINVER >= 0x0200
    CHECK(GetMenuItemCount(menu) == 1);
    CHECK(file && GetMenuItemCount(file) == 3);
    CHECK(file && GetMenuItemID(file, 2) == IDM_EXIT);
#endif
    CHECK(GetMenuString(menu, 0, buf, sizeof buf, MF_BYPOSITION) == 5 &&
          strcmp(buf, "&File") == 0);
    CHECK(GetMenuString(menu, IDM_OPEN, buf, sizeof buf, MF_BYCOMMAND) > 0 &&
          strcmp(buf, "&Open...") == 0);
    DestroyMenu(menu);
  }

  CHECK(LoadAccelerators(inst, MAKEINTRESOURCE(IDA_MAIN)) != 0);
#if WINVER >= 0x0300
  CHECK(LoadIcon(inst, "APPICON") != 0);
#else
  /* Wine cannot load a 1.x or 2.x icon, so look at the resource: the
   * file, with its second byte set. */
  HRSRC ri = FindResource(inst, "APPICON", RT_ICON);
  CHECK(ri != 0);
  if (ri) {
    const unsigned char __far *p = LockResource(LoadResource(inst, ri));
    CHECK(p && p[0] == 1 && p[1] == 1 && p[6] == 32 && p[8] == 32);
  }
#endif

  HRSRC r = FindResource(inst, MAKEINTRESOURCE(IDR_DATA), RT_RCDATA);
  CHECK(r != 0);
  if (r) {
    CHECK(SizeofResource(inst, r) >= 11);
    const unsigned char __far *p = LockResource(LoadResource(inst, r));
    CHECK(p && p[0] == 0x34 && p[1] == 0x12 && p[2] == 0x78 && p[5] == 0 &&
          p[6] == 'd' && p[9] == 'a' && p[10] == 0);
  }

  r = FindResource(inst, MAKEINTRESOURCE(VS_VERSION_INFO), VS_FILE_INFO);
  CHECK(r != 0);
  if (r) {
    const unsigned char __far *p = LockResource(LoadResource(inst, r));
    /* WORD length, WORD value length, "VS_VERSION_INFO", then the fixed
     * information at offset 20, which starts with its signature. */
    CHECK(p && p[2] == 52 && p[3] == 0 && lstrcmp((LPCSTR)p + 4,
                                                   "VS_VERSION_INFO") == 0);
    CHECK(p && p[20] == 0xBD && p[21] == 0x04 && p[22] == 0xEF &&
          p[23] == 0xFE);
    /* FILEVERSION 1,2,3,4: the most significant word first. */
    CHECK(p && p[28] == 2 && p[30] == 1 && p[32] == 4 && p[34] == 3);
  }

  HWND dlg = CreateDialog(inst, MAKEINTRESOURCE(IDD_ABOUT), 0, AboutProc);
  CHECK(dlg != 0);
  if (dlg) {
    SetDlgItemText(dlg, IDC_NAME, "typed");
    CHECK(GetDlgItemText(dlg, IDC_NAME, buf, sizeof buf) == 5 &&
          strcmp(buf, "typed") == 0);
    CHECK(GetWindowText(dlg, buf, sizeof buf) == 5 &&
          strcmp(buf, "About") == 0);
    CHECK(GetDlgItem(dlg, IDOK) != 0);
    DestroyWindow(dlg);
  }

  printf(failures ? "FAILED\n" : "ALL PASSED\n");
  return failures != 0;
}
