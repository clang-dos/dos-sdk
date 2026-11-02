#!/bin/sh
# Build one of the package's sysroots with the clang-dos tools in BIN.
#
#   packaging/build-sysroot.sh NAME OUTPUT
#
#   NAME            i8086-msdos     DOS and Win16, every memory model
#                   win9x-msvcrt    95/98/ME and NT 4.0 (msvcrt.dll), libunicows
#                   win9x-crtdll    NT 3.x, Win32s and bare 95 (crtdll.dll)
#                   win2k-xp        2000/XP with libc++, libc++abi, libunwind
#                   win-vista       Vista to 11, 32-bit, with libc++
#                   win-vista-x64   the same, 64-bit
#   OUTPUT          the sysroot directory
#   BIN             directory with clang, llvm-ar and so on (and jwlink)
#   LLVM_SRC        clang-dos/llvm checkout
#   PICOLIBC_SRC    clang-dos/picolibc checkout (i8086-msdos)
#   TVISION_SRC     magiblot/tvision checkout (i8086-msdos, optional; see
#                   tvision/README.md for the commit)
#   MINGW_SRC       mingw-w64 checkout (the Windows sysroots)
#   LIBUNICOWS_SRC  unpacked libunicows-1.1.2-src.tar.gz (win9x-msvcrt)
#   WORK            build directory (default: ./build-sysroot-NAME)
set -e
name=${1:?usage: build-sysroot.sh NAME OUTPUT} out=${2:?}
: "${BIN:?set BIN}" "${LLVM_SRC:?set LLVM_SRC}"
sdk=$(cd "$(dirname "$0")/.." && pwd)
WORK=${WORK:-$PWD/build-sysroot-$name}
mkdir -p "$out" "$WORK"
SYSROOT=$(cd "$out" && pwd) WORK=$(cd "$WORK" && pwd)
export BIN LLVM_SRC SYSROOT WORK
# The scripts find JWlink next to clang, and llvm-ar and the rest on PATH.
PATH=$BIN:$PATH
export PATH
case $name in
i8086-msdos)
  : "${PICOLIBC_SRC:?set PICOLIBC_SRC}"
  export PICOLIBC_SRC MSDOS_LD=${MSDOS_LD:-$BIN/jwlink}
  sh "$sdk/scripts/build-sysroot.sh"
  ;;
win9x-msvcrt)
  : "${MINGW_SRC:?set MINGW_SRC}" "${LIBUNICOWS_SRC:?set LIBUNICOWS_SRC}"
  export MINGW_SRC LIBUNICOWS_SRC
  sh "$sdk/win9x/build-sysroot.sh"
  WORK=$WORK/unicows sh "$sdk/win9x/build-unicows.sh"
  ;;
win9x-crtdll)
  : "${MINGW_SRC:?set MINGW_SRC}"
  export MINGW_SRC
  CRT=crtdll sh "$sdk/win9x/build-sysroot.sh"
  ;;
win2k-xp)
  : "${MINGW_SRC:?set MINGW_SRC}"
  export MINGW_SRC
  WINNT=0x0500 CPU=i586 sh "$sdk/win9x/build-sysroot.sh"
  WORK=$WORK/cxx CPU=i586 sh "$sdk/win9x/build-cxx.sh"
  ;;
win-vista)
  : "${MINGW_SRC:?set MINGW_SRC}"
  export MINGW_SRC
  ARCH=i686 sh "$sdk/packaging/build-modern-sysroot.sh"
  ;;
win-vista-x64)
  : "${MINGW_SRC:?set MINGW_SRC}"
  export MINGW_SRC
  ARCH=x86_64 sh "$sdk/packaging/build-modern-sysroot.sh"
  ;;
*)
  echo "unknown sysroot $name" >&2; exit 1 ;;
esac
echo "sysroot $name built in $SYSROOT"
