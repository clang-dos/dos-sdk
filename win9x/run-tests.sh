#!/bin/sh
# Build the Windows 9x tests, check their imports and run them under Wine.
#
#   BIN      directory with clang (with the Windows 9x triple support)
#   SYSROOT  sysroot from build-sysroot.sh
#   DLLS     optional: one directory of system DLLs per Windows version, for
#            check-imports.py --dlls
#   WINE     optional: wine, to run the console tests (default: wine on PATH)
#   TARGETS  triples (default: 95, 98 and ME)
#   OPTS     optimization levels (default: O0 O2 Os)
: "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
here=$(cd "$(dirname "$0")" && pwd)
out=${WORK:-$PWD/build-win9x-tests}
mkdir -p "$out"
cd "$out" || exit 1
WINE=${WINE:-wine}
command -v "$WINE" >/dev/null 2>&1 || WINE=
export WINEDEBUG=${WINEDEBUG:--all}
pass=0
fail=0

ok() { pass=$((pass + 1)); }
bad() {
  fail=$((fail + 1))
  echo "FAIL $1"
  sed 's/^/    /' "$1.log" | head -20
}

for target in ${TARGETS:-i486-pc-windows4.0-gnu i486-pc-windows4.10-gnu \
    i586-pc-windows4.90-gnu}; do
  for opt in ${OPTS:-O0 O2 Os}; do
    for t in hello libc tchar gui; do
      name=$t-$target-$opt
      flags=
      [ $t = gui ] && flags=-mwindows
      if ! "$BIN/clang" --target=$target --sysroot="$SYSROOT" -fuse-ld=lld \
          -$opt $flags "$here/tests/$t.c" -o $name.exe >$name.log 2>&1; then
        bad $name
        continue
      fi
      if ! python3 "$here/check-imports.py" ${DLLS:+--dlls "$DLLS"} \
          $name.exe >$name.log 2>&1; then
        bad $name
        continue
      fi
      case $t in
        hello) want="hello, world" ;;
        libc | tchar) want="ALL PASSED" ;;
        *) want= ;;
      esac
      if [ -n "$want" ] && [ -n "$WINE" ]; then
        if ! "$WINE" ./$name.exe >$name.log 2>&1 ||
            ! grep -q "$want" $name.log; then
          bad $name
          continue
        fi
      fi
      ok
    done
  done
done
[ -n "$WINE" ] || echo "note: wine not found, so the tests were built and checked but not run"
echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
