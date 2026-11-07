/* <cwin.h>: character windows for 16-bit DOS, in C.
 *
 * A text-mode window system in the style of the character-mode Windows
 * used by DOS applications of the late 1980s (Word 5, Works, QuickC,
 * the DOS Shell, EDIT): a menu bar on the top line with pull-down menus,
 * overlapping windows, modal dialogs with shadows, push buttons drawn as
 * < OK >, check boxes, option buttons, edit fields, list boxes, scroll
 * bars and message boxes, driven by the keyboard and the mouse.
 *
 * The programming interface follows the public Windows 3.x API: window
 * classes and window procedures, a message queue (GetMessage,
 * DispatchMessage, SendMessage, PostMessage), WM_ messages, the BUTTON,
 * STATIC, EDIT and LISTBOX classes with their BM_, EM_ and LB_ messages,
 * menus built with AppendMenu, MessageBox, carets, timers and
 * accelerators, with the same names, values and parameter packing, so
 * that code written for Windows 3.x translates with little change.
 *
 * The differences come from text mode:
 *   - Coordinates and sizes are in character cells, everywhere (window
 *     positions, client areas, dialog templates, mouse messages).
 *   - A color is a text attribute, background * 16 + foreground, and a
 *     brush is CW_BRUSH(attribute) or (HBRUSH)(COLOR_xxx + 1).
 *   - Dialogs are made from a CWDLGTEMPLATE in C instead of a resource,
 *     with CwDialogBox.
 *   - The whole screen is repainted (WM_PAINT for every visible window)
 *     when anything is invalid, before GetMessage waits; draw in WM_PAINT.
 *   - Lines of a multiline edit control end in '\n' only.
 *   - There are no GDI pens, fonts or bitmaps, no horizontal scroll
 *     bars, no MDI and no clipboard outside one program.
 *
 * Functions without a Windows counterpart start with Cw.
 *
 * A program defines WinMain, as for Windows; the library's main calls it
 * (a program can define main instead). Link with -lcwin.
 *
 * This is an independent implementation written from the public
 * Windows 3.x documentation and from how such programs look and behave.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef _CWIN_H_
#define _CWIN_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Types, as in Windows 3.x. ---- */

#define FAR
#define NEAR
#define PASCAL
#define CALLBACK
#define WINAPI
#define CONST const

#ifndef NULL
#define NULL 0
#endif
#define FALSE 0
#define TRUE 1

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef long LONG;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef void *LPVOID;
typedef UINT WPARAM;
typedef LONG LPARAM;
typedef LONG LRESULT;

/* Handles are 16-bit numbers, as in Windows 3.x, so they fit in a WPARAM
 * or a half of an LPARAM. 0 is no handle. */
typedef UINT HANDLE;
typedef HANDLE HWND;
typedef HANDLE HMENU;
typedef HANDLE HDC;
typedef HANDLE HINSTANCE;
typedef HANDLE HBRUSH;
typedef HANDLE HACCEL;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HBITMAP;

#define MAKELONG(lo, hi) ((LONG)(((WORD)(lo)) | ((DWORD)(WORD)(hi)) << 16))
#define LOWORD(l) ((WORD)(DWORD)(l))
#define HIWORD(l) ((WORD)((DWORD)(l) >> 16))
#define LOBYTE(w) ((BYTE)(w))
#define HIBYTE(w) ((BYTE)((WORD)(w) >> 8))
#define MAKELPARAM(lo, hi) ((LPARAM)MAKELONG(lo, hi))
#define MAKEPOINT(l) (*(POINT *)&(l))

typedef struct tagPOINT {
    int x, y;
} POINT, *LPPOINT;

typedef struct tagRECT {
    int left, top, right, bottom;
} RECT, *LPRECT;

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef BOOL (*DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void (*TIMERPROC)(HWND, UINT, UINT, DWORD);

typedef struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;     /* BIOS ticks */
    POINT pt;       /* the mouse, in screen cells */
} MSG, *LPMSG;

