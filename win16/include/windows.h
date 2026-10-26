/* A small windows.h for 16-bit Windows (clang --target=i8086-pc-win16).
 *
 * It declares the common types, messages and functions of KERNEL, USER and
 * GDI. Every function of the Windows 3.1 API can be linked (the import list
 * is lib/win16/windows.lnk); declare others the same way, with WINAPI.
 * WINVER is the oldest Windows the program is for, from the target
 * (--target=i8086-pc-win162.03 makes it 0x0203); the functions that came
 * later are left out, here and in the import list (windows300.lnk for 3.0,
 * windows200.lnk for 2.x, windows100.lnk for 1.x).
 * Resource scripts can include it too (llvm-rc -win16 defines RC_INVOKED):
 * they get the constants only. */
#ifndef _WINDOWS_H
#define _WINDOWS_H

#ifndef __WINDOWS__
#error "windows.h is for --target=i8086-pc-win16"
#endif

#ifndef WINVER
#ifdef __WIN16_VERSION__
#define WINVER __WIN16_VERSION__
#else
#define WINVER 0x030A
#endif
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

#ifndef RC_INVOKED
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
typedef BOOL(CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);
#endif /* RC_INVOKED */

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

#ifndef RC_INVOKED
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
#endif /* RC_INVOKED */

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
#define WM_INITDIALOG 0x0110
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

/* Dialog styles, and the styles of the controls in dialogs. */
#define DS_ABSALIGN 0x01L
#define DS_SYSMODAL 0x02L
#define DS_LOCALEDIT 0x20L
#define DS_SETFONT 0x40L
#define DS_MODALFRAME 0x80L
#define DS_NOIDLEMSG 0x100L
#define WS_GROUP 0x00020000L
#define WS_TABSTOP 0x00010000L
#define WS_DISABLED 0x08000000L
#define WS_DLGFRAME 0x00400000L
#define WS_VSCROLL 0x00200000L
#define WS_HSCROLL 0x00100000L
#define WS_MINIMIZE 0x20000000L
#define WS_MAXIMIZE 0x01000000L
#define WS_CLIPSIBLINGS 0x04000000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_EX_DLGMODALFRAME 0x00000001L
#define BS_PUSHBUTTON 0x00L
#define BS_DEFPUSHBUTTON 0x01L
#define BS_CHECKBOX 0x02L
#define BS_AUTOCHECKBOX 0x03L
#define BS_RADIOBUTTON 0x04L
#define BS_3STATE 0x05L
#define BS_AUTO3STATE 0x06L
#define BS_GROUPBOX 0x07L
#define BS_USERBUTTON 0x08L
#define BS_AUTORADIOBUTTON 0x09L
#define BS_OWNERDRAW 0x0BL
#define BS_LEFTTEXT 0x20L
#define ES_LEFT 0x0000L
#define ES_CENTER 0x0001L
#define ES_RIGHT 0x0002L
#define ES_MULTILINE 0x0004L
#define ES_UPPERCASE 0x0008L
#define ES_LOWERCASE 0x0010L
#define ES_PASSWORD 0x0020L
#define ES_AUTOVSCROLL 0x0040L
#define ES_AUTOHSCROLL 0x0080L
#define ES_NOHIDESEL 0x0100L
#define ES_READONLY 0x0800L
#define ES_WANTRETURN 0x1000L
#define SS_LEFT 0x00L
#define SS_CENTER 0x01L
#define SS_RIGHT 0x02L
#define SS_ICON 0x03L
#define SS_BLACKRECT 0x04L
#define SS_GRAYRECT 0x05L
#define SS_WHITERECT 0x06L
#define SS_BLACKFRAME 0x07L
#define SS_GRAYFRAME 0x08L
#define SS_WHITEFRAME 0x09L
#define SS_SIMPLE 0x0BL
#define SS_LEFTNOWORDWRAP 0x0CL
#define SS_NOPREFIX 0x80L
#define LBS_NOTIFY 0x0001L
#define LBS_SORT 0x0002L
#define LBS_NOREDRAW 0x0004L
#define LBS_MULTIPLESEL 0x0008L
#define LBS_STANDARD (LBS_NOTIFY | LBS_SORT | WS_VSCROLL | WS_BORDER)
#define CBS_SIMPLE 0x0001L
#define CBS_DROPDOWN 0x0002L
#define CBS_DROPDOWNLIST 0x0003L
#define CBS_AUTOHSCROLL 0x0040L
#define CBS_SORT 0x0100L
#define SBS_HORZ 0x0000L
#define SBS_VERT 0x0001L

