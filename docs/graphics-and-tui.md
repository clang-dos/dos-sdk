# Graphics, console and windows for DOS

The sysroot has these libraries in the style of the DOS compilers and
applications of the late 1980s and early 1990s, for programs that draw on
the whole screen:

| Header | Library | What it is |
| --- | --- | --- |
| `<conio.h>` | in `libc.a` | Text-mode console: windows, colors, cursor, direct keyboard input (Turbo C and Microsoft C names) |
| `<graphics.h>` | `-lgraph` | The Borland Graphics Interface (BGI) functions of Turbo C and Borland C++ |
| `<fastgraf.h>` | `-lgraph` | The `fg_` functions of the Fastgraph library |
| `<cwin.h>` | `-lcwin` | Character windows in C: the Windows 3.x programming interface in text mode, looking like the character-mode Microsoft applications |
| `<tui.h>` | `-ltui` | A C++ text-mode window toolkit: menus, windows, dialogs, mouse |

All of it is new code under the BSD-3-Clause license, written against the
documented behavior of those libraries. All but `<cwin.h>` work in every
memory model; `<cwin.h>` needs small, medium, large or huge (see below).
`<dos.h>` and `<i86.h>` also have `int86`, `intdos`, `union REGS`,
`delay`, `sound`, `inportb` and the other helpers that such programs use.

```sh
clang --target=i8086-pc-msdos -Os demo.c -lgraph -o demo.exe
clang --target=i8086-pc-msdos -mcmodel=medium -Os app.c -lcwin -o app.exe
clang++ --target=i8086-pc-msdos -Os app.cpp -ltui -o app.exe
```

`examples/dos/` has a demo of each: `bgidemo.c`, `fgdemo.c`, `cwdemo.c`
and `tuidemo.cpp`. [tests/dos/README.md](../tests/dos/README.md) shows how to
run them in DOSBox.

`scripts/build-doslibs.sh` builds `libgraph.a`, `libcwin.a` and `libtui.a` for each
model into an existing sysroot (`build-sysroot.sh` runs it last;
`NO_CXX=1` leaves out `libtui.a`).

## conio.h

The console functions write straight to text-mode video memory and keep
the BIOS cursor in step:

- Windows and the cursor: `window`, `clrscr`, `clreol`, `insline`,
  `delline`, `gotoxy`, `wherex`, `wherey`, `_setcursortype`. Coordinates
  start at 1 and are inside the current window.
- Colors: `textcolor`, `textbackground`, `textattr`, `highvideo`,
  `lowvideo`, `normvideo`, with the `COLORS` names (`BLACK` to `WHITE`)
  and `BLINK`.
- Output: `putch`, `cputs`, `cprintf`, `vcprintf`. Text wraps at the
  window's edge and scrolls the window (`_wscroll = 0` stops scrolling).
- Input: `getch`, `getche`, `kbhit`, `ungetch`, `cgets`, `cscanf`,
  `getpass`. Extended keys arrive as 0 followed by the scan code.
- Screen contents: `gettext`, `puttext`, `movetext`, and `textmode` and
  `gettextinfo` (`C80`, `BW80`, `C40`, `MONO`, `C4350` for 43 or 50
  lines).
- Ports: `inp`, `inpw`, `outp`, `outpw`.

## graphics.h

The BGI drivers are built in, so `initgraph`'s path is ignored and there
are no `.BGI` or `.CHR` files. `DETECT` picks the best of these:

| Driver | Modes |
| --- | --- |
| `CGA` | `CGAC0`-`CGAC3` 320x200, 4 colors; `CGAHI` 640x200, 2 colors |
| `MCGA` | as CGA, and `MCGAMED` 640x200, `MCGAHI` 640x480, 2 colors |
| `EGA` | `EGALO` 640x200, 4 pages; `EGAHI` 640x350, 2 pages; 16 colors |
| `EGA64` | `EGA64LO` 640x200, 16 colors; `EGA64HI` 640x350, 4 colors |
| `HERCMONO` | `HERCMONOHI` 720x348, 2 colors, 2 pages |
| `VGA` | `VGALO` 640x200, `VGAMED` 640x350, `VGAHI` 640x480; 16 colors |
| `VGA256` (11) | `VGA256LO` 320x200, 256 colors |
| `SVGA256` (12) | VESA 640x400, 640x480, 800x600 and 1024x768, 256 colors |

`VGA256` and `SVGA256` are additions (Borland had them only as
third-party drivers); `DETECT` does not choose them, so ask for them by
number. `SVGA256` needs a VESA BIOS.

Everything else is there: lines with styles and thickness, rectangles,
`bar` and `bar3d`, polygons, circles, arcs, ellipses, pie slices and
sectors, the twelve fill patterns and `setfillpattern`, `floodfill`,
viewports and clipping, write modes (`COPY_PUT`, `XOR_PUT`), pages
(`setactivepage`, `setvisualpage`), palettes (`setpalette`,
`setallpalette`, `setrgbpalette`), `getimage`, `putimage` with all five
operations, `getarccoords`, `getaspectratio`, and the text functions with
justification, direction and `setusercharsize`.

