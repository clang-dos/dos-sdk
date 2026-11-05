# clang-dos toolchain

clang, lld and the LLVM tools from clang-dos/llvm, with JWlink, compiler-rt
builtins and C/C++ sysroots for 16-bit DOS, 16-bit Windows (1.x to 3.x),
Win32s, Windows NT 3.x and 4.0, Windows 95/98/ME, Windows 2000/XP, and
Windows Vista, 7, 8, 8.1 and 10/11 (32 and 64-bit), and for classic
AmigaOS (Kickstart 1.3 to 3.x, every 68k CPU).

There are two packages with the same contents and layout:

- `clang-dos-linux-x86_64.tar.xz` runs on 64-bit x86 Linux with glibc 2.38
  or later (Ubuntu 24.04, Debian 13, Fedora 39 or newer).
- `clang-dos-windows-x86_64.zip` (or the smaller `.7z`, same contents)
  runs on 64-bit Windows 10 or later. It needs no other software
  (everything is statically linked); the `.py` checkers need Python 3.

Unpack anywhere and put `bin` on `PATH`, or call `bin/clang` by its full
path. The examples below are written for a Linux shell; on Windows use
`clang.exe` the same way from `cmd` or PowerShell.

## Picking a target

Each target has a configuration file in `bin/`, which sets the target
triple, the sysroot and the linker. Pass it with `--config=NAME.cfg`:

| Target | Config | Triple | Output |
| --- | --- | --- | --- |
| DOS, any memory model | `dos.cfg` | `i8086-pc-msdos` | `.COM` (tiny) or MZ `.EXE` |
| Windows 3.0 (standard/enhanced), 3.1+ | `win16.cfg` | `i8086-pc-win16` | NE, protected mode |
| Windows 3.1 and later | `win31.cfg` | `i8086-pc-win163.10` | NE |
| Windows 3.0, any mode | `win30.cfg` | `i8086-pc-win163.00` | NE, real-mode safe |
| Windows 2.x | `win2.cfg` | `i8086-pc-win162.03` | NE, real-mode safe |
| Windows 1.x | `win1.cfg` | `i8086-pc-win161.01` | NE, real-mode safe |
| Win32s | `win32s.cfg` | `i386-pc-windows3.10-gnu` | PE, crtdll.dll, libc++ |
| Windows NT 3.1 | `nt31.cfg` | `i386-pc-windows3.10-gnu` | PE, crtdll.dll, libc++ |
| Windows NT 3.5 | `nt35.cfg` | `i386-pc-windows3.50-gnu` | PE, crtdll.dll, libc++ |
| Windows NT 3.51 | `nt351.cfg` | `i386-pc-windows3.51-gnu` | PE, crtdll.dll, libc++ |
| Windows NT 4.0 | `nt4.cfg` | `i386-pc-windows4.0-gnu` | PE, msvcrt.dll, libc++ |
| Windows 95 | `win95.cfg` | `i386-pc-windows4.0-gnu` | PE, msvcrt.dll, libc++ |
| Windows 95 without msvcrt.dll | `win95-crtdll.cfg` | `i386-pc-windows4.0-gnu` | PE, crtdll.dll, libc++ |
| Windows 98 | `win98.cfg` | `i486-pc-windows4.10-gnu` | PE, msvcrt.dll, libc++ |
| Windows ME | `winme.cfg` | `i586-pc-windows4.90-gnu` | PE, msvcrt.dll, libc++ |
| Windows 2000 | `win2000.cfg` | `i586-pc-windows5.0-gnu` | PE, msvcrt.dll, libc++ |
| Windows XP | `winxp.cfg` | `i586-pc-windows5.1-gnu` | PE, msvcrt.dll, libc++ |
| 95, 98, ME, NT 4.0 and later, no C runtime DLL | `win32-picolibc.cfg` | `i386-pc-windows4.0-gnu` | PE, picolibc, libc++ |
| NT 3.x and later, no C runtime DLL | `nt31-picolibc.cfg` | `i386-pc-windows3.10-gnu` | PE, picolibc, libc++ |
| Windows Vista | `vista.cfg`, `vista-x64.cfg` | `i686`/`x86_64-pc-windows6.0-gnu` | PE, msvcrt.dll, libc++ |
| Windows 7 | `win7.cfg`, `win7-x64.cfg` | `i686`/`x86_64-pc-windows6.1-gnu` | PE, msvcrt.dll, libc++ |
| Windows 8 | `win8.cfg`, `win8-x64.cfg` | `i686`/`x86_64-pc-windows6.2-gnu` | PE, msvcrt.dll, libc++ |
| Windows 8.1 | `win81.cfg`, `win81-x64.cfg` | `i686`/`x86_64-pc-windows6.3-gnu` | PE, msvcrt.dll, libc++ |
| Windows 10 and 11 | `win10.cfg`, `win10-x64.cfg` | `i686`/`x86_64-pc-windows10.0-gnu` | PE, msvcrt.dll, libc++ |
| AmigaOS 1.3 to 3.x | `amiga.cfg` | `m68k-amigaos` | hunk executable |
| Atari ST, STe, TT, Falcon | `atari.cfg` | `m68k-atari-tos` | GEMDOS PRG |

