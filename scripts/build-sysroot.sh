#!/bin/sh
# Build a complete i8086-msdos sysroot: picolibc, compiler-rt builtins and
# libc++ for each memory model, the graphics and tui libraries, and Turbo
# Vision when TVISION_SRC is set. See build-picolibc.sh, build-builtins.sh,
# build-libcxx.sh, build-doslibs.sh and build-tvision.sh for the variables
# (LLVM_SRC, PICOLIBC_SRC, TVISION_SRC, BIN, SYSROOT, MODELS). NO_CXX=1 skips
# libc++ (and Turbo Vision), and NO_WIN16=1 the 16-bit Windows libraries
# (build-win16.sh).
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
[ -n "$NO_WIN16" ] || sh "$here/build-win16.sh"
