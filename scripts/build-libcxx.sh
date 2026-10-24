#!/bin/sh
# Build libc++ and libc++abi for i8086-msdos, one build per memory model,
# into
#   $SYSROOT/include/c++/v1           headers (the same for every model)
#   $SYSROOT/lib/<model>/libc++.a     C++ library
#   $SYSROOT/lib/<model>/libc++abi.a  C++ ABI support (no exceptions)
#
#   LLVM_SRC  llvm-project checkout     BIN  directory with clang and llvm-ar
#   SYSROOT   sysroot that already has picolibc and the builtins
#   WORK      build directory (default: ./build)
#   MODELS    memory models (default: tiny small medium compact large huge
#             huge16)
#   OS        msdos (default), or win16 for $SYSROOT/lib/win16/<model>
#
# There are no exceptions, threads, locales, wide characters or filesystem
# library. libc++abi is built with RTTI, which its typeinfo code needs; C++
# programs default to -fno-exceptions -fno-rtti.
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
  # The demangler's code is close to 64 KiB, the most a module can have. It
  # does not fit with far data pointers, so these models go without it.
  case $m in
    compact|large|huge|huge16) demangler=OFF ;;
    *) demangler=ON ;;
  esac
  rm -rf "$WORK/libcxx$tag-$m" "$WORK/libcxx-install$tag-$m"
  cmake -G Ninja -S "$LLVM_SRC/runtimes" -B "$WORK/libcxx$tag-$m" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
    -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_C_COMPILER="$BIN/clang" -DCMAKE_CXX_COMPILER="$BIN/clang++" \
    -DCMAKE_ASM_COMPILER="$BIN/clang" \
    -DCMAKE_C_COMPILER_TARGET=i8086-pc-$OS \
    -DCMAKE_CXX_COMPILER_TARGET=i8086-pc-$OS \
    -DCMAKE_ASM_COMPILER_TARGET=i8086-pc-$OS \
    -DCMAKE_C_FLAGS="$(model_flags $m) --sysroot=$SYSROOT" \
    -DCMAKE_CXX_FLAGS="$(model_flags $m) --sysroot=$SYSROOT -frtti" \
    -DCMAKE_AR="$BIN/llvm-ar" -DCMAKE_RANLIB="$BIN/llvm-ranlib" \
    -DCMAKE_NM="$BIN/llvm-nm" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCMAKE_INSTALL_PREFIX="$WORK/libcxx-install$tag-$m" \
    -DLIBCXX_ENABLE_SHARED=OFF -DLIBCXX_ENABLE_EXCEPTIONS=OFF \
    -DLIBCXX_ENABLE_RTTI=OFF -DLIBCXX_ENABLE_THREADS=OFF \
    -DLIBCXX_ENABLE_MONOTONIC_CLOCK=OFF -DLIBCXX_ENABLE_LOCALIZATION=OFF \
    -DLIBCXX_ENABLE_WIDE_CHARACTERS=OFF -DLIBCXX_ENABLE_UNICODE=OFF \
    -DLIBCXX_ENABLE_FILESYSTEM=OFF -DLIBCXX_ENABLE_RANDOM_DEVICE=OFF \
    -DLIBCXX_ENABLE_TIME_ZONE_DATABASE=OFF -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
    -DLIBCXX_INCLUDE_TESTS=OFF -DLIBCXX_USE_COMPILER_RT=ON \
    -DLIBCXX_CXX_ABI=libcxxabi \
    -DLIBCXXABI_ENABLE_SHARED=OFF -DLIBCXXABI_ENABLE_EXCEPTIONS=OFF \
    -DLIBCXXABI_ENABLE_THREADS=OFF -DLIBCXXABI_BAREMETAL=ON \
    -DLIBCXXABI_ENABLE_DEMANGLER=$demangler \
    -DLIBCXXABI_USE_LLVM_UNWINDER=OFF -DLIBCXXABI_USE_COMPILER_RT=ON \
    -DLIBCXXABI_INCLUDE_TESTS=OFF >"$WORK/libcxx$tag-$m.log" 2>&1 ||
    { tail -20 "$WORK/libcxx$tag-$m.log"; exit 1; }
  ninja -C "$WORK/libcxx$tag-$m" install-cxx install-cxxabi >>"$WORK/libcxx$tag-$m.log" 2>&1 ||
    { grep -E 'error|FAILED' "$WORK/libcxx$tag-$m.log" | head -20; exit 1; }
  mkdir -p "$libdir/$m"
  cp "$WORK/libcxx-install$tag-$m/lib/libc++.a" "$WORK/libcxx-install$tag-$m/lib/libc++abi.a" \
     "$libdir/$m/"
  # The headers are the same for every model and both systems.
  if [ "$OS" = msdos ]; then
    mkdir -p "$SYSROOT/include/c++"
    rm -rf "$SYSROOT/include/c++/v1"
    cp -R "$WORK/libcxx-install$tag-$m/include/c++/v1" "$SYSROOT/include/c++/v1"
  fi
done
echo "libc++ installed in $libdir/<model>"
