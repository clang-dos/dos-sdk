# Choosing a memory model

This guide picks a memory model for common kinds of DOS and 16-bit Windows
programs and gives the clang command for each. The [README](../README.md)
describes what each model does; this page is about which one to use.

All commands assume `--sysroot=<sysroot>` (or a sysroot in
`<clang dir>/../i8086-msdos`). Every object file and library in a program
must be built for the same model: clang picks the libraries in
`lib/<model>` from `-mcmodel=`, so pass the same flags when compiling and
when linking.

## Quick pick

| Your program | Model | Flags |
| --- | --- | --- |
| A small tool, under 64 KiB in all | tiny | `-mcmodel=tiny -Os -o tool.com` |
| An ordinary DOS program | small | (none: small is the default) |
| A resident program (TSR) or interrupt handler | tiny or small | `-mcmodel=tiny`, `__loadds` on handlers |
| Lots of code, data that fits in 64 KiB | medium | `-mcmodel=medium` |
| Little code, lots of data | compact | `-mcmodel=compact` |
| Lots of code and lots of data | large | `-mcmodel=large` |
| Single arrays or objects over 64 KiB | huge | `-mcmodel=huge` |
| Open Watcom huge-model code (16-bit `size_t`) | huge16 | `-mcmodel=huge -msize-t=16` |
| A few big tables in a small program | small + `__far` | `int __far table[20000];` |
| One big buffer, any model but tiny | `halloc` | `long __huge *a = halloc(n, sizeof(long));` |
| C++ with containers and strings | small, `-Os` | `clang++ -Os` |
| C++ built without optimization | medium | `clang++ -mcmodel=medium -O0` |
| C++ with the full iostreams, `<filesystem>` | medium or large | `clang++ -mcmodel=medium -Os` |
| A Windows 3.x program | small or medium | `--target=i8086-pc-win16` |
| A Windows 3.x DLL | large (or compact) | `--target=i8086-pc-win16 -mcmodel=large -shared` |

If you are not sure, start with small. When the linker says a segment is
too big, read the message: too much code in `_TEXT` means a model with far
code (medium, large); too much in DGROUP means far data (compact, large) or
`__far` variables.

## The models in one table

| Model | Code pointers | Data pointers | Code limit | Data limit | `size_t` | Output |
| --- | --- | --- | --- | --- | --- | --- |
| tiny | near | near | 64 KiB for code, data, heap and stack together | (shared with code) | 16 bits | .COM |
| small | near | near | 64 KiB | 64 KiB DGROUP (globals, stack, heap) | 16 bits | .EXE |
| medium | far | near | 64 KiB per function; any number of segments | 64 KiB DGROUP | 16 bits | .EXE |
| compact | near | far | 64 KiB | DGROUP for small globals and the stack, a 64 KiB heap segment, big variables in segments of their own | 16 bits | .EXE |
| large | far | far | as medium | as compact | 16 bits | .EXE |
| huge | far | huge | as medium | objects over 64 KiB; the heap takes all free DOS memory | 32 bits | .EXE |
| huge16 | far | huge | as medium | objects under 64 KiB each; the heap takes all free DOS memory | 16 bits | .EXE |

Far pointers cost speed and size: every access through one loads a segment
register, and in compact the same C++ code is about a third larger than in
small. Huge pointers cost more again, since arithmetic on them carries into
the segment. Pick the smallest model that holds the program.

## Situations

### A small utility

A command-line tool, a file filter or a little game that fits in 64 KiB in
all: build a .COM program with the tiny model.

```sh
clang --target=i8086-pc-msdos -mcmodel=tiny -Os tool.c -o tool.com
```

Tiny has one segment for code, data, heap and stack. The stack is at the
top of the segment, and the heap ends 2 KiB below it. A .COM has no stack
segment, so `-Wl,option,stack=` has no effect, and `__far`, `__huge` and
`halloc` are not available (they need segment fixups, which a .COM does
not have). When the program outgrows 64 KiB, move to small: the source
needs no changes.

### An ordinary DOS program

Most programs fit the small model: up to 64 KiB of code and 64 KiB of
data, heap and stack. It is the default, and its pointers are the
cheapest.

```sh
clang --target=i8086-pc-msdos -Os prog.c -o prog.exe
```

Use `-Os` unless you are debugging: it often makes the difference between
fitting and not fitting.

### A resident program (TSR) or interrupt handler

Keep a resident program as small as you can, since every byte stays in
memory after it exits: use tiny, or small when it outgrows 64 KiB.

```sh
clang --target=i8086-pc-msdos -mcmodel=tiny -Os tsr.c -o tsr.com
```

