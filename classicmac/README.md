# Classic Mac OS

Tools for building PowerPC applications for Mac OS 7.5 to 9.2 with clang's
`powerpc-apple-macos9` target (clang-dos/llvm#12). The compiler makes
AIX-style XCOFF objects; `peflink.py` links them into a PEF application
that the Code Fragment Manager loads.

## Hello, Mac OS 9

```sh
clang --target=powerpc-apple-macos9 -O2 -c examples/hello.c -o hello.o
python3 peflink.py -o Hello.bin --name Hello hello.o
hmount xfer.img && hcopy -m Hello.bin :Hello && humount
```

`examples/hello.c` sets up the Toolbox, opens a window, draws two lines
of text and waits for a click. It declares the few Toolbox calls it needs
itself, since there are no Mac headers in the SDK yet.

## peflink.py

Reads XCOFF objects and writes one PEF container:

- The code section holds every text csect, plus a 24-byte glue stub for
  each imported function. A call `bl .Name` to an import goes to the stub,
  and the `nop` after it becomes `lwz r2,20(r1)` to restore the TOC.
- The data section holds the data csects, then the TOC (the compiler's
  TC entries, then one slot per imported function), then zeroed bss.
- The loader section lists the imported libraries and symbols, and the
  relocations that fix up data words to the code section, the data
  section, or an import.
- `main`'s function descriptor is the entry point.

The output is MacBinary II with a `cfrg` 0 resource (one application
fragment in the data fork) and a `SIZE` -1 resource (2 MB by default;
`--size KB`). `--pef FILE` also writes the bare container, and `--carbon`
resolves names against CarbonLib first.

Not done yet: archives (`.a`), static constructors, exports (shared
libraries), and pattern-compressed data.

## 68k applications

`m68k/codelink.py` links the ELF objects that clang's `m68k-apple-macos`
target makes into a 68k application for System 7 and Mac OS 8:

```sh
clang --target=m68k-apple-macos -c m68k/start.s -o start.o
clang --target=m68k-apple-macos -O2 -c examples/hello68.c -o hello68.o
python3 m68k/codelink.py -o Hello68.bin start.o hello68.o
```

The whole program (code, data, zeroed bss) goes into one CODE 1
resource, linked at address 0, followed by a table of the absolute
pointers in it. CODE 0 is a one-entry jump table. At launch, `start.s`
adds the load address to each pointer, flushes the instruction cache,
and calls `main`. Code reaches data PC-relative, so a program must stay
within 32 KB until the compiler gets a large code model.

`examples/hello68.c` declares its Toolbox calls the way Apple's headers
do. `pascal` selects the Toolbox calling convention (arguments pushed
left to right, the callee pops them, the result in a slot above them),
and `__attribute__((raw_inline(0xA86E)))`, which `ONEWORDINLINE` wraps,
makes a call emit the trap word instead of a `jsr`. clang reserves A5
for the application globals.

### Checking without a Mac

`m68k/sim68k.py` runs an application on a 68k CPU emulator (the
`unicorn` Python package) and prints each Toolbox trap it makes, with
its arguments. Only the traps it knows are faked, and they only log, so
this checks code generation, relocation and stack balance, not drawing:

```sh
pip install unicorn
python3 m68k/sim68k.py Hello68.bin
```

### On System 7.6.1 and Mac OS 8.1

`qemu/boot-q800.sh` boots QEMU's `q800` machine straight from a copy of
the install CD, with the app copied onto it by hfsutils, so there is
nothing to install. It needs a Quadra 800 ROM image, which the SDK
cannot supply. `examples/hello68.c` runs and quits cleanly on both.

## Import names

`peflink.py` looks up which shared library exports each name in
`imports.json`. There are two ways to make that file.

**From a Mac OS install (our own tool).** `macos-exports.py` reads the
real libraries. It finds each library through its `cfrg` resource and
reads the export table from its PEF loader section. On Mac OS 8.5 and
later, InterfaceLib and the other core libraries are in the ROM image
inside the "Mac OS ROM" file. The tool unpacks that image itself (the
parcels, then LZSS) and finds the libraries among the ROM's `nlib` and
`ncod` resources:

```sh
qemu-img convert -O raw hd.qcow2 hd.raw
7z x hd.raw untitled.hfs          # the Mac OS partition
# A Mac OS 9 volume is HFS+ inside an HFS wrapper: cut out the embedded
# HFS+ volume first (its place is in the wrapper's master directory block)
7z x plus.img 'untitled/System Folder/*'
python3 macos-exports.py imports.json 'untitled/System Folder'
```

On Mac OS 8.1 and earlier, the System file keeps some libraries as
`nlib` resources, which are read too. InterfaceLib itself is in the
machine's ROM on those systems, so pass a ROM image file as well (not
supported yet).

By default it keeps only the libraries that Apple's Universal Interfaces
link against; `--all` keeps every library it finds, including drivers
and third-party ones. You can pass several System Folders to merge them.

**From Retro68 (the checked-in file).** `gen-imports.py` builds the list
from Retro68's `ImportLibraries/lib*.a`. Those archives hold only symbol
names (from Apple's Universal Interfaces 3.4.1), and only the names are
read:

```sh
python3 gen-imports.py llvm-nm Retro68/ImportLibraries imports.json
```

The two lists differ a little. A list from 9.2.1 has what that system
really exports: newer calls that the 3.4.1 stubs lack, but not the 21
Component Manager calls that only CarbonLib exports on 9.2.1. Linking
`examples/hello.c` gives the same PEF with either list.

InterfaceLib wins a name that other libraries also export. CarbonLib and
`Apple;Carbon;Networking` are imported weakly.

## Testing in QEMU

`qemu/boot.sh` boots Mac OS 9 on QEMU's mac99 machine, with a second HFS
disk (`xfer.img`) for passing files in. `qemu/mon.py` drives the guest
through the QEMU monitor: screenshots, keys, and clicks. The guest only
has a relative USB mouse, so it finds the arrow cursor on the screen and
steers it to the target.

To make the transfer disk, add a blank raw image as the second drive and
initialize it in Drive Setup with Custom Setup: 1 partition, type Mac OS
Standard. hfsutils cannot write Mac OS Extended.
