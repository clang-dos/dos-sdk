/* A small text editor with <cwin.h>: a menu bar, a document window with
 * a scrolling edit control, Open, Save As, Find and Options dialogs,
 * message boxes, accelerators, a status line with menu help and a clock.
 * Works with the keyboard alone (Alt or F10 for the menus, Tab between
 * the controls of a dialog, Enter and Esc) and with a mouse.
 *
 * Build: clang --target=i8086-pc-msdos -Os cwdemo.c -lcwin -o cwdemo.exe
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <cwin.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    IDM_NEW = 100, IDM_OPEN, IDM_SAVE, IDM_SAVEAS, IDM_EXIT,
    IDM_CUT, IDM_COPY, IDM_PASTE, IDM_CLEAR, IDM_SELALL,
    IDM_FIND, IDM_FINDNEXT,
    IDM_OPTIONS, IDM_STATUS,
    IDM_ABOUT, IDM_KEYS,
    IDC_NAME = 200, IDC_FILES, IDC_CASE, IDC_BLUE, IDC_BLACK, IDC_CYAN, IDC_SCROLL, IDC_TABS,
};

static HWND mainWnd, docWnd, editWnd;
static HMENU menu;
static char fileName[80] = "UNTITLED.TXT";
static char findText[40];
static int matchCase, colors, scrollBars = 1, statusOn = 1;
static char *clip;

static const char *const helpText[] = {
    /* IDM_NEW... */ "Starts a new document", "Opens a file", "Saves the document",
    "Saves the document under a new name", "Leaves the editor",
    "Deletes the selection and keeps it", "Keeps a copy of the selection",
    "Puts the kept text in", "Deletes the selection", "Selects all the text",
    "Finds text", "Finds the same text again",
    "Changes the colors and the scroll bar", "Shows or hides the status line",
    "About this program", "Lists the keys",
};

static void setTitle(void);

/* ---- Files. ---- */

