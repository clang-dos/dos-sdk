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
#   WIN16_SDK       clang-dos/win16-sdk checkout (i8086-msdos: Windows 1.x-3.x)
#   WIN32_SDK       clang-dos/win32-sdk checkout (the Windows sysroots, which
#                   its build-sysroot.sh builds)
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
  : "${PICOLIBC_SRC:?set PICOLIBC_SRC}" "${WIN16_SDK:?set WIN16_SDK}"
  export PICOLIBC_SRC WIN16_SDK MSDOS_LD=${MSDOS_LD:-$BIN/jwlink}
  sh "$sdk/scripts/build-sysroot.sh"
  ;;
win9x-msvcrt|win9x-crtdll|win2k-xp|win-vista|win-vista-x64)
  : "${WIN32_SDK:?set WIN32_SDK}" "${MINGW_SRC:?set MINGW_SRC}"
  export MINGW_SRC LIBUNICOWS_SRC
  sh "$WIN32_SDK/build-sysroot.sh" "$name" "$SYSROOT"
  ;;
*)
  echo "unknown sysroot $name" >&2; exit 1 ;;
esac
echo "sysroot $name built in $SYSROOT"
