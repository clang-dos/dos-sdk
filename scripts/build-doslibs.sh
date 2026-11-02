#!/bin/sh
# Build the DOS graphics library (graphics.h, libgraph.a), PDCurses
# (curses.h, panel.h, libcurses.a), the character window system (cwin.h,
# libcwin.a), the text window toolkit (tui.h, libtui.a) and the CXL/TCXL
# compatible text library (cxl*.h, tcxl*.h, libcxl.a) for each memory model:
#   $SYSROOT/include/graphics.h, curses.h, panel.h, cwin.h, tui.h, cxl*.h,
#   tcxl*.h
#   $SYSROOT/lib/<model>/libgraph.a, libcurses.a, libpanel.a, libcwin.a,
#   libtui.a, libcxl.a
# libpanel.a is empty: the panel functions are in libcurses.a, as in
# PDCurses, and -lpanel only keeps makefiles written for ncurses working.
#
#   BIN      directory with clang and llvm-ar
#   SYSROOT  sysroot with picolibc (and libc++ for libtui)
#   WORK     build directory (default: ./build)
#   MODELS   memory models (default: tiny small medium compact large huge
#            huge16)
#   NO_CXX   set to skip libtui, which is C++
set -e
: "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
sdk=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$PWD/build}
model_flags() {
  case $1 in
    huge16) echo "-mcmodel=huge -msize-t=16" ;;
    *) echo "-mcmodel=$1" ;;
  esac
}
mkdir -p "$SYSROOT/include"
cp "$sdk/graphics/graphics.h" "$sdk/graphics/fastgraf.h" "$sdk/cwin/cwin.h" "$SYSROOT/include/"
cp "$sdk/curses/curses.h" "$sdk/curses/panel.h" "$SYSROOT/include/"
cp "$sdk"/cxl/include/*.h "$SYSROOT/include/"
[ -n "$NO_CXX" ] || cp "$sdk/tui/tui.h" "$SYSROOT/include/"
for m in ${MODELS:-tiny small medium compact large huge huge16}; do
  out=$WORK/doslibs-$m
  rm -rf "$out"
  mkdir -p "$out/graph" "$out/curses" "$out/cwin" "$out/tui" "$out/cxl" "$SYSROOT/lib/$m"
  for f in "$sdk"/graphics/*.c; do
    o=$out/graph/$(basename "$f" .c).o
    "$BIN/clang" --target=i8086-pc-msdos $(model_flags "$m") --sysroot="$SYSROOT" \
      -Os -Wall -ffunction-sections -fdata-sections -c "$f" -o "$o"
  done
  rm -f "$SYSROOT/lib/$m/libgraph.a"
  "$BIN/llvm-ar" rcs "$SYSROOT/lib/$m/libgraph.a" "$out"/graph/*.o
  for f in "$sdk"/curses/pdcurses/*.c "$sdk"/curses/dos/*.c; do
    o=$out/curses/$(basename "$f" .c).o
    "$BIN/clang" --target=i8086-pc-msdos $(model_flags "$m") --sysroot="$SYSROOT" \
      -I"$sdk/curses" -I"$sdk/curses/dos" \
      -Os -Wall -ffunction-sections -fdata-sections -c "$f" -o "$o"
  done
  rm -f "$SYSROOT/lib/$m/libcurses.a" "$SYSROOT/lib/$m/libpanel.a"
  "$BIN/llvm-ar" rcs "$SYSROOT/lib/$m/libcurses.a" "$out"/curses/*.o
  "$BIN/llvm-ar" rcs "$SYSROOT/lib/$m/libpanel.a"
  # -Oz: the window system is about 40K of code, and this keeps it (with
  # a program) inside the 64K code segment of the small model.
  for f in "$sdk"/cwin/*.c; do
    o=$out/cwin/$(basename "$f" .c).o
    "$BIN/clang" --target=i8086-pc-msdos $(model_flags "$m") --sysroot="$SYSROOT" \
      -Oz -Wall -ffunction-sections -fdata-sections -c "$f" -o "$o"
  done
  rm -f "$SYSROOT/lib/$m/libcwin.a"
  "$BIN/llvm-ar" rcs "$SYSROOT/lib/$m/libcwin.a" "$out"/cwin/*.o

  for f in "$sdk"/cxl/src/*.c; do
    o=$out/cxl/$(basename "$f" .c).o
    "$BIN/clang" --target=i8086-pc-msdos $(model_flags "$m") --sysroot="$SYSROOT" \
      -Os -Wall -ffunction-sections -fdata-sections -I"$sdk/cxl/include" \
      -c "$f" -o "$o"
  done
  rm -f "$SYSROOT/lib/$m/libcxl.a"
  "$BIN/llvm-ar" rcs "$SYSROOT/lib/$m/libcxl.a" "$out"/cxl/*.o
  if [ -z "$NO_CXX" ]; then
    for f in "$sdk"/tui/*.cpp; do
      o=$out/tui/$(basename "$f" .cpp).o
      "$BIN/clang++" --target=i8086-pc-msdos $(model_flags "$m") --sysroot="$SYSROOT" \
        -Os -Wall -ffunction-sections -fdata-sections -c "$f" -o "$o"
    done
    rm -f "$SYSROOT/lib/$m/libtui.a"
    "$BIN/llvm-ar" rcs "$SYSROOT/lib/$m/libtui.a" "$out"/tui/*.o
  fi
done
echo "graphics, curses, cwin, tui and cxl libraries installed in $SYSROOT"
