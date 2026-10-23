#!/bin/sh
# Build compiler-rt builtins for i8086-msdos, one library per memory model,
# into $SYSROOT/lib/<model>/libclang_rt.builtins.a.
#   LLVM_SRC  llvm-project checkout     BIN  directory with clang and llvm-ar
#   SYSROOT   output sysroot            WORK build directory (default: ./build)
set -e
: "${LLVM_SRC:?set LLVM_SRC}" "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
WORK=${WORK:-$PWD/build}
for m in ${MODELS:-tiny small medium compact large}; do
  cmake -G Ninja -S "$LLVM_SRC/compiler-rt/lib/builtins" -B "$WORK/builtins-$m" \
    -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$BIN/clang" -DCMAKE_ASM_COMPILER="$BIN/clang" \
    -DCMAKE_C_COMPILER_TARGET=i8086-pc-msdos \
    -DCMAKE_ASM_COMPILER_TARGET=i8086-pc-msdos \
    -DCMAKE_C_FLAGS="-mcmodel=$m" -DCMAKE_ASM_FLAGS="-mcmodel=$m" \
    -DCMAKE_AR="$BIN/llvm-ar" -DCMAKE_RANLIB="$BIN/llvm-ranlib" \
    -DCMAKE_NM="$BIN/llvm-nm" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON -DCOMPILER_RT_BAREMETAL_BUILD=ON \
    -DCOMPILER_RT_OS_DIR=msdos -DLLVM_CMAKE_DIR="$BIN/../lib/cmake/llvm" >/dev/null
  ninja -C "$WORK/builtins-$m" >/dev/null
  mkdir -p "$SYSROOT/lib/$m"
  cp "$WORK/builtins-$m/lib/msdos/libclang_rt.builtins-i8086.a" \
     "$SYSROOT/lib/$m/libclang_rt.builtins.a"
done
echo "builtins installed in $SYSROOT/lib/<model>"
