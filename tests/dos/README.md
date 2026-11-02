# DOS console and graphics tests

`conio.c` tests `<conio.h>` and the Turbo C parts of `<dos.h>`: text
windows and wrapping, colors, scrolling, `gettext`, `puttext` and
`movetext`, `ungetch`, `int86`, `intdos` and `intdosx` with the carry
flag, `segread`, `peekb` and `delay`. `tests/run-tests.sh` runs it in emu2
for every model. emu2 shows video memory only on a terminal, so the
script runs it under `script(1)`.

The graphics and window libraries need a real display adapter, so
`run-tests.sh` only builds their examples (`examples/dos/bgidemo.c`,
`fgdemo.c`, `cwdemo.c` and `tuidemo.cpp`; `NO_DOSLIBS=1` skips them).
`cwin.c` tests `<cwin.h>` without a display: it reads back video memory
and drives a dialog with keys posted from a timer. Run them in
DOSBox to look at them:

```sh
clang --target=i8086-pc-msdos --sysroot=$SYSROOT -Os \
    examples/dos/bgidemo.c -lgraph -o out/bgidemo.exe
clang --target=i8086-pc-msdos --sysroot=$SYSROOT -Os \
    examples/dos/fgdemo.c -lgraph -o out/fgdemo.exe
clang --target=i8086-pc-msdos --sysroot=$SYSROOT -mcmodel=medium -Os \
    examples/dos/cwdemo.c -lcwin -o out/cwdemo.exe
clang++ --target=i8086-pc-msdos --sysroot=$SYSROOT -Os \
    examples/dos/tuidemo.cpp -ltui -o out/tuidemo.exe
dosbox -c "mount c out" -c "c:" -c "bgidemo"
```

`bgidemo` takes a driver and a mode (the numbers of `<graphics.h>`) and
`fgdemo` a Fastgraph mode number:

| Command | Screen |
| --- | --- |
| `bgidemo` | the best mode found (VGA 640x480 in DOSBox) |
| `bgidemo 1 0` ... `bgidemo 1 4` | CGA 320x200 palettes 0-3, 640x200 |
| `bgidemo 3 0`, `bgidemo 3 1` | EGA 640x200, 640x350 |
| `bgidemo 7 0` | Hercules 720x348 (DOSBox with `machine=hercules`) |
| `bgidemo 11 0` | VGA 320x200, 256 colors |
| `bgidemo 12 1` | VESA 640x480, 256 colors |
| `fgdemo 13`, `fgdemo 18`, `fgdemo 19`, `fgdemo 25` | EGA, VGA, 256-color VGA and VESA |

DOSBox's `machine=` setting picks the adapter: `cga`, `ega`, `hercules`,
`vgaonly` or `svga_s3` (the default, with VESA). A screenshot is Ctrl+F5.

`tuidemo` runs in 80x25 text mode on any adapter. With the keyboard: F10
or Alt and a letter for the menus, F6 for the next window, F5 to zoom,
Alt+F3 to close, Tab between the controls of a dialog. The mouse works
when DOSBox has captured it (click in its window): drag a title bar to
move a window, its bottom right corner to resize it.

`cwdemo` is a small text editor in 80x25 text mode on any adapter (it
takes a file name). Alt or F10 opens the menu bar, Alt and a letter a
menu; in dialogs Tab and Shift+Tab move between the controls, the arrows
within a group of option buttons, Enter presses the default button and
Esc cancels. Shift with the arrows selects text; Shift+Del, Ctrl+Ins and
Shift+Ins cut, copy and paste, F3 finds again and Alt+X quits.