Mark each function that DOS, the BIOS or another program calls with
`__loadds`: it loads DGROUP into `DS` on entry and restores the caller's
`DS` on return, so it works whatever `DS` the caller had. In tiny it
loads `CS`, which is DGROUP there.

```c
void __loadds on_tick(void);   /* called from the interrupt entry */
```

The SDK has no `__interrupt` keyword and no `_dos_keep`, so the entry
point that an interrupt vector points to (which must end with `IRET`), and
the INT 21h function 31h call that stays resident, need a few lines of
assembly that call your C code.

### A big code base with little data

An editor, a compiler or a game engine with more than 64 KiB of code but
modest data: use medium. Each source file gets its own code segment (and
`<file>_TEXT_1` and so on when one file is over 64 KiB), while data
pointers stay near and fast.

```sh
clang --target=i8086-pc-msdos -mcmodel=medium -Os *.c -o big.exe
```

A single function is still at most 64 KiB. A function marked `__near` in
medium can only be called from its own source file; use it for small
static helpers you want to call cheaply.

### Little code, lots of data

A data-heavy tool whose code fits in 64 KiB but whose tables, buffers or
heap do not fit in DGROUP: use compact. Data pointers are far; small
globals and the stack stay in DGROUP, the heap is a 64 KiB segment of its
own, and any global or static variable over 32767 bytes goes in a segment
of its own outside DGROUP.

```sh
clang --target=i8086-pc-msdos -mcmodel=compact -Os data.c -o data.exe
```

`-mlarge-data-threshold=N` moves the 32767-byte limit (in compact, large,
huge and huge16); `-mlarge-data-threshold=0` keeps everything in DGROUP.

### Lots of code and lots of data

Use large: far code and far data, the combination of medium and compact.

```sh
clang --target=i8086-pc-msdos -mcmodel=large -Os *.c -o app.exe
```

`malloc` still gives at most 64 KiB in all, from one heap segment. For
more heap, use `halloc` (below) or the huge model.

### Arrays or objects over 64 KiB

When one array or structure is over 64 KiB, or you want `malloc` to reach
all free DOS memory, use huge. `size_t` and `ptrdiff_t` are 32 bits and
pointer arithmetic carries into the segment, so `big[100000]` just works,
at the cost of slower pointers everywhere.

```sh
clang --target=i8086-pc-msdos -mcmodel=huge -Os sim.c -o sim.exe
```

Code written for Open Watcom's huge model expects a 16-bit `size_t`; build
it as huge16. Pointers are still huge and the heap still takes all free
memory, but each object is under 64 KiB.

```sh
clang --target=i8086-pc-msdos -mcmodel=huge -msize-t=16 -Os port.c -o port.exe
```

### One or two big buffers in an otherwise small program

You do not need a bigger model for a few large items.

- A table that does not fit in DGROUP: declare it `__far` in small or
  medium. Pass it to your own functions through `__far` pointers; the C
  library takes near pointers in these models, so don't hand it to
  `strlen` or `memcpy` (the compiler expands copies and fills of far data
  inline).

  ```c
  int __far table[20000];
  ```

- A buffer over 64 KiB: allocate it with `halloc`, in any model but tiny.
  The blocks come from DOS, up to 1 MiB in all, and are zeroed.

  ```c
  #include <malloc.h>
  long __huge *a = halloc(100000, sizeof(long));
  /* ... */
  hfree(a);
  ```

### Deep recursion or big locals

The stack of an .EXE program is 4 KiB. Make it larger at link time:

```sh
clang --target=i8086-pc-msdos -Os parser.c -Wl,option,stack=16384 -o parser.exe
```

In small and medium the stack shares DGROUP with your globals and the
heap, so a bigger stack leaves less heap. In tiny the stack is the top of
the .COM segment and cannot be changed this way.

### C++ with containers and strings

The linker drops what a program does not use, so `vector`, `string`,
`map`, smart pointers and `<functional>` fit in small at `-Os` (the C++
test is about 41 KB of code there).

```sh
clang++ --target=i8086-pc-msdos -Os prog.cpp -o prog.exe
```

Unoptimized C++ with libc++ does not fit in the 64 KiB of code that tiny
and compact have, and a large program may not fit in small. For debug
builds, use medium:

```sh
clang++ --target=i8086-pc-msdos -mcmodel=medium -O0 -g prog.cpp -o prog.exe
```

Exceptions and RTTI are off by default. `-fexceptions` turns exceptions
on in every model (clang links the libraries in `lib/<model>/eh`), and
`-frtti` gives `dynamic_cast` and `typeid`. `abi::__cxa_demangle` exists
only in medium, large, huge and huge16, and needs about 5 KiB of stack:
link with `-Wl,option,stack=8192`.

