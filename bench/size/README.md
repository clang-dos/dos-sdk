# C++ size benchmark

Builds the same small C and C++ programs with clang-dos and with other
16-bit DOS C++ compilers, checks that each one runs, and records the size
of what it makes: the .EXE or .COM file, and the code, data and BSS from
the linker's map (the stack is left out, since each toolchain picks its
own).

```sh
BIN=~/llvm-build/bin SYSROOT=~/sysroot \
WATCOM=~/ow DMC=~/dm IA16=~/build-ia16/prefix/bin \
EMU2=~/emu2/emu2 TUNED=1 sh bench/size/run.sh
```

Each toolchain is built only when its variable is set; results go to
`out/results.csv` (`OUT=` moves it). `MODELS=` (default `tiny small large`)
and `PROGS=` choose the builds.

| Variable | Toolchain | Flags |
|---|---|---|
| `BIN`, `SYSROOT` | clang-dos (`clang++ --target=i8086-pc-msdos`) | `-Os`, `-fexceptions` for `except` |
| `TUNED=1` | clang-dos with the experiments in `experiments/` | as above |
| `WATCOM` | Open Watcom 2 (`wcl`, `-bt=dos`, `-bcl=com` for tiny) | `-os -s`, `-xs` for `except` and `strvec` |
| `DMC` | Digital Mars C++ (`dmc.exe`, through Wine off Windows) | `-o+space`, `-Ae` for `except` and `strvec` |
| `IA16` | gcc-ia16 (`ia16-elf-g++`) | `-Os -ffunction-sections -fdata-sections -Wl,--gc-sections`, and `-fno-exceptions -fno-rtti` except for `except` |

`EMU2` runs each program in [emu2](https://github.com/dmsc/emu2) and
compares its output with `expected/`.

## Programs

- `hello_c.c`, `hello_printf.cpp`: printf of a constant string (the C
  runtime alone; compilers that turn it into `puts` pay less).
- `hello_iostream.cpp`: `std::cout << "Hello, world!" << std::endl`.
- `strvec.cpp`: `std::string` and `std::vector`, `std::sort` on both,
  `find`, printf of the results.
- `virtual.cpp`: a class hierarchy called through base pointers, `new` and
  `delete`, a class template and a function template used with two types.
- `except.cpp`: throw and catch by type and by base class
  (`std::runtime_error`), destructors run while unwinding, rethrow,
  `catch (...)`.

## Notes on the other compilers

- Open Watcom's and Digital Mars's iostreams are the classic pre-standard
  ones (not templates, no locales), so `hello_iostream` measures a much
  smaller library than libc++'s.
- Open Watcom's `<string>` and `<vector>` need exceptions on (`-xs`).
- Digital Mars has no standard library headers for 16-bit code:
  `compat/dmc16/` maps `<iostream>` onto `<iostream.h>` and gives a minimal
  `<stdexcept>`; `strvec` does not build. Its linker leaves tiny programs as
  an .EXE, which `exe2com.py` turns into a .COM as EXE2BIN would (the
  iostream one has relocations, so it cannot be one).
- gcc-ia16 has no large model, and no C++ in medium. Its libstdc++
  iostreams do not fit in 64 KiB of code.

## Tools

- `mapsize.py prog.map`: code, data and BSS from a Watcom/JWlink, OPTLINK
  or GNU ld map.
- `mapsyms.py prog.map [count] [code|data|bss]`: the largest library
  members and symbols in a Watcom or JWlink map, to see where the bytes go.
