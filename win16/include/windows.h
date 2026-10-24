/* A small windows.h for 16-bit Windows (clang --target=i8086-pc-win16).
 *
 * It declares the common types, messages and functions of KERNEL, USER and
 * GDI. Every function of the Windows 3.1 API can be linked (the import list
 * is lib/win16/windows.lnk); declare others the same way, with WINAPI. */
#ifndef _WINDOWS_H
#define _WINDOWS_H

#ifndef __WINDOWS__
#error "windows.h is for --target=i8086-pc-win16"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define WINAPI __far __pascal
#define CALLBACK __far __pascal
#define FAR __far
#define NEAR __near
#define PASCAL __pascal
#define CDECL __cdecl
#define VOID void

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef long LONG;
typedef char __far *LPSTR;
typedef const char __far *LPCSTR;
typedef void __far *LPVOID;
typedef const void __far *LPCVOID;
typedef BYTE __far *LPBYTE;
typedef WORD __far *LPWORD;
typedef DWORD __far *LPDWORD;
typedef int __far *LPINT;
typedef UINT WPARAM;
typedef LONG LPARAM;
typedef LONG LRESULT;
typedef DWORD COLORREF;
typedef UINT ATOM;

typedef UINT HANDLE;
typedef HANDLE HWND, HINSTANCE, HMODULE, HGLOBAL, HLOCAL, HTASK, HDC, HMENU,
    HICON, HCURSOR, HBRUSH, HPEN, HFONT, HBITMAP, HRGN, HGDIOBJ, HPALETTE,
    HRSRC, HACCEL;