static void
loadFile(const char *name)
{
    FILE *f = fopen(name, "rb");
    if (!f) {
        char msg[120];
        snprintf(msg, sizeof msg, "Cannot open %s.", name);
        MessageBox(mainWnd, msg, "Open", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    char *buf = (char *)malloc(30001);
    size_t n = buf ? fread(buf, 1, 30000, f) : 0;
    fclose(f);
    if (!buf)
        return;
    buf[n] = 0;
    SetWindowText(editWnd, buf);
    free(buf);
    strncpy(fileName, name, sizeof fileName - 1);
    setTitle();
}

static int
saveFile(void)
{
    int n = GetWindowTextLength(editWnd);
    char *buf = (char *)malloc(n + 1);
    if (!buf)
        return 0;
    GetWindowText(editWnd, buf, n + 1);
    FILE *f = fopen(fileName, "wb");
    int ok = 0;
    if (f) {
        /* DOS text files end their lines in CR LF. */
        ok = 1;
        for (int i = 0; i < n && ok; i++)
            ok = (buf[i] == '\n' ? fputs("\r\n", f) : fputc(buf[i], f)) >= 0;
        ok = fclose(f) == 0 && ok;
    }
    free(buf);
    if (!ok) {
        MessageBox(mainWnd, "The file could not be written.", "Save", MB_OK | MB_ICONHAND);
        return 0;
    }
    SendMessage(editWnd, EM_SETMODIFY, FALSE, 0);
    return 1;
}

/* TRUE to go on when the document may be thrown away. */
static BOOL
askSave(void)
{
    if (!SendMessage(editWnd, EM_GETMODIFY, 0, 0))
        return TRUE;
    char msg[120];
    snprintf(msg, sizeof msg, "Save the changes to %s?", fileName);
    switch (MessageBox(mainWnd, msg, "Editor", MB_YESNOCANCEL | MB_ICONQUESTION)) {
    case IDYES:
        return saveFile();
    case IDNO:
        return TRUE;
    }
    return FALSE;
}

/* ---- Dialogs. ---- */

static BOOL
openProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG: {
        SetDlgItemText(dlg, IDC_NAME, "*.TXT");
        DIR *d = opendir(".");
        struct dirent *e;
        while (d && (e = readdir(d)) != 0)
            if (e->d_name[0] != '.')
                SendDlgItemMessage(dlg, IDC_FILES, LB_ADDSTRING, 0, (LPARAM)(LPSTR)e->d_name);
        if (d)
            closedir(d);
        SendDlgItemMessage(dlg, IDC_FILES, LB_SETCURSEL, 0, 0);
        return TRUE;
    }
    case WM_COMMAND:
        if (wp == IDC_FILES && (HIWORD(lp) == LBN_SELCHANGE || HIWORD(lp) == LBN_DBLCLK)) {
            char name[80];
            int i = (int)SendDlgItemMessage(dlg, IDC_FILES, LB_GETCURSEL, 0, 0);
            if (i >= 0) {
                SendDlgItemMessage(dlg, IDC_FILES, LB_GETTEXT, i, (LPARAM)(LPSTR)name);
                SetDlgItemText(dlg, IDC_NAME, name);
            }
            if (HIWORD(lp) == LBN_DBLCLK)
                PostMessage(dlg, WM_COMMAND, IDOK, 0);
            return TRUE;
        }
        if (wp == IDOK) {
            char name[80];
            GetDlgItemText(dlg, IDC_NAME, name, sizeof name);
            if (!name[0] || strchr(name, '*') || strchr(name, '?')) {
                MessageBox(dlg, "Choose a file from the list or type its name.", "Open", MB_OK);
                return TRUE;
            }
            loadFile(name);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        if (wp == IDCANCEL) {
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
    }
    return FALSE;
}

static const CWDLGITEM openItems[] = {
    { "STATIC", "File &name:", 0, 0, 2, 1, 12, 1 },
    { "EDIT", "", IDC_NAME, WS_BORDER | WS_TABSTOP | ES_UPPERCASE, 14, 0, 26, 3 },
    { "STATIC", "&Files:", 0, 0, 2, 3, 10, 1 },
    { "LISTBOX", "", IDC_FILES, WS_BORDER | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_SORT, 2, 4, 38, 9 },
    { "BUTTON", "OK", IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP, 8, 14, 10, 1 },
    { "BUTTON", "Cancel", IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 24, 14, 10, 1 },
};
static const CWDLGTEMPLATE openDlg = {
    WS_POPUP | WS_CAPTION | DS_CENTER, 0, 0, 44, 18, "Open", 6, openItems
};

static BOOL
saveAsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemText(dlg, IDC_NAME, fileName);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK) {
            GetDlgItemText(dlg, IDC_NAME, fileName, sizeof fileName);
            EndDialog(dlg, IDOK);
        } else if (wp == IDCANCEL) {
            EndDialog(dlg, IDCANCEL);
        }
        return TRUE;
    }
    return FALSE;
}

static const CWDLGITEM saveItems[] = {
    { "STATIC", "File &name:", 0, 0, 2, 1, 12, 1 },
    { "EDIT", "", IDC_NAME, WS_BORDER | WS_TABSTOP | ES_UPPERCASE, 14, 0, 26, 3 },
    { "BUTTON", "OK", IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP, 8, 4, 10, 1 },
    { "BUTTON", "Cancel", IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 24, 4, 10, 1 },
};
static const CWDLGTEMPLATE saveDlg = {
    WS_POPUP | WS_CAPTION | DS_CENTER, 0, 0, 44, 8, "Save As", 4, saveItems
};

static BOOL
findProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemText(dlg, IDC_NAME, findText);
        CheckDlgButton(dlg, IDC_CASE, matchCase);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK) {
            GetDlgItemText(dlg, IDC_NAME, findText, sizeof findText);
            matchCase = IsDlgButtonChecked(dlg, IDC_CASE);
            EndDialog(dlg, IDOK);
        } else if (wp == IDCANCEL) {
            EndDialog(dlg, IDCANCEL);
        }
        return TRUE;
    }
    return FALSE;
}

