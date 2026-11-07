/* Runtime checks for the CXL/TCXL compatible library: windows, menus,
 * forms and pick lists, driven by keys stuffed with kbput(). The screen is
 * read back from video memory.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdio.h>
#include <string.h>
#include <i86.h>
#include "tcxl.h"

static int fails;
static char log_[2048];
static int logn;

static void check(int ok, const char *what)
{
    if (!ok) {
        fails++;
        logn += snprintf(log_ + logn, sizeof log_ - logn, "FAIL %s\n", what);
    }
}

static unsigned cell(int row, int col)
{
    unsigned __far *v = MK_FP(_vinfo.videoseg, 0);
    return v[row * _vinfo.numcols + col];
}

/* Compares the characters on the screen at (row, col) with s. */
static int screen_has(int row, int col, const char *s)
{
    while (*s)
        if ((cell(row, col++) & 0xff) != (unsigned char)*s++)
            return 0;
    return 1;
}

/* Whether s appears on the screen row between columns c1 and c2. */
static int row_has(int row, int c1, int c2, const char *s)
{
    int c;

    for (c = c1; c + (int)strlen(s) - 1 <= c2; c++)
        if (screen_has(row, c, s))
            return 1;
    return 0;
}

static void test_strings(void)
{
    char a[40];

    strcpy(a, "  hello  ");
    check(!strcmp(strbtrim(a), "hello"), "strbtrim");
    strcpy(a, "abcdef");
    check(!strcmp(strdel("cd", a), "abef"), "strdel");
    strcpy(a, "hello world");
    check(strocc(a, 'o') == 2, "strocc");
    check(!strcmp(struplow(a), "Hello World"), "struplow");
    strcpy(a, "abc");
    check(!strcmp(strins("XY", a, 1), "aXYbc"), "strins");
    strcpy(a, "aXbXc");
    check(strchg(a, 'X', '-') == 2 && !strcmp(a, "a-b-c"), "strchg");
    cvtic(a, 42, 5);
    a[5] = 0;
    check(cvtci(a) == 42, "cvtic/cvtci");
}

static void test_windows(void)
{
    WINDOW w1, w2;
    unsigned under = cell(5, 10);

    w1 = wopen(4, 8, 10, 40, 0, 0x1f, 0x1e);
    check(w1 > 0, "wopen");
    check((cell(4, 8) & 0xff) == 0xda, "single border corner");
    wprints(0, 0, 0x1e, "First");
    check(screen_has(5, 9, "First"), "wprints");
    wtitle("Title", TCENTER, 0x1f);
    check(row_has(4, 9, 39, "Title"), "wtitle");
    w2 = wopen(6, 20, 12, 50, 1, 0x2f, 0x2e);
    check(w2 > 0 && (cell(6, 20) & 0xff) == 0xc9, "double border");
    wputs("Second\n");
    check(screen_has(7, 21, "Second"), "wputs");
    /* Writing to the covered window must not disturb the top one. */
    wwprints(w1, 2, 0, 0x1e, "Covered text that runs under");
    check(screen_has(7, 21, "Second"), "wwprints keeps top window");
    check(screen_has(7, 9, "Covered tex"), "wwprints visible part");
    wactiv(w1);
    check(screen_has(7, 9, "Covered text that runs under"), "wactiv raises");
    wclose();
    check(_winfo.active && _winfo.active->whandle == w2, "wclose");
    check(screen_has(7, 21, "Second"), "lower window intact");
    wmove(14, 30);
    check(screen_has(15, 31, "Second"), "wmove");
    wclose();
    check(cell(5, 10) == under, "screen restored");
    check(!_winfo.active, "no window left");
}

static int saved;
static void do_save(void) { saved++; }

