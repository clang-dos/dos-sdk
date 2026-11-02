#!/bin/sh
# Put a clang-dos package together.
#
#   packaging/assemble.sh HOST OUTDIR
#
#   HOST          linux or windows
#   OUTDIR        the package directory (replaced)
#   LLVM_PREFIX   the install directory from build-llvm.sh for HOST
#   JWLINK        jwlink (or jwlink.exe) for HOST
#   SYSROOTS      directory with the sysroots from build-sysroot.sh, one
#                 subdirectory per name
#   LLVM_SRC      clang-dos/llvm checkout (licence)
#   PICOLIBC_SRC  clang-dos/picolibc checkout (README, licence)
#   JWLINK_SRC    clang-dos/JWlink checkout (licence)
#   WIN16_SDK     clang-dos/win16-sdk checkout (config files, tools, README)
#   WIN32_SDK     clang-dos/win32-sdk checkout (config files, tools, README)
#   BRANCHES      optional: BRANCHES.txt to include
set -e
host=${1:?usage: assemble.sh linux|windows OUTDIR} out=${2:?}
: "${LLVM_PREFIX:?}" "${JWLINK:?}" "${SYSROOTS:?}" "${LLVM_SRC:?}" \
  "${PICOLIBC_SRC:?}" "${JWLINK_SRC:?}" "${WIN16_SDK:?}" "${WIN32_SDK:?}"
here=$(cd "$(dirname "$0")" && pwd)
sdk=$(cd "$here/.." && pwd)
inst=$LLVM_PREFIX
x=; [ "$host" = windows ] && x=.exe
rm -rf "$out"
mkdir -p "$out/bin" "$out/lib" "$out/sysroots" "$out/share/clang-dos/docs" \
  "$out/share/licenses"

keep="clang clang++ clang-cpp clang-cl clang-format clang-scan-deps lld ld.lld
 lld-link llvm-ar llvm-ranlib llvm-nm llvm-objcopy llvm-objdump llvm-strip
 llvm-size llvm-readobj llvm-readelf llvm-rc llvm-windres llvm-dlltool llvm-lib
 llvm-cxxfilt llvm-strings llvm-symbolizer llvm-ml"
for t in $keep; do
  if [ -e "$inst/bin/$t$x" ] || [ -L "$inst/bin/$t$x" ]; then
    if [ -n "$x" ]; then
      cp -L "$inst/bin/$t$x" "$out/bin/"   # Windows has no symbolic links
    else
      cp -P "$inst/bin/$t$x" "$out/bin/"
    fi
  else
    echo "warning: $t$x is not in $inst/bin" >&2
  fi
done
# On Linux clang is a link to clang-<major>.
if [ -z "$x" ]; then
  for f in "$inst"/bin/clang-[0-9]*; do
    if [ -e "$f" ]; then cp -P "$f" "$out/bin/"; fi
  done
fi
cp -R "$inst/lib/clang" "$out/lib/"
cp "$JWLINK" "$out/bin/jwlink$x"
chmod +x "$out/bin/jwlink$x"
cp "$here"/cfg/*.cfg "$WIN16_SDK"/cfg/*.cfg "$WIN32_SDK"/cfg/*.cfg "$out/bin/"

cp -R "$SYSROOTS/i8086-msdos" "$out/i8086-msdos"
for s in win9x-msvcrt win9x-crtdll win2k-xp win-vista win-vista-x64; do
  cp -R "$SYSROOTS/$s" "$out/sysroots/"
done

cp "$WIN16_SDK/tools/ne-check.py" "$WIN16_SDK/tools/win16-imports.py" \
  "$WIN32_SDK/tools/check-imports.py" "$WIN32_SDK/tools/msvcrt-versions.txt" \
  "$WIN32_SDK/scripts/build-modern-sysroot.sh" "$out/share/clang-dos/"
cp "$sdk/README.md" "$out/share/clang-dos/docs/dos-sdk-README.md"
cp "$WIN16_SDK/README.md" "$out/share/clang-dos/docs/win16-README.md"
cp "$WIN32_SDK/README.md" "$out/share/clang-dos/docs/win32-README.md"
cp "$sdk/docs/memory-models.md" "$out/share/clang-dos/docs/"
if [ -f "$PICOLIBC_SRC/libos/msdos/README.md" ]; then
  cp "$PICOLIBC_SRC/libos/msdos/README.md" \
    "$out/share/clang-dos/docs/picolibc-msdos-README.md"
fi

cp -R "$here/examples" "$out/examples"
cp "$here/smoke-test.sh" "$here/README.md" "$out/"
if [ -n "${BRANCHES:-}" ]; then cp "$BRANCHES" "$out/BRANCHES.txt"; fi

cp "$LLVM_SRC/llvm/LICENSE.TXT" "$out/share/licenses/LLVM-LICENSE.TXT"
cp "$PICOLIBC_SRC/COPYING.picolibc" "$out/share/licenses/" 2>/dev/null || true
cp "$JWLINK_SRC/Ver/license.txt" "$out/share/licenses/JWlink-OWPL.txt" 2>/dev/null || true
du -sh "$out"
