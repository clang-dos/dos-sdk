/* A tour of the CXL compatible library: a menu bar with pull-down menus,
 * overlapping windows with shadows, a data entry form, pick lists, a
 * colour picker and context-sensitive help (F1, from CXLDEMO.HLP).
 *
 *   clang --target=i8086-pc-msdos -mcmodel=large cxldemo.c -lcxl -o cxldemo.exe
 *
 * It needs the small, medium, large or huge model: in tiny and compact it
 * is over 64 KB.
 *
 * Run "cxldemo /tour" to have it type the keys for a short tour itself.
 *
 * Copyright (c) 2026 clang-retro contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <cxldef.h>
#include <cxlkey.h>
#include <cxlmou.h>
#include <cxlstr.h>
#include <cxlvid.h>
#include <cxlwin.h>

/* Help categories, matching the *B lines of CXLDEMO.HLP. */
enum { H_MAIN = 1, H_FILE, H_WINDOWS, H_DATA, H_FORM, H_PICK, H_COLOUR };

/* Menu tags. */
enum {
    T_FILE = 1, T_WINDOWS, T_DATA, T_HELP,
    T_ABOUT = 10, T_DOSVER, T_QUIT,
    T_STACK = 20, T_MOVE, T_HIDE,
    T_FORM = 30, T_FRUIT, T_FILES, T_COLOUR,
};

static int desk_attr, bar_attr, hot_attr, sel_attr, win_attr, box_attr,
    msg_attr, shadow_attr;
static int colour;

static void setup_colours(void)
{
    colour = !_vinfo.mono;
    desk_attr = colour ? 0x17 : 0x07;
    bar_attr = colour ? 0x70 : 0x70;
    hot_attr = colour ? 0x74 : 0x7f;
    sel_attr = colour ? 0x2f : 0x0f;
    win_attr = colour ? 0x3f : 0x07;
    box_attr = colour ? 0x3e : 0x0f;
    msg_attr = colour ? 0x4f : 0x70;
    shadow_attr = colour ? 0x08 : 0x07;
}

/* The status line on the bottom row. */
static void status(const char *s)
{
    int row = _vinfo.numrows - 1;
    char buf[80];
    int n = _vinfo.numcols - 22;

    if (n > (int)sizeof buf - 1)
        n = sizeof buf - 1;
    strncpy(buf, s, n);
    buf[n] = 0;
    fill_(row, 0, row, _vinfo.numcols - 1, ' ', bar_attr);
    prints(row, 1, bar_attr, buf);
    prints(row, _vinfo.numcols - 19, hot_attr, "F1");
    prints(row, _vinfo.numcols - 16, bar_attr, "Help");
    prints(row, _vinfo.numcols - 10, hot_attr, "Esc");
    prints(row, _vinfo.numcols - 6, bar_attr, "Back");
}

static void desktop(void)
{
    fill_(1, 0, _vinfo.numrows - 2, _vinfo.numcols - 1, 0xb1, desk_attr);
    fill_(0, 0, 0, _vinfo.numcols - 1, ' ', bar_attr);
    status("Arrow keys or highlighted letters pick a command.");
}

static WINDOW popup(int srow, int scol, int erow, int ecol, int btype,
                    char *title)
{
    WINDOW w = wopen(srow, scol, erow, ecol, btype, box_attr, win_attr);
    if (!w) {
        wperror("cannot open a window");
        exit(1);
    }
    wshadow(shadow_attr);
    if (title)
        wtitle(title, TCENTER, box_attr);
    return w;
}

/* Shows a message in a small window and waits for a key. */
static void message(char *line1, char *line2)
{
    int w = strlen(line1), w2 = line2 ? (int)strlen(line2) : 0;
    int row = _vinfo.numrows / 2 - 2, col;

    if (w2 > w)
        w = w2;
    if (w < 24)
        w = 24;
    col = (_vinfo.numcols - w - 4) / 2;
    wopen(row, col, row + (line2 ? 5 : 4), col + w + 3, 1, msg_attr, msg_attr);
    wshadow(shadow_attr);
    wcenters(0, msg_attr, line1);
    if (line2)
        wcenters(1, msg_attr, line2);
    wcenters(line2 ? 3 : 2, msg_attr, "Press any key");
    hidecur();
    waitkey();
    wclose();
}

