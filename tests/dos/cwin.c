/* <cwin.h> without a person: windows, messages, controls, menus, a modal
 * dialog driven by posted keys, a message box, and what reaches video
 * memory. Prints ALL PASSED.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#include <cwin.h>
#include <stdio.h>
#include <string.h>

static int fails, line[32], nfail;
#define CHECK(c) do { if (!(c) && nfail < 32) line[nfail++] = __LINE__, fails++; } while (0)

static int created, destroyed, sized, painted, lastCmd, lastNotify, timers;

static LRESULT
testProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        created++;
        CHECK(((CREATESTRUCT *)(LPVOID)lp)->lpCreateParams == (LPVOID)&created);
        return 0;
    case WM_SIZE:
        sized++;
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        CwSetTextAttr(dc, 0x1e);
        TextOut(dc, 0, 0, "PAINT", 5);
        RECT r = { 0, 1, 10, 2 };
        DrawText(dc, "&Hot", -1, &r, DT_SINGLELINE | DT_CENTER);
        EndPaint(h, &ps);
        painted++;
        return 0;
    }
    case WM_COMMAND:
        lastCmd = (int)wp;
        lastNotify = HIWORD(lp);
        return 0;
    case WM_TIMER:
        timers++;
        KillTimer(h, wp);
        PostQuitMessage(7);
        return 0;
    case WM_DESTROY:
        destroyed++;
        return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

/* A cell of video memory, character in the low byte. */
static unsigned
cell(int x, int y)
{
    unsigned char mode = *(volatile unsigned char __far *)0x00400049L;
    unsigned long seg = mode == 7 ? 0xb000 : 0xb800;
    return *(volatile unsigned __far *)((seg << 16) | (unsigned)((y * CwScreenCols() + x) * 2));
}

static int
screenHas(int x, int y, const char *s)
{
    for (int i = 0; s[i]; i++)
        if ((cell(x + i, y) & 0xff) != (unsigned char)s[i])
            return 0;
    return 1;
}

enum { ID_NAME = 10, ID_CHECK, ID_R1, ID_R2, ID_LIST };
static char gotName[32];
static int gotCheck, gotRadio, gotList, initSeen;

static BOOL
dlgProc(HWND d, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        initSeen = (int)lp;
        SetDlgItemText(d, ID_NAME, "abc");
        CheckRadioButton(d, ID_R1, ID_R2, ID_R1);
        SendDlgItemMessage(d, ID_LIST, LB_ADDSTRING, 0, (LPARAM)(LPSTR) "pear");
        SendDlgItemMessage(d, ID_LIST, LB_ADDSTRING, 0, (LPARAM)(LPSTR) "apple");
        SendDlgItemMessage(d, ID_LIST, LB_ADDSTRING, 0, (LPARAM)(LPSTR) "fig");
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK) {
            GetDlgItemText(d, ID_NAME, gotName, sizeof gotName);
            gotCheck = IsDlgButtonChecked(d, ID_CHECK);
            gotRadio = IsDlgButtonChecked(d, ID_R2) ? 2 : IsDlgButtonChecked(d, ID_R1) ? 1 : 0;
            gotList = (int)SendDlgItemMessage(d, ID_LIST, LB_GETCURSEL, 0, 0);
            EndDialog(d, 42);
        } else if (wp == IDCANCEL) {
            EndDialog(d, IDCANCEL);
        }
        return TRUE;
    }
    return FALSE;
}

static const CWDLGITEM items[] = {
    { "STATIC", "&Name:", 0, 0, 1, 1, 6, 1 },
    { "EDIT", "", ID_NAME, WS_BORDER | WS_TABSTOP, 8, 0, 20, 3 },
    { "BUTTON", "&Check", ID_CHECK, BS_AUTOCHECKBOX | WS_TABSTOP, 1, 3, 12, 1 },
    { "BUTTON", "One", ID_R1, BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP, 1, 4, 10, 1 },
    { "BUTTON", "Two", ID_R2, BS_AUTORADIOBUTTON, 1, 5, 10, 1 },
    { "LISTBOX", "", ID_LIST, WS_BORDER | WS_TABSTOP | LBS_SORT | WS_GROUP, 14, 3, 14, 5 },
    { "BUTTON", "OK", IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP, 2, 8, 10, 1 },
    { "BUTTON", "Cancel", IDCANCEL, WS_TABSTOP, 16, 8, 10, 1 },
};
static const CWDLGTEMPLATE dlg = { WS_POPUP | WS_CAPTION | DS_CENTER, 0, 0, 32, 12, "Test", 8, items };