static const CWDLGITEM findItems[] = {
    { "STATIC", "Find &What:", 0, 0, 2, 1, 12, 1 },
    { "EDIT", "", IDC_NAME, WS_BORDER | WS_TABSTOP, 14, 0, 30, 3 },
    { "BUTTON", "Match &Upper/Lowercase", IDC_CASE, BS_AUTOCHECKBOX | WS_TABSTOP, 2, 4, 30, 1 },
    { "BUTTON", "OK", IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP, 9, 6, 10, 1 },
    { "BUTTON", "Cancel", IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 27, 6, 10, 1 },
};
static const CWDLGTEMPLATE findDlg = {
    WS_POPUP | WS_CAPTION | DS_CENTER, 0, 0, 48, 10, "Find", 5, findItems
};

static BOOL
optionsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        CheckRadioButton(dlg, IDC_BLUE, IDC_CYAN, IDC_BLUE + colors);
        CheckDlgButton(dlg, IDC_SCROLL, scrollBars);
        SetDlgItemInt(dlg, IDC_TABS, 8, FALSE);
        return TRUE;
    case WM_COMMAND:
        if (wp == IDOK) {
            BOOL ok;
            UINT tabs = GetDlgItemInt(dlg, IDC_TABS, &ok, FALSE);
            if (!ok || tabs < 1 || tabs > 16) {
                MessageBox(dlg, "The tab size is a number from 1 to 16.", "Options", MB_OK | MB_ICONEXCLAMATION);
                SetFocus(GetDlgItem(dlg, IDC_TABS));
                return TRUE;
            }
            for (int i = 0; i < 3; i++)
                if (IsDlgButtonChecked(dlg, IDC_BLUE + i))
                    colors = i;
            scrollBars = IsDlgButtonChecked(dlg, IDC_SCROLL);
            EndDialog(dlg, IDOK);
        } else if (wp == IDCANCEL) {
            EndDialog(dlg, IDCANCEL);
        }
        return TRUE;
    }
    return FALSE;
}

static const CWDLGITEM optionItems[] = {
    { "BUTTON", "Colors", 0, BS_GROUPBOX, 2, 0, 20, 5 },
    { "BUTTON", "&Blue", IDC_BLUE, BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP, 4, 1, 14, 1 },
    { "BUTTON", "B&lack", IDC_BLACK, BS_AUTORADIOBUTTON, 4, 2, 14, 1 },
    { "BUTTON", "&Cyan", IDC_CYAN, BS_AUTORADIOBUTTON, 4, 3, 14, 1 },
    { "BUTTON", "&Scroll Bars", IDC_SCROLL, BS_AUTOCHECKBOX | WS_TABSTOP | WS_GROUP, 25, 1, 18, 1 },
    { "STATIC", "&Tab Stops:", 0, WS_GROUP, 25, 3, 11, 1 },
    { "EDIT", "", IDC_TABS, WS_BORDER | WS_TABSTOP, 36, 2, 6, 3 },
    { "BUTTON", "OK", IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP, 8, 6, 10, 1 },
    { "BUTTON", "Cancel", IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 26, 6, 10, 1 },
};
static const CWDLGTEMPLATE optionsDlg = {
    WS_POPUP | WS_CAPTION | DS_CENTER, 0, 0, 46, 10, "Options", 9, optionItems
};

/* ---- Editing. ---- */

static void
findNext(void)
{
    if (!findText[0]) {
        PostMessage(mainWnd, WM_COMMAND, IDM_FIND, 0);
        return;
    }
    int n = GetWindowTextLength(editWnd), k = (int)strlen(findText);
    char *buf = (char *)malloc(n + 1);
    if (!buf)
        return;
    GetWindowText(editWnd, buf, n + 1);
    int from = HIWORD(SendMessage(editWnd, EM_GETSEL, 0, 0));
    for (int pass = 0; pass < 2; pass++) {
        for (int i = pass ? 0 : from; i + k <= n; i++) {
            int j = 0;
            while (j < k && (matchCase ? buf[i + j] == findText[j]
                                       : (buf[i + j] | 32) == (findText[j] | 32)))
                j++;
            if (j == k) {
                SendMessage(editWnd, EM_SETSEL, 0, MAKELONG(i, i + k));
                free(buf);
                return;
            }
        }
    }
    free(buf);
    MessageBox(mainWnd, "Match not found.", "Find", MB_OK);
}

