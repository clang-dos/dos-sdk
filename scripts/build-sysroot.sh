#!/bin/sh
# Build a complete i8086-msdos sysroot: picolibc, compiler-rt builtins and
# libc++ for each memory model. See build-picolibc.sh, build-builtins.sh and
# build-libcxx.sh for the variables (LLVM_SRC, PICOLIBC_SRC, BIN, SYSROOT,
# MODELS). NO_CXX=1 skips libc++.
#
# clang finds the sysroot with --sysroot=, or at <clang dir>/../i8086-msdos.
set -e
here=$(dirname "$0")
sh "$here/build-picolibc.sh"
sh "$here/build-builtins.sh"
[ -n "$NO_CXX" ] || sh "$here/build-libcxx.sh"