typedef struct tagWNDCLASS {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground; /* CW_BRUSH(attr), (HBRUSH)(COLOR_xxx + 1) or 0 */
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
} WNDCLASS, *LPWNDCLASS;

typedef struct tagCREATESTRUCT {
    LPVOID lpCreateParams;
    HINSTANCE hInstance;
    HMENU hMenu;
    HWND hwndParent;
    int cy, cx, y, x;
    LONG style;
    LPCSTR lpszName;
    LPCSTR lpszClass;
    DWORD dwExStyle;
} CREATESTRUCT, *LPCREATESTRUCT;

typedef struct tagPAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
} PAINTSTRUCT, *LPPAINTSTRUCT;

typedef struct tagACCEL {
    BYTE fVirt;
    WORD key;
    WORD cmd;
} ACCEL;

/* ---- Window styles. ---- */
#define WS_OVERLAPPED   0x00000000L
#define WS_POPUP        0x80000000L
#define WS_CHILD        0x40000000L
#define WS_MINIMIZE     0x20000000L
#define WS_VISIBLE      0x10000000L
#define WS_DISABLED     0x08000000L
#define WS_CLIPSIBLINGS 0x04000000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_MAXIMIZE     0x01000000L
#define WS_CAPTION      0x00C00000L
#define WS_BORDER       0x00800000L
#define WS_DLGFRAME     0x00400000L
#define WS_VSCROLL      0x00200000L
#define WS_HSCROLL      0x00100000L
#define WS_SYSMENU      0x00080000L
#define WS_THICKFRAME   0x00040000L
#define WS_GROUP        0x00020000L
#define WS_TABSTOP      0x00010000L
#define WS_MINIMIZEBOX  0x00020000L
#define WS_MAXIMIZEBOX  0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUPWINDOW  (WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_CHILDWINDOW  WS_CHILD

#define CS_VREDRAW  0x0001
#define CS_HREDRAW  0x0002
#define CS_DBLCLKS  0x0008

#define CW_USEDEFAULT ((int)0x8000)

/* Dialog styles. */
#define DS_MODALFRAME 0x80L
#define DS_CENTER     0x0800L  /* center over the parent (x and y ignored) */

/* ---- Messages. ---- */
#define WM_NULL           0x0000
#define WM_CREATE         0x0001
#define WM_DESTROY        0x0002
#define WM_MOVE           0x0003
#define WM_SIZE           0x0005
#define WM_ACTIVATE       0x0006
#define WM_SETFOCUS       0x0007
#define WM_KILLFOCUS      0x0008
#define WM_ENABLE         0x000A
#define WM_SETTEXT        0x000C
#define WM_GETTEXT        0x000D
#define WM_GETTEXTLENGTH  0x000E
#define WM_PAINT          0x000F
#define WM_CLOSE          0x0010
#define WM_QUIT           0x0012
#define WM_ERASEBKGND     0x0014
#define WM_SHOWWINDOW     0x0018
#define WM_GETDLGCODE     0x0087
#define WM_KEYDOWN        0x0100
#define WM_KEYUP          0x0101
#define WM_CHAR           0x0102
#define WM_SYSKEYDOWN     0x0104
#define WM_SYSCHAR        0x0106
#define WM_INITDIALOG     0x0110
#define WM_COMMAND        0x0111
#define WM_SYSCOMMAND     0x0112
#define WM_TIMER          0x0113
#define WM_VSCROLL        0x0115
#define WM_INITMENUPOPUP  0x0117
#define WM_MENUSELECT     0x011F
#define WM_MOUSEMOVE      0x0200
#define WM_LBUTTONDOWN    0x0201
#define WM_LBUTTONUP      0x0202
#define WM_LBUTTONDBLCLK  0x0203
#define WM_RBUTTONDOWN    0x0204
#define WM_RBUTTONUP      0x0205
#define WM_USER           0x0400

