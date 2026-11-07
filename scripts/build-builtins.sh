#!/bin/sh
# Build compiler-rt builtins for i8086-msdos, one library per memory model,
# into $SYSROOT/lib/<model>/libclang_rt.builtins.a.
#   LLVM_SRC  llvm-project checkout     BIN  directory with clang and llvm-ar
#   SYSROOT   output sysroot            WORK build directory (default: ./build)
#   OS        msdos (default), win16 or os2 for $SYSROOT/lib/OS/<model>;
#             win16 also gets lib/win16/<model>/dll for DLLs in the small
#             and medium models
set -e
: "${LLVM_SRC:?set LLVM_SRC}" "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
WORK=${WORK:-$PWD/build}
OS=${OS:-msdos}
if [ "$OS" = win16 ] || [ "$OS" = os2 ]; then
  libdir=$SYSROOT/lib/$OS tag=-$OS
else
  libdir=$SYSROOT/lib tag=
fi
# The compiler flags for a memory model. huge16 is the huge model with a
# 16-bit size_t, as in Open Watcom.
model_flags() {
  case $1 in
    huge16) echo "-mcmodel=huge -msize-t=16" ;;
    *) echo "-mcmodel=$1" ;;
  esac
}
for m in ${MODELS:-tiny small medium compact large huge huge16}; do
variants=normal
if [ "$OS" = win16 ]; then
  case $m in small|medium) variants="normal dll" ;; esac
fi
for v in $variants; do
  if [ $v = dll ]; then
    b=builtins$tag-dll-$m dest=$libdir/$m/dll flags=-mwindows-dll
  else
    b=builtins$tag-$m dest=$libdir/$m flags=
  fi
  # The builtins need nothing from LLVM's CMake package, so
  # LLVM_RUNTIMES_BUILD=ON skips looking for it. Otherwise a standalone
  # build looks it up anywhere on the system: on GitHub's runners it finds
  # LLVM 18 and imports its shared-library targets, which CMake warns
  # about ten times per build on a target without shared libraries, and
  # with no LLVM installed it warns that the package is missing.
  cmake -G Ninja -S "$LLVM_SRC/compiler-rt/lib/builtins" -B "$WORK/$b" \
    -DLLVM_RUNTIMES_BUILD=ON \
    -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$BIN/clang" -DCMAKE_ASM_COMPILER="$BIN/clang" \
    -DCMAKE_C_COMPILER_TARGET=i8086-pc-$OS \
    -DCMAKE_ASM_COMPILER_TARGET=i8086-pc-$OS \
    -DCMAKE_C_FLAGS="$(model_flags $m) $flags" -DCMAKE_ASM_FLAGS="$(model_flags $m)" \
    -DCMAKE_AR="$BIN/llvm-ar" -DCMAKE_RANLIB="$BIN/llvm-ranlib" \
    -DCMAKE_NM="$BIN/llvm-nm" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON -DCOMPILER_RT_BAREMETAL_BUILD=ON \
    -DCOMPILER_RT_OS_DIR=msdos >/dev/null
  ninja -C "$WORK/$b" >/dev/null
  mkdir -p "$dest"
  cp "$WORK/$b/lib/msdos/libclang_rt.builtins-i8086.a" \
     "$dest/libclang_rt.builtins.a"
done
done
echo "builtins installed in $libdir/<model>"
