# Mac OS X on PowerPC: tests in QEMU

`tests/run-macosx-tests.sh` builds tests with clang for
`powerpc-apple-macosx10.4`, links them with Apple's ld64 against the 10.4
SDK, and runs them on Mac OS X 10.4 (Tiger) in QEMU. With
`OSX_VERSION=10.0` it does the same for Mac OS X 10.0 (see the end of this
file).

## What you need

- clang from clang-dos/llvm with PowerPC Darwin support.
- Apple's assembler and linker for PowerPC: `scripts/build-cctools-ppc.sh`.
- The Mac OS X 10.4 SDK: `MacOSX10.4.0.sdk` (Xcode 2.0) or
  `MacOSX10.4u.sdk` (Xcode 2.5 or 3.1). Apple's license does not allow us
  to ship it. Xcode's `MacOSX10.4SDK.pax.gz` unpacks with
  `gzip -dc MacOSX10.4SDK.pax.gz | cpio -idm --no-absolute-filenames`
  into `Developer/SDKs/MacOSX10.4.0.sdk`.
- `qemu-system-ppc`, `dosfstools` and `mtools`
  (`apt install qemu-system-ppc dosfstools mtools`).
- A disk image with Mac OS X 10.4 for PowerPC installed and the test
  runner from `macosx/guest` set up in it (below).

## Making the guest image

1. If the DVD image is a bare HFS+ volume (`file` says "Apple HFS Plus"
   rather than showing a partition map), OpenBIOS can't boot it. Wrap it
   first: `python3 macosx/apm-wrap.py tiger.iso dvd.img`.

2. Boot the installer:

       qemu-img create -f qcow2 tiger.qcow2 20G
       qemu-system-ppc -M mac99,via=pmu -cpu G4 -m 1024 \
         -drive file=tiger.qcow2,format=qcow2,media=disk \
         -drive file=dvd.img,format=raw,media=cdrom -boot d

   Use Utilities > Disk Utility to erase the 20 GB disk (Mac OS Extended),
   then install. The install takes about 30 minutes without KVM.

3. When the installer has finished (it restarts into the DVD again),
   open Utilities > Terminal, which runs as root, and add the runner to
   the installed volume (`/Volumes/Untitled` unless you renamed it):

       T=/Volumes/Untitled
       mkdir -p $T/usr/local/bin
       # put macosx/guest/clangdos-testrunner in $T/usr/local/bin and
       # org.clang-dos.testrunner.plist in $T/Library/LaunchDaemons,
       # for example by pasting them into  cat > FILE <<'EOF'
       chmod 755 $T/usr/local/bin/clangdos-testrunner
       chmod 644 $T/Library/LaunchDaemons/org.clang-dos.testrunner.plist
       $T/usr/sbin/chown root:wheel $T/Library/LaunchDaemons/org.clang-dos.testrunner.plist
       touch $T/var/db/.AppleSetupDone

   The last line skips the Setup Assistant: the tests need no user
   account, since the runner starts from launchd before anyone logs in.
   The installer can't mount FAT disks, so the files can't come from one
   here. On the installed system, `macosx/guest/install.sh` does the same
   from a FAT disk.

4. Quit QEMU. The disk image is now the test image.

Driving the installer headless: QEMU's monitor `screendump` gives
screenshots and `sendkey` types. 10.4 does not follow `usb-tablet`
absolute positions, so move the USB mouse (`mouse_set` to it) in small
relative steps and check where the pointer landed.

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

## Mac OS X 10.0

Set `OSX_VERSION=10.0`, point `SDK` at a 10.0 sysroot and `OSX_IMAGE` at a
10.0 image. The same 36 tests pass.

### The sysroot

There was no SDK for 10.0; the files come from three places:

- headers and `usr/lib/libcc_dynamic.a` from the Developer Tools CD
  (`Developer.pax.gz` in its `Developer.pkg`),
- `usr/lib/crt1.o`, `dylib1.o`, `bundle1.o`, `gcrt1.o` and
  `System/Library/Frameworks` from `Essentials.pax.gz` on the install CD,
- `usr/lib/libSystem.B.dylib` and `libSystem.dylib` from the root of the
  install CD.

The 10.0 libSystem has no `strtoll` or `gmtime_r`, and its `scanf` reads
`%lld` as a 32-bit long, so `tests/macosx/abi.c` uses `strtoq`, `gmtime`
and `%ld` there.

For 10.0 the clang driver passes ld64 `-commons use_dylibs`, because
10.0's `crt1.o` has common symbols, such as `__cthread_init_routine`, that
libSystem defines too, and 10.0's dyld will not run a program that defines
them twice. It links `libcc_dynamic.a` in place of `libgcc_s.1.dylib`,
which 10.0 does not have. Other ld64 output (two-level namespace,
`LC_UUID`, `LC_VERSION_MIN_MACOSX` and so on) runs on 10.0 as it is.

### The guest image

The 10.0 installer hangs in QEMU before its window appears, so install by
hand from single-user mode:

1. Make a 4 GB disk with an HFS+ volume behind a partition map:

       truncate -s 4G vol.img && mkfs.hfsplus -v MacOSX vol.img
       python3 macosx/apm-wrap.py vol.img disk.raw
       qemu-img convert -O qcow2 disk.raw osx100.qcow2

   The installer has no gzip, so unpack the packages for it, each onto a
   raw disk of its own:

       zcat Essentials.pax.gz >ess.raw; zcat BSD.pax.gz >bsd.raw

2. Boot the CD in single-user mode on the cuda machine, which has an ADB
   keyboard (the keyboard did not work with `via=pmu`):

       qemu-system-ppc -M mac99 -cpu G4 -m 512 \
         -drive file=osx100.qcow2,format=qcow2,media=disk \
         -drive file=install.iso,format=raw,media=cdrom \
         -drive file=ess.raw,format=raw,media=disk,snapshot=on \
         -boot d -prom-env 'boot-args=-v -s'

3. At the `localhost#` prompt (the hard disk is `disk0`, the raw package
   disk `disk1`):

       /sbin/fsck_hfs -y /dev/rdisk0s2
       /sbin/mount_hfs /dev/disk0s2 /tmp
       cd /tmp && pax -r -pe -f /dev/rdisk1

   `pax` may stop at the end of the disk instead of returning; once the
   disk is quiet, Control-C is fine. Reboot with `bsd.raw` in place of
   `ess.raw` and unpack that the same way. Then copy the base system,
   which the installer takes from the CD itself, and add BootX:

       /tmp/usr/bin/ditto -bom /System/Library/CoreServices/BaseSystem.bom / /tmp
       cp /usr/standalone/ppc/bootx.bootinfo /tmp/System/Library/CoreServices/BootX
       echo LANGUAGE=English >/tmp/var/log/CDIS.custom
       mkdir -p /tmp/Library/StartupItems/ClangDosTest

   10.0's `/etc/rc` reads `/var/log/CDIS.custom` and stops if it is
   missing. Put the two files from `macosx/guest-10.0/ClangDosTest` in
   that folder (type them in with `cat > FILE <<'EOF'`),
   `chmod 755` the script, and `umount /dev/disk0s2`.

4. The volume is not blessed, so boot it by naming BootX:

       qemu-system-ppc -M mac99 -cpu G4 -m 512 \
         -drive file=osx100.qcow2,format=qcow2,media=disk -boot c \
         -prom-env 'boot-device=hd:,\System\Library\CoreServices\BootX'

10.0 has no launchd, so the runner is a StartupItem that SystemStarter
runs after the Disks item has mounted the `CDTEST` disk. It waits 30
seconds for it, then does the same as the 10.4 runner.