/* WM_ACTIVATE */
#define WA_INACTIVE 0
#define WA_ACTIVE   1
#define WA_CLICKACTIVE 2

/* WM_SIZE */
#define SIZE_RESTORED  0
#define SIZE_MAXIMIZED 2

/* WM_SYSCOMMAND */
#define SC_SIZE     0xF000
#define SC_MOVE     0xF010
#define SC_MAXIMIZE 0xF030
#define SC_CLOSE    0xF060
#define SC_RESTORE  0xF120

/* Mouse key state in wParam. */
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT   0x0004
#define MK_CONTROL 0x0008

/* WM_VSCROLL codes, and scroll bar functions. */
#define SB_LINEUP        0
#define SB_LINEDOWN      1
#define SB_PAGEUP        2
#define SB_PAGEDOWN      3
#define SB_THUMBPOSITION 4
#define SB_THUMBTRACK    5
#define SB_TOP           6
#define SB_BOTTOM        7
#define SB_ENDSCROLL     8
#define SB_HORZ 0
#define SB_VERT 1
#define SB_CTL  2

/* ---- Virtual keys (WM_KEYDOWN wParam). ---- */
#define VK_BACK    0x08
#define VK_TAB     0x09
#define VK_RETURN  0x0D
#define VK_SHIFT   0x10
#define VK_CONTROL 0x11
#define VK_MENU    0x12
#define VK_ESCAPE  0x1B
#define VK_SPACE   0x20
#define VK_PRIOR   0x21
#define VK_NEXT    0x22
#define VK_END     0x23
#define VK_HOME    0x24
#define VK_LEFT    0x25
#define VK_UP      0x26
#define VK_RIGHT   0x27
#define VK_DOWN    0x28
#define VK_INSERT  0x2D
#define VK_DELETE  0x2E
#define VK_MULTIPLY 0x6A
#define VK_ADD     0x6B
#define VK_SUBTRACT 0x6D
#define VK_F1  0x70
#define VK_F2  0x71
#define VK_F3  0x72
#define VK_F4  0x73
#define VK_F5  0x74
#define VK_F6  0x75
#define VK_F7  0x76
#define VK_F8  0x77
#define VK_F9  0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
/* Letters and digits are their upper case ASCII codes. */

/* ---- Window functions. ---- */
BOOL RegisterClass(const WNDCLASS *wc);
BOOL UnregisterClass(LPCSTR name, HINSTANCE inst);
BOOL GetClassInfo(HINSTANCE inst, LPCSTR name, WNDCLASS *wc);
HWND CreateWindow(LPCSTR cls, LPCSTR title, DWORD style, int x, int y, int cx, int cy,
                  HWND parent, HMENU menuOrId, HINSTANCE inst, LPVOID param);
HWND CreateWindowEx(DWORD exStyle, LPCSTR cls, LPCSTR title, DWORD style, int x, int y,
                    int cx, int cy, HWND parent, HMENU menuOrId, HINSTANCE inst, LPVOID param);
