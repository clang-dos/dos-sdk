#!/bin/sh
# Build picolibc for i8086-msdos, one build per memory model, into
#   $SYSROOT/include                 headers (the same for every model)
#   $SYSROOT/lib/<model>/crt0.obj    startup code
#   $SYSROOT/lib/<model>/libc.a      C library with the DOS layer
#
#   PICOLIBC_SRC  clang-dos/picolibc checkout (branch i8086-msdos)
#   BIN           directory with clang, llvm-ar and llvm-nm
#   SYSROOT       output sysroot
#   WORK          build directory (default: ./build)
#   MODELS        memory models (default: tiny small medium)
# meson and ninja must be on PATH.
set -e
: "${PICOLIBC_SRC:?set PICOLIBC_SRC}" "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
WORK=${WORK:-$PWD/build}
mkdir -p "$WORK"
PATH=$BIN:$PATH
export PATH
for m in ${MODELS:-tiny small medium}; do
  rm -rf "$WORK/picolibc-$m" "$WORK/picolibc-install-$m"
  meson setup "$WORK/picolibc-$m" "$PICOLIBC_SRC" \
    --cross-file "$PICOLIBC_SRC/scripts/cross-clang-i8086-msdos-$m.txt" \
    -Dos-msdos=true -Dmultilib=false -Dpicocrt=false -Dsemihost=false \
    -Dtests=false -Dposix-console=true -Dsingle-thread=true \
    -Dthread-local-storage=false -Dspecsdir=none \
    --prefix="$WORK/picolibc-install-$m" >"$WORK/picolibc-$m.log" 2>&1 ||
    { tail -20 "$WORK/picolibc-$m.log"; exit 1; }
  ninja -C "$WORK/picolibc-$m" install >>"$WORK/picolibc-$m.log" 2>&1 ||
    { grep -E 'error|FAILED' "$WORK/picolibc-$m.log" | head -20; exit 1; }
  mkdir -p "$SYSROOT/lib/$m"
  cp "$WORK/picolibc-install-$m/lib/crt0.obj" "$WORK/picolibc-install-$m/lib/libc.a" \
     "$SYSROOT/lib/$m/"
  # Replace the C headers but keep the C++ ones from build-libcxx.sh.
  mkdir -p "$SYSROOT/include"
  find "$SYSROOT/include" -mindepth 1 -maxdepth 1 ! -name c++ -exec rm -rf {} +
  cp -R "$WORK/picolibc-install-$m/include/." "$SYSROOT/include/"
done
echo "picolibc installed in $SYSROOT"