/* File menu. */
static void about(void)
{
    whelppcat(H_MAIN);
    popup(5, 14, 16, 65, 1, " About ");
    wprints(1, 2, win_attr, "CXL compatible text library for clang-dos");
    wprints(3, 2, win_attr, "Windows, menus, forms, pick lists and help,");
    wprints(4, 2, win_attr, "built for every 8086 memory model.");
    wprintf("\n\n\n\n\n\n  Library version %s, video adapter %d,\n",
            cxlver(), _vinfo.adapter);
    wprintf("  screen %d x %d.", _vinfo.numcols, _vinfo.numrows);
    hidecur();
    waitkey();
    wclose();
    whelpop();
}

static void dosver(void)
{
    char buf[40];
    sprintf(buf, "BIOS date %s", biosver());
    message("You are running the demo under DOS.", buf);
}

/* Windows menu. */
static void stack(void)
{
    static const char *names[] = { " One ", " Two ", " Three ", " Four " };
    static const int attrs[] = { 0x1f, 0x2f, 0x4f, 0x5f };
    WINDOW w[4];
    int i;

    for (i = 0; i < 4; i++) {
        w[i] = wopen(4 + i * 3, 6 + i * 12, 12 + i * 3, 36 + i * 12, i % 4,
                     colour ? attrs[i] : 0x07, colour ? attrs[i] : 0x07);
        wshadow(shadow_attr);
        wtitle((char *)names[i], TCENTER, colour ? attrs[i] : 0x0f);
        wprintf("\n Window %d of 4.\n Press a key to bring\n the next one to the top.", i + 1);
    }
    hidecur();
    for (i = 0; i < 4; i++) {
        status("Press a key to raise the next window.");
        waitkey();
        wactiv(w[i]);
        wprints(5, 1, _winfo.active->wattr, "I am on top now. ");
    }
    status("Press a key to close the windows.");
    waitkey();
    for (i = 0; i < 4; i++) {
        wactiv(w[i]);
        wclose();
    }
}

static void move(void)
{
    int c;

    popup(6, 20, 12, 52, 0, " Move me ");
    wprintf("\n Arrow keys move this window,\n + and - change its size,\n Enter or Esc finish.");
    hidecur();
    status("Arrows move the window, + and - resize it.");
    for (;;) {
        unsigned k = getxch();
        struct _wrec_t *w = _winfo.active;
        c = k & 0xff;
        if (c == '\r' || c == 0x1b)
            break;
        if (c == '+' && w->erow < _vinfo.numrows - 3 && w->ecol < _vinfo.numcols - 3)
            wsize(w->erow + 1, w->ecol + 2);
        else if (c == '-' && w->erow > w->srow + 5)
            wsize(w->erow - 1, w->ecol - 2);
        else if (k == 0x4800 && w->srow > 1)
            wdrag(D_UP);
        else if (k == 0x5000 && w->erow < _vinfo.numrows - 3)
            wdrag(D_DOWN);
        else if (k == 0x4b00 && w->scol > 0)
            wdrag(D_LEFT);
        else if (k == 0x4d00 && w->ecol < _vinfo.numcols - 3)
            wdrag(D_RIGHT);
    }
    wclose();
}

static void hide(void)
{
    WINDOW a, b;

    a = popup(5, 8, 13, 40, 1, " Hidden ");
    wprintf("\n I was hidden and\n have come back.");
    b = popup(8, 30, 15, 70, 0, " Front ");
    wprintf("\n Press a key to hide\n the other window.");
    hidecur();
    waitkey();
    wactiv(a);
    whide();
    wclear();
    wprintf("\n Press a key to\n bring it back.");
    wactiv(b);
    wclear();
    wprintf("\n The other window is hidden.\n Press a key to bring it back.");
    waitkey();
    wunhide(a);
    waitkey();
    wclose();
    wactiv(b);
    wclose();
}

/* Data menu. */
static int check_zip(char *s)
{
    int i;
    for (i = 0; i < 5; i++)
        if (s[i] == ' ')
            return i + 1;
    return 0;
}

