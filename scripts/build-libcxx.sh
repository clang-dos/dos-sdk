#!/bin/sh
# Build libc++ and libc++abi for i8086-msdos, one build per memory model,
# into
#   $SYSROOT/include/c++/v1           headers (the same for every model)
#   $SYSROOT/lib/<model>/libc++.a     C++ library
#   $SYSROOT/lib/<model>/libc++abi.a  C++ ABI support (no exceptions)
#   $SYSROOT/lib/<model>/eh/          the same with C++ exceptions, and
#                                     libunwind.a, for -fexceptions
#   $SYSROOT/lib/win16/<model>/dll/   for 16-bit Windows DLLs in the small
#                                     and medium models (-mwindows-dll)
#
#   LLVM_SRC  llvm-project checkout     BIN  directory with clang and llvm-ar
#   SYSROOT   sysroot that already has picolibc and the builtins
#   WORK      build directory (default: ./build)
#   MODELS    memory models (default: tiny small medium compact large huge
#             huge16)
#   OS        msdos (default), or win16 for $SYSROOT/lib/win16/<model>
#   EH        which builds: "off on" (default), for without and with C++
#             exceptions
#
# Locales, wide characters and <filesystem> are built, but not threads. On
# i8086 the classic locale makes its facets on first use, so that a program
# links only the facets it uses.
# libc++abi is built with RTTI, which its typeinfo code needs; C++ programs
# default to -fno-exceptions -fno-rtti, and the driver links the eh builds
# for -fexceptions.
set -e
: "${LLVM_SRC:?set LLVM_SRC}" "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
WORK=${WORK:-$PWD/build}
sdk=$(cd "$(dirname "$0")/.." && pwd)
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
# Near-model Windows DLLs need code that keeps escaping locals in DGROUP.
dll=
if [ "$OS" = win16 ]; then
  case $m in small|medium) dll=dll ;; esac
fi
for eh in ${EH:-off on} $dll; do
  # The build with exceptions or for DLLs, its directory and its flags.
  # The exception runtime (libc++abi and the unwinder in the eh build) is
  # built at -Oz, which makes the exception test 0.7 KB smaller in small and
  # 2 KB in large. -Oz makes libc++ and the libc++abi without exceptions a
  # few bytes larger, so they stay at -Os.
  ehopt=
  if [ "$eh" = on ]; then
    b=libcxx$tag-eh-$m dest=$libdir/$m/eh ehflag=-fexceptions EXC=ON ehopt=-Oz
  elif [ "$eh" = dll ]; then
    b=libcxx$tag-dll-$m dest=$libdir/$m/dll ehflag=-mwindows-dll EXC=OFF
  else
    b=libcxx$tag-$m dest=$libdir/$m ehflag= EXC=OFF
  fi
  # The demangler's code is close to 64 KiB. With far code it takes
  # several code segments; in the models with one code segment for the
  # whole program it leaves no room for the program.
  case $m in
    tiny|small|compact) demangler=OFF ;;
    *) demangler=ON ;;
  esac
  rm -rf "$WORK/$b" "$WORK/install-$b"
  cmake -G Ninja -S "$LLVM_SRC/runtimes" -B "$WORK/$b" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
    -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_C_COMPILER="$BIN/clang" -DCMAKE_CXX_COMPILER="$BIN/clang++" \
    -DCMAKE_ASM_COMPILER="$BIN/clang" \
    -DCMAKE_C_COMPILER_TARGET=i8086-pc-$OS \
    -DCMAKE_CXX_COMPILER_TARGET=i8086-pc-$OS \
    -DCMAKE_ASM_COMPILER_TARGET=i8086-pc-$OS \
    -DCMAKE_C_FLAGS="$(model_flags $m) --sysroot=$SYSROOT $ehflag" \
    -DCMAKE_CXX_FLAGS="$(model_flags $m) --sysroot=$SYSROOT -frtti -D_GNU_SOURCE $ehflag" \
    -DCMAKE_AR="$BIN/llvm-ar" -DCMAKE_RANLIB="$BIN/llvm-ranlib" \
    -DCMAKE_NM="$BIN/llvm-nm" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCMAKE_INSTALL_PREFIX="$WORK/install-$b" \
    -DLIBCXX_ENABLE_SHARED=OFF -DLIBCXX_ENABLE_EXCEPTIONS=$EXC \
    -DLIBCXX_ENABLE_RTTI=$EXC -DLIBCXX_ENABLE_THREADS=OFF \
    -DLIBCXX_ENABLE_MONOTONIC_CLOCK=OFF -DLIBCXX_ENABLE_LOCALIZATION=ON \
    -DLIBCXX_ENABLE_WIDE_CHARACTERS=ON -DLIBCXX_ENABLE_UNICODE=ON \
    -DLIBCXX_ENABLE_FILESYSTEM=ON -DLIBCXX_ENABLE_RANDOM_DEVICE=OFF \
    -DLIBCXX_ENABLE_TIME_ZONE_DATABASE=OFF -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
    -DLIBCXX_INCLUDE_TESTS=OFF -DLIBCXX_USE_COMPILER_RT=ON \
    -DLIBCXX_CXX_ABI=libcxxabi -DRUNTIMES_USE_LIBC=picolibc \
    -DLIBCXXABI_ENABLE_SHARED=OFF -DLIBCXXABI_ENABLE_EXCEPTIONS=$EXC \
    -DLIBCXXABI_ENABLE_THREADS=OFF -DLIBCXXABI_BAREMETAL=ON \
    -DLIBCXXABI_ENABLE_DEMANGLER=$demangler \
    -DLIBCXXABI_NON_DEMANGLING_TERMINATE=ON \
    -DLIBCXXABI_USE_LLVM_UNWINDER=OFF -DLIBCXXABI_USE_COMPILER_RT=ON \
    -DLIBCXXABI_INCLUDE_TESTS=OFF \
    -DLIBCXXABI_ADDITIONAL_COMPILE_FLAGS="$ehopt" >"$WORK/$b.log" 2>&1 ||
    { tail -20 "$WORK/$b.log"; exit 1; }
  ninja -C "$WORK/$b" install-cxx install-cxxabi >>"$WORK/$b.log" 2>&1 ||
    { grep -E 'error|FAILED' "$WORK/$b.log" | head -20; exit 1; }
  mkdir -p "$dest"
  cp "$WORK/install-$b/lib/libc++.a" "$WORK/install-$b/lib/libc++abi.a" \
     "$dest/"
  # The unwinder: SjLj function contexts (runtime/unwind-sjlj.c).
  if [ "$eh" = on ]; then
    "$BIN/clang" --target=i8086-pc-$OS $(model_flags $m) --sysroot="$SYSROOT" \
      -fexceptions -Oz -ffunction-sections -c "$sdk/runtime/unwind-sjlj.c" \
      -o "$WORK/$b/unwind-sjlj.o"
    rm -f "$dest/libunwind.a"
    "$BIN/llvm-ar" rcs "$dest/libunwind.a" "$WORK/$b/unwind-sjlj.o"
  fi
  # The headers are the same for every model and both systems.
  if [ "$OS" = msdos ] && [ "$eh" = off ]; then
    mkdir -p "$SYSROOT/include/c++"
    rm -rf "$SYSROOT/include/c++/v1"
    cp -R "$WORK/install-$b/include/c++/v1" "$SYSROOT/include/c++/v1"
  fi
done
done
echo "libc++ installed in $libdir/<model>"
