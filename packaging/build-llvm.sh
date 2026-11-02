#!/bin/sh
# Build and install the clang-dos LLVM tools for one host.
#
#   packaging/build-llvm.sh HOST
#
#   HOST      linux (x86_64, built with clang and lld) or windows (x86_64,
#             cross-compiled on Linux with the mingw-w64 GCC, posix threads)
#   LLVM_SRC  clang-dos/llvm checkout
#   PREFIX    install directory
#   WORK      build directory (default: ./build-llvm-HOST)
#   TARGETS   LLVM_TARGETS_TO_BUILD (default: X86;PowerPC)
#   LAUNCHER  compiler launcher, such as ccache (default: ccache if found)
#   JOBS      parallel jobs (default: nproc)
#
# Only the tools the package ships are built and installed
# (LLVM_DISTRIBUTION_COMPONENTS). Both hosts link the C++ runtime statically
# and use no zlib, zstd, libxml2 or terminfo, so the Linux tools need only
# glibc and the Windows ones nothing but Windows.
set -e
host=${1:?usage: build-llvm.sh linux|windows}
: "${LLVM_SRC:?set LLVM_SRC}" "${PREFIX:?set PREFIX}"
WORK=${WORK:-$PWD/build-llvm-$host}
TARGETS=${TARGETS:-X86;PowerPC}
JOBS=${JOBS:-$(nproc)}
if [ -z "${LAUNCHER+x}" ]; then LAUNCHER=$(command -v ccache || true); fi

components="clang;clang-resource-headers;lld;clang-format;clang-scan-deps"
components="$components;llvm-ar;llvm-ranlib;llvm-lib;llvm-dlltool;llvm-nm"
components="$components;llvm-objcopy;llvm-strip;llvm-objdump;llvm-size"
components="$components;llvm-readobj;llvm-readelf;llvm-rc;llvm-windres"
components="$components;llvm-cxxfilt;llvm-strings;llvm-symbolizer;llvm-ml"

set -- \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD="$TARGETS" \
  -DLLVM_DISTRIBUTION_COMPONENTS="$components" \
  -DLLVM_ENABLE_ASSERTIONS=OFF \
  -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF \
  -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_DOCS=OFF \
  -DCLANG_INCLUDE_TESTS=OFF -DCLANG_INCLUDE_DOCS=OFF \
  -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF \
  -DLLVM_ENABLE_LIBEDIT=OFF -DLLVM_ENABLE_LIBPFM=OFF \
  -DLLVM_ENABLE_TERMINFO=OFF -DLLVM_ENABLE_Z3_SOLVER=OFF \
  -DLLVM_ENABLE_BINDINGS=OFF -DCLANG_ENABLE_ARCMT=OFF \
  -DCLANG_ENABLE_STATIC_ANALYZER=OFF -DCLANG_ENABLE_CIR=OFF \
  -DLLVM_PARALLEL_LINK_JOBS=2

if [ -n "$LAUNCHER" ]; then
  set -- "$@" -DCMAKE_C_COMPILER_LAUNCHER="$LAUNCHER" \
    -DCMAKE_CXX_COMPILER_LAUNCHER="$LAUNCHER"
fi

case $host in
linux)
  CC=${CC:-clang} CXX=${CXX:-clang++}
  # With clang LLVM uses precompiled headers, and ccache then gives up on
  # nearly every file ("could not use precompiled header") unless told to
  # accept them. These are the settings LLVM_CCACHE_BUILD uses: depend mode
  # takes the headers a file used from the compiler, so a changed macro is
  # still a miss. (LLVM turns precompiled headers off for the mingw GCC.)
  case $LAUNCHER in
  *ccache*)
    export CCACHE_DEPEND=${CCACHE_DEPEND-1}
    export CCACHE_SLOPPINESS=${CCACHE_SLOPPINESS-pch_defines,time_macros,include_file_mtime,include_file_ctime}
    ;;
  esac
  set -- "$@" -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
    -DLLVM_USE_LINKER=lld \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"
  ;;
windows)
  mingw=${MINGW_PREFIX:-x86_64-w64-mingw32}
  native=
  [ -n "$LAUNCHER" ] && native="-DCMAKE_C_COMPILER_LAUNCHER=$LAUNCHER;-DCMAKE_CXX_COMPILER_LAUNCHER=$LAUNCHER"
  set -- "$@" -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=AMD64 \
    -DCMAKE_C_COMPILER=$mingw-gcc-posix -DCMAKE_CXX_COMPILER=$mingw-g++-posix \
    -DCMAKE_RC_COMPILER=$mingw-windres \
    -DCMAKE_FIND_ROOT_PATH=/usr/$mingw \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DLLVM_HOST_TRIPLE=x86_64-w64-windows-gnu \
    -DCMAKE_EXE_LINKER_FLAGS="-static" \
    -DLLVM_USE_SYMLINKS=OFF \
    -DCROSS_TOOLCHAIN_FLAGS_NATIVE="$native"
  ;;
*)
  echo "unknown host $host" >&2; exit 1 ;;
esac

cmake -G Ninja -S "$LLVM_SRC/llvm" -B "$WORK" "$@"
ninja -C "$WORK" -j"$JOBS" install-distribution-stripped
echo "LLVM ($host) installed in $PREFIX"