static void
copySel(int cut)
{
    DWORD sel = (DWORD)SendMessage(editWnd, EM_GETSEL, 0, 0);
    int a = LOWORD(sel), b = HIWORD(sel);
    if (a == b)
        return;
    int n = GetWindowTextLength(editWnd);
    char *buf = (char *)malloc(n + 1);
    if (!buf)
        return;
    GetWindowText(editWnd, buf, n + 1);
    free(clip);
    clip = (char *)malloc(b - a + 1);
    if (clip) {
        memcpy(clip, buf + a, b - a);
        clip[b - a] = 0;
    }
    free(buf);
    if (cut)
        SendMessage(editWnd, EM_REPLACESEL, 0, (LPARAM)(LPSTR) "");
    EnableMenuItem(menu, IDM_PASTE, MF_ENABLED);
}

static void
applyOptions(void)
{
    static const BYTE attrs[3] = { 0x17, 0x07, 0x30 };
    int idx[] = { COLOR_WINDOW, COLOR_INACTIVEBORDER, COLOR_ACTIVEBORDER };
    COLORREF c[] = { attrs[colors], attrs[colors], (COLORREF)(attrs[colors] | 0x0f) };
    SetSysColors(3, idx, c);
    DWORD style = (DWORD)GetWindowLong(editWnd, GWL_STYLE);
    style = scrollBars ? style | WS_VSCROLL : style & ~WS_VSCROLL;
    SetWindowLong(editWnd, GWL_STYLE, (LONG)style);
    RECT r;
    GetClientRect(docWnd, &r);
    MoveWindow(editWnd, 0, 0, r.right, r.bottom, TRUE);
}

static void
setTitle(void)
{
    SetWindowText(docWnd, fileName);
}

static void
setStatus(const char *help)
{
    static char buf[100];
    if (!statusOn) {
        CwSetStatusText(NULL);
        return;
    }
    DWORD t = GetTickCount() / 1000;
    long line = SendMessage(editWnd, EM_LINEFROMCHAR, (WPARAM)-1, 0) + 1;
    long col = HIWORD(SendMessage(editWnd, EM_GETSEL, 0, 0)) -
               SendMessage(editWnd, EM_LINEINDEX, (WPARAM)-1, 0) + 1;
    snprintf(buf, sizeof buf, "%s\t%05ld:%03ld  %02lu:%02lu:%02lu ",
             help ? help : "<F1=Help> <Alt=Menu> <F3=Find Next> <Alt+X=Exit>", line, col,
             (unsigned long)(t / 3600 % 24), (unsigned long)(t / 60 % 60), (unsigned long)(t % 60));
    CwSetStatusText(buf);
}

/* ---- Windows. ---- */

static LRESULT
docProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        editWnd = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN,
                               0, 0, 10, 10, h, (HMENU)1, 0, 0);
        SendMessage(editWnd, EM_LIMITTEXT, 30000, 0);
        return 0;
    case WM_SIZE:
        MoveWindow(editWnd, 0, 0, LOWORD(lp), HIWORD(lp), TRUE);
        return 0;
    case WM_SETFOCUS:
        SetFocus(editWnd);
        return 0;
    case WM_COMMAND:
        if (HIWORD(lp) == EN_CHANGE)
            setStatus(0);
        return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