The DOS and Win16 targets need no config file: `--target=i8086-pc-msdos`
(or `i8086-pc-win16...`) is enough, since clang finds the sysroot in
`i8086-msdos/` next to `bin/` and JWlink in `bin/`. For the 32-bit Windows
targets, `--config` is the easy way to pass `--sysroot=sysroots/... -fuse-ld=lld`.

The Windows targets above use mingw-w64. To build with Microsoft's own
Win32 SDKs and DDKs instead (NT 3.1, NT 3.51, NT 4.0/95, 2000, XP and
Server 2003), which can't be shipped in the package, import your own
copies once; each kit becomes an `ms-KIT.cfg` (see
`share/clang-dos/docs/win32-README.md`):

```sh
python3 share/clang-dos/import-ms-sdk.py --sdks /path/to/WindowsSDKs .
bin/clang --config=ms-nt351.cfg -O2 hello.c -o hello.exe
```

## 16-bit DOS

```sh
clang --config=dos.cfg -mcmodel=tiny    -Os hello.c -o hello.com
clang --config=dos.cfg -mcmodel=small   -Os hello.c -o hello.exe
clang --config=dos.cfg -mcmodel=medium  -Os hello.c -o hello.exe
clang --config=dos.cfg -mcmodel=compact -Os hello.c -o hello.exe
clang --config=dos.cfg -mcmodel=large   -Os hello.c -o hello.exe
clang --config=dos.cfg -mcmodel=huge    -Os hello.c -o hello.exe
clang --config=dos.cfg -mcmodel=huge -msize-t=16 -Os hello.c -o hello.exe   # Watcom-style huge
clang++ --config=dos.cfg -mcmodel=small -Os hello.cpp -o hello.exe
clang++ --config=dos.cfg -mcmodel=large -Os -fexceptions -frtti except.cpp -o except.exe
clang --config=dos.cfg -mcmodel=small -Os curses.c -lcurses -o curses.exe   # PDCurses
```

- Objects are OMF (`.obj`), linked by JWlink with `option eliminate`. Pass
  linker directives with `-Wl,`: `-Wl,option,stack=16384` for a 16 KiB
  stack (the default is 4 KiB).
- The C library is picolibc with a DOS layer; C++ is libc++ (no threads).
  C++ defaults to `-fno-exceptions -fno-rtti`; `-fexceptions` links the
  SjLj exception libraries.
- Code is for an 8086 unless `-march` says otherwise (`i286`, `i386`).
  Before the 486 there is no `XADD` or `CMPXCHG`, so atomics (`std::atomic`,
  `__atomic_*`) are calls to compiler-rt, which turns interrupts off around
  each one.
- `__near`, `__far`, `__huge`, `__interrupt`, `__loadds` and `__pascal`
  work as in Open Watcom and Microsoft C. `<dos.h>` has `_dos_getvect`,
  `_dos_setvect`, `_chain_intr` and `_dos_keep` for resident programs.
- Which model to use: `share/clang-dos/docs/memory-models.md`.
- Full details: `share/clang-dos/docs/dos-sdk-README.md` and
  `picolibc-msdos-README.md`.

## 16-bit Windows (1.x, 2.x, 3.x)

```sh
clang --config=win31.cfg -Os hello.c -o hello.exe                 # small model
clang --config=win31.cfg -mcmodel=large -Os app.c -o app.exe
clang --config=win31.cfg -mcmodel=large -shared mylib.c -o MYLIB.DLL
clang --config=win1.cfg -Os app.c -o app.exe                      # Windows 1.x
llvm-rc -win16 app.rc                                              # resources
python3 share/clang-dos/ne-check.py --real-mode --version 2.03 app.exe
```

- Small, medium, compact and large models (no tiny or huge on Windows).
- A program has `WinMain` (from `<windows.h>`) or `main`; `printf` and
  files go through DOS. Every Windows 3.1 API function can be called.
- For Windows 1.x and 2.x name DLLs `MYLIB.EXE`, keep code fixed, and check
  with `ne-check.py --real-mode`. Only Wine has run these programs.
- `share/clang-dos/win16-imports.py MYLIB.DLL > mylib.lnk` makes an import
  list for a third-party DLL.
- Full details: `share/clang-dos/docs/win16-README.md`.

## Win32s and Windows NT 3.x

```sh
clang --config=nt31.cfg -O2 hello.c -o hello.exe
clang --config=win32s.cfg -O2 -mwindows gui.c -o gui.exe
python3 share/clang-dos/check-imports.py --nt 3.10 hello.exe
```