/* Controls: notifications and messages. */
#define BN_CLICKED 0
#define EN_CHANGE 0x0300
#define BM_GETCHECK (WM_USER + 0)
#define BM_SETCHECK (WM_USER + 1)
#define EM_LIMITTEXT (WM_USER + 21)
#define LB_ADDSTRING (WM_USER + 1)
#define LB_GETCURSEL (WM_USER + 9)
#define LB_GETCOUNT (WM_USER + 12)
#define CB_ADDSTRING (WM_USER + 3)
#define CB_GETCURSEL (WM_USER + 7)
#define CB_SETCURSEL (WM_USER + 14)
#define BST_UNCHECKED 0
#define BST_CHECKED 1

/* Menus. */
#define MF_ENABLED 0x0000
#define MF_GRAYED 0x0001
#define MF_DISABLED 0x0002
#define MF_STRING 0x0000
#define MF_BITMAP 0x0004
#define MF_UNCHECKED 0x0000
#define MF_CHECKED 0x0008
#define MF_POPUP 0x0010
#define MF_MENUBARBREAK 0x0020
#define MF_MENUBREAK 0x0040
#define MF_HELP 0x4000
#define MF_SEPARATOR 0x0800
#define MF_BYCOMMAND 0x0000
#define MF_BYPOSITION 0x0400

/* Virtual keys, for ACCELERATORS. */
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B

/* Resource types, for FindResource. */
#define RT_CURSOR MAKEINTRESOURCE(1)
#define RT_BITMAP MAKEINTRESOURCE(2)
#define RT_ICON MAKEINTRESOURCE(3)
#define RT_MENU MAKEINTRESOURCE(4)
#define RT_DIALOG MAKEINTRESOURCE(5)
#define RT_STRING MAKEINTRESOURCE(6)
#define RT_FONTDIR MAKEINTRESOURCE(7)
#define RT_FONT MAKEINTRESOURCE(8)
#define RT_ACCELERATOR MAKEINTRESOURCE(9)
#define RT_RCDATA MAKEINTRESOURCE(10)
#define RT_GROUP_CURSOR MAKEINTRESOURCE(12)
#define RT_GROUP_ICON MAKEINTRESOURCE(14)

/* VERSIONINFO (ver.h in the Windows SDK). */
#define VS_FILE_INFO MAKEINTRESOURCE(16)
#define VS_VERSION_INFO 1
#define VS_FFI_FILEFLAGSMASK 0x0000003FL
#define VS_FF_DEBUG 0x00000001L
#define VS_FF_PRERELEASE 0x00000002L
#define VS_FF_PATCHED 0x00000004L
#define VS_FF_PRIVATEBUILD 0x00000008L
#define VS_FF_SPECIALBUILD 0x00000020L
#define VOS_DOS 0x00010000L
#define VOS__WINDOWS16 0x00000001L
#define VOS_DOS_WINDOWS16 0x00010001L
#define VFT_UNKNOWN 0x00000000L
#define VFT_APP 0x00000001L
#define VFT_DLL 0x00000002L
#define VFT_DRV 0x00000003L
#define VFT_FONT 0x00000004L
#define VFT2_UNKNOWN 0x00000000L

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
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
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