BOOL DestroyWindow(HWND hwnd);
BOOL IsWindow(HWND hwnd);
BOOL ShowWindow(HWND hwnd, int cmd);
BOOL IsWindowVisible(HWND hwnd);
BOOL IsZoomed(HWND hwnd);
BOOL EnableWindow(HWND hwnd, BOOL enable);
BOOL IsWindowEnabled(HWND hwnd);
BOOL MoveWindow(HWND hwnd, int x, int y, int cx, int cy, BOOL repaint);
void GetWindowRect(HWND hwnd, LPRECT rc);   /* screen cells */
void GetClientRect(HWND hwnd, LPRECT rc);   /* 0, 0, width, height */
void ClientToScreen(HWND hwnd, LPPOINT pt);
void ScreenToClient(HWND hwnd, LPPOINT pt);
HWND WindowFromPoint(POINT pt);
HWND GetParent(HWND hwnd);
HWND GetDesktopWindow(void);
HWND GetWindow(HWND hwnd, UINT cmd);
HWND GetTopWindow(HWND hwnd);
BOOL BringWindowToTop(HWND hwnd);
HWND SetActiveWindow(HWND hwnd);
HWND GetActiveWindow(void);
HWND SetFocus(HWND hwnd);
HWND GetFocus(void);
HWND SetCapture(HWND hwnd);
void ReleaseCapture(void);
HWND GetCapture(void);
void SetWindowText(HWND hwnd, LPCSTR text);
int GetWindowText(HWND hwnd, LPSTR buf, int max);
int GetWindowTextLength(HWND hwnd);
LONG GetWindowLong(HWND hwnd, int index);
LONG SetWindowLong(HWND hwnd, int index, LONG value);
WORD GetWindowWord(HWND hwnd, int index);
WORD SetWindowWord(HWND hwnd, int index, WORD value);
int GetClassName(HWND hwnd, LPSTR buf, int max);
HWND FindWindow(LPCSTR cls, LPCSTR title);
LRESULT DefWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CallWindowProc(WNDPROC proc, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#define SW_HIDE            0
#define SW_SHOWNORMAL      1
#define SW_NORMAL          1
#define SW_SHOWMINIMIZED   2
#define SW_SHOWMAXIMIZED   3
#define SW_MAXIMIZE        3
#define SW_SHOWNOACTIVATE  4
#define SW_SHOW            5
#define SW_RESTORE         9

#define GW_HWNDFIRST 0
#define GW_HWNDLAST  1
#define GW_HWNDNEXT  2
#define GW_HWNDPREV  3
#define GW_OWNER     4
#define GW_CHILD     5

#define GWL_WNDPROC   (-4)
#define GWW_HINSTANCE (-6)
#define GWW_HWNDPARENT (-8)
#define GWW_ID        (-12)
#define GWL_ID        (-12)
#define GWL_STYLE     (-16)
#define GWL_EXSTYLE   (-20)
#define GWL_USERDATA  (-21)

/* ---- Messages. ---- */
BOOL GetMessage(LPMSG msg, HWND hwnd, UINT first, UINT last);
BOOL PeekMessage(LPMSG msg, HWND hwnd, UINT first, UINT last, UINT flags);
BOOL TranslateMessage(const MSG *msg);
LRESULT DispatchMessage(const MSG *msg);
LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
BOOL PostMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void PostQuitMessage(int code);
BOOL WaitMessage(void);
DWORD GetTickCount(void);    /* milliseconds, from the BIOS clock */
DWORD GetMessageTime(void);
int GetKeyState(int vkey);   /* negative while the key is down */
#define PM_NOREMOVE 0
#define PM_REMOVE   1

UINT SetTimer(HWND hwnd, UINT id, UINT ms, TIMERPROC proc);
BOOL KillTimer(HWND hwnd, UINT id);

HACCEL CwCreateAcceleratorTable(const ACCEL *table, int n);
BOOL DestroyAcceleratorTable(HACCEL acc);
int TranslateAccelerator(HWND hwnd, HACCEL acc, LPMSG msg);
#define FVIRTKEY  0x01
#define FNOINVERT 0x02
#define FSHIFT    0x04
#define FCONTROL  0x08
#define FALT      0x10

void MessageBeep(UINT type);

/* ---- Painting. ---- */
HDC BeginPaint(HWND hwnd, LPPAINTSTRUCT ps);
void EndPaint(HWND hwnd, const PAINTSTRUCT *ps);
HDC GetDC(HWND hwnd);
int ReleaseDC(HWND hwnd, HDC hdc);
void InvalidateRect(HWND hwnd, const RECT *rc, BOOL erase);
void ValidateRect(HWND hwnd, const RECT *rc);
void UpdateWindow(HWND hwnd);

/* The colors of SetTextColor and SetBkColor are the 16 text colors. */
#define CW_BLACK 0
#define CW_BLUE 1
#define CW_GREEN 2
#define CW_CYAN 3
#define CW_RED 4
#define CW_MAGENTA 5
#define CW_BROWN 6
#define CW_LIGHTGRAY 7
#define CW_DARKGRAY 8
#define CW_LIGHTBLUE 9
#define CW_LIGHTGREEN 10
#define CW_LIGHTCYAN 11
#define CW_LIGHTRED 12
#define CW_LIGHTMAGENTA 13
#define CW_YELLOW 14
#define CW_WHITE 15
#define CW_ATTR(fg, bg) ((BYTE)((bg) << 4 | (fg)))
#define CW_BRUSH(attr) ((HBRUSH)(0x100 | (BYTE)(attr)))
typedef DWORD COLORREF;
COLORREF SetTextColor(HDC hdc, COLORREF color);
COLORREF SetBkColor(HDC hdc, COLORREF color);
BYTE CwSetTextAttr(HDC hdc, BYTE attr);   /* both at once; returns the old */
BOOL TextOut(HDC hdc, int x, int y, LPCSTR s, int n);
int DrawText(HDC hdc, LPCSTR s, int n, LPRECT rc, UINT fmt);
int FillRect(HDC hdc, const RECT *rc, HBRUSH br);
/* Fill with a character and attribute; draw a single or double frame. */
void CwFillRect(HDC hdc, const RECT *rc, BYTE ch, BYTE attr);
void CwDrawBox(HDC hdc, const RECT *rc, BYTE attr, BOOL dbl);
void CwPutChar(HDC hdc, int x, int y, BYTE ch, BYTE attr);

#define DT_LEFT       0x0000
#define DT_CENTER     0x0001
#define DT_RIGHT      0x0002
#define DT_VCENTER    0x0004
#define DT_BOTTOM     0x0008
#define DT_WORDBREAK  0x0010
#define DT_SINGLELINE 0x0020
#define DT_CALCRECT   0x0400
#define DT_NOPREFIX   0x0800

/* System colors: text attributes. */
#define COLOR_SCROLLBAR       0
#define COLOR_BACKGROUND      1
#define COLOR_ACTIVECAPTION   2
#define COLOR_INACTIVECAPTION 3
#define COLOR_MENU            4
#define COLOR_WINDOW          5
#define COLOR_WINDOWFRAME     6
#define COLOR_MENUTEXT        7
#define COLOR_WINDOWTEXT      8
#define COLOR_CAPTIONTEXT     9
#define COLOR_ACTIVEBORDER    10
#define COLOR_INACTIVEBORDER  11
#define COLOR_APPWORKSPACE    12
#define COLOR_HIGHLIGHT       13
#define COLOR_HIGHLIGHTTEXT   14
#define COLOR_BTNFACE         15
#define COLOR_BTNSHADOW       16
#define COLOR_GRAYTEXT        17
#define COLOR_BTNTEXT         18
/* The rest have no Windows counterpart. */
#define COLOR_MENUHOT         19  /* a menu's mnemonic letter */
#define COLOR_MENUSEL         20  /* the selected menu item */
#define COLOR_MENUSELHOT      21
#define COLOR_DIALOG          22  /* dialog and message box background */
#define COLOR_DIALOGHOT       23  /* mnemonics in dialogs */
#define COLOR_BTNFOCUS        24  /* a focused push button */
#define COLOR_EDIT            25  /* edit fields and list boxes in dialogs */
#define COLOR_STATUS          26  /* the status line */
#define COLOR_SHADOW          27
#define CW_NCOLORS            28
DWORD GetSysColor(int index);
void SetSysColors(int n, const int *indices, const COLORREF *colors);

/* ---- Carets. ---- */
BOOL CreateCaret(HWND hwnd, HBITMAP bm, int w, int h);
BOOL DestroyCaret(void);
BOOL ShowCaret(HWND hwnd);
BOOL HideCaret(HWND hwnd);
BOOL SetCaretPos(int x, int y);
BOOL GetCaretPos(LPPOINT pt);

/* ---- Scroll bars (WS_VSCROLL: the right column of the window). ---- */
int SetScrollPos(HWND hwnd, int bar, int pos, BOOL redraw);
int GetScrollPos(HWND hwnd, int bar);
void SetScrollRange(HWND hwnd, int bar, int min, int max, BOOL redraw);
void GetScrollRange(HWND hwnd, int bar, int *min, int *max);

/* ---- Menus. A & marks the mnemonic letter; text after a \t is shown at
 * the right (an accelerator key). ---- */
HMENU CreateMenu(void);
HMENU CreatePopupMenu(void);
BOOL DestroyMenu(HMENU menu);
BOOL AppendMenu(HMENU menu, UINT flags, UINT idOrPopup, LPCSTR text);
BOOL SetMenu(HWND hwnd, HMENU menu);
HMENU GetMenu(HWND hwnd);
HMENU GetSubMenu(HMENU menu, int pos);
int GetMenuItemCount(HMENU menu);
UINT GetMenuItemID(HMENU menu, int pos);
UINT GetMenuState(HMENU menu, UINT item, UINT flags);
int GetMenuString(HMENU menu, UINT item, LPSTR buf, int max, UINT flags);
BOOL ModifyMenu(HMENU menu, UINT item, UINT flags, UINT idOrPopup, LPCSTR text);
BOOL DeleteMenu(HMENU menu, UINT item, UINT flags);
UINT CheckMenuItem(HMENU menu, UINT item, UINT flags);
BOOL EnableMenuItem(HMENU menu, UINT item, UINT flags);
void DrawMenuBar(HWND hwnd);
BOOL TrackPopupMenu(HMENU menu, UINT flags, int x, int y, int reserved, HWND owner, const RECT *rc);

#define MF_INSERT       0x0000
#define MF_CHANGE       0x0080
#define MF_APPEND       0x0100
#define MF_DELETE       0x0200
#define MF_REMOVE       0x1000
#define MF_BYCOMMAND    0x0000
#define MF_BYPOSITION   0x0400
#define MF_SEPARATOR    0x0800
#define MF_ENABLED      0x0000
#define MF_GRAYED       0x0001
#define MF_DISABLED     0x0002
#define MF_UNCHECKED    0x0000
#define MF_CHECKED      0x0008
#define MF_STRING       0x0000
#define MF_POPUP        0x0010
#define MF_MENUBARBREAK 0x0020
#define MF_MENUBREAK    0x0040
#define MF_HELP         0x4000  /* in a menu bar: at the right end */
#define MF_SYSMENU      0x2000
#define MF_HILITE       0x0080
#define TPM_LEFTALIGN   0x0000

/* ---- Controls. ---- */

/* BUTTON */
#define BS_PUSHBUTTON      0x00L
#define BS_DEFPUSHBUTTON   0x01L
#define BS_CHECKBOX        0x02L
#define BS_AUTOCHECKBOX    0x03L
#define BS_RADIOBUTTON     0x04L
#define BS_3STATE          0x05L
#define BS_AUTO3STATE      0x06L
#define BS_GROUPBOX        0x07L
#define BS_AUTORADIOBUTTON 0x09L
#define BS_LEFTTEXT        0x20L
#define BM_GETCHECK  (WM_USER + 0)
#define BM_SETCHECK  (WM_USER + 1)
#define BM_GETSTATE  (WM_USER + 2)
#define BM_SETSTATE  (WM_USER + 3)
#define BM_SETSTYLE  (WM_USER + 4)
#define BN_CLICKED 0
#define BST_UNCHECKED 0
#define BST_CHECKED   1
#define BST_INDETERMINATE 2

/* STATIC */
#define SS_LEFT     0x00L
#define SS_CENTER   0x01L
#define SS_RIGHT    0x02L
#define SS_SIMPLE   0x0BL
#define SS_NOPREFIX 0x80L

/* EDIT */
#define ES_LEFT        0x0000L
#define ES_CENTER      0x0001L
#define ES_RIGHT       0x0002L
#define ES_MULTILINE   0x0004L
#define ES_UPPERCASE   0x0008L
#define ES_LOWERCASE   0x0010L
#define ES_PASSWORD    0x0020L
#define ES_AUTOVSCROLL 0x0040L
#define ES_AUTOHSCROLL 0x0080L
#define ES_NOHIDESEL   0x0100L
#define ES_READONLY    0x0800L
#define ES_WANTRETURN  0x1000L
#define EM_GETSEL        (WM_USER + 0)
#define EM_SETSEL        (WM_USER + 1)
#define EM_GETRECT       (WM_USER + 2)
#define EM_SCROLL        (WM_USER + 5)
#define EM_LINESCROLL    (WM_USER + 6)
#define EM_GETMODIFY     (WM_USER + 8)
#define EM_SETMODIFY     (WM_USER + 9)
#define EM_GETLINECOUNT  (WM_USER + 10)
#define EM_LINEINDEX     (WM_USER + 11)
#define EM_LINELENGTH    (WM_USER + 17)
#define EM_REPLACESEL    (WM_USER + 18)
#define EM_GETLINE       (WM_USER + 20)
#define EM_LIMITTEXT     (WM_USER + 21)
#define EM_LINEFROMCHAR  (WM_USER + 25)
#define EM_SETREADONLY   (WM_USER + 31)
#define EN_SETFOCUS  0x0100
#define EN_KILLFOCUS 0x0200
#define EN_CHANGE    0x0300
#define EN_MAXTEXT   0x0501

/* LISTBOX */
#define LBS_NOTIFY   0x0001L
#define LBS_SORT     0x0002L
#define LBS_STANDARD (LBS_NOTIFY | LBS_SORT | WS_VSCROLL | WS_BORDER)
#define LB_ADDSTRING    (WM_USER + 1)
#define LB_INSERTSTRING (WM_USER + 2)
#define LB_DELETESTRING (WM_USER + 3)
#define LB_RESETCONTENT (WM_USER + 5)
#define LB_SETCURSEL    (WM_USER + 7)
#define LB_GETCURSEL    (WM_USER + 9)
#define LB_GETTEXT      (WM_USER + 10)
#define LB_GETTEXTLEN   (WM_USER + 11)
#define LB_GETCOUNT     (WM_USER + 12)
#define LB_SELECTSTRING (WM_USER + 13)
#define LB_GETTOPINDEX  (WM_USER + 15)
#define LB_FINDSTRING   (WM_USER + 16)
#define LB_SETTOPINDEX  (WM_USER + 24)
#define LB_OKAY 0
#define LB_ERR (-1)
#define LB_ERRSPACE (-2)
#define LBN_ERRSPACE (-2)
#define LBN_SELCHANGE 1
#define LBN_DBLCLK    2

/* WM_GETDLGCODE */
#define DLGC_WANTARROWS     0x0001
#define DLGC_WANTTAB        0x0002
#define DLGC_WANTALLKEYS    0x0004
#define DLGC_HASSETSEL      0x0008
#define DLGC_DEFPUSHBUTTON  0x0010
#define DLGC_UNDEFPUSHBUTTON 0x0020
#define DLGC_RADIOBUTTON    0x0040
#define DLGC_WANTCHARS      0x0080
#define DLGC_STATIC         0x0100
#define DLGC_BUTTON         0x2000

/* ---- Dialogs. ---- */

/* A control of a dialog template: its class ("BUTTON", "EDIT", "STATIC",
 * "LISTBOX" or a registered class), text, id, style (WS_CHILD and
 * WS_VISIBLE are added) and place in the dialog's client area. */
typedef struct tagCWDLGITEM {
    LPCSTR cls;
    LPCSTR text;
    UINT id;
    DWORD style;
    int x, y, cx, cy;
} CWDLGITEM;

/* A dialog: style (WS_POPUP | WS_CAPTION ... ; DS_CENTER to center it),
 * the place and size of the whole dialog, its caption and controls. */
typedef struct tagCWDLGTEMPLATE {
    DWORD style;
    int x, y, cx, cy;
    LPCSTR caption;
    int nitems;
    const CWDLGITEM *items;
} CWDLGTEMPLATE;

int CwDialogBox(HWND parent, const CWDLGTEMPLATE *t, DLGPROC proc, LPARAM param);
HWND CwCreateDialog(HWND parent, const CWDLGTEMPLATE *t, DLGPROC proc, LPARAM param);
BOOL EndDialog(HWND dlg, int result);
LRESULT DefDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
BOOL IsDialogMessage(HWND dlg, LPMSG msg);
HWND GetDlgItem(HWND dlg, int id);
int GetDlgCtrlID(HWND hwnd);
LRESULT SendDlgItemMessage(HWND dlg, int id, UINT msg, WPARAM wp, LPARAM lp);
void SetDlgItemText(HWND dlg, int id, LPCSTR text);
int GetDlgItemText(HWND dlg, int id, LPSTR buf, int max);
void SetDlgItemInt(HWND dlg, int id, UINT value, BOOL sign);
UINT GetDlgItemInt(HWND dlg, int id, BOOL *ok, BOOL sign);
void CheckDlgButton(HWND dlg, int id, UINT check);
UINT IsDlgButtonChecked(HWND dlg, int id);
void CheckRadioButton(HWND dlg, int first, int last, int check);
HWND GetNextDlgTabItem(HWND dlg, HWND ctl, BOOL prev);
HWND GetNextDlgGroupItem(HWND dlg, HWND ctl, BOOL prev);
#define DWL_MSGRESULT 0
#define DWL_DLGPROC   4
#define DWL_USER      8

#define IDOK     1
#define IDCANCEL 2
#define IDABORT  3
#define IDRETRY  4
#define IDIGNORE 5
#define IDYES    6
#define IDNO     7

int MessageBox(HWND parent, LPCSTR text, LPCSTR caption, UINT type);
#define MB_OK               0x0000
#define MB_OKCANCEL         0x0001
#define MB_ABORTRETRYIGNORE 0x0002
#define MB_YESNOCANCEL      0x0003
#define MB_YESNO            0x0004
#define MB_RETRYCANCEL      0x0005
#define MB_ICONHAND         0x0010
#define MB_ICONSTOP         MB_ICONHAND
#define MB_ICONQUESTION     0x0020
#define MB_ICONEXCLAMATION  0x0030
#define MB_ICONASTERISK     0x0040
#define MB_ICONINFORMATION  MB_ICONASTERISK
#define MB_DEFBUTTON1       0x0000
#define MB_DEFBUTTON2       0x0100
#define MB_DEFBUTTON3       0x0200

/* ---- The screen. ---- */

/* The status line on the bottom row of the screen (NULL removes it). A
 * \t separates the part shown at the right. The desktop gets one row
 * less while it is there. */
void CwSetStatusText(LPCSTR text);
/* The size of the screen, in cells. */
int CwScreenCols(void);
int CwScreenRows(void);
/* Put the screen back as it was and stop (also done at exit). */
void CwShutdown(void);

int GetSystemMetrics(int index);
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_MOUSEPRESENT 19

/* The program's entry point, as for Windows. */
int PASCAL WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int cmdShow);

#ifdef __cplusplus
}
#endif

#endif
