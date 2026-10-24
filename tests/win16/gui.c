/* A window with a message loop: create it, paint it, close it from a
 * timer. Needs a display. */
#include <stdio.h>
#include <windows.h>

static int painted, timers, created;

LRESULT CALLBACK __export WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_CREATE:
    created = 1;
    return 0;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT r;
    GetClientRect(hwnd, &r);
    DrawText(dc, "Hello, Windows", -1, &r,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    EndPaint(hwnd, &ps);
    painted = 1;
    return 0;
  }
  case WM_TIMER:
    if (++timers == 2) {
      KillTimer(hwnd, 1);
      DestroyWindow(hwnd);
    }
    return 0;
  case WM_DESTROY:
    PostQuitMessage(5);
    return 0;
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
  WNDCLASS wc = {0};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(0, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = "ClangWin16";
  if (!RegisterClass(&wc)) {
    printf("FAIL RegisterClass\n");
    return 1;
  }
  HWND hwnd = CreateWindow("ClangWin16", "clang win16", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, 0, 0, inst,
                           0);
  if (!hwnd) {
    printf("FAIL CreateWindow\n");
    return 1;
  }
  ShowWindow(hwnd, show);
  UpdateWindow(hwnd);
  SetTimer(hwnd, 1, 50, 0);
  MSG m;
  while (GetMessage(&m, 0, 0, 0)) {
    TranslateMessage(&m);
    DispatchMessage(&m);
  }
  int ok = created && painted && timers == 2 && m.wParam == 5;
  printf(ok ? "ALL PASSED\n" : "FAILED created=%d painted=%d timers=%d\n",
         created, painted, timers);
  return !ok;
}
