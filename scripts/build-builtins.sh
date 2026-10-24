#!/bin/sh
# Build compiler-rt builtins for i8086-msdos, one library per memory model,
# into $SYSROOT/lib/<model>/libclang_rt.builtins.a.
#   LLVM_SRC  llvm-project checkout     BIN  directory with clang and llvm-ar
#   SYSROOT   output sysroot            WORK build directory (default: ./build)
#   OS        msdos (default), or win16 for $SYSROOT/lib/win16/<model>, and
#             lib/win16/<model>/dll for DLLs in the small and medium models
set -e
: "${LLVM_SRC:?set LLVM_SRC}" "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
WORK=${WORK:-$PWD/build}
OS=${OS:-msdos}
if [ "$OS" = win16 ]; then
  libdir=$SYSROOT/lib/win16 tag=-win16
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
  cmake -G Ninja -S "$LLVM_SRC/compiler-rt/lib/builtins" -B "$WORK/$b" \
    -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$BIN/clang" -DCMAKE_ASM_COMPILER="$BIN/clang" \
    -DCMAKE_C_COMPILER_TARGET=i8086-pc-$OS \
    -DCMAKE_ASM_COMPILER_TARGET=i8086-pc-$OS \
    -DCMAKE_C_FLAGS="$(model_flags $m) $flags" -DCMAKE_ASM_FLAGS="$(model_flags $m)" \
    -DCMAKE_AR="$BIN/llvm-ar" -DCMAKE_RANLIB="$BIN/llvm-ranlib" \
    -DCMAKE_NM="$BIN/llvm-nm" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON -DCOMPILER_RT_BAREMETAL_BUILD=ON \
    -DCOMPILER_RT_OS_DIR=msdos -DLLVM_CMAKE_DIR="$BIN/../lib/cmake/llvm" >/dev/null
  ninja -C "$WORK/$b" >/dev/null
  mkdir -p "$dest"
  cp "$WORK/$b/lib/msdos/libclang_rt.builtins-i8086.a" \
     "$dest/libclang_rt.builtins.a"
done
done
echo "builtins installed in $libdir/<model>"