Text is drawn with the 8x8 font of the video BIOS, magnified by the
character size, for every font that `settextstyle` names: the stroked
fonts (`TRIPLEX_FONT` and the others) are not drawn as strokes.
`registerbgidriver` and `registerbgifont` exist so old programs link, and
do nothing.

```c
#include <graphics.h>
#include <conio.h>

int main(void)
{
    int gd = DETECT, gm;
    initgraph(&gd, &gm, "");
    if (graphresult() != grOk)
        return 1;
    setfillstyle(SOLID_FILL, LIGHTBLUE);
    fillellipse(getmaxx() / 2, getmaxy() / 2, 100, 60);
    outtextxy(10, 10, "Press a key");
    getch();
    closegraph();
    return 0;
}
```

## fastgraf.h

About 110 of the `fg_` functions, on the same drivers, with Fastgraph's
mode numbers:

| Mode | Screen |
| --- | --- |
| 0-3, 7 | text |
| 4, 5 | 320x200, 4 colors (CGA) |
| 6 | 640x200, 2 colors (CGA) |
| 11 | 720x348, 2 colors (Hercules) |
| 13, 14, 16 | 320x200, 640x200 and 640x350, 16 colors (EGA) |
| 17, 18 | 640x480, 2 and 16 colors (VGA) |
| 19 | 320x200, 256 colors (VGA) |
| 24-27 | 640x400, 640x480, 800x600, 1024x768, 256 colors (VESA) |

There are modes, colors and the DAC (`fg_setrgb`, `fg_setdacs`), pages
and `fg_transfer`, clipping, points, lines, dashes, boxes, circles,
ellipses, polygons, `fg_paint` and `fg_flood`, text with `fg_locate` and
`fg_print`, images and bitmaps, the keyboard, the mouse, the clock and
the speaker. `fg_bestmode`, `fg_testmode` and `fg_automode` look at the
hardware.

Not here: the unchained 256-color modes (20-23), the 16-color SVGA modes
(28, 29), virtual buffers, sprites, image files (PCX, GIF) and the sound
and music functions beyond `fg_sound`, `fg_quiet` and `fg_beep`. Images
for `fg_getimage`, `fg_putimage` and `fg_drwimage` always hold one byte
per pixel, in every mode, rather than Fastgraph's packed formats.

## cwin.h

`<cwin.h>` is a window system for text mode in the style of the
character-mode applications Microsoft built in the late 1980s (Word 5,
Works, QuickC, the DOS Shell, EDIT): a menu bar on the top line with
pull-down menus, a status line on the bottom one, overlapping windows,
dialogs with a shadow, push buttons drawn as `< OK >`, `[X]` check boxes,
`( )` option buttons, edit fields, list boxes and scroll bars, with the
keyboard (Alt or F10 for the menus, Alt+letter, Tab, the arrows, Enter
and Esc) and the mouse.

Those applications were built on an internal Microsoft library, "CW"
(Character Windows), a text-mode descendant of Windows. Microsoft never
published it: no SDK, headers or reference shipped, and it is not in the
Programmer's Library CD-ROMs of 1988 to 1991. `<cwin.h>` is not that
library and does not have its interface. It is an independent
implementation whose interface is the **public Windows 3.x API**, with the
same function names, messages, constants and parameter packing, so that
programs read like (and partly port from) Windows programs:

```c
#include <cwin.h>

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        TextOut(dc, 2, 1, "Hello, world", 12);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (wp == 1)
            MessageBox(h, "Hello from DOS.", "About", MB_OK);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, msg, wp, lp);
}

int PASCAL WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASS wc = { 0, WndProc, 0, 0, inst, 0, 0, (HBRUSH)(COLOR_WINDOW + 1), 0, "Hello" };
    RegisterClass(&wc);
    HMENU bar = CreateMenu(), file = CreatePopupMenu();
    AppendMenu(file, MF_STRING, 1, "&About...");
    AppendMenu(bar, MF_POPUP, file, "&File");
    CreateWindow("Hello", "Hello", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                 CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, bar, inst, 0);
    MSG m;
    while (GetMessage(&m, 0, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return m.wParam;
}
```

What is there:

- Window classes and procedures, `CreateWindow`, `DestroyWindow`,
  `ShowWindow` (with maximizing), `MoveWindow`, z-order, activation,
  focus, capture, `GetWindowLong`/`SetWindowLong` (with extra bytes and
  subclassing), `DefWindowProc`, `CallWindowProc`.
