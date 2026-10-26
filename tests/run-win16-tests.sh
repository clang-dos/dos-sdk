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
#   VERSIONS Windows versions for the versioned triples, as in
#            i8086-pc-win162.03 (default: 1.01 2.03 3.00 3.10). Their
#            programs are checked with scripts/ne-check.py: real-mode safe
#            before 3.10, protected mode only from 3.10. Wine runs them
#            reporting the closest Windows version it has (2.0, 3.0, 3.1);
#            it runs every program in protected mode.
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
    # The near and far heaps and string functions (GlobalAlloc and the
    # local heap), as C and as C++.
    build "nearfar-$t" $cc "$tests/../libc/nearfar.c" -o "nearfar-$t.exe" &&
      run "nearfar-$t" "$WINE" "nearfar-$t.exe"
    if [ -z "$NO_CXX" ] && [ -f "$SYSROOT/lib/win16/$m/libc++.a" ]; then
      build "nearfarxx-$t" "$BIN/clang++" --target=i8086-pc-win16 -mcmodel=$m \
          --sysroot="$SYSROOT" $ldflag -$o -x c++ "$tests/../libc/nearfar.c" \
          -o "nearfarxx-$t.exe" &&
        run "nearfarxx-$t" "$WINE" "nearfarxx-$t.exe"
    fi
    # getentropy and arc4random; two runs must get different seeds.
    if build "random-$t" $cc "$tests/../libc/random.c" -o "random-$t.exe"; then
      run "random-$t" "$WINE" "random-$t.exe"
      mv "random-$t.out" "random-$t.out1"
      run "random-$t" "$WINE" "random-$t.exe"
      if grep seed "random-$t.out1" >"random-$t.s1" &&
         grep seed "random-$t.out" >"random-$t.s2" &&
         ! cmp -s "random-$t.s1" "random-$t.s2"; then
        pass=$((pass + 1))
      else
        fail=$((fail + 1))
        echo "FAIL random-$t (the same seed twice)"
      fi
    fi
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
    # A C++ DLL with objects on its stack. Unoptimized, its libc++ code
    # does not fit in the one code segment of the compact model.
    if [ -z "$NO_CXX" ] && [ -f "$SYSROOT/lib/win16/$m/libc++.a" ] &&
       ! { [ "$o" = O0 ] && [ "$m" = compact ]; }; then
      mkdir -p "cxxdll-$t"
      cxx="$BIN/clang++ --target=i8086-pc-win16 -mcmodel=$m --sysroot=$SYSROOT $ldflag -$o"
      if build "cxxdll-$t" $cxx -shared "$tests/cxxdll.cpp" -o "cxxdll-$t/CXXDLL.DLL" &&
         python3 "$scripts/win16-imports.py" "cxxdll-$t/CXXDLL.DLL" >"cxxdll-$t/cxxdll.lnk" &&
         build "cxxdllapp-$t" $cc "$tests/cxxdllapp.c" "-Wl,@cxxdll-$t/cxxdll.lnk" -o "cxxdll-$t/app.exe"; then
        run "cxxdll-$t" sh -c "cd cxxdll-$t && '$WINE' app.exe"
      fi
    fi
    # Resources, compiled by llvm-rc.
    if build "rc-$t" "$BIN/llvm-rc" -win16 -I "$SYSROOT/include" -I "$tests" \
         -FO "res-$t.res" "$tests/res.rc" &&
       build "res-$t" $cc -I "$tests" "$tests/res.c" "res-$t.res" -o "res-$t.exe"; then
      run "res-$t" $gui "$WINE" "res-$t.exe"
    fi
    if [ -n "$gui" ]; then
      build "gui-$t" $cc "$tests/gui.c" -o "gui-$t.exe" &&
        run "gui-$t" $gui "$WINE" "gui-$t.exe"
    fi
  done
done

# The versioned triples. ne-check.py checks the header and, for real mode,
# that no relocation needs a segment to stay where it was loaded.
check() {
  # check NAME FILE ARGS...: ne-check.py's report, in NAME.ne
  name=$1 file=$2
  shift 2
  if python3 "$scripts/ne-check.py" "$@" "$file" >"$name.ne" 2>&1; then
    return 0
  fi
  fail=$((fail + 1))
  echo "FAIL $name (ne-check)"
  sed 's/^/    /' "$name.ne" | head -20
  return 1
}

# wineversion VERSION: the Windows version Wine reports for it.
wineversion() {
  case $1 in
    1.* | 2.*) echo win20 ;;
    3.0*) echo win30 ;;
    *) echo win31 ;;
  esac
}

# winrun NAME VERSION EXE ARGS...: run EXE with Wine reporting VERSION.
winrun() {
  name=$1 ver=$2 exe=$3
  shift 3
  "$WINE" reg add "HKCU\\Software\\Wine\\AppDefaults\\$(basename "$exe")" \
    /v Version /d "$(wineversion "$ver")" /f >/dev/null 2>&1
  run "$name" "$WINE" "$exe" "$@"
}

for v in ${VERSIONS:-1.01 2.03 3.00 3.10}; do
  case $v in
    3.1* | [4-9].*) mode=--protmode ;;
    *) mode=--real-mode ;;
  esac
  for m in ${VERSION_MODELS:-small medium large}; do
    o=Os
    cc="$BIN/clang --target=i8086-pc-win16$v -mcmodel=$m --sysroot=$SYSROOT $ldflag -$o"
    t=win$v-$m
    build "hello-$t" $cc "$tests/hello.c" -o "hello-$t.exe" &&
      check "hello-$t" "hello-$t.exe" --version "$v" $mode &&
      winrun "hello-$t" "$v" "hello-$t.exe" one two
    build "api-$t" $cc "$tests/api.c" -o "api-$t.exe" &&
      check "api-$t" "api-$t.exe" --version "$v" $mode &&
      winrun "api-$t" "$v" "api-$t.exe" abc
    if [ -n "$gui" ]; then
      build "gui-$t" $cc "$tests/gui.c" -o "gui-$t.exe" &&
        check "gui-$t" "gui-$t.exe" --version "$v" $mode &&
        run "gui-$t" $gui "$WINE" "gui-$t.exe"
    fi
    # Resources for that version: before 3.0, menus have the old format.
    if build "rc-$t" "$BIN/llvm-rc" -win16-version "$v" -I "$SYSROOT/include" \
         -I "$tests" -FO "res-$t.res" "$tests/res.rc" &&
       build "res-$t" $cc -I "$tests" "$tests/res.c" "res-$t.res" -o "res-$t.exe" &&
       check "res-$t" "res-$t.exe" --version "$v" $mode; then
      winrun "res-$t" "$v" "res-$t.exe"
    fi
    # Before 3.0, Windows loads an imported module from MODULE.EXE.
    case $v in
      [12].*) dll=TESTDLL.EXE ;;
      *) dll=TESTDLL.DLL ;;
    esac
    mkdir -p "dll-$t"
    if build "testdll-$t" $cc -shared "$tests/testdll.c" -o "dll-$t/$dll" &&
       check "testdll-$t" "dll-$t/$dll" --version "$v" $mode &&
       python3 "$scripts/win16-imports.py" "dll-$t/$dll" >"dll-$t/testdll.lnk" &&
       build "dllapp-$t" $cc "$tests/dllapp.c" "-Wl,@dll-$t/testdll.lnk" -o "dll-$t/dllapp.exe" &&
       check "dllapp-$t" "dll-$t/dllapp.exe" --version "$v" $mode; then
      run "dll-$t" sh -c "cd dll-$t && '$WINE' dllapp.exe"
    fi
  done
done

echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
