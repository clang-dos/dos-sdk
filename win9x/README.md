# Windows 95, 98 and ME

clang builds 32-bit programs for Windows 95, 98 and ME with a Windows
version in the target triple and a sysroot made from mingw-w64.

| Windows | Triple | Default CPU |
| --- | --- | --- |
| 95 (and NT 4.0) | `i486-pc-windows4.0-gnu` | 80486 |
| 98 | `i486-pc-windows4.10-gnu` | 80486 |
| ME | `i586-pc-windows4.90-gnu` | Pentium |

The version in the triple is the oldest Windows the program runs on. clang
then:

* targets the CPU of the arch name (`i386`, `i486`, `i586` for a Pentium,
  `i686` for a Pentium Pro) instead of a Pentium 4, so no SSE. Windows 95 and
  NT 4.0 do not save SSE registers. `-march` still overrides it.
* tells the linker to write that subsystem and OS version. Windows 9x refuses
  programs that ask for Windows 5.0 or later, and LLD's default is 6.0.
  `-Wl,--subsystem,console:4.0` and the like still override it.

## Building the sysroot

```sh
LLVM_SRC=~/llvm MINGW_SRC=~/mingw-w64 BIN=~/llvm/build/bin \
  SYSROOT=~/win9x-sysroot sh win9x/build-sysroot.sh
```

Use `llvm-dlltool`, `lld` and `clang` from the same new build. LLVM 18's
`llvm-dlltool` turns renamed imports in mingw-w64's `.def` files into weak
aliases, so `atexit` calls itself and programs hang at startup; a new
`llvm-dlltool` with LLVM 18's `lld` imports functions by ordinal 0.

It needs a recent mingw-w64 (current master works): recent mingw-w64 calls
newer Windows functions only through `GetProcAddress` with fallbacks, and
its own test suite runs on Windows 9x. The sysroot holds:

* mingw-w64 headers with `_WIN32_WINNT` defaulting to `0x0400`, so APIs
  newer than Windows 95 and NT 4.0 are hidden unless a program asks for them;
* the mingw-w64 CRT and import libraries, linked against the `msvcrt.dll`
  that ships with Windows (`libmsvcrt.a` is `libmsvcrt-os.a`);
* compiler-rt builtins, as `libgcc.a` so clang links them by default, and
  empty `libgcc_eh.a`, `crtbegin.o` and `crtend.o`, which C does not need.

Everything is compiled for an 80486 (`CPU=i486`). An 80386 has no
`CMPXCHG` or `XADD`, so 386 code calls `InterlockedCompareExchange` and
`InterlockedExchangeAdd` in kernel32, which Windows 95 does not have.
`CPU=i386` builds a sysroot for a 386 that runs on Windows 98 and NT 4.0
only.

## Building programs

```sh
clang --target=i486-pc-windows4.0-gnu --sysroot=$SYSROOT -fuse-ld=lld \
  hello.c -o hello.exe
clang --target=i486-pc-windows4.0-gnu --sysroot=$SYSROOT -fuse-ld=lld \
  -mwindows gui.c -o gui.exe
```

## The C runtime DLL

Programs import `msvcrt.dll`. Windows 98 and ME ship it; Windows 95 has it
only when something installed it (Internet Explorer 4, Office, many other
programs). For a bare Windows 95, ship Microsoft's redistributable
`msvcrt.dll` 6.0 with the program, or build a second sysroot with
`CRT=crtdll`, whose programs import `crtdll.dll`, which every Windows 95 has.

## Native ANSI builds

Windows 9x programs are native ANSI builds: they call the `A` functions
directly, with no unicows layer. Windows 95, 98 and ME implement only a
few `W` functions (`MessageBoxW`, `TextOutW`, `lstrlenW`,
`MultiByteToWideChar` and a handful more); the others exist only as stubs
that fail with `ERROR_CALL_NOT_IMPLEMENTED`.

* Generic-text code (`TCHAR`, `_T()`, `CreateFile`, `_tmain`) compiles to
  the `A` functions, because nothing defines `UNICODE` or `_UNICODE`. Don't
  pass `-municode` or `-DUNICODE` for a 9x program.
* The sysroot's own startup code and C library import no `W` functions;
  `check-imports.py` confirms it for every test program.
* `check-imports.py` fails a build that imports a `W` stub. A program that
  checks the Windows version and picks `W` functions only on NT can pass
  `--allow-unicode` to see those as warnings instead.
* Text is in the system's ANSI code page (`GetACP()`), as in any ANSI
  program.

## What does not work

* PE TLS callbacks never run on 9x. `__thread` variables work in an EXE;
  in a DLL loaded with `LoadLibrary`, they work on no Windows before Vista.
  C++ `thread_local` destructors do not run.
* C++: libc++ needs Windows Vista or later. Only C is supported so far.

## Checking imports

`check-imports.py` lists the functions a program imports that Windows 9x
lacks or has only as Unicode stubs. Without DLLs it uses a built-in list of
NT-only and newer functions. With `--dlls DIR`, where `DIR` holds one
directory of system DLLs per Windows version (for example `95osr2/` and
`98se/` with `KERNEL32.DLL`, `USER32.DLL`, `GDI32.DLL`, `ADVAPI32.DLL` and
`MSVCRT.DLL` copied from those systems), it checks every import against
the real export tables.

```sh
python3 win9x/check-imports.py --dlls ~/win9x-dlls hello.exe
```

## Tests

```sh
BIN=~/llvm/build/bin SYSROOT=~/win9x-sysroot DLLS=~/win9x-dlls \
  sh win9x/run-tests.sh
```

builds `tests/hello.c`, `tests/libc.c`, `tests/tchar.c` (a native ANSI
build of generic-text code) and `tests/gui.c` for 95, 98 and
ME at `-O0`, `-O2` and `-Os`, checks their imports and runs the console
tests under Wine when it is installed. Wine does not limit programs to the
9x API, so a real Windows 9x machine is still the final test.
