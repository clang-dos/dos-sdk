#!/bin/sh
# Build Turbo Vision (magiblot/tvision, with tvision/clang-dos.patch and the
# BIOS and DOS platform layer in tvision/) for the memory models with more
# than one code segment:
#   $SYSROOT/include/tvision/            headers (#include <tvision/tv.h>)
#   $SYSROOT/lib/<model>/libtvision.a    library
#   $SYSROOT/share/tvision/COPYRIGHT     licence
#
#   TVISION_SRC  magiblot/tvision checkout at the commit in tvision/README.md
#   BIN          directory with clang++ and llvm-ar
#   SYSROOT      sysroot with picolibc and libc++
#   WORK         build directory (default: ./build)
#   MODELS       memory models (default: medium large huge huge16; Turbo
#                Vision's code does not fit in the 64 KiB code segment of
#                tiny, small and compact)
set -e
: "${TVISION_SRC:?set TVISION_SRC}" "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
sdk=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$PWD/build}
model_flags() {
  case $1 in
    huge16) echo "-mcmodel=huge -msize-t=16" ;;
    *) echo "-mcmodel=$1" ;;
  esac
}
# A patched copy of the sources.
src=$WORK/tvision-src
rm -rf "$src"
mkdir -p "$src"
cp -R "$TVISION_SRC/include" "$TVISION_SRC/source" "$TVISION_SRC/examples" \
  "$TVISION_SRC/COPYRIGHT" "$src/"
(cd "$src" && patch -p1 -s <"$sdk/tvision/clang-dos.patch")
cp "$sdk/tvision/doshw.cpp" "$sdk/tvision/dosrtl.cpp" "$src/source/platform/"

rm -rf "$SYSROOT/include/tvision"
mkdir -p "$SYSROOT/include" "$SYSROOT/share/tvision"
cp -R "$src/include/tvision" "$SYSROOT/include/"
cp "$src/COPYRIGHT" "$SYSROOT/share/tvision/"

inc="-I$src/include -I$src/include/tvision -I$src/include/tvision/compat/borland"
for m in ${MODELS:-medium large huge huge16}; do
  out=$WORK/tvision-$m
  rm -rf "$out"
  mkdir -p "$out" "$SYSROOT/lib/$m"
  for f in "$src"/source/tvision/*.cpp "$src"/source/platform/dos*.cpp; do
    b=$(basename "$f" .cpp)
    # geninc is a host tool for the generated headers.
    [ "$b" = geninc ] && continue
    "$BIN/clang++" --target=i8086-pc-msdos $(model_flags "$m") --sysroot="$SYSROOT" \
      -Os -std=gnu++17 -w $inc -c "$f" -o "$out/$b.o"
  done
  rm -f "$SYSROOT/lib/$m/libtvision.a"
  "$BIN/llvm-ar" rcs "$SYSROOT/lib/$m/libtvision.a" "$out"/*.o
done
echo "Turbo Vision installed in $SYSROOT"