- The message queue: `GetMessage`, `PeekMessage`, `PostMessage`,
  `SendMessage`, `TranslateMessage` (`WM_CHAR`, `WM_SYSCHAR`),
  `DispatchMessage`, `PostQuitMessage`; keyboard (`WM_KEYDOWN` with
  virtual keys, `GetKeyState`) and mouse messages (INT 33h, with
  `CS_DBLCLKS` double clicks); timers; accelerators; carets.
- Painting: `BeginPaint`, `GetDC`, `TextOut`, `DrawText` (alignment, word
  wrap, `&` mnemonics), `FillRect`, `SetTextColor`, `SetBkColor`, and the
  `COLOR_` system colors (`GetSysColor`, `SetSysColors`).
- Menus: `CreateMenu`, `AppendMenu`, `ModifyMenu`, `DeleteMenu`,
  `CheckMenuItem`, `EnableMenuItem`, `TrackPopupMenu`, `WM_INITMENUPOPUP`
  and `WM_MENUSELECT` (for help on the status line).
- Controls: `BUTTON` (push, default, check box, 3-state, radio, group
  box), `STATIC`, `EDIT` (single and multiline, selection, scrolling,
  `EM_` messages) and `LISTBOX` (sorted, notifications, `LB_` messages);
  `WS_VSCROLL` scroll bars on any window.
- Dialogs: modal and modeless, `IsDialogMessage` (Tab and group order,
  mnemonics, the default button, Esc), `GetDlgItemText`,
  `GetDlgItemInt`, `CheckRadioButton` and the rest, and `MessageBox`.

What is different, because this is text mode and DOS:

- Every coordinate and size is in character cells: windows, client
  areas, dialog templates and mouse messages.
- A color is a text attribute (background × 16 + foreground). A brush is
  `CW_BRUSH(attribute)` or `(HBRUSH)(COLOR_xxx + 1)`.
- There are no resources: a dialog is a `CWDLGTEMPLATE` in C, run with
  `CwDialogBox` or `CwCreateDialog`, and an accelerator table comes from
  `CwCreateAcceleratorTable`.
- The whole screen is repainted (every visible window gets `WM_PAINT`)
  before `GetMessage` waits, when anything was invalidated; draw in
  `WM_PAINT`. Only the cells that changed reach video memory.
- Lines of a multiline edit control end in `\n` alone. There are no GDI
  objects, horizontal scroll bars, combo boxes, MDI or clipboard API.
- `CwSetStatusText` shows a status line on the bottom row (a `\t` puts
  the rest at the right).

A program defines `WinMain` (the library's `main` calls it with the
command line) or its own `main`. Handles are 16-bit numbers, as in
Windows 3.x, so they fit in a `WPARAM`.

The library is about 40K of code. It is built with `-Oz` and fits with a
program in the small model when the program is optimized; the far-code
models (medium, large, huge) have room for anything, and are what most
Windows 3.x programs used. Tiny and compact are too small.

`examples/dos/cwdemo.c` is a text editor with File, Edit, Search, Options
and Help menus, Open, Save As, Find and Options dialogs, message boxes,
accelerators and a status line with menu help and a clock.
`tests/dos/cwin.c` checks windows, controls, menus, painting and a
dialog driven by keys.

## tui.h

A C++ toolkit for text-mode programs, in the style of the application
frameworks of Turbo Pascal and Borland C++ (everything is in the `tui`
namespace):

- `Application` with a `MenuBar`, a `Desktop` and a `StatusLine`; `run()`
  runs the event loop until `cmQuit`.
- `Window`: overlapping windows with a frame, a title, a number (Alt+1 to
  Alt+9), close and zoom boxes, moved and resized with the mouse.
  `Desktop` tiles and cascades them.
- `Menu` and `MenuBar`: pull-down menus with hot letters (`~F~ile`),
  shortcut keys, separators, submenus and disabled items.
- `Dialog` with `Button`, `InputLine`, `CheckBoxes`, `RadioButtons`,
  `ListBox`, `StaticText` and `Label`; `execView` runs one modally.
- `TextView`: scrolling text.
- `messageBox` and `inputBox` for the common cases.

Views handle `Event`s (keys, mouse and commands) in `handleEvent` and
draw into a screen buffer, which is copied to video memory after each
event. The mouse is used through INT 33h when a driver is loaded.

```cpp
#include <tui.h>
using namespace tui;

enum { cmHello = cmUser };

class App : public Application {
public:
    App()
    {
        menuBar->add("~F~ile").item("~H~ello", cmHello, kbF2, "F2")
                              .separator()
                              .item("E~x~it", cmQuit, kbAltX, "Alt-X");
        statusLine->add("~Alt-X~ Exit", kbAltX, cmQuit);
    }
    void handleEvent(Event &e) override
    {
        Application::handleEvent(e);
        if (e.what == evCommand && e.command == cmHello) {
            messageBox("Hello from DOS", mfInformation | mfOKButton);
            e.what = evNothing;
        }
    }
};

int main() { App app; return app.run(); }
```