static void form(void)
{
    char name[26], street[26], city[16], state[3], zip[6], phone[11], vip[2];
    int r;

    memset(name, ' ', 25); name[25] = 0;
    memset(street, ' ', 25); street[25] = 0;
    memset(city, ' ', 15); city[15] = 0;
    strcpy(state, "  ");
    strcpy(zip, "     ");
    strcpy(phone, "          ");
    strcpy(vip, "N");
    popup(4, 12, 17, 67, 0, " Address card ");
    wprints(1, 2, win_attr, "Name");
    wprints(2, 2, win_attr, "Street");
    wprints(3, 2, win_attr, "City");
    wprints(4, 2, win_attr, "State");
    wprints(4, 22, win_attr, "Zip");
    wprints(5, 2, win_attr, "Phone");
    wprints(6, 2, win_attr, "Regular customer (Y/N)");
    wprints(8, 2, win_attr, "Enter moves to the next field; filling");
    wprints(9, 2, win_attr, "the last one, or Ctrl-Enter, ends the form.");
    winpbeg(colour ? 0x1f : 0x70, colour ? 0x1e : 0x70);
    winpdef(1, 10, name, "MMMMMMMMMMMMMMMMMMMMMMMMM", 0, 0, NULL, H_FORM);
    winpdef(2, 10, street, "*************************", 0, 0, NULL, H_FORM);
    winpdef(3, 10, city, "MMMMMMMMMMMMMMM", 0, 0, NULL, H_FORM);
    winpdef(4, 10, state, "UU", 0, 0, NULL, H_FORM);
    winpdef(4, 26, zip, "#####", 0, 0, check_zip, H_FORM);
    winpdef(5, 10, phone, "'('###') '###'-'####", 0, 0, NULL, H_FORM);
    winpdef(6, 26, vip, "Y", 'U', 0, NULL, H_FORM);
    status("Type the address. Esc cancels.");
    r = winpread();
    wclose();
    if (r == 0) {
        char line[60];
        strtrim(name);
        strtrim(city);
        sprintf(line, "%s, %s %s %s", *name ? name : "(no name)", city, state, zip);
        message("Saved this card:", line);
    }
}

static void fruit(void)
{
    static char *fruits[] = {
        "Apple", "Apricot", "Banana", "Blackberry", "Cherry", "Damson",
        "Fig", "Gooseberry", "Grape", "Kiwi", "Lemon", "Lime", "Mango",
        "Melon", "Orange", "Peach", "Pear", "Plum", "Quince", "Raspberry",
        "Strawberry", NULL,
    };
    static int last;
    int i;
    char buf[40];

    whelppcat(H_PICK);
    status("Pick a fruit with the arrow keys or its first letter.");
    i = wpickstr(5, 50, 15, 70, 0, box_attr, win_attr, sel_attr, fruits, last, NULL);
    whelpop();
    if (i >= 0) {
        last = i;
        sprintf(buf, "You picked: %s", fruits[i]);
        message(buf, NULL);
    }
}

static void files(void)
{
    char *f;

    whelppcat(H_PICK);
    status("Pick a file. Directories end with a backslash.");
    f = wpickfile(4, 10, 18, 60, 0, box_attr, win_attr, sel_attr, 1, "*.*", NULL);
    whelpop();
    if (f)
        message("You picked the file:", f);
}

static void colours(void)
{
    int a;
    char buf[30];

    whelppcat(H_COLOUR);
    status("Arrows change the colours, Enter picks, Esc cancels.");
    a = wpickattr(5, 24, 0, win_attr, 0, NULL);
    whelpop();
    if (a >= 0) {
        win_attr = a;
        sprintf(buf, "Windows now use attribute %02X.", a);
        message(buf, NULL);
    }
}

static void status_item(void)
{
    static const struct { int tag; const char *text; } tips[] = {
        { T_FILE, "About the demo and leaving it." },
        { T_WINDOWS, "Overlapping, moving and hiding windows." },
        { T_DATA, "Data entry, pick lists and colours." },
        { T_HELP, "How to use the demo." },
        { T_ABOUT, "Shows the library version." },
        { T_DOSVER, "Reads the BIOS date." },
        { T_QUIT, "Leaves the demo." },
        { T_STACK, "Opens four windows and raises each in turn." },
        { T_MOVE, "Drags and resizes a window." },
        { T_HIDE, "Hides a window and brings it back." },
        { T_FORM, "Fills in an address card." },
        { T_FRUIT, "Picks a string from a list." },
        { T_FILES, "Picks a file from the current directory." },
        { T_COLOUR, "Picks a text colour." },
    };
    int tag = wmenuicurr()->tagid;
    unsigned i;
    for (i = 0; i < sizeof tips / sizeof tips[0]; i++)
        if (tips[i].tag == tag)
            status(tips[i].text);
}

static void help_now(void)
{
    whelpget();
}

static void item(int row, int col, char *s, int sc, int tag, int fmask,
                 void (*fn)(void), unsigned hot, int help)
{
    wmenuitem(row, col, s, sc, tag, fmask, fn, hot, help);
    wmenuiba(status_item, NULL);
}

