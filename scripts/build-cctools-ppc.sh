#!/bin/sh
# Build Apple's assembler (cctools-877.8) and linker (ld64-253.9) for
# PowerPC Mac OS X on Linux, from tpoechtrager's cctools-port. Installs
# powerpc-apple-darwin8-{as,ld,otool,nm,lipo,...} in PREFIX/bin.
#
#   PREFIX  install prefix (default: $PWD/cctools-ppc)
#   SRC     cctools-port checkout (default: cloned into $PWD/cctools-port)
#   CC/CXX  host compilers (default: clang, clang++)
#
# Needs: git, clang, automake, autoconf, libtool, uuid-dev, libssl-dev.
# This ld64 links for PowerPC with -macosx_version_min down to 10.4 and
# writes the classic layout (LC_UNIXTHREAD, LC_DYSYMTAB, no LC_DYLD_INFO).
# It does not target 10.0 to 10.3; that needs the classic cctools ld.
set -e
PREFIX=${PREFIX:-$PWD/cctools-ppc}
SRC=${SRC:-$PWD/cctools-port}
if [ ! -d "$SRC" ]; then
  git clone -b 877.8-ld64-253.9-ppc https://github.com/tpoechtrager/cctools-port.git "$SRC"
fi
cd "$SRC/cctools"
./autogen.sh
./configure --prefix="$PREFIX" --target=powerpc-apple-darwin8 \
  CC="${CC:-clang}" CXX="${CXX:-clang++}"
make -j"$(nproc)"
make install
