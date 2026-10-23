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

The models built so far are `tiny` (.COM programs) and `small` (.EXE
programs). clang links `crt0.obj`, `libc.a` and the builtins by default:

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
unoptimized C++ program does not fit in a 64 KiB .COM file.

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
