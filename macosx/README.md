# Mac OS X on PowerPC: tests in QEMU

`tests/run-macosx-tests.sh` builds tests with clang for
`powerpc-apple-macosx10.4`, links them with Apple's ld64 against the 10.4
SDK, and runs them on Mac OS X 10.4 (Tiger) in QEMU. This is phase 1 of
the Mac OS X plan; 10.0 comes later.

## What you need

- clang from clang-dos/llvm with PowerPC Darwin support.
- Apple's assembler and linker for PowerPC: `scripts/build-cctools-ppc.sh`.
- The Mac OS X 10.4 SDK, `MacOSX10.4u.sdk` (Xcode 2.5 or 3.1). Apple's
  license does not allow us to ship it.
- `qemu-system-ppc`, `dosfstools` and `mtools`
  (`apt install qemu-system-ppc dosfstools mtools`).
- A disk image with Mac OS X 10.4 for PowerPC installed and the test
  runner from `macosx/guest` set up in it (below).

## Making the guest image

1. Install 10.4 from its DVD:

       qemu-img create -f qcow2 tiger.qcow2 20G
       qemu-system-ppc -M mac99,via=pmu -cpu G4 -m 1024 \
         -drive file=tiger.qcow2,media=disk -cdrom tiger.iso -boot d

   In the installer, erase the disk with Disk Utility (Mac OS Extended),
   install, and make a user. Turning on automatic login is not needed:
   the runner starts from launchd before anyone logs in.

2. Copy `macosx/guest` to a FAT disk, attach it, and install the runner:

       dd if=/dev/zero of=guest.img bs=1M count=8
       mkfs.vfat -n GUEST guest.img
       mcopy -i guest.img macosx/guest/* ::
       qemu-system-ppc -M mac99,via=pmu -cpu G4 -m 1024 \
         -drive file=tiger.qcow2,media=disk \
         -drive file=guest.img,format=raw,media=disk -boot c

   In Terminal: `cd /Volumes/GUEST && sudo sh install.sh`, then shut down.

The runner (`/usr/local/bin/clangdos-testrunner`, started by
`/Library/LaunchDaemons/org.clang-dos.testrunner.plist`) waits up to three
minutes after boot for a disk named `CDTEST` with a `run.sh` on it. If one
appears, it runs the script in `/tmp/cdtest`, writes the output to
`OUT.TXT` and `DONE.TXT` on that disk, and shuts the machine down. With no
such disk it does nothing.

## Running the tests

    BIN=/path/to/llvm/build/bin SDK=/path/to/MacOSX10.4u.sdk \
    OSX_IMAGE=tiger.qcow2 LD=/path/to/cctools-ppc/bin/powerpc-apple-darwin8-ld \
    tests/run-macosx-tests.sh

QEMU runs the image with `snapshot=on`, so the image is not changed. Each
test is built for PIC and `-mdynamic-no-pic`, at `-O0`, `-Os` and `-O2`,
for G3 and G4. QEMU without KVM is slow: allow a few minutes for boot.

The tests:

- `tests/macosx/abi.c` calls into libSystem, which Apple built with gcc:
  varargs with doubles and long longs, struct returns, callbacks from
  `qsort` and `bsearch`, `setjmp`, `errno`, stdio, constructors and
  `atexit`. It also checks struct layout and `bool` size against gcc's.
- `tests/libc/hello.c` and `tests/libc/stdin.c`, shared with the DOS tests.
