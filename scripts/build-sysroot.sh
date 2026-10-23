#!/bin/sh
# Build a complete i8086-msdos sysroot: picolibc and compiler-rt builtins
# for each memory model. See build-picolibc.sh and build-builtins.sh for
# the variables (LLVM_SRC, PICOLIBC_SRC, BIN, SYSROOT, MODELS).
#
# clang finds the sysroot with --sysroot=, or at <clang dir>/../i8086-msdos.
set -e
here=$(dirname "$0")
sh "$here/build-picolibc.sh"
sh "$here/build-builtins.sh"