- Programs import `crtdll.dll`, which every NT 3.x has, and are marked with
  the NT 3.x subsystem version; the import table is writable, as the NT 3.x
  loader needs.
- Win32s uses the NT 3.1 setup (subsystem 3.10, with base relocations). It
  has not been run on real Win32s: avoid threads and keep to the Win32s API
  subset.

## Windows 95, 98, ME and NT 4.0

```sh
clang --config=win95.cfg -O2 hello.c -o hello.exe
clang --config=win98.cfg -O2 -mwindows gui.c -o gui.exe
clang --config=win95-crtdll.cfg -O2 hello.c -o hello.exe   # bare Windows 95
clang --config=win95.cfg -O2 -municode uni.c -lunicows -o uni.exe   # ship unicows.dll
python3 share/clang-dos/check-imports.py hello.exe
```

- C and C++ (libc++). Code is for a 386 (486 for 98, Pentium for ME)
  without SSE. On a 386, which has no `XADD` or `CMPXCHG`, atomics are calls
  to compiler-rt that use those instructions when the CPU has them.
- `msvcrt.dll` ships with 98 and ME, and with 95 only after IE4/Office; use
  `win95-crtdll.cfg` for a bare Windows 95.
- `check-imports.py` flags functions 9x lacks or has only as Unicode stubs.
  `--dlls DIR` checks against DLLs copied from real systems.
- Full details: `share/clang-dos/docs/win32-README.md`.

## Windows 2000 and XP (C and C++)

```sh
clang   --config=winxp.cfg -O2 hello.c -o hello.exe
clang++ --config=winxp.cfg -O2 hello.cpp -o hello.exe
clang++ --config=win2000.cfg -O2 threads.cpp -o threads.exe
python3 share/clang-dos/check-imports.py --nt 5.0 threads.exe
```

- libc++, libc++abi and libunwind (DWARF exceptions) are static and use no
  Vista APIs: threads, mutexes, condition variables, `thread_local`,
  `<filesystem>` and locales all work on 2000 and XP.
- The sysroot's headers default to `_WIN32_WINNT=0x0500`, so a program runs
  on both; add `-D_WIN32_WINNT=0x0501` for XP-only APIs.
- `__thread`/`thread_local` use emulated TLS (also in DLLs loaded with
  `LoadLibrary`).

## Any 32-bit Windows without a C runtime DLL (picolibc)

```sh
clang   --config=win32-picolibc.cfg -O2 hello.c -o hello.exe
clang++ --config=win32-picolibc.cfg -O2 hello.cpp -o hello.exe
clang++ --config=nt31-picolibc.cfg -O2 hello.cpp -o hello.exe   # NT 3.x too
```

- picolibc is the C library, linked statically: programs import only
  `kernel32.dll` and the system DLLs they use, and run on 95, 98, ME and
  NT 4.0 and later (`nt31-picolibc.cfg`: NT 3.x and later) with nothing to
  install.
- C99/C11 `printf` (`%zu`, `%lld`, `long double`), 64-bit `time_t` and
  `off_t`, POSIX functions, and the C library libc++ expects, so
  `std::format`, `std::print` and `<filesystem>` work as elsewhere.
- No Microsoft-only CRT headers such as `<tchar.h>` or `<conio.h>`.
  `__thread`/`thread_local` are native TLS, so a DLL loaded with
  `LoadLibrary` needs Vista or later.

## Windows Vista, 7, 8, 8.1, 10 and 11

```sh
clang   --config=win7.cfg -O2 hello.c -o hello.exe          # 32-bit
clang++ --config=win10-x64.cfg -O2 hello.cpp -o hello.exe   # 64-bit
clang++ --config=vista-x64.cfg -O2 -mwindows gui.cpp -o gui.exe
```

- One 32-bit (`sysroots/win-vista`) and one 64-bit (`sysroots/win-vista-x64`)
  mingw-w64 sysroot serve all five versions. Both use the system
  `msvcrt.dll` (every version has it), so programs need no runtime DLLs.
  libc++, libc++abi and libunwind are static, with native TLS.
- Each config sets the triple (which sets the PE subsystem version, so a
  `win10` program is refused by Windows 7 and older) and `_WIN32_WINNT` for that
  version (0x0600, 0x0601, 0x0602, 0x0603, 0x0A00), so the headers show
  that version's API. A program built with `vista.cfg` runs on all of them.
- These configs are examples of the stock MinGW setup, not new compiler
  work; `share/clang-dos/build-modern-sysroot.sh` rebuilds the sysroots
  (for example with `CRT=ucrt`).

## AmigaOS (Kickstart 1.3 to 3.x)