/* Keys typed by a timer, one each tick, to whatever has the focus then. */
struct key {
    UINT msg;
    int v;
};
static const struct key *script;
static int scriptLen, scriptPos;
static UINT scriptTimer;

static void
typist(HWND h, UINT msg, UINT id, DWORD time)
{
    (void)h;
    (void)msg;
    (void)time;
    if (scriptPos >= scriptLen) {
        KillTimer(0, id);
        scriptTimer = 0;
        return;
    }
    const struct key *k = &script[scriptPos++];
    PostMessage(GetFocus(), k->msg, k->v, 0);
}

static void
type(const struct key *k, int n)
{
    script = k;
    scriptLen = n;
    scriptPos = 0;
    if (scriptTimer)
        KillTimer(0, scriptTimer);
    scriptTimer = SetTimer(0, 0, 55, typist);
}

int
main(void)
{
    WNDCLASS wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = testProc;
    wc.hbrBackground = CW_BRUSH(0x17);
    wc.cbWndExtra = 4;
    wc.lpszClassName = "Test";
    CHECK(RegisterClass(&wc));
    CHECK(!RegisterClass(&wc));

    HMENU bar = CreateMenu(), pop = CreatePopupMenu();
    CHECK(AppendMenu(pop, MF_STRING, 300, "&Open"));
    CHECK(AppendMenu(pop, MF_SEPARATOR, 0, 0));
    CHECK(AppendMenu(pop, MF_STRING | MF_GRAYED, 301, "&Close"));
    CHECK(AppendMenu(bar, MF_POPUP, pop, "&File"));
    CHECK(AppendMenu(bar, MF_STRING, 302, "&Run"));
    CHECK(GetMenuItemCount(bar) == 2 && GetSubMenu(bar, 0) == pop);
    CHECK(GetMenuItemID(pop, 0) == 300 && GetMenuItemID(bar, 0) == (UINT)-1);
    CHECK(CheckMenuItem(bar, 300, MF_CHECKED) == MF_UNCHECKED);
    CHECK(GetMenuState(bar, 300, MF_BYCOMMAND) & MF_CHECKED);
    CHECK(EnableMenuItem(bar, 301, MF_ENABLED) == MF_GRAYED);
    char ms[16];
    CHECK(GetMenuString(bar, 1, ms, sizeof ms, MF_BYPOSITION) == 4 && !strcmp(ms, "&Run"));

    HWND w = CreateWindow("Test", "Main", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 2, 1, 40, 12, 0, bar, 0, &created);
    CHECK(w && created == 1 && sized >= 1);
    CHECK(GetActiveWindow() == w && GetMenu(w) == bar);
    RECT r;
    GetClientRect(w, &r);
    CHECK(r.left == 0 && r.right == 38 && r.bottom == 9); /* frame and menu bar */
    GetWindowRect(w, &r);
    CHECK(r.left == 2 && r.top == 1 && r.right == 42 && r.bottom == 13);
    POINT pt = { 0, 0 };
    ClientToScreen(w, &pt);
    CHECK(pt.x == 3 && pt.y == 3);
    CHECK(WindowFromPoint(pt) == w);
    CHECK(GetWindowTextLength(w) == 4);
    SetWindowText(w, "Changed");
    char t[16];
    CHECK(GetWindowText(w, t, sizeof t) == 7 && !strcmp(t, "Changed"));
    SetWindowLong(w, 0, 0x12345678L);
    CHECK(GetWindowLong(w, 0) == 0x12345678L);
    CHECK(GetWindowLong(w, GWL_WNDPROC) == (LONG)testProc);
    SetWindowLong(w, GWL_USERDATA, 99);
    CHECK(GetWindowLong(w, GWL_USERDATA) == 99);

    /* Controls on the window. */
    HWND btn = CreateWindow("BUTTON", "&Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 1, 7, 8, 1, w, (HMENU)50, 0, 0);
    HWND chk = CreateWindow("BUTTON", "Opt", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 12, 7, 8, 1, w, (HMENU)51, 0, 0);
    HWND ed = CreateWindow("EDIT", "one", WS_CHILD | WS_VISIBLE | ES_MULTILINE, 1, 3, 30, 3, w, (HMENU)52, 0, 0);
    HWND lb = CreateWindow("LISTBOX", "", WS_CHILD | WS_VISIBLE | LBS_NOTIFY, 32, 1, 6, 4, w, (HMENU)53, 0, 0);
    CHECK(btn && chk && ed && lb);
    CHECK(GetParent(btn) == w && GetDlgCtrlID(chk) == 51 && GetDlgItem(w, 52) == ed);
    SendMessage(chk, WM_CHAR, ' ', 0);
    CHECK(SendMessage(chk, BM_GETCHECK, 0, 0) == BST_CHECKED);
    CHECK(lastCmd == 51 && lastNotify == BN_CLICKED);

    /* The edit control. */
    SendMessage(ed, EM_SETSEL, 0, MAKELONG(3, 3));
    SendMessage(ed, EM_REPLACESEL, 0, (LPARAM)(LPSTR) "\r\ntwo\nthree");
    CHECK(GetWindowTextLength(ed) == 13);
    CHECK(SendMessage(ed, EM_GETLINECOUNT, 0, 0) == 3);
    CHECK(SendMessage(ed, EM_LINEINDEX, 2, 0) == 8);
    CHECK(SendMessage(ed, EM_LINELENGTH, 8, 0) == 5);
    CHECK(SendMessage(ed, EM_LINEFROMCHAR, 5, 0) == 1);
    CHECK(lastCmd == 52 && lastNotify == EN_CHANGE);
    CHECK(SendMessage(ed, EM_GETMODIFY, 0, 0));
    char lb2[8];
    *(WORD *)lb2 = 7;
    CHECK(SendMessage(ed, EM_GETLINE, 1, (LPARAM)(LPSTR)lb2) == 3 && !memcmp(lb2, "two", 3));
    SetFocus(ed);
    CHECK(GetFocus() == ed);
    SendMessage(ed, EM_SETSEL, 0, MAKELONG(0, 0));
    SendMessage(ed, WM_KEYDOWN, VK_DELETE, 0);
    SendMessage(ed, WM_CHAR, 'O', 0);
    SendMessage(ed, WM_KEYDOWN, VK_DOWN, 0);
    SendMessage(ed, WM_CHAR, 8, 0); /* the caret was after the t */
    GetWindowText(ed, t, sizeof t);
    CHECK(!strcmp(t, "One\nwo\nthree"));
    SendMessage(ed, EM_LIMITTEXT, 13, 0);
    SendMessage(ed, WM_CHAR, 'x', 0);
    SendMessage(ed, WM_CHAR, 'y', 0);
    GetWindowText(ed, t, sizeof t);
    CHECK(!strcmp(t, "One\nxwo\nthree"));
    CHECK(lastNotify == EN_MAXTEXT);

    /* The list box. */
    CHECK(SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)(LPSTR) "b") == 0);
    CHECK(SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)(LPSTR) "a") == 1);
    CHECK(SendMessage(lb, LB_INSERTSTRING, 0, (LPARAM)(LPSTR) "c") == 0);
    CHECK(SendMessage(lb, LB_GETCOUNT, 0, 0) == 3);
    CHECK(SendMessage(lb, LB_FINDSTRING, (WPARAM)-1, (LPARAM)(LPSTR) "A") == 2);
    CHECK(SendMessage(lb, LB_SETCURSEL, 1, 0) == 1);
    SetFocus(lb);
    SendMessage(lb, WM_KEYDOWN, VK_DOWN, 0);
    CHECK(SendMessage(lb, LB_GETCURSEL, 0, 0) == 2);
    CHECK(lastCmd == 53 && lastNotify == LBN_SELCHANGE);
    CHECK(SendMessage(lb, LB_GETTEXT, 2, (LPARAM)(LPSTR)t) == 1 && t[0] == 'a');
    CHECK(SendMessage(lb, LB_DELETESTRING, 0, 0) == 2);
    CHECK(SendMessage(lb, LB_GETCURSEL, 0, 0) == 1);

    /* Painting reaches video memory. */
    UpdateWindow(w);
    CHECK(painted >= 1);
    CHECK(screenHas(3, 3, "PAINT"));
    CHECK((cell(3, 3) >> 8) == 0x1e);
    CHECK(screenHas(6, 4, "Hot") && (cell(6, 4) >> 8) == 0x1f);
    CHECK(screenHas(5, 2, "File") && screenHas(11, 2, "Run"));   /* the menu bar */
    CHECK(screenHas(17, 1, " Changed "));                         /* the title */
    CHECK(screenHas(4, 10, "<  Go  >") && screenHas(15, 10, "[X] Opt"));
    CHECK(screenHas(4, 6, "One") && screenHas(4, 7, "xwo") && screenHas(4, 8, "three"));
    CHECK(screenHas(36, 4, "b") && screenHas(36, 5, "a"));

    /* A menu command by accelerator. */
    ACCEL acc[] = { { FVIRTKEY, VK_F5, 777 } };
    HACCEL ha = CwCreateAcceleratorTable(acc, 1);
    MSG m = { w, WM_KEYDOWN, VK_F5, 0, 0, { 0, 0 } };
    CHECK(TranslateAccelerator(w, ha, &m) && lastCmd == 777);
    m.wParam = VK_F6;
    CHECK(!TranslateAccelerator(w, ha, &m));

    /* A modal dialog driven by keys: type in the edit field, Tab to the
     * check box and check it, Tab to the radio group and go down, Tab to
     * the list and go down, then Enter for the default button. */
    static const struct key dkeys[] = {
        { WM_KEYDOWN, VK_END }, { WM_CHAR, 'd' }, { WM_KEYDOWN, VK_TAB }, { WM_CHAR, ' ' },
        { WM_KEYDOWN, VK_TAB }, { WM_KEYDOWN, VK_DOWN }, { WM_KEYDOWN, VK_TAB },
        { WM_KEYDOWN, VK_DOWN }, { WM_KEYDOWN, VK_RETURN },
    };
    type(dkeys, sizeof dkeys / sizeof dkeys[0]);
    int res = CwDialogBox(w, &dlg, dlgProc, 5);
    CHECK(res == 42);
    CHECK(initSeen == 5);
    CHECK(!strcmp(gotName, "abcd"));
    CHECK(gotCheck == 1);
    CHECK(gotRadio == 2);
    CHECK(gotList == 1);
    CHECK(GetActiveWindow() == w);

    /* A message box: Tab to No and press Space. */
    static const struct key mkeys[] = { { WM_KEYDOWN, VK_TAB }, { WM_CHAR, ' ' } };
    type(mkeys, 2);
    int mb = MessageBox(w, "Proceed?", "Ask", MB_YESNO);
    CHECK(mb == IDNO);

    /* Timers and the end of the message loop. */
    CHECK(SetTimer(w, 9, 60, 0) == 9);
    MSG q;
    int got = 0;
    while (GetMessage(&q, 0, 0, 0)) {
        TranslateMessage(&q);
        DispatchMessage(&q);
        if (++got > 2000)
            break;
    }
    CHECK(q.message == WM_QUIT && q.wParam == 7 && timers == 1);

    DestroyWindow(w);
    CHECK(destroyed == 1 && !IsWindow(w) && !IsWindow(btn));
    CHECK(!DestroyMenu(bar)); /* destroyed with its window */
    CwShutdown();
    for (int i = 0; i < nfail; i++)
        printf("FAIL cwin.c:%d\n", line[i]);
    printf(fails ? "%d FAILED\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