static void build_menu(void)
{
    wmenubeg(0, 0, 0, _vinfo.numcols - 1, 5, bar_attr, bar_attr, NULL);
    item(0, 1, " File ", 'F', T_FILE, M_HASPD, NULL, 0, H_FILE);
    wmenubeg(1, 1, 5, 18, 0, bar_attr, bar_attr, NULL);
    item(0, 0, " About...", 'A', T_ABOUT, 0, about, 0, H_FILE);
    item(1, 0, " BIOS date", 'B', T_DOSVER, 0, dosver, 0, H_FILE);
    item(2, 0, " Exit   Alt-X", 'x', M_EXITALL, 0, NULL, 0x2d00, H_FILE);
    wmenuend(T_ABOUT, M_PD | M_VERT | M_SAVE, 16, 0, bar_attr, hot_attr, 0x78, sel_attr);
    item(0, 8, " Windows ", 'W', T_WINDOWS, M_HASPD, NULL, 0, H_WINDOWS);
    wmenubeg(1, 8, 5, 26, 0, bar_attr, bar_attr, NULL);
    item(0, 0, " Overlap", 'O', T_STACK, 0, stack, 0, H_WINDOWS);
    item(1, 0, " Move and size", 'M', T_MOVE, 0, move, 0, H_WINDOWS);
    item(2, 0, " Hide and show", 'H', T_HIDE, 0, hide, 0, H_WINDOWS);
    wmenuend(T_STACK, M_PD | M_VERT | M_SAVE, 17, 0, bar_attr, hot_attr, 0x78, sel_attr);
    item(0, 18, " Data ", 'D', T_DATA, M_HASPD, NULL, 0, H_DATA);
    wmenubeg(1, 18, 6, 37, 0, bar_attr, bar_attr, NULL);
    item(0, 0, " Address form", 'A', T_FORM, 0, form, 0, H_DATA);
    item(1, 0, " Pick a fruit", 'P', T_FRUIT, 0, fruit, 0, H_DATA);
    item(2, 0, " Pick a file", 'f', T_FILES, 0, files, 0, H_DATA);
    item(3, 0, " Colours", 'C', T_COLOUR, 0, colours, 0, H_DATA);
    wmenuend(T_FORM, M_PD | M_VERT | M_SAVE, 18, 0, bar_attr, hot_attr, 0x78, sel_attr);
    item(0, 25, " Help ", 'H', T_HELP, 0, help_now, 0, H_MAIN);
    wmenuend(T_FILE, M_HORZ | M_SAVE, 0, 0, bar_attr, hot_attr, 0x78, sel_attr);
}

static int confirm_quit(void)
{
    int r;
    wopen(10, 26, 14, 53, 1, msg_attr, msg_attr);
    wshadow(shadow_attr);
    wprints(1, 2, msg_attr, "Leave the demo? (Y/N) ");
    showcur();
    r = wgetyn('Y');
    wclose();
    return r == 'Y';
}

static void tour(void)
{
    /* Open the Data menu, then the address form, and type a card. */
    kbputs("d");
    kbput(0x1c0d);
    kbputs("ada lovelace\r12 St James's Square\rlondon\rgb");
}

int main(int argc, char **argv)
{
    int t;
    char helpfile[80], *p;

    videoinit();
    setup_colours();
    if (msinit())
        mssupport(MS_FULL);
    strncpy(helpfile, argv[0], sizeof helpfile - 13);
    helpfile[sizeof helpfile - 13] = 0;
    p = strrchr(helpfile, '\\');
    strcpy(p ? p + 1 : helpfile, "CXLDEMO.HLP");
    whelpdef(helpfile, 0x3b00, colour ? 0x1f : 0x70, colour ? 0x1e : 0x70,
             colour ? 0x3f : 0x0f, colour ? 0x2f : 0x70, NULL);
    hidecur();
    desktop();
    if (argc > 1 && !strcasecmp(argv[1], "/tour"))
        tour();
    for (;;) {
        build_menu();
        t = wmenuget();
        if ((t == M_EXITALL || _winfo.errno == W_ESCPRESS) && confirm_quit())
            break;
        hidecur();
    }
    wcloseall();
    if (_mouse)
        mshidecur();
    fill_(0, 0, _vinfo.numrows - 1, _vinfo.numcols - 1, ' ', 0x07);
    gotoxy_(0, 0);
    showcur();
    puts("Thanks for trying the CXL demo.");
    return 0;
}