#ifndef RC_INVOKED
/* KERNEL. */
DWORD WINAPI GetVersion(void);
#if WINVER >= 0x0300
DWORD WINAPI GetWinFlags(void);
DWORD WINAPI GetFreeSpace(UINT);
#endif
DWORD WINAPI GetTickCount(void);
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
#if WINVER >= 0x0200
void WINAPI OutputDebugString(LPCSTR);
#endif
void WINAPI Yield(void);
int WINAPI lstrlen(LPCSTR);
LPSTR WINAPI lstrcpy(LPSTR, LPCSTR);
LPSTR WINAPI lstrcat(LPSTR, LPCSTR);
int WINAPI lstrcmp(LPCSTR, LPCSTR); /* KERNEL in 2.x, USER from 3.0 */
#if WINVER >= 0x0300
int WINAPI lstrcmpi(LPCSTR, LPCSTR);
#endif
LPSTR WINAPI AnsiUpper(LPSTR); /* also KERNEL in 2.x */
LPSTR WINAPI AnsiLower(LPSTR);
LPSTR WINAPI AnsiNext(LPCSTR);
LPSTR WINAPI AnsiPrev(LPCSTR, LPCSTR);
int WINAPI LoadString(HINSTANCE, UINT, LPSTR, int);
HRSRC WINAPI FindResource(HINSTANCE, LPCSTR, LPCSTR);
HGLOBAL WINAPI LoadResource(HINSTANCE, HRSRC);
void __far *WINAPI LockResource(HGLOBAL);
BOOL WINAPI FreeResource(HGLOBAL);
DWORD WINAPI SizeofResource(HINSTANCE, HRSRC);

/* USER. */
#if WINVER >= 0x0300
int __far __cdecl wsprintf(LPSTR, LPCSTR, ...);
int WINAPI wvsprintf(LPSTR, LPCSTR, const void __far *);
#endif
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
int WINAPI DialogBox(HINSTANCE, LPCSTR, HWND, DLGPROC);
#if WINVER >= 0x0300
int WINAPI DialogBoxParam(HINSTANCE, LPCSTR, HWND, DLGPROC, LPARAM);
#endif
HWND WINAPI CreateDialog(HINSTANCE, LPCSTR, HWND, DLGPROC);
void WINAPI EndDialog(HWND, int);
HWND WINAPI GetDlgItem(HWND, int);
void WINAPI SetDlgItemText(HWND, int, LPCSTR);
int WINAPI GetDlgItemText(HWND, int, LPSTR, int);
void WINAPI SetDlgItemInt(HWND, int, UINT, BOOL);
UINT WINAPI GetDlgItemInt(HWND, int, BOOL __far *, BOOL);
void WINAPI CheckDlgButton(HWND, int, UINT);
UINT WINAPI IsDlgButtonChecked(HWND, int);
LRESULT WINAPI SendDlgItemMessage(HWND, int, UINT, WPARAM, LPARAM);
BOOL WINAPI IsDialogMessage(HWND, LPMSG);
HMENU WINAPI LoadMenu(HINSTANCE, LPCSTR);
HMENU WINAPI GetMenu(HWND);
BOOL WINAPI SetMenu(HWND, HMENU);
HMENU WINAPI GetSubMenu(HMENU, int);
#if WINVER >= 0x0200
int WINAPI GetMenuItemCount(HMENU);
UINT WINAPI GetMenuItemID(HMENU, int);
#endif
int WINAPI GetMenuString(HMENU, UINT, LPSTR, int, UINT);
BOOL WINAPI DestroyMenu(HMENU);
HACCEL WINAPI LoadAccelerators(HINSTANCE, LPCSTR);
int WINAPI TranslateAccelerator(HWND, HACCEL, LPMSG);
HBITMAP WINAPI LoadBitmap(HINSTANCE, LPCSTR);

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
#endif /* RC_INVOKED */

#ifdef __cplusplus
}
#endif

#endif /* _WINDOWS_H */