static void test_menu(void)
{
    int t;

    wopen(0, 0, 0, 79, 5, 0x70, 0x70);
    wmenubeg(0, 0, 0, 79, 5, 0x70, 0x70, NULL);
    wmenuitem(0, 1, "File", 'F', 1, M_HASPD, NULL, 0, 0);
    wmenubeg(1, 0, 4, 10, 0, 0x70, 0x70, NULL);
    wmenuitem(0, 0, "Open", 'O', 11, 0, NULL, 0, 0);
    wmenuitem(1, 0, "Save", 'S', 12, M_CLOSE, do_save, 0, 0);
    wmenuend(11, M_PD | M_VERT, 9, 0, 0x70, 0x74, 0x78, 0x07);
    wmenuitem(0, 7, "Edit", 'E', 2, 0, NULL, 0, 0);
    wmenuend(1, M_HORZ, 0, 0, 0x70, 0x74, 0x78, 0x07);
    /* Enter opens File, Down moves to Save, Enter picks it. */
    kbput(0x1c0d);
    kbput(0x5000);
    kbput(0x1c0d);
    t = wmenuget();
    check(t == 1 && saved == 1, "pull-down menu select");
    wmenubeg(0, 0, 0, 79, 5, 0x70, 0x70, NULL);
    wmenuitem(0, 1, "One", 'O', 1, 0, NULL, 0, 0);
    wmenuitem(0, 7, "Two", 'T', 2, 0, NULL, 0, 0);
    wmenuend(1, M_HORZ, 0, 0, 0x70, 0x74, 0x78, 0x07);
    kbput(0x1474); /* 't' */
    t = wmenuget();
    check(t == 2, "quick select letter");
    wclose();
}

static void test_form(void)
{
    char name[11] = "          ";
    char zip[6] = "00000";
    int e;

    wopen(5, 5, 12, 60, 0, 0x1f, 0x1e);
    winpbeg(0x70, 0x70);
    winpdef(1, 1, name, "UUUUUUUUUU", 0, 0, NULL, 0);
    winpdef(2, 1, zip, "#####", 0, 0, NULL, 0);
    kbputs("smith\r12345"); /* filling the last field ends the form */
    e = winpread();
    check(e == 0, "winpread");
    check(!strcmp(name, "SMITH     "), "upper case field");
    check(!strcmp(zip, "12345"), "numeric field");
    wclose();
}

static void test_pick(void)
{
    static char *items[] = { "Apple", "Banana", "Cherry", "Damson", NULL };
    int i;

    kbput(0x5000);
    kbput(0x5000);
    kbput(0x1c0d);
    i = wpickstr(3, 3, 8, 12, 0, 0x1f, 0x1e, 0x70, items, 0, NULL);
    check(i == 2, "wpickstr");
    kbput(0x011b);
    i = wpickstr(3, 3, 8, 12, 0, 0x1f, 0x1e, 0x70, items, 1, NULL);
    check(i == -1, "wpickstr Esc");
}

static void test_tcxl(void)
{
    WndT w;
    int i;
    char buf[6] = "     ";

    w = WpopUp(CNT_CNT, 0, 0, 4, 19, BOX_SNG, 0x4f, 0x4e);
    check(w > 0, "WpopUp");
    check(WctlAct->srow == (_vinfo.numrows - 5) / 2 && WctlAct->scol == 30,
          "WpopUp centers");
    check(Wprts(0, 0, 0x4e, "TCXL") == ERR_NUL, "Wprts");
    check(Wgotoxy(99, 99) == ERR_ARG, "Wgotoxy error");
    EntDef(0x70, 0x70);
    EntFld(1, 0, buf, "XXXXX", FLD_UPR, 7, FDF_INI, NULL, 0);
    check(FldLoc(7) != NULL && FldLoc(8) == NULL, "FldLoc");
    kbputs("ab\r");
    i = EntShow();
    check(i == ERR_NUL && !strcmp(buf, "AB   "), "EntFld");
    Wclose();
    check(Key_F1 == 0x3b00 && Key_Esc == 0x011b, "key codes");
}

int main(void)
{
    TcxlInit();
    clrscrn();
    test_strings();
    test_windows();
    test_menu();
    check(!_kbinfo.kbuf, "menu: keys left over");
    test_form();
    check(!_kbinfo.kbuf, "form: keys left over");
    test_pick();
    check(!_kbinfo.kbuf, "pick: keys left over");
    test_tcxl();
    check(!_kbinfo.kbuf, "tcxl: keys left over");
    wcloseall();
    clrscrn();
    gotoxy_(0, 0);
    fputs(log_, stdout);
    if (!fails)
        puts("CXL PASSED");
    return fails != 0;
}
