#!/bin/sh
# Build libc++, libc++abi and libunwind as static libraries for 32-bit
# PowerPC Mac OS X 10.4 and later, for C++ programs that want a modern
# standard library in place of Apple's libstdc++.
#
#   LLVM_SRC  llvm-project checkout     BIN  directory with clang
#   SDK       MacOSX10.4.0.sdk (or 10.4u)
#   WORK      build directory (default: ./build-libcxx-macosx)
#   PREFIX    install directory (default: ./libcxx-macosx); headers go to
#             PREFIX/include/c++/v1 and libraries to PREFIX/lib
#
# Needs powerpc-apple-darwin8-ar and -ranlib on PATH (scripts/build-cctools-
# ppc.sh): ld64 can't read the symbol table llvm-ar writes for PowerPC.
#
# The libraries leave out threads, locales, wide characters and the file
# system library, which need more of the C library than 10.4 has, and the
# parallel algorithms run serially (libdispatch arrived in 10.6). Build a
# program with:
#   clang++ --target=powerpc-apple-macosx10.4 -isysroot $SDK -nostdinc++ \
#     -isystem $PREFIX/include/c++/v1 x.cpp -nostdlib++ -L$PREFIX/lib \
#     -lc++ -lc++abi -lunwind
set -e
: "${LLVM_SRC:?set LLVM_SRC}" "${BIN:?set BIN}" "${SDK:?set SDK}"
WORK=${WORK:-$PWD/build-libcxx-macosx}
PREFIX=${PREFIX:-$PWD/libcxx-macosx}
target=powerpc-apple-macosx10.4
# PRIx64 and friends: the 10.4 inttypes.h only defines them for C++ when
# asked.
flags=-D__STDC_FORMAT_MACROS

cmake -G Ninja -S "$LLVM_SRC/runtimes" -B "$WORK" \
  -DLLVM_ENABLE_RUNTIMES="libunwind;libcxxabi;libcxx" \
  -DCMAKE_SYSTEM_NAME=Darwin -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DCMAKE_C_COMPILER="$BIN/clang" -DCMAKE_CXX_COMPILER="$BIN/clang++" \
  -DCMAKE_ASM_COMPILER="$BIN/clang" \
  -DCMAKE_C_COMPILER_TARGET=$target -DCMAKE_CXX_COMPILER_TARGET=$target \
  -DCMAKE_ASM_COMPILER_TARGET=$target \
  -DCMAKE_OSX_SYSROOT="$SDK" -DCMAKE_OSX_ARCHITECTURES=ppc \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.4 \
  -DCMAKE_AR="$(command -v powerpc-apple-darwin8-ar)" \
  -DCMAKE_RANLIB="$(command -v powerpc-apple-darwin8-ranlib)" \
  -DCMAKE_C_FLAGS="$flags" -DCMAKE_CXX_FLAGS="$flags" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DLIBUNWIND_ENABLE_SHARED=OFF -DLIBUNWIND_ENABLE_THREADS=OFF \
  -DLIBCXXABI_ENABLE_SHARED=OFF -DLIBCXXABI_ENABLE_THREADS=OFF \
  -DLIBCXXABI_USE_LLVM_UNWINDER=ON -DLIBCXXABI_ENABLE_STATIC_UNWINDER=ON \
  -DLIBCXX_ENABLE_SHARED=OFF -DLIBCXX_ENABLE_THREADS=OFF \
  -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON -DLIBCXX_CXX_ABI=libcxxabi \
  -DLIBCXX_ENABLE_LOCALIZATION=OFF -DLIBCXX_ENABLE_WIDE_CHARACTERS=OFF \
  -DLIBCXX_ENABLE_FILESYSTEM=OFF -DLIBCXX_ENABLE_RANDOM_DEVICE=OFF \
  -DLIBCXX_ENABLE_MONOTONIC_CLOCK=OFF -DLIBCXX_PSTL_BACKEND=serial \
  -DLIBCXX_ENABLE_TIME_ZONE_DATABASE=OFF \
  -DLIBCXX_ENABLE_VENDOR_AVAILABILITY_ANNOTATIONS=OFF \
  -DLIBCXX_INCLUDE_TESTS=OFF -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
  -DLIBCXXABI_INCLUDE_TESTS=OFF -DLIBUNWIND_INCLUDE_TESTS=OFF
ninja -C "$WORK" install-unwind install-cxxabi install-cxx
echo "installed $PREFIX"
