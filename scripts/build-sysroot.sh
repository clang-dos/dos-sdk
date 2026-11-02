#!/bin/sh
# Build a complete i8086-msdos sysroot: picolibc, compiler-rt builtins and
# libc++ for each memory model, the graphics and tui libraries, and Turbo
# Vision when TVISION_SRC is set. See build-picolibc.sh, build-builtins.sh,
# build-libcxx.sh, build-doslibs.sh and build-tvision.sh for the variables
# (LLVM_SRC, PICOLIBC_SRC, TVISION_SRC, BIN, SYSROOT, MODELS). NO_CXX=1 skips
# libc++ (and Turbo Vision). The 16-bit Windows libraries come last, from
# clang-dos/win16-sdk: WIN16_SDK names its checkout (default: ../win16-sdk
# next to this one), and NO_WIN16=1 skips them.
#
# clang finds the sysroot with --sysroot=, or at <clang dir>/../i8086-msdos.
set -e
here=$(dirname "$0")
sh "$here/build-picolibc.sh"
sh "$here/build-builtins.sh"
[ -n "$NO_CXX" ] || sh "$here/build-libcxx.sh"
sh "$here/build-doslibs.sh"
# Turbo Vision is only for the models with far code; MODELS here is for the
# other libraries.
if [ -z "$NO_CXX" ] && [ -n "$TVISION_SRC" ]; then
  MODELS= sh "$here/build-tvision.sh"
fi
if [ -z "$NO_WIN16" ]; then
  WIN16_SDK=${WIN16_SDK:-$here/../../win16-sdk}
  [ -f "$WIN16_SDK/build-sysroot.sh" ] || {
    echo "set WIN16_SDK to a clang-dos/win16-sdk checkout, or NO_WIN16=1" >&2
    exit 1
  }
  DOS_SDK=$(cd "$here/.." && pwd) sh "$WIN16_SDK/build-sysroot.sh"
fi