### C++ with iostreams

iostreams, string and file streams, wide characters and `<filesystem>`
come with the libc++ build in
[#2](https://github.com/clang-dos/dos-sdk/pull/2); before it, libc++ has
none of them. With it:

- Writing to `std::cout` and `std::cerr` fits in every model with 64 KiB
  of code: a hello world is about 27 KB of code in small at `-Os`, and a
  36 KB .COM in tiny.
- Reading numbers from `std::cin` and string streams fits only in small,
  and only with optimization (about 62 KiB of code at `-Os`). Tiny has
  64 KiB for code and data together, and compact's far pointers make the
  code too large.
- File streams, floating-point I/O, most of iostreams, `<filesystem>` and
  the demangler need a model with far code: medium, or large when the data
  is big too.

```sh
clang++ --target=i8086-pc-msdos -Os hello.cpp -o hello.exe              # cout only
clang++ --target=i8086-pc-msdos -mcmodel=medium -Os tool.cpp -o tool.exe  # full iostreams
```

### Porting code from Open Watcom or Microsoft C

Use the model the code was written for: the models, the `__near`,
`__far` and `__huge` keywords, `halloc`, and the model macros
(`__SMALL__`, `_M_I86SM` and the others) match. Open Watcom's huge model is
huge16 here (`-mcmodel=huge -msize-t=16`). `-march=i186`, `-march=i286`,
`-march=v20` or `-march=v30` lets the compiler use the newer instructions
when the program does not need to run on an 8086.

## 16-bit Windows

`--target=i8086-pc-win16` supports small, medium, compact and large.
Windows runs programs in protected mode, so there is no tiny or huge.
A Windows program gets an 8 KiB stack and a 4 KiB local heap to start
with (the local heap grows).

### A Windows program

Small suits most programs, as on DOS; use medium when the code grows past
64 KiB. Near data keeps `malloc` in the local heap (`LocalAlloc`) and pointers
short.

```sh
clang --target=i8086-pc-win16 -Os app.c app.res -o app.exe
clang --target=i8086-pc-win16 -mcmodel=medium -Os *.c app.res -o app.exe
```

Use compact or large when the data does not fit in 64 KiB: `malloc` then
takes a `GlobalAlloc` block of up to 64 KiB. Allocate more with
`GlobalAlloc` directly.

### A Windows DLL

A DLL runs on the stack of the program that calls it, so `SS` is not
`DS`. Large (or compact) is the simplest choice: all data pointers are far
and carry the right segment, and nothing extra is needed.

```sh
clang --target=i8086-pc-win16 -mcmodel=large -Os -shared mylib.c -o MYLIB.DLL
```

Small and medium DLLs work too, and keep near pointers: clang keeps every
local whose address escapes in a frame in DGROUP, 4 KiB per calling task
by default. A DLL that needs more defines
`unsigned short __win16_dll_frame_size = 8192;`. When you compile a DLL's
objects with `-c`, add `-mwindows-dll`; `-shared` implies it at link time.

```sh
clang --target=i8086-pc-win16 -Os -mwindows-dll -c mylib.c
clang --target=i8086-pc-win16 -Os -shared mylib.o -o MYLIB.DLL
```

### C++ on Windows

C++ works as on DOS, with the same code limits. `-fexceptions` works in
programs of every model and in compact and large DLLs; an exception must
not cross between a DLL and its caller.

## Flag reference

| Flag | What it does |
| --- | --- |
| `-mcmodel=tiny\|small\|medium\|compact\|large\|huge` | The memory model (default small). |
| `-msize-t=16` | With `-mcmodel=huge`: a 16-bit `size_t` (huge16). |
| `-mlarge-data-threshold=N` | Variables over N bytes go outside DGROUP in compact, large and huge (default 32767; 0 keeps all in DGROUP). |
| `-Wl,option,stack=N` | Stack size in bytes (default 4 KiB for DOS .EXE, 8 KiB for Windows programs; not for .COM). |
| `-fexceptions`, `-frtti` | C++ exceptions and RTTI (off by default). |
| `-march=i186\|i286\|v20\|v30` | Use instructions of newer CPUs (default 8086). |
| `-shared` | With `--target=i8086-pc-win16`: build a DLL. |
| `-mwindows-dll` | Compile objects for a small or medium Windows DLL. |
| `-mwindows-prologue=none\|exported\|all` | The Windows entry sequence on far functions (the Windows target uses `exported`). |
