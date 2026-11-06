#!/bin/sh
# Build compiler-rt builtins for 32-bit PowerPC Mac OS X into
# <clang resource dir>/lib/darwin/libclang_rt.ppc_osx.a, which clang links
# in place of Apple's libcc_dynamic.a (10.0 to 10.3) and after
# libgcc_s.1.dylib (10.4 and 10.5).
#
#   LLVM_SRC  llvm-project checkout     BIN  directory with clang and llvm-ar
#   WORK      build directory (default: ./build-builtins-macosx)
#   DEST      output directory (default: clang's resource dir/lib/darwin)
#   LIBTOOL   Apple's libtool from scripts/build-cctools-ppc.sh (default:
#             powerpc-apple-darwin8-libtool on PATH)
#
# No SDK is needed: the builtins only use clang's own headers. The archive
# is made with Apple's libtool because llvm-ar writes the BSD symbol table
# little-endian, and ld64 reads it big-endian for PowerPC.
#
# The generic helpers (64-bit division, float conversions and so on) are
# built for 10.0, so one library serves every version. The IBM double-double
# long double helpers (__gcc_qadd, __fixtfdi, ...) are built for 10.4, where
# long double became 128-bit; code for older versions never calls them.
set -e
: "${LLVM_SRC:?set LLVM_SRC}" "${BIN:?set BIN}"
LIBTOOL=${LIBTOOL:-powerpc-apple-darwin8-libtool}
WORK=${WORK:-$PWD/build-builtins-macosx}
DEST=${DEST:-$("$BIN/clang" -print-resource-dir)/lib/darwin}
src=$LLVM_SRC/compiler-rt/lib/builtins
# Keep the host's headers out: Linux headers don't describe Mac OS X.
cflags="-ffreestanding -nostdlibinc -mcpu=750"

# The generic helpers, through compiler-rt's own build to get its file list.
cmake -G Ninja -S "$src" -B "$WORK/generic" \
  -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$BIN/clang" -DCMAKE_ASM_COMPILER="$BIN/clang" \
  -DCMAKE_C_COMPILER_TARGET=powerpc-apple-macosx10.0 \
  -DCMAKE_ASM_COMPILER_TARGET=powerpc-apple-macosx10.0 \
  -DCMAKE_C_FLAGS="$cflags" -DCMAKE_ASM_FLAGS="$cflags" \
  -DCMAKE_AR="$BIN/llvm-ar" -DCMAKE_RANLIB="$BIN/llvm-ranlib" \
  -DCMAKE_NM="$BIN/llvm-nm" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON -DCOMPILER_RT_BAREMETAL_BUILD=ON \
  -DCOMPILER_RT_OS_DIR=darwin -DLLVM_CMAKE_DIR="$BIN/../lib/cmake/llvm" \
  >"$WORK.cmake.log"
ninja -C "$WORK/generic" >/dev/null
objs=$WORK/objs
rm -rf "$objs"
mkdir -p "$objs"
(cd "$objs" && "$BIN/llvm-ar" x "$WORK"/generic/lib/darwin/libclang_rt.builtins-powerpc.a)

# The double-double helpers, as libgcc_s.1.dylib has them on 10.4.
for f in divtc3 fixtfdi fixunstfdi floatditf floatunditf gcc_qadd gcc_qdiv \
         gcc_qmul gcc_qsub multc3; do
  "$BIN/clang" --target=powerpc-apple-macosx10.4 $cflags -O2 \
    -fno-builtin -c "$src/ppc/$f.c" -o "$objs/ppc_$f.o"
done

# __eprintf, which <assert.h> calls before 10.4.
"$BIN/clang" --target=powerpc-apple-macosx10.0 $cflags -O2 \
  -c "$(dirname "$0")/../macosx/builtins/eprintf.c" -o "$objs/eprintf.o"

mkdir -p "$DEST"
rm -f "$DEST/libclang_rt.ppc_osx.a"
"$LIBTOOL" -static -no_warning_for_no_symbols \
  -o "$DEST/libclang_rt.ppc_osx.a" "$objs"/*
echo "installed $DEST/libclang_rt.ppc_osx.a"
