#!/bin/sh
# Build the 16-bit Windows tests for each memory model and optimization
# level, run them in Wine and report which fail. Wine runs a 16-bit program
# with the exit code 0 whatever it returns, so the tests print ALL PASSED.
#
#   BIN      directory with clang
#   SYSROOT  sysroot from scripts/build-sysroot.sh
#   LD       wlink or JWlink (default: found by clang on its own path)
#   WINE     wine (default: wine on PATH); WINEPREFIX as usual
#   MODELS   memory models (default: small medium compact large)
#   OPTS     optimization levels (default: O0 Os O2)
#   NO_CXX   set to skip the C++ test
#   NO_GUI   set to skip the test with a window, which needs a display
#            (DISPLAY, or xvfb-run on PATH)
: "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
WINE=${WINE:-wine}
WINEDEBUG=${WINEDEBUG:--all}
export WINEDEBUG
tests=$(cd "$(dirname "$0")" && pwd)/win16
scripts=$(cd "$(dirname "$0")/../scripts" && pwd)
out=${WORK:-$PWD/build-win16-tests}
mkdir -p "$out"
cd "$out" || exit 1
ldflag=${LD:+--ld-path=$LD}
pass=0
fail=0

# Wine keeps the display its server started with, so with no display the
# whole run goes under xvfb-run.
if [ -z "$NO_GUI" ] && [ -z "$DISPLAY" ] && [ -z "$WIN16_TESTS_XVFB" ] &&
   command -v xvfb-run >/dev/null 2>&1; then
  # A Wine server left from a run without a display would be reused.
  wineserver -k 2>/dev/null
  WIN16_TESTS_XVFB=1 exec xvfb-run -a sh "$tests/../run-win16-tests.sh" "$@"
fi
gui=
[ -n "$NO_GUI" ] || [ -z "$DISPLAY" ] || gui=env

run() {
  # run NAME COMMAND...: the output of a test, in NAME.out
  name=$1
  shift
  timeout 60 "$@" >"$name.out" 2>&1
  pattern="ALL PASSED"
  if grep -q -- "$pattern" "$name.out"; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1))
    echo "FAIL $name"
    sed 's/^/    /' "$name.out" | head -20
  fi
}

build() {
  # build NAME COMMAND...
  name=$1
  shift
  if "$@" >"$name.log" 2>&1; then
    return 0
  fi
  fail=$((fail + 1))
  echo "FAIL $name (build)"
  sed 's/^/    /' "$name.log" | head -20
  return 1
}

for m in ${MODELS:-small medium compact large}; do
  for o in ${OPTS:-O0 Os O2}; do
    cc="$BIN/clang --target=i8086-pc-win16 -mcmodel=$m --sysroot=$SYSROOT $ldflag -$o"
    t=$m-$o
    build "hello-$t" $cc "$tests/hello.c" -o "hello-$t.exe" &&
      run "hello-$t" "$WINE" "hello-$t.exe" one two
    build "api-$t" $cc "$tests/api.c" -o "api-$t.exe" &&
      run "api-$t" "$WINE" "api-$t.exe" abc
    if [ -z "$NO_CXX" ] && [ -f "$SYSROOT/lib/win16/$m/eh/libc++.a" ]; then
      cxx="$BIN/clang++ --target=i8086-pc-win16 -mcmodel=$m --sysroot=$SYSROOT $ldflag -$o"
      build "except-$t" $cxx -fexceptions "$tests/../cxx/except.cpp" -o "except-$t.exe" &&
        run "except-$t" "$WINE" "except-$t.exe"
    fi
    # A DLL, and a program that imports it. The DLL file name is its module
    # name, so it is the same for every build; each build has a directory.
    mkdir -p "dll-$t"
    if build "testdll-$t" $cc -shared "$tests/testdll.c" -o "dll-$t/TESTDLL.DLL" &&
       python3 "$scripts/win16-imports.py" "dll-$t/TESTDLL.DLL" >"dll-$t/testdll.lnk" &&
       build "dllapp-$t" $cc "$tests/dllapp.c" "-Wl,@dll-$t/testdll.lnk" -o "dll-$t/dllapp.exe"; then
      run "dll-$t" sh -c "cd dll-$t && '$WINE' dllapp.exe"
    fi
    if [ -n "$gui" ]; then
      build "gui-$t" $cc "$tests/gui.c" -o "gui-$t.exe" &&
        run "gui-$t" $gui "$WINE" "gui-$t.exe"
    fi
  done
done
echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