static LRESULT
mainProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        RECT r;
        GetClientRect(h, &r);
        docWnd = CreateWindow("CwDemoDoc", fileName, WS_CHILD | WS_VISIBLE | WS_CAPTION, 0, 0,
                              r.right, r.bottom, h, (HMENU)1, 0, 0);
        SetTimer(h, 1, 1000, NULL);
        return 0;
    }
    case WM_SIZE:
        if (docWnd)
            MoveWindow(docWnd, 0, 0, LOWORD(lp), HIWORD(lp), TRUE);
        return 0;
    case WM_SETFOCUS:
        SetFocus(editWnd);
        return 0;
    case WM_TIMER:
        setStatus(0);
        return 0;
    case WM_MENUSELECT:
        if (LOWORD(lp) == 0xffff || (LOWORD(lp) & MF_POPUP))
            setStatus(0);
        else if (wp >= IDM_NEW && wp <= IDM_KEYS)
            setStatus(helpText[wp - IDM_NEW]);
        return 0;
    case WM_INITMENUPOPUP: {
        DWORD sel = (DWORD)SendMessage(editWnd, EM_GETSEL, 0, 0);
        UINT has = LOWORD(sel) != HIWORD(sel) ? MF_ENABLED : MF_GRAYED;
        EnableMenuItem(menu, IDM_CUT, has);
        EnableMenuItem(menu, IDM_COPY, has);
        EnableMenuItem(menu, IDM_CLEAR, has);
        EnableMenuItem(menu, IDM_PASTE, clip ? MF_ENABLED : MF_GRAYED);
        return 0;
    }
    case WM_CLOSE:
        if (askSave())
            DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_COMMAND:
        switch (wp) {
        case IDM_NEW:
            if (askSave()) {
                SetWindowText(editWnd, "");
                strcpy(fileName, "UNTITLED.TXT");
                setTitle();
            }
            break;
        case IDM_OPEN:
            if (askSave())
                CwDialogBox(h, &openDlg, openProc, 0);
            break;
        case IDM_SAVE:
            if (strcmp(fileName, "UNTITLED.TXT"))
                saveFile();
            else if (CwDialogBox(h, &saveDlg, saveAsProc, 0) == IDOK && saveFile())
                setTitle();
            break;
        case IDM_SAVEAS:
            if (CwDialogBox(h, &saveDlg, saveAsProc, 0) == IDOK && saveFile())
                setTitle();
            break;
        case IDM_EXIT:
            PostMessage(h, WM_CLOSE, 0, 0);
            break;
        case IDM_CUT:
        case IDM_COPY:
            copySel(wp == IDM_CUT);
            break;
        case IDM_PASTE:
            if (clip)
                SendMessage(editWnd, EM_REPLACESEL, 0, (LPARAM)(LPSTR)clip);
            break;
        case IDM_CLEAR:
            SendMessage(editWnd, EM_REPLACESEL, 0, (LPARAM)(LPSTR) "");
            break;
        case IDM_SELALL:
            SendMessage(editWnd, EM_SETSEL, 0, MAKELONG(0, -1));
            break;
        case IDM_FIND:
            if (CwDialogBox(h, &findDlg, findProc, 0) == IDOK)
                findNext();
            break;
        case IDM_FINDNEXT:
            findNext();
            break;
        case IDM_OPTIONS:
            if (CwDialogBox(h, &optionsDlg, optionsProc, 0) == IDOK)
                applyOptions();
            break;
        case IDM_STATUS:
            statusOn = !statusOn;
            CheckMenuItem(menu, IDM_STATUS, statusOn ? MF_CHECKED : MF_UNCHECKED);
            setStatus(0);
            break;
        case IDM_ABOUT:
            MessageBox(h, "Character Windows Demo\n\n"
                          "A text editor written with <cwin.h> and built with clang for 16-bit DOS.",
                       "About", MB_OK);
            break;
        case IDM_KEYS:
            MessageBox(h, "Alt or F10       the menu bar\n"
                          "Alt+letter       a menu\n"
                          "Tab, Shift+Tab   between controls\n"
                          "Shift+arrows     select text\n"
                          "Shift+Del        cut\n"
                          "Ctrl+Ins         copy\n"
                          "Shift+Ins        paste\n"
                          "F3               find next\n"
                          "Alt+X            exit",
                       "Keys", MB_OK);
            break;
        }
        SetFocus(editWnd);
        return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

