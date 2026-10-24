#!/bin/sh
# Build the 16-bit Windows (i8086-pc-win16) libraries of the sysroot, after
# the DOS ones:
#   $SYSROOT/include/windows.h              Windows API declarations
#   $SYSROOT/lib/win16/windows.lnk          imports of the Windows API
#   $SYSROOT/lib/win16/<model>/crt0.obj     program startup code
#   $SYSROOT/lib/win16/<model>/libentry.obj DLL startup code
#   $SYSROOT/lib/win16/<model>/libc.a       C library with the Windows layer
#   $SYSROOT/lib/win16/<model>/libclang_rt.builtins.a, libc++.a, libc++abi.a
#   $SYSROOT/lib/win16/<model>/dll/         the libraries again for DLLs in
#                                           the small and medium models,
#                                           built with -mwindows-dll
# picolibc's DOS layer serves Windows too: Windows programs call DOS for
# files and the clock. The builtins and libc++ are built again, as Windows
# returns structures through far pointers in every model.
#   LLVM_SRC, PICOLIBC_SRC, BIN, SYSROOT, WORK as for build-sysroot.sh
#   WIN16_MODELS  memory models (default: small medium compact large; the
#                 huge models and tiny do not exist on Windows)
#   NO_CXX        set to skip libc++
set -e
: "${LLVM_SRC:?set LLVM_SRC}" "${PICOLIBC_SRC:?set PICOLIBC_SRC}" "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
WORK=${WORK:-$PWD/build}
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$WORK" "$SYSROOT/lib/win16"
PATH=$BIN:$PATH
export PATH
cp "$here/../win16/windows.lnk" "$SYSROOT/lib/win16/"
mkdir -p "$SYSROOT/include"
cp "$here/../win16/include/windows.h" "$SYSROOT/include/"
for m in ${WIN16_MODELS:-small medium compact large}; do
variants=normal
case $m in small|medium) variants="normal dll" ;; esac
for v in $variants; do
  cross="$PICOLIBC_SRC/scripts/cross-clang-i8086-win16-$m.txt"
  if [ $v = dll ]; then
    b="$WORK/picolibc-win16-dll-$m" i="$WORK/picolibc-install-win16-dll-$m"
    d="$SYSROOT/lib/win16/$m/dll"
    # The cross file with -mwindows-dll added to the compiler.
    sed "s|'-nostdlib'\]|'-nostdlib', '-mwindows-dll']|; s|@DIRNAME@|$PICOLIBC_SRC/scripts|" \
      "$cross" >"$WORK/cross-win16-dll-$m.txt"
    cross="$WORK/cross-win16-dll-$m.txt"
  else
    b="$WORK/picolibc-win16-$m" i="$WORK/picolibc-install-win16-$m"
    d="$SYSROOT/lib/win16/$m"
  fi
  rm -rf "$b" "$i"
  meson setup "$b" "$PICOLIBC_SRC" \
    --cross-file "$cross" \
    -Dos-msdos=true -Dmultilib=false -Dpicocrt=false -Dsemihost=false \
    -Dtests=false -Dposix-console=true -Dsingle-thread=true \
    -Dthread-local-storage=false -Dspecsdir=none \
    --prefix="$i" >"$b.log" 2>&1 || { tail -20 "$b.log"; exit 1; }
  ninja -C "$b" install >>"$b.log" 2>&1 ||
    { grep -E 'error|FAILED' "$b.log" | head -20; exit 1; }
  mkdir -p "$d"
  if [ $v = dll ]; then
    cp "$i/lib/libc.a" "$d/"
  else
    cp "$i/lib/crt0.obj" "$i/lib/libentry.obj" "$i/lib/libc.a" "$d/"
  fi
done
done
models=${WIN16_MODELS:-small medium compact large}
OS=win16 MODELS=$models sh "$here/build-builtins.sh"
[ -n "$NO_CXX" ] || OS=win16 MODELS=$models sh "$here/build-libcxx.sh"
echo "Windows libraries installed in $SYSROOT/lib/win16"