typedef LRESULT(CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef int(CALLBACK *FARPROC)();

#ifndef NULL
#define NULL 0
#endif
#define TRUE 1
#define FALSE 0

#define LOBYTE(w) ((BYTE)(w))
#define HIBYTE(w) ((BYTE)((UINT)(w) >> 8))
#define LOWORD(l) ((WORD)(DWORD)(l))
#define HIWORD(l) ((WORD)((DWORD)(l) >> 16))
#define MAKELONG(lo, hi) ((LONG)(((WORD)(lo)) | ((DWORD)((WORD)(hi))) << 16))
#define MAKELPARAM(lo, hi) ((LPARAM)MAKELONG(lo, hi))
#define MAKEINTRESOURCE(i) ((LPCSTR)MAKELONG(i, 0))
#define RGB(r, g, b)                                                           \
  ((COLORREF)((BYTE)(r) | ((WORD)(BYTE)(g) << 8) | ((DWORD)(BYTE)(b) << 16)))

typedef struct tagPOINT {
  int x, y;
} POINT;
typedef POINT __far *LPPOINT;

typedef struct tagRECT {
  int left, top, right, bottom;
} RECT;
typedef RECT __far *LPRECT;
typedef const RECT __far *LPCRECT;

typedef struct tagMSG {
  HWND hwnd;
  UINT message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD time;
  POINT pt;
} MSG;
typedef MSG __far *LPMSG;

typedef struct tagWNDCLASS {
  UINT style;
  WNDPROC lpfnWndProc;
  int cbClsExtra;
  int cbWndExtra;
  HINSTANCE hInstance;
  HICON hIcon;
  HCURSOR hCursor;
  HBRUSH hbrBackground;
  LPCSTR lpszMenuName;
  LPCSTR lpszClassName;
} WNDCLASS;
typedef WNDCLASS __far *LPWNDCLASS;
typedef const WNDCLASS __far *LPCWNDCLASS;

typedef struct tagPAINTSTRUCT {
  HDC hdc;
  BOOL fErase;
  RECT rcPaint;
  BOOL fRestore;
  BOOL fIncUpdate;
  BYTE rgbReserved[16];
} PAINTSTRUCT;
typedef PAINTSTRUCT __far *LPPAINTSTRUCT;

/* Messages. */
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_SETTEXT 0x000C
#define WM_GETTEXT 0x000D
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_SHOWWINDOW 0x0018
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_COMMAND 0x0111
#define WM_TIMER 0x0113
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_USER 0x0400

/* Window and class styles. */
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_VISIBLE 0x10000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_OVERLAPPEDWINDOW                                                    \
  (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | \
   WS_MAXIMIZEBOX)
#define CW_USEDEFAULT ((int)0x8000)
#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002

/* ShowWindow. */
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOW 5

/* MessageBox. */
#define MB_OK 0x0000
#define MB_OKCANCEL 0x0001
#define MB_YESNO 0x0004
#define MB_ICONHAND 0x0010
#define MB_ICONQUESTION 0x0020
#define MB_ICONEXCLAMATION 0x0030
#define MB_ICONASTERISK 0x0040
#define MB_ICONINFORMATION MB_ICONASTERISK
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7

/* PeekMessage. */
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

/* DrawText. */
#define DT_CENTER 0x0001
#define DT_VCENTER 0x0004
#define DT_SINGLELINE 0x0020

/* SetBkMode. */
#define TRANSPARENT 1
#define OPAQUE 2

/* GetStockObject. */
#define WHITE_BRUSH 0
#define LTGRAY_BRUSH 1
#define GRAY_BRUSH 2
#define BLACK_BRUSH 4
#define NULL_BRUSH 5
#define WHITE_PEN 6
#define BLACK_PEN 7
#define SYSTEM_FONT 13

/* Colors for hbrBackground, plus one. */
#define COLOR_WINDOW 5

/* LoadCursor and LoadIcon. */
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDI_APPLICATION MAKEINTRESOURCE(32512)

/* GetSystemMetrics. */
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1

/* GlobalAlloc and LocalAlloc. */
#define GMEM_FIXED 0x0000
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040
#define GHND (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define GPTR (GMEM_FIXED | GMEM_ZEROINIT)
#define LMEM_FIXED 0x0000
#define LMEM_MOVEABLE 0x0002
#define LMEM_ZEROINIT 0x0040
#define LPTR (LMEM_FIXED | LMEM_ZEROINIT)

/* GetWinFlags. */
#define WF_PMODE 0x0001
#define WF_CPU286 0x0002
#define WF_CPU386 0x0004
#define WF_CPU486 0x0008
#define WF_STANDARD 0x0010
#define WF_ENHANCED 0x0020

/* KERNEL. */
DWORD WINAPI GetVersion(void);
DWORD WINAPI GetWinFlags(void);
DWORD WINAPI GetTickCount(void);
DWORD WINAPI GetFreeSpace(UINT);
HMODULE WINAPI GetModuleHandle(LPCSTR);
int WINAPI GetModuleFileName(HINSTANCE, LPSTR, int);
HINSTANCE WINAPI LoadLibrary(LPCSTR);
void WINAPI FreeLibrary(HINSTANCE);
FARPROC WINAPI GetProcAddress(HINSTANCE, LPCSTR);
FARPROC WINAPI MakeProcInstance(FARPROC, HINSTANCE);
void WINAPI FreeProcInstance(FARPROC);
HGLOBAL WINAPI GlobalAlloc(UINT, DWORD);
void __far *WINAPI GlobalLock(HGLOBAL);
BOOL WINAPI GlobalUnlock(HGLOBAL);
HGLOBAL WINAPI GlobalFree(HGLOBAL);
DWORD WINAPI GlobalSize(HGLOBAL);
HLOCAL WINAPI LocalAlloc(UINT, UINT);
void __near *WINAPI LocalLock(HLOCAL);
BOOL WINAPI LocalUnlock(HLOCAL);
HLOCAL WINAPI LocalFree(HLOCAL);
void WINAPI OutputDebugString(LPCSTR);
void WINAPI Yield(void);
int WINAPI lstrlen(LPCSTR);
LPSTR WINAPI lstrcpy(LPSTR, LPCSTR);
LPSTR WINAPI lstrcat(LPSTR, LPCSTR);
int WINAPI lstrcmp(LPCSTR, LPCSTR);
int WINAPI lstrcmpi(LPCSTR, LPCSTR);
int WINAPI LoadString(HINSTANCE, UINT, LPSTR, int);

/* USER. */
int __far __cdecl wsprintf(LPSTR, LPCSTR, ...);
int WINAPI wvsprintf(LPSTR, LPCSTR, const void __far *);
int WINAPI MessageBox(HWND, LPCSTR, LPCSTR, UINT);
void WINAPI MessageBeep(UINT);
ATOM WINAPI RegisterClass(LPCWNDCLASS);
HWND WINAPI CreateWindow(LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND,
                         HMENU, HINSTANCE, LPVOID);
BOOL WINAPI DestroyWindow(HWND);
BOOL WINAPI IsWindow(HWND);
BOOL WINAPI ShowWindow(HWND, int);
void WINAPI UpdateWindow(HWND);
void WINAPI InvalidateRect(HWND, LPCRECT, BOOL);
void WINAPI GetClientRect(HWND, LPRECT);
void WINAPI SetWindowText(HWND, LPCSTR);
int WINAPI GetWindowText(HWND, LPSTR, int);
BOOL WINAPI GetMessage(LPMSG, HWND, UINT, UINT);
BOOL WINAPI PeekMessage(LPMSG, HWND, UINT, UINT, UINT);
BOOL WINAPI TranslateMessage(const MSG __far *);
LONG WINAPI DispatchMessage(const MSG __far *);
BOOL WINAPI PostMessage(HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI SendMessage(HWND, UINT, WPARAM, LPARAM);
void WINAPI PostQuitMessage(int);
LRESULT WINAPI DefWindowProc(HWND, UINT, WPARAM, LPARAM);
UINT WINAPI SetTimer(HWND, UINT, UINT, FARPROC);
BOOL WINAPI KillTimer(HWND, UINT);
HCURSOR WINAPI LoadCursor(HINSTANCE, LPCSTR);
HICON WINAPI LoadIcon(HINSTANCE, LPCSTR);
int WINAPI GetSystemMetrics(int);
HDC WINAPI BeginPaint(HWND, LPPAINTSTRUCT);
void WINAPI EndPaint(HWND, const PAINTSTRUCT __far *);
HDC WINAPI GetDC(HWND);
int WINAPI ReleaseDC(HWND, HDC);
int WINAPI DrawText(HDC, LPCSTR, int, LPRECT, UINT);

/* GDI. */
BOOL WINAPI TextOut(HDC, int, int, LPCSTR, int);
COLORREF WINAPI SetTextColor(HDC, COLORREF);
int WINAPI SetBkMode(HDC, int);
HGDIOBJ WINAPI GetStockObject(int);
HGDIOBJ WINAPI SelectObject(HDC, HGDIOBJ);
BOOL WINAPI DeleteObject(HGDIOBJ);
HBRUSH WINAPI CreateSolidBrush(COLORREF);
BOOL WINAPI Rectangle(HDC, int, int, int, int);
BOOL WINAPI Ellipse(HDC, int, int, int, int);
DWORD WINAPI MoveTo(HDC, int, int);
BOOL WINAPI LineTo(HDC, int, int);

/* The program entry point, called by the C library's main() when the
 * program has no main() of its own. */
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

#ifdef __cplusplus
}
#endif

#endif /* _WINDOWS_H */
