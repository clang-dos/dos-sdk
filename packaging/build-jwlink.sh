#!/bin/sh
# Build JWlink for one host.
#
#   packaging/build-jwlink.sh HOST OUTPUT
#
#   HOST        linux (64-bit, GCC) or windows (32-bit jwlink.exe, cross-built
#               with the mingw-w64 GCC)
#   OUTPUT      where to put the program
#   JWLINK_SRC  clang-dos/JWlink checkout
set -e
host=${1:?usage: build-jwlink.sh linux|windows OUTPUT} out=${2:?}
: "${JWLINK_SRC:?set JWLINK_SRC}"
cd "$JWLINK_SRC"
rm -rf build
case $host in
linux) make -f GccUnix.mak -j"$(nproc)" ;;
windows) make -f GccUnix.mak -j"$(nproc)" CC=i686-w64-mingw32-gcc bitopts= ;;
*) echo "unknown host $host" >&2; exit 1 ;;
esac
mkdir -p "$(dirname "$out")"
if [ -f build/jwlinkLR/jwlink.exe ]; then
  cp build/jwlinkLR/jwlink.exe "$out"
else
  cp build/jwlinkLR/jwlink "$out"
fi
echo "JWlink ($host) built as $out"
