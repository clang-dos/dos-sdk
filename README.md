# clang-dos SDK

Scripts and tests for building and checking a DOS sysroot for the
`i8086-pc-msdos` target of the [clang-dos LLVM toolchain](https://github.com/clang-dos/llvm),
and for 16-bit Windows (`i8086-pc-win16`, see below).

A sysroot holds, for each memory model:

```
<sysroot>/include/                       picolibc headers
<sysroot>/include/c++/v1/                libc++ headers
<sysroot>/lib/<model>/crt0.obj           startup code
<sysroot>/lib/<model>/libc.a             picolibc with the DOS layer
<sysroot>/lib/<model>/libclang_rt.builtins.a
<sysroot>/lib/<model>/libc++.a           libc++
<sysroot>/lib/<model>/libc++abi.a        libc++abi
<sysroot>/include/windows.h              16-bit Windows API (win16-sdk)
<sysroot>/lib/win16/windows.lnk          Windows API imports
<sysroot>/lib/win16/windows300.lnk       ... for Windows 3.0
<sysroot>/lib/win16/windows200.lnk       ... for Windows 2.x
<sysroot>/lib/win16/windows100.lnk       ... for Windows 1.x
<sysroot>/lib/win16/<model>/             the same libraries for Windows,
                                         with libentry.obj for DLLs
```

The models are `tiny` (.COM programs), `small`, `medium`, `compact`,
`large` and `huge` (.EXE programs). Medium has far code (each source
file gets its own code segment, so code can exceed 64 KiB) and near data.
Compact has near code and far data: data pointers are 32-bit
segment:offset values, while small globals and the stack stay in the
64 KiB DGROUP, the heap is a 64 KiB segment of its own after the program,
and `size_t` stays 16 bits. Large has both far code and far data. Huge has
far code and huge data pointers, which address objects over 64 KiB:
`size_t` and `ptrdiff_t` are 32 bits, pointer arithmetic carries into the
segment, and the heap takes all free DOS memory.

[docs/memory-models.md](docs/memory-models.md) helps you choose a model
for your program, with the clang flags for common cases.

For full-screen DOS programs, the sysroot also has `<conio.h>` (in the C
library), `<graphics.h>` and `<fastgraf.h>` (`-lgraph`: the Borland
Graphics Interface and Fastgraph functions, from CGA to VESA),
`<curses.h>` and `<panel.h>` (`-lcurses`: PDCurses 3.9, the curses library
of X/Open and System V), `<cwin.h>` (`-lcwin`: the Windows 3.x API in text
mode, in the style of the character-mode Microsoft applications) and
`<tui.h>` (`-ltui`: a C++ text-mode window toolkit with menus, dialogs and
the mouse). [docs/graphics-and-tui.md](docs/graphics-and-tui.md) describes
them. Programs written for Mike Smedley's CXL and TCXL text libraries build
against `-lcxl`, a new implementation of their API; see
[docs/cxl.md](docs/cxl.md). Borland's Turbo Vision is there too, as
`<tvision/tv.h>` and `-ltvision` in the models with far code (medium,
large, huge and huge16): see [tvision/README.md](tvision/README.md).

A source file's code over 64 KiB, in the models with far code, goes on in
more code segments, `<file>_TEXT_1` and so on. A function is in one
segment, so it is at most 64 KiB.

The stack of an .EXE program is 4 KiB. The linker's `option stack` makes
it larger: `-Wl,option,stack=16384`. The heap starts after it.

In compact, large, huge and huge16, a global or static variable larger than
32767 bytes is far data: it gets a segment of its own outside DGROUP, as in
Open Watcom, so DGROUP keeps its 64 KiB for everything else. In huge such a
variable can be over 64 KiB; it is split into consecutive 64 KiB segments.
`-mlarge-data-threshold=N` moves the limit, and 0 keeps every variable in
DGROUP. A variable declared `extern` with no size (`extern char buf[];`)
may be far data in another file, so it is reached with far pointers, which
work for near variables too. Far zero-initialized variables come after
DGROUP and the stack, so they take no room in the .EXE file; the startup
code clears them.

`__near`, `__far` and `__huge` (also `_near`, `_far` and `_huge`) qualify
data as in Open Watcom and Microsoft C, in every model but tiny:
`char __far *p` is a far pointer, `char __near *q` a 16-bit pointer into
DGROUP, and `int __far table[20000];` a far variable, which the small and
medium models can use for data that does not fit in DGROUP. A near pointer
converts to a far or huge one, and far and huge pointers convert to each
other; far or huge to near needs a cast, which keeps the offset. The C
library takes default pointers, so in small and medium pass far data to
your own functions, not to `strlen` or `memcpy`; the compiler expands
copies and fills of far data inline.

On functions, `__far` and `__near` choose the call, as in Open Watcom:
`int __far f(int)` (or `int * __far g(void)`) is called with CALL FAR and
returns with RETF in any model, and `void (__far *p)(void)` is a far
function pointer. A `__near` function in the medium, large or huge model is
called with a near call, so only code in the same source file can call it.
A function qualified with the model's own kind is an ordinary function;
near and far function pointers do not convert to each other. `char __far
*h(void)` is a near function (in small) that returns a far pointer: the
qualifier applies to what follows it.

`__pascal` (also `_pascal`) is the 16-bit Windows API convention:
arguments are pushed left to right, the called function removes them
(`RET n`), and the symbol is the upper-case name with no underscore, so
`int __far __pascal MessageBox(...)` matches the Windows import. A
struct return still passes its hidden pointer first. Variadic `__pascal`
functions use the C rules. `__export` (also `_export`) marks a function or
variable for export; it is written as an export record for a Windows
linker, and accepted and ignored in DOS programs, which have no exports.

`-mwindows-prologue=exported` gives `__far __export` functions the 16-bit
Windows entry sequence, as Open Watcom `-zW` does (`=all` gives it to every
far function, as `-zw`; `=none` is the default). The function starts with
`push ds; pop ax; nop`, which the Windows loader patches so that `DS` is
loaded from `AX` (the program's data segment, set by a `MakeProcInstance`
thunk) or from DGROUP in a DLL, marks its frame as far with `inc bp`, and
restores the caller's `DS` on return. Near functions are left alone, since
Windows cannot call them.

`__loadds` (also `_loadds`) makes a function load DGROUP into `DS` on entry
and restore the caller's `DS` on return, so it can be called with any `DS`:
from an interrupt handler or a resident program, for example. Combined with
the Windows sequence, the first bytes are `mov ax, DGROUP`, as a DLL's
loader would patch them.

`__interrupt` (also `_interrupt`) makes an interrupt handler, as in Open
Watcom and Microsoft C: `void __interrupt __far on_timer(void)`. The
function is far (with or without `__far`), saves every register, loads
DGROUP into `DS` (`CS` in tiny), clears the direction flag and returns with
`IRET`. Its parameter, if it has one, is `union INTPACK` from `<i86.h>`,
the saved registers in Open Watcom's layout, and what the handler stores
there is what the interrupted program gets back:
`void __interrupt __far on_dos(union INTPACK r) { r.w.ax = 0; }`.
Microsoft C's register parameters (`unsigned es, unsigned ds, unsigned di,
unsigned si, unsigned bp, unsigned sp, unsigned bx, unsigned dx,
unsigned cx, unsigned ax, unsigned ip, unsigned cs, unsigned flags`, or the
first few of them) can be read too. Calling a pointer to an interrupt
function, such as the handler a vector held before, pushes the flags and
makes a far call, as the interrupt would. `<dos.h>` has `_dos_getvect`,
`_dos_setvect`, `_chain_intr` (go on to another handler with the
registers as saved, changes included) and `_dos_keep` (stay resident); see
`libos/msdos/README.md` in picolibc.

A handler runs on the stack of the program it interrupted, so `SS` is not
`DGROUP`. Locals work, but in the tiny, small and medium models a near
pointer means `DS`: don't pass the address of a local, or of the
`INTPACK`, to other functions; use static variables. A resident program
keeps its memory with `_dos_keep(status, 0)`, which keeps the code, data
and heap and gives back the stack of a .COM.

In DOS programs these are for writing and calling Win16-style code; the
`i8086-pc-win16` target below makes Windows programs and DLLs.

`huge16` is the huge model with a 16-bit `size_t`, as in Open Watcom:
build with `-mcmodel=huge -msize-t=16`, and clang links the libraries in
`lib/huge16`. Pointers are huge and `ptrdiff_t` stays 32 bits, but objects
stay under 64 KiB. The heap still takes all free DOS memory, in objects
of up to 64 KiB each.

`halloc(n, size)` and `hfree(p)` (also `_halloc` and `_hfree`, declared in
`<malloc.h>`) allocate blocks larger than 64 KiB, as in Open Watcom and
Microsoft C, in every model but tiny: `long __huge *a = halloc(100000,
sizeof(long));` is 400000 zeroed bytes reached through a huge pointer. This
is how the huge16 model, or a small-model program, gets one: `malloc` is
limited by a 16-bit `size_t`. The blocks come from DOS, up to 1 MiB in all.
The first `halloc` that finds no free DOS memory gives the end of the
program's memory back to DOS: everything after the heap in the models
whose heap is at most 64 KiB, or just the block's size in the huge models,
whose heap then ends below it.

The near and far functions of Open Watcom and Microsoft C are in every
model, on DOS and on 16-bit Windows:

- `<string.h>` has the far string and memory functions: `_fmemcpy`,
  `_fmemmove`, `_fmemset`, `_fmemcmp`, `_fmemicmp`, `_fmemchr`,
  `_fmemccpy`, `_fstrlen`, `_fstrcpy`, `_fstrncpy`, `_fstrcat`,
  `_fstrncat`, `_fstrcmp`, `_fstrncmp`, `_fstricmp`, `_fstrnicmp`,
  `_fstrchr`, `_fstrrchr`, `_fstrstr`, `_fstrspn`, `_fstrcspn`,
  `_fstrpbrk`, `_fstrtok`, `_fstrdup`, `_fstrlwr`, `_fstrupr`, `_fstrrev`,
  `_fstrset` and `_fstrnset`. Their blocks stay within one segment.
- `<malloc.h>` (and `<stdlib.h>`) has the far heap, `_fmalloc`,
  `_fcalloc`, `_frealloc`, `_ffree` and `_fmsize`, and the near heap,
  `_nmalloc`, `_ncalloc`, `_nrealloc`, `_nfree` and `_nmsize`, and
  `_msize`. With near data (tiny, small, medium) the near heap is the heap
  of `malloc`, and the far heap takes segments of 8 KiB or more from DOS
  (or `GlobalAlloc`), so a small-model program can use most of memory in
  blocks of up to 64 KiB. With far data the far heap is the heap of
  `malloc`, and the near heap is in DGROUP: the local heap on Windows, and
  on DOS a 4 KiB array that `_NEAR_HEAP_SIZE(bytes);` in one source file
  makes larger.

You don't have to pick the function by hand. In the tiny, small and
medium models the headers add overloads of the standard names for far
pointers, so `memcpy(farbuf, nearbuf, n)` calls `_fmemcpy`, `strlen(fp)`
calls `_fstrlen`, and `free`, `realloc` and `_msize` of a far pointer use
the far heap. In the other models a near pointer converts to the default
one, and `free`, `realloc` and `_msize` of a `__near` pointer use the near
heap. Only allocation needs a choice, since `malloc` only sees a size:
`_fmalloc` or `_nmalloc` when the other kind is wanted. The overloads use
clang's `overloadable` in C and ordinary overloads in C++;
`-D__I86_NO_OVERLOADS` leaves them out. `strtok` has none, as it keeps
its place between calls: use `_fstrtok`.

`arc4random` is seeded by `getentropy`, which has no random source on DOS
or Windows to read: it mixes the clocks, the stack and many readings of
the 8253 timer, with RDTSC when the CPU has it, through the ChaCha
permutation. The seed is different on each run; on a CPU with RDTSC it
is hard to guess, but on an 8088 PC it comes mostly from the time the
program started, so it is no good for keys there.

clang links `crt0.obj`, `libc.a` and the builtins by default:

```sh
clang --target=i8086-pc-msdos --sysroot=<sysroot> -Os hello.c -o hello.exe
clang --target=i8086-pc-msdos -mcmodel=tiny --sysroot=<sysroot> hello.c -o hello.com
```

Without `--sysroot`, clang looks in `<clang dir>/../i8086-msdos`.

C++ uses `clang++`, which also links libc++ and libc++abi:

```sh
clang++ --target=i8086-pc-msdos --sysroot=<sysroot> -Os prog.cpp -o prog.exe
```

C++ programs build with `-fno-exceptions -fno-rtti` by default. libc++
has iostreams, string and file streams, the "C" locale and its facets,
wide characters and `<filesystem>`, but no threads. The linker drops the
functions, vtables and other COMDATs that nothing uses (clang passes
`option eliminate`), so a program only pays for the parts of libc++ it
uses: in the small model at `-Os`, the C++ test is about 32 KB of code,
and a hello world that writes to `std::cout` about 27 KB (a 36 KB .COM in
the tiny model). For that, on i8086 the "C" locale makes each facet the first time
a program asks for it, and the wide streams (`std::wcout` and the others)
are linked only when a program uses one. Programs that use most of
iostreams (file and string streams, and reading and writing numbers of
each type, floating point among them), `<filesystem>` or the demangler
are more than the 64 KiB of code that the tiny, small and compact models
have, and need a model with far code. Reading numbers from `std::cin` and
string streams fits in the small model with optimization; tiny has
64 KiB for code and data together, and far pointers make code in the
compact model about a third larger.

`<filesystem>` takes DOS paths: `C:\DIR\FILE.TXT`, with a drive letter
as the root name and `\` as the preferred separator; `/` works too, and
`generic_string()` uses it. There are no links or POSIX permissions:
`permissions` sets or clears the read-only attribute from the owner write
bit, and `last_write_time` has the 2-second resolution of DOS.

libc++ is built for every model. With `-frtti`, `dynamic_cast` and
`typeid` work too. In the models with far code or far data, a vtable
entry is 32 bits, and a member function pointer holds a far code pointer
where code is far. In the far-data models, vtables and type_info objects
are far data too, outside DGROUP.

C++ exceptions are off by default. `-fexceptions` turns them on, and
clang then links the libc++, libc++abi and unwinder built with them,
from `lib/<model>/eh` in the sysroot. They work in every model.
Exceptions unwind with setjmp/longjmp-style (SjLj) tables: each function
with a `try` or with destructors to run registers a context on entry and
unregisters it on return, so code that throws nothing still pays a few
instructions per call, but there are no unwind tables to keep in memory.
A throw needs RTTI, which the eh libraries have; your own code needs
`-frtti` only for `dynamic_cast` and `typeid`. There is no forced unwind
(`_Unwind_ForcedUnwind`), and an exception cannot leave a Windows DLL:
each DLL keeps its own list of contexts.

`abi::__cxa_demangle` is in the medium, large, huge and huge16 builds of
libc++abi. Its code is over 64 KiB with far data pointers, and close to it
with near ones; with far code the compiler spreads a module's code over
several code segments (`<module>_TEXT`, `<module>_TEXT_1`, ...), so it
fits. The tiny, small and compact models have one code segment for the
whole program and go without it. Its parser takes about 5 KiB of stack,
more than the 4 KiB default: link such a program with
`-Wl,option,stack=8192`.

Files opened without `b` in the `fopen` mode, or without `O_BINARY`, are
text files: line ends are CR LF on disk and Ctrl-Z ends the file. See
`libos/msdos/README.md` in picolibc.

## 16-bit Windows

`--target=i8086-pc-win16` makes Windows 1.x, 2.x and 3.x programs and DLLs
(NE executables) in the small, medium, compact and large models. Their
`<windows.h>`, the Windows API import lists and the script that builds the
Windows libraries into this sysroot are in
[clang-dos/win16-sdk](https://github.com/clang-dos/win16-sdk), with the
documentation and the tests. `scripts/build-sysroot.sh` runs that script
after the DOS libraries (see below).

The 32 and 64-bit Windows sysroots of the toolchain package (Win32s, NT
3.x to 11, 95/98/ME) are built by
[clang-dos/win32-sdk](https://github.com/clang-dos/win32-sdk).

## What you need

- clang, `llvm-ar`, `llvm-ranlib` and `llvm-nm` built from
  [clang-dos/llvm](https://github.com/clang-dos/llvm) (branch `i8086`).
- An llvm-project source tree for compiler-rt (the same checkout).
- [clang-dos/picolibc](https://github.com/clang-dos/picolibc) (branch
  `i8086-msdos`).
- meson, ninja and cmake.
- A Watcom-style linker: OpenWatcom `wlink` or
  [JWlink](https://github.com/clang-dos/jwlink) (branch `i8086`). clang
  finds `wlink` or `jwlink` next to itself or on `PATH`, or takes
  `--ld-path=`.
- [emu2](https://github.com/dmsc/emu2) to run the tests, and Wine (with
  16-bit support, as in the 32-bit or WoW64 builds) for the Windows tests.

## Building a sysroot

```sh
LLVM_SRC=~/llvm-project PICOLIBC_SRC=~/picolibc BIN=~/llvm-build/bin \
WIN16_SDK=~/win16-sdk SYSROOT=$PWD/sysroot sh scripts/build-sysroot.sh
```

`MODELS="small"` builds one model only. `WORK=` sets the build directory
(default `./build`). `NO_CXX=1` skips libc++. The parts run on their own,
in this order, as `scripts/build-picolibc.sh`, `scripts/build-builtins.sh`,
`scripts/build-libcxx.sh`, `scripts/build-doslibs.sh` (`libgraph.a`,
`libcurses.a`, `libtui.a` and `libcxl.a`), `scripts/build-tvision.sh`
(Turbo Vision, when `TVISION_SRC=` names a magiblot/tvision checkout) and
win16-sdk's `build-sysroot.sh` (the 16-bit Windows libraries, which runs
the builtins and libc++ scripts here again with `OS=win16`, as Windows returns
structures through far pointers in every model; `WIN16_MODELS=` picks its
models). `WIN16_SDK=` names the win16-sdk checkout (default `../win16-sdk`
next to this one), and `NO_WIN16=1` skips the Windows libraries.

## Tests

```sh
BIN=~/llvm-build/bin SYSROOT=$PWD/sysroot LD=/path/to/jwlink \
EMU2=/path/to/emu2 sh tests/run-tests.sh
```

This builds each test for each model at `-O0`, `-Os` and `-O2` and runs
it in emu2:

- `tests/libc/libc.c`: malloc, printf and strtod, qsort, setjmp, files,
  directories, constructors, destructors and `atexit`, environment, time.
- `tests/libc/hello.c`: command-line arguments.
- `tests/libc/stdin.c`: reading lines from standard input.
- `tests/libc/textmode.c`: text and binary files, `O_TEXT`, `O_BINARY`,
  `_fmode`, `setmode`, and `ftell`/`fseek` in text files.
- `tests/libc/random.c`: `getentropy`, `arc4random`, `arc4random_uniform`
  and `arc4random_buf`. It runs twice, and the two runs must print
  different first values.
- `tests/libc/halloc.c`: `halloc` and `hfree` blocks over 64 KiB next to
  `malloc` (not built for tiny).
- `tests/libc/nearfar.c`: the far string functions, the near and far
  heaps, and the standard names with near and far pointers, as C and as
  C++ (also on Windows).
- `tests/libc/bigstack.c`: a 16 KiB stack from `option stack`, with heap
  blocks next to it (not built for tiny).
- `tests/libc/fsys.c`: `opendir` and `readdir`, `stat` on directories,
  `realpath`, `chmod`, `truncate`, `utime` and `statvfs` (built with
  `-DEMU2`, which skips the checks that emu2 does not support: it does
  not set file attributes or times).
- `tests/cxx/cxx.cpp`: virtual functions, `vector`, `unique_ptr`,
  `shared_ptr`, `std::function`, `string`, `map`, `sort`, function-local
  statics and global constructors and destructors.
- `tests/cxx/oop.cpp`: inheritance, `dynamic_cast` and `typeid` with
  `-frtti`.
- `tests/cxx/iohello.cpp`: `std::cout` and `std::cerr` with strings and
  integers (tiny, small and compact).
- `tests/cxx/iobasic.cpp`: `std::cout`, `std::cin` and `std::cerr`,
  string streams, integers and strings (small, at `-Os` and `-O2`).
- `tests/cxx/iostream.cpp`, `tests/cxx/wchar.cpp`, `tests/cxx/fs.cpp`:
  all of iostreams with file streams, manipulators and floating point;
  wide strings and streams, `<cwchar>` and the locale's facets;
  `<filesystem>` (medium, large, huge and huge16; `fs.cpp` is built with
  `-DEMU2`, which skips file times and permissions).
- `tests/cxx/syncio.cpp`: `sync_with_stdio(false)` and back keeps what
  `cout` and `cin` buffered, with `wcout` too in the models with far code
  (small at `-Os` and `-O2`; medium, large, huge and huge16).
- `tests/cxx/demangle.cpp`: `abi::__cxa_demangle` (medium, large, huge
  and huge16).
- `tests/cxx/except.cpp`: exceptions with `-fexceptions`: throwing and
  catching by type and base class, destructors during unwinding,
  rethrow, `catch (...)`, constructors that throw, `std::out_of_range`,
  `std::bad_alloc`, `std::exception_ptr` and `std::uncaught_exceptions`.
  `NO_CXX=1` skips it.
- `tests/libc/intr.c`: `__interrupt` handlers with `union INTPACK` and
  with Microsoft C parameters, called with INT (also with another `DS`),
  through a pointer and with `_chain_intr`, and a timer interrupt handler
  that interrupts the program while it computes.
- `tests/dos/conio.c`: `<conio.h>` text windows, colors and screen
  copies, `int86` and `intdos`, and `delay`.
- `tests/dos/cwin.c`: `<cwin.h>` windows, messages, controls, menus,
  painting into video memory, and a modal dialog and a message box
  driven by keys (in small at `-Os` and in the far-code models). The graphics and window
  examples in `examples/dos` are built but not run, as they need a
  display; [tests/dos/README.md](tests/dos/README.md) shows how to run
  them in DOSBox. `NO_DOSLIBS=1` skips them.
- `tests/builtins/rtt.c`: 32 and 64-bit integer and soft float builtins
  against results computed on the host by `tests/builtins/gen.py`.

The resident program test runs in DOSBox, which keeps programs resident
as DOS does (emu2 does not):

```sh
BIN=~/llvm-build/bin SYSROOT=$PWD/sysroot LD=/path/to/jwlink \
sh tests/run-tsr-tests.sh
```

`tests/tsr/tsr.c` hooks INT 60h and the clock (INT 1Ch, chained) and stays
resident with `_dos_keep(0, 0)`, in each model at `-O0` and `-Os`; then
`tests/tsr/check.c` fills all the free DOS memory, which would overwrite
the resident program if it kept too little, and calls INT 60h twice.

The 16-bit Windows tests, which run in Wine, are in clang-dos/win16-sdk
(`tests/run-tests.sh`), and the 32-bit Windows ones in clang-dos/win32-sdk.

## Size benchmark

`bench/size/run.sh` builds a few C and C++ programs (printf and iostream
hello worlds, strings and vectors, virtual classes and templates,
exceptions) with clang-dos, Open Watcom 2, Digital Mars C++ and gcc-ia16 in
the tiny, small and large models, runs them in emu2, and records the file,
code and data sizes. See `bench/size/README.md`.

## Toolchain packages and CI

`packaging/` builds the toolchain packages (clang, lld, the LLVM tools,
JWlink and every sysroot, for Linux and Windows hosts), and GitHub Actions
runs it:

- `.github/workflows/toolchain.yml` builds both packages and runs
  `packaging/smoke-test.sh` on them in emu2 and Wine, for every push to
  `main` and every pull request here. clang-dos/llvm, picolibc, JWlink,
  win16-sdk and win32-sdk call it for their own pull requests, with their commit in place of the
  default branch. Run it by hand (Actions > Toolchain > Run workflow) for
  other branches; the packages are the run's artifacts.
- `.github/workflows/release.yml` publishes them: a tag `v*` pushed here
  makes a release of that name, and the `nightly` pre-release is rebuilt
  every night when any of the eight repositories has changed. Besides the
  packages it has each sysroot alone (`clang-dos-sysroot-NAME.tar.xz`).

### Using the nightly instead of building

Other repositories' CI, and anyone who only needs the compiler, can take
the nightly instead of building the suite. The nightly's assets are
replaced in place, so its links (such as
`https://github.com/clang-retro/dos-sdk/releases/download/nightly/clang-dos-linux-x86_64.tar.xz`)
do not change.

```yaml
# In a workflow: unpacks the Linux package, puts bin on PATH and sets
# $CLANG_DOS to it.
- uses: clang-retro/dos-sdk/.github/actions/setup-toolchain@main
- run: clang --config=winxp.cfg -O2 hello.c -o hello.exe
# Only some sysroots, into a directory of your choice:
- uses: clang-retro/dos-sdk/.github/actions/setup-toolchain@main
  with:
    sysroots: i8086-msdos,win2k-xp
    path: sysroots
```

```sh
packaging/get-toolchain.sh ~/clang-dos                  # the Linux package
packaging/get-toolchain.sh -H windows ~/clang-dos-win   # the Windows one
packaging/get-toolchain.sh -s win2k-xp,i8086-msdos ~/sysroots
packaging/get-toolchain.sh -t v0.2.0 ~/clang-dos        # a release
```

Each checks the files against the release's `SHA256SUMS`.

The LLVM build is the slow part, about two hours for Linux and three for
Windows on GitHub's runners. Each LLVM commit is built once per host and
kept in the Actions cache, and ccache makes a new commit quick. A cold
Windows build that does not fit in one job stops in time to save its
ccache, so running the workflow again finishes it.

The scripts also work outside CI, one step each:

```sh
LLVM_SRC=~/llvm PREFIX=$PWD/llvm-linux sh packaging/build-llvm.sh linux
LLVM_SRC=~/llvm PREFIX=$PWD/llvm-windows sh packaging/build-llvm.sh windows
JWLINK_SRC=~/jwlink sh packaging/build-jwlink.sh linux bin/jwlink
JWLINK_SRC=~/jwlink sh packaging/build-jwlink.sh windows bin/jwlink.exe
BIN=$PWD/llvm-linux/bin LLVM_SRC=~/llvm PICOLIBC_SRC=~/picolibc \
  WIN16_SDK=~/win16-sdk WIN32_SDK=~/win32-sdk \
  MINGW_SRC=~/mingw-w64 LIBUNICOWS_SRC=~/libunicows-1.1.2 \
  sh packaging/build-sysroot.sh i8086-msdos sysroots/i8086-msdos   # and so on
LLVM_PREFIX=$PWD/llvm-linux JWLINK=bin/jwlink SYSROOTS=$PWD/sysroots \
  LLVM_SRC=~/llvm PICOLIBC_SRC=~/picolibc JWLINK_SRC=~/jwlink \
  WIN16_SDK=~/win16-sdk WIN32_SDK=~/win32-sdk \
  sh packaging/assemble.sh linux clang-dos-linux-x86_64
```

The DOS sysroot is built with the JWlink in `BIN`, so put `jwlink` there
first. `packaging/README.md` is the README inside the package.

## License

The clang-retro DOS SDK is free software under the GNU Affero General Public
License, version 3 only (`AGPL-3.0-only`); the full text is in
[LICENSE](LICENSE). If you distribute a program built with it, or let
people use a modified version over a network, you must offer the
program's complete source under the same license.

These parts come from other projects, or are meant to go back to one,
and keep their own licenses:

- `curses/` is PDCurses 3.9, in the public domain, except
  `curses/demos/rain.c` and `curses/demos/worm.c`, which are under the
  ncurses license (see [curses/README.md](curses/README.md)).
- `tvision/` is the port of Turbo Vision, which is under the MIT license
  with Borland's disclaimer for the original code; the port's files are
  under that license too, like the library they build into.
- `runtime/unwind-sjlj.c` is under Apache-2.0 WITH LLVM-exception, like
  LLVM's own unwinder, so it can go upstream.

A commercial license, for programs that cannot meet those terms, is
available from the clang-retro project; ask through
[the clang-retro organization on GitHub](https://github.com/clang-retro).

Contributions are accepted under the Contributor License Agreement in
[CONTRIBUTING.md](CONTRIBUTING.md), which lets the project offer the
code under both licenses.