static HMENU
makeMenu(void)
{
    HMENU bar = CreateMenu(), m;
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, IDM_NEW, "&New");
    AppendMenu(m, MF_STRING, IDM_OPEN, "&Open...");
    AppendMenu(m, MF_STRING, IDM_SAVE, "&Save");
    AppendMenu(m, MF_STRING, IDM_SAVEAS, "Save &As...");
    AppendMenu(m, MF_SEPARATOR, 0, 0);
    AppendMenu(m, MF_STRING, IDM_EXIT, "E&xit\tAlt+X");
    AppendMenu(bar, MF_POPUP, m, "&File");
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, IDM_CUT, "Cu&t\tShift+Del");
    AppendMenu(m, MF_STRING, IDM_COPY, "&Copy\tCtrl+Ins");
    AppendMenu(m, MF_STRING | MF_GRAYED, IDM_PASTE, "&Paste\tShift+Ins");
    AppendMenu(m, MF_STRING, IDM_CLEAR, "Cl&ear\tDel");
    AppendMenu(m, MF_SEPARATOR, 0, 0);
    AppendMenu(m, MF_STRING, IDM_SELALL, "Select &All");
    AppendMenu(bar, MF_POPUP, m, "&Edit");
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, IDM_FIND, "&Find...");
    AppendMenu(m, MF_STRING, IDM_FINDNEXT, "Find &Next\tF3");
    AppendMenu(bar, MF_POPUP, m, "&Search");
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, IDM_OPTIONS, "&Display...");
    AppendMenu(m, MF_STRING | MF_CHECKED, IDM_STATUS, "&Status Line");
    AppendMenu(bar, MF_POPUP, m, "&Options");
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, IDM_KEYS, "&Keys...");
    AppendMenu(m, MF_SEPARATOR, 0, 0);
    AppendMenu(m, MF_STRING, IDM_ABOUT, "&About...");
    AppendMenu(bar, MF_POPUP | MF_HELP, m, "&Help");
    return bar;
}

int PASCAL
WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int cmdShow)
{
    (void)prev;
    (void)cmdShow;
    WNDCLASS wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = mainProc;
    wc.hInstance = inst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "CwDemoMain";
    RegisterClass(&wc);
    wc.lpfnWndProc = docProc;
    wc.lpszClassName = "CwDemoDoc";
    RegisterClass(&wc);

    static const ACCEL keys[] = {
        { FVIRTKEY | FSHIFT, VK_DELETE, IDM_CUT },
        { FVIRTKEY | FCONTROL, VK_INSERT, IDM_COPY },
        { FVIRTKEY | FSHIFT, VK_INSERT, IDM_PASTE },
        { FVIRTKEY, VK_F3, IDM_FINDNEXT },
        { FVIRTKEY, VK_F1, IDM_KEYS },
        { FVIRTKEY | FALT, 'X', IDM_EXIT },
    };
    HACCEL acc = CwCreateAcceleratorTable(keys, sizeof keys / sizeof keys[0]);

    CwSetStatusText("");
    menu = makeMenu();
    mainWnd = CreateWindow("CwDemoMain", "Editor", WS_POPUP | WS_MAXIMIZE | WS_VISIBLE, 0, 0,
                           CW_USEDEFAULT, CW_USEDEFAULT, 0, menu, inst, 0);
    if (cmdLine && cmdLine[0])
        loadFile(cmdLine);
    else
        SetWindowText(editWnd, "Welcome to the Character Windows demo.\n\n"
                               "Press Alt or F10 for the menus, or click them.\n"
                               "Type here; Shift and the arrows select text.\n");
    SendMessage(editWnd, EM_SETMODIFY, FALSE, 0);
    SetFocus(editWnd);
    setStatus(0);

    MSG m;
    while (GetMessage(&m, 0, 0, 0)) {
        if (TranslateAccelerator(mainWnd, acc, &m))
            continue;
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return (int)m.wParam;
}
