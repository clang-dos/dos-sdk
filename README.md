# clang-dos SDK

Scripts and tests for building and checking a DOS sysroot for the
`i8086-pc-msdos` target of the [clang-dos LLVM toolchain](https://github.com/clang-dos/llvm).

A sysroot holds, for each memory model:

```
<sysroot>/include/                       picolibc headers
<sysroot>/include/c++/v1/                libc++ headers
<sysroot>/lib/<model>/crt0.obj           startup code
<sysroot>/lib/<model>/libc.a             picolibc with the DOS layer
<sysroot>/lib/<model>/libclang_rt.builtins.a
<sysroot>/lib/<model>/libc++.a           libc++
<sysroot>/lib/<model>/libc++abi.a        libc++abi
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

`huge16` is the huge model with a 16-bit `size_t`, as in Open Watcom:
build with `-mcmodel=huge -msize-t=16`, and clang links the libraries in
`lib/huge16`. Pointers are huge and `ptrdiff_t` stays 32 bits, but objects
stay under 64 KiB. The heap still takes all free DOS memory, in objects
of up to 64 KiB each.

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

C++ programs build with `-fno-exceptions -fno-rtti` by default, and libc++
has no threads, locales, wide characters, iostreams or `<filesystem>`.
Containers, strings, algorithms, smart pointers, `<functional>`,
`std::to_string` and `std::stoi` work. The linker drops unused functions
(clang passes `option eliminate`), so a program only pays for the parts
of libc++ it uses: the C++ test is about 41 KB of code at `-Os`. An
unoptimized C++ program does not fit in the 64 KiB of code that the tiny
and compact models have.

libc++ is built for every model. With `-frtti`, `dynamic_cast` and
`typeid` work too. In the models with far code or far data, a vtable
entry is 32 bits, and a member function pointer holds a far code pointer
where code is far. In the far-data models, vtables and type_info objects
are far data too, outside DGROUP. The compact, large, huge and huge16
builds of libc++abi leave out `__cxa_demangle`, whose code does not fit in
64 KiB with far data pointers.

Files opened without `b` in the `fopen` mode, or without `O_BINARY`, are
text files: line ends are CR LF on disk and Ctrl-Z ends the file. See
`libos/msdos/README.md` in picolibc.

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
- [emu2](https://github.com/dmsc/emu2) to run the tests.

## Building a sysroot

```sh
LLVM_SRC=~/llvm-project PICOLIBC_SRC=~/picolibc BIN=~/llvm-build/bin \
SYSROOT=$PWD/sysroot sh scripts/build-sysroot.sh
```

`MODELS="small"` builds one model only. `WORK=` sets the build directory
(default `./build`). `NO_CXX=1` skips libc++. The parts run on their own,
in this order, as `scripts/build-picolibc.sh`, `scripts/build-builtins.sh`
and `scripts/build-libcxx.sh`.

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
- `tests/cxx/cxx.cpp`: virtual functions, `vector`, `unique_ptr`,
  `shared_ptr`, `std::function`, `string`, `map`, `sort`, function-local
  statics and global constructors and destructors (not built for tiny at
  `-O0`).
  `NO_CXX=1` skips it.
- `tests/builtins/rtt.c`: 32 and 64-bit integer and soft float builtins
  against results computed on the host by `tests/builtins/gen.py`.
