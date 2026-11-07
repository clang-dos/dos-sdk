# Turbo Vision for clang-dos

[magiblot/tvision](https://github.com/magiblot/tvision) is the modern
version of Borland's Turbo Vision 2.0, the C++ text-mode application
framework of Borland C++. It still builds with Borland C++ for 16-bit DOS,
and this directory makes that DOS variant build with clang-dos.

`scripts/build-tvision.sh` takes a checkout of magiblot/tvision at commit
`b5fc1e50dcc2da062d72f43eb3662894d25aaf30` (the CI fetches it), applies
`clang-dos.patch`, adds `doshw.cpp` and `dosrtl.cpp`, and builds
`libtvision.a` for the medium, large, huge and huge16 models. Tiny, small
and compact are left out: the library has about 300 KB of code, and those
models have one 64 KiB code segment.

```sh
git clone https://github.com/magiblot/tvision
git -C tvision checkout b5fc1e50dcc2da062d72f43eb3662894d25aaf30
TVISION_SRC=$PWD/tvision BIN=... SYSROOT=... sh scripts/build-tvision.sh
```

## What the port does

- `tv.h` defines `__BORLANDC__` (as Borland C++ 4.6), `__FLAT__` and
  `__TV_CLANG_DOS__` when it sees clang-dos (`__clang__`, `__MSDOS__` and
  `__I86__`). Turbo Vision then builds its Borland variant (one byte per
  character, two-byte screen cells, code page 437) with the C++ routines of
  its 32-bit build in place of the TASM ones. `__TV_CLANG_DOS__` leaves out
  the Win32 code of the 32-bit build.
- `doshw.cpp` is a `THardwareInfo` for real-mode DOS: the screen through
  INT 10h and text-mode video memory, the keyboard through INT 16h (the
  enhanced functions when the BIOS has them), the mouse through INT 33h,
  Ctrl-Break (INT 1Bh and 23h) and the critical-error handler (INT 24h,
  which fails the call). It polls the mouse; the press counts of INT 33h
  function 5 catch a click between two polls. Idle time goes to INT 2Fh
  function 1680h, or HLT.
- `dosrtl.cpp` has the Borland run-time functions that picolibc lacks:
  `stricmp`, `strnicmp`, `memicmp`, `ltoa`, `ultoa`, `getdisk`,
  `setdisk`, `getcurdir`, `findfirst`, `findnext`, `_dos_findfirst`,
  `_dos_findnext`, `fnsplit` and `fnmerge`.
- `clang-dos.patch` changes the headers and sources: the Borland
  compatibility headers (`compat/borland`) are used with clang, `Boolean`
  is `bool`, `TScreenCell` copies without calling its own conversion
  operator, `new.cpp` defines every form of `operator new` and `delete`
  (so that libc++abi's are not linked as well), and `TMouse`, the drive
  checks, the code page and the critical-error handler go through
  `THardwareInfo` or DOS calls. `examples/tvdemo/gadgets.cpp` shows the
  heap in use with `mallinfo`.

## Using it

```sh
clang++ --target=i8086-pc-msdos -mcmodel=large -Os app.cpp -ltvision -o app.exe
```

`#include <tvision/tv.h>` after the `Uses_` macros, as with Borland C++.
Programs written for Borland C++ that include `<iostream.h>`,
`<strstrea.h>`, `<dir.h>` and the like also need
`-I<sysroot>/include/tvision/compat/borland`.
`packaging/examples/tvhello.cpp` is a small example. The demo of the
library, `examples/tvdemo` in magiblot/tvision, builds with:

```sh
clang++ --target=i8086-pc-msdos -mcmodel=large -Os -DNDEBUG \
  -I<sysroot>/include/tvision/compat/borland tvdemo/*.cpp -ltvision \
  -Wl,option,stack=16384 -o tvdemo.exe
```

In the medium and large models the heap is one 64 KiB segment (in medium,
shared with the data and the stack), so a program that opens many windows
wants huge or huge16, which use all free DOS memory. There is no
clipboard, and `nothrow` `operator new` aborts when memory runs out, as
the plain one does.

Turbo Vision is under the MIT license, with Borland's disclaimer for the
original code; the sysroot has it in `share/tvision/COPYRIGHT`.
The port's own code (the changes in `clang-dos.patch`, `doshw.cpp` and
`dosrtl.cpp`) is under AGPL-3.0-only (see the dos-sdk LICENSE).