```sh
clang --config=amiga.cfg -O2 hello.c -o hello                   # 68000, runs on every Amiga
clang --config=amiga.cfg -mcpu=68020 -O2 share/clang-dos/amiga-examples/window.c -o window
```

- The sysroot is `m68k-amigaos/`: picolibc with an AmigaOS layer (startup
  from the CLI or Workbench, `malloc` on `AllocMem`, files through
  dos.library, AmigaOS paths) and the compiler-rt builtins, built for the
  68000. `-mcpu=68020` (to 68060) only changes your own code.
- The sysroot also has the AmigaOS headers, compatible with Hyperion's NDK
  3.2 but built from AROS's (so they can be passed on), and `libamiga.a`:
  a stub for every OS library function, and the amiga.lib, debug.lib,
  ddebug.lib and reaction.lib helpers (`HookEntry`, `DoMethod`,
  `CreatePort`, `KPrintF`, `ClickTabs`, ...). `amiga-ndk.py` can install
  the NDK's own headers in their place (it downloads the NDK from Aminet,
  or takes `--archive NDK3.2.lha`).
- Programs are tested in vamos and FS-UAE (Kickstart 1.3, 2.04 and 3.1);
  full details are in `share/clang-dos/docs/amiga-README.md`.

## Atari ST, STe, TT and Falcon (TOS)

```sh
clang --config=atari.cfg -O2 hello.c -o hello.tos                 # 68000, runs on every Atari
clang --config=atari.cfg -mcpu=68030 -O2 share/clang-dos/atari-examples/gemhello.c -lgem -o gemhello.prg
```

- The sysroot is `m68k-atari-tos/`: picolibc with a TOS layer (GEMDOS
  files, `ARGV` arguments, `Mshrink` and one `Malloc` heap), the compiler-rt
  builtins, and atari-sdk's free headers for GEMDOS, BIOS, XBIOS
  (`osbind.h`), AES and VDI (`gem.h`, with `libgem.a`) and NatFeats. It is
  all redistributable, so nothing has to be downloaded.
- `ld.lld` links and `llvm-elf2tos` writes the PRG (with a symbol table
  unless `-s`). The ABI matches gcc's m68k-atari-mint: 2-byte alignment,
  32-bit `int`, pointers returned in D0.
- `share/clang-dos/hatari-run.sh` runs a program in Hatari with EmuTOS
  (free, GPL; no Atari ROM is needed). Programs are tested on the ST,
  STe, TT and Falcon there; details are in
  `share/clang-dos/docs/atari-README.md`.

## Checking the package

`smoke-test.sh` builds `examples/` for every target above and checks the
results with `ne-check.py` and `check-imports.py`. With
[emu2](https://github.com/dmsc/emu2), Wine and vamos (`pip install amitools
machine68k==0.3.0`) on `PATH` (or `EMU2=`, `WINE=` and `VAMOS=`), and
Hatari with `EMUTOS_DIR` set, it runs them too:

```sh
sh smoke-test.sh                          # Linux package
HOSTRUN=wine sh smoke-test.sh             # Windows package, from Linux with Wine
```

Every program is tested only in emu2, Wine, Amiga emulators and Hatari; none has
run on real DOS, Windows 1.x-3.x, Win32s, NT 3.x, 9x, 2000, XP, later
Windows, Amiga or Atari machines yet.

## Contents

```
bin/               clang, clang++, lld, llvm-ar/nm/objdump/rc/dlltool/elf2hunk/elf2tos/..., jwlink, *.cfg
lib/clang/         clang's own headers
i8086-msdos/       DOS and Win16 sysroot: picolibc, builtins, libc++ per memory model
sysroots/win9x-msvcrt/  mingw-w64 for 95/98/ME/NT 4.0 (msvcrt.dll), with libunicows
sysroots/win9x-crtdll/  mingw-w64 for NT 3.x, Win32s and bare 95 (crtdll.dll)
sysroots/win2k-xp/      mingw-w64 for 2000/XP with libc++, libc++abi, libunwind
sysroots/win32-picolibc/ mingw-w64 API headers with picolibc and libc++, every 32-bit Windows
sysroots/win-vista/     mingw-w64 for Vista to 11, 32-bit, with libc++
sysroots/win-vista-x64/ the same, 64-bit
m68k-amigaos/      AmigaOS sysroot: picolibc, builtins, AmigaOS headers, libamiga.a
m68k-atari-tos/    Atari TOS sysroot: picolibc, builtins, TOS/GEM headers, libgem
share/clang-dos/   ne-check.py, check-imports.py, win16-imports.py, build-modern-sysroot.sh,
                   amiga-ndk.py (with lha.py, fd2stub.py), amiga-examples/,
                   hatari-run.sh, atari-examples/, docs/
examples/          hello.c, hello.cpp, except.cpp, threads.cpp, curses.c
BRANCHES.txt       the source branches and commits this was built from
```
