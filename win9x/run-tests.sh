#!/bin/sh
# Build the Windows 9x tests, check their imports and run them under Wine.
#
#   BIN      directory with clang (with the Windows 9x triple support)
#   SYSROOT  sysroot from build-sysroot.sh
#   DLLS     optional: one directory of system DLLs per Windows version, for
#            check-imports.py --dlls
#   WINE     optional: wine, to run the console tests (default: wine on PATH)
#   TARGETS  triples (default: 95, 98 and ME). Windows NT 3.x triples, such
#            as i386-pc-windows3.10-gnu, are checked for that NT version
#            (check-imports.py --nt) and run with Wine reporting Windows
#            NT 3.51; they need the crtdll sysroot.
#   OPTS     optimization levels (default: O0 O2 Os)
#
# When the sysroot has libunicows.a (build-unicows.sh), the tests include a
# Unicode build linked with it.
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
tests="hello libc tchar gui"
# crtdll.dll has no wide C library (no wmain startup, no _wfopen).
wide=
"$BIN/llvm-nm" "$SYSROOT/lib/libmsvcrt.a" 2>/dev/null |
  grep -q __wgetmainargs && wide=1
[ -n "$wide" ] && [ -f "$SYSROOT/lib/libunicows.a" ] && tests="$tests unicode"

ok() { pass=$((pass + 1)); }
bad() {
  fail=$((fail + 1))
  echo "FAIL $1"
  sed 's/^/    /' "$1.log" | head -20
}

for target in ${TARGETS:-i486-pc-windows4.0-gnu i486-pc-windows4.10-gnu \
    i586-pc-windows4.90-gnu}; do
  for opt in ${OPTS:-O0 O2 Os}; do
    for t in $tests; do
      name=$t-$target-$opt
      flags=
      [ $t = gui ] && flags=-mwindows
      [ $t = unicode ] && flags="-municode -lunicows"
      if ! "$BIN/clang" --target=$target --sysroot="$SYSROOT" -fuse-ld=lld \
          -$opt $flags "$here/tests/$t.c" -o $name.exe >$name.log 2>&1; then
        bad $name
        continue
      fi
      nt=
      case $target in
        *-windows3.*) nt=${target#*-windows}; nt=--nt=${nt%%-*} ;;
      esac
      if ! python3 "$here/check-imports.py" ${DLLS:+--dlls "$DLLS"} $nt \
          $name.exe >$name.log 2>&1; then
        bad $name
        continue
      fi
      case $t in
        hello) want="hello, world" ;;
        libc | tchar | unicode) want="ALL PASSED" ;;
        *) want= ;;
      esac
      if [ -n "$want" ] && [ -n "$WINE" ]; then
        # Wine has no NT 3.1 or 3.5; it reports NT 3.51 to the program.
        [ -n "$nt" ] && "$WINE" reg add \
          "HKCU\\Software\\Wine\\AppDefaults\\$name.exe" /v Version \
          /d nt351 /f >/dev/null 2>&1
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
# The checker must reject wide C library calls that fail on 9x.
if [ -n "$wide" ]; then
  printf '#include <stdio.h>\nint main(void) { return !_wfopen(L"x", L"r"); }\n' \
    >wfopen.c
  if "$BIN/clang" --target=i486-pc-windows4.0-gnu --sysroot="$SYSROOT" \
      -fuse-ld=lld wfopen.c -o wfopen.exe >wfopen.log 2>&1 &&
      ! python3 "$here/check-imports.py" wfopen.exe >>wfopen.log 2>&1 &&
      grep -q "_wfopen calls a Unicode stub" wfopen.log; then
    ok
  else
    bad wfopen
  fi
fi
# For NT 3.x the checker must reject later kernel32 functions.
case "${TARGETS:-}" in
  *-windows3.*)
    printf '#include <windows.h>\nint main(void) { return IsDebuggerPresent(); }\n' \
      >nt4only.c
    if "$BIN/clang" --target=i386-pc-windows3.51-gnu --sysroot="$SYSROOT" \
        -fuse-ld=lld nt4only.c -o nt4only.exe >nt4only.log 2>&1 &&
        ! python3 "$here/check-imports.py" --nt=3.51 nt4only.exe \
          >>nt4only.log 2>&1 &&
        grep -q "IsDebuggerPresent needs Windows NT 4.00" nt4only.log; then
      ok
    else
      bad nt4only
    fi
    ;;
esac
[ -n "$WINE" ] || echo "note: wine not found, so the tests were built and checked but not run"
echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
