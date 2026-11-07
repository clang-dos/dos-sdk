# PDCurses for clang-dos

This is [PDCurses](https://pdcurses.org/) 3.9, the public domain curses
library, from <https://github.com/wmcbrine/PDCurses> at commit
`e9af89a7e7fc69d5b194b5432df57eefb6d83505`, with what the DOS port needs
to build with clang for `i8086-pc-msdos`. `scripts/build-doslibs.sh`
builds it into `libcurses.a` for each memory model and installs
`curses.h` and `panel.h`; [docs/graphics-and-tui.md](../docs/graphics-and-tui.md)
describes the library as the sysroot has it.

| Directory | From PDCurses |
| --- | --- |
| `curses.h`, `panel.h`, `curspriv.h` | the headers |
| `pdcurses/` | the portable core, with the panel library |
| `dos/` | the DOS port |
| `common/acs437.h` | the code page 437 line drawing characters |
| `demos/` | the demos, which are not built into the library |

## Changes

Only `dos/pdcdos.h`, `dos/pdcdisp.c` and `dos/pdcscrn.c` are changed, and
only where `PDC_CLANG_DOS` is defined (clang for the 8086):

- Memory below 1 MiB (the BIOS data area and video memory) is reached
  with `__far` pointers made with `MK_FP`, in every memory model. The
  BIOS data area is read through `volatile` pointers, as `napms` polls
  the timer tick there.
- The copies to and from video memory (each line in `PDC_transform_line`,
  and the screen saved for `PDC_RESTORE_SCREEN`) are loops over far
  pointers, `PDC_vram_put` and `PDC_vram_get`, rather than `memcpy` or
  `movedata`, as the C library takes default (near) pointers in the
  tiny, small and medium models. They copy a character cell of two bytes
  at a time through a type that may alias the byte arrays they are given.

The BIOS calls go through `int86` and `int86x` with `union REGS`, as
with Turbo C, which needed no change.

## License

PDCurses is in the public domain. `demos/rain.c` and `demos/worm.c` are
under the ncurses license (MIT-style), given at the top of each file.
The clang-dos changes described above (the `PDC_CLANG_DOS` parts of
`dos/pdcdos.h`, `dos/pdcdisp.c` and `dos/pdcscrn.c`) are under
AGPL-3.0-only (see the dos-sdk LICENSE).
