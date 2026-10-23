#!/bin/sh
# Build the tests for each memory model and optimization level, run them
# in emu2 and report which fail.
#
#   BIN      directory with clang
#   SYSROOT  sysroot from scripts/build-sysroot.sh
#   LD       wlink or JWlink (default: found by clang on its own path)
#   EMU2     emu2 (default: emu2 on PATH)
#   MODELS   memory models (default: tiny small medium compact)
#   OPTS     optimization levels (default: O0 Os O2)
#   NO_CXX   set to skip the C++ test (for a sysroot without libc++)
: "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
EMU2=${EMU2:-emu2}
tests=$(cd "$(dirname "$0")" && pwd)
out=${WORK:-$PWD/build-tests}
mkdir -p "$out"
cd "$out" || exit 1
ldflag=${LD:+--ld-path=$LD}
pass=0
fail=0

check() {
  # check NAME EXPECTED-OUTPUT-PATTERN COMMAND...
  name=$1 pattern=$2
  shift 2
  if "$@" >"$name.out" 2>&1 && grep -q -- "$pattern" "$name.out"; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1))
    echo "FAIL $name"
    sed 's/^/    /' "$name.out" | head -20
  fi
}

for m in ${MODELS:-tiny small medium compact}; do
  ext=exe
  [ "$m" = tiny ] && ext=com
  for o in ${OPTS:-O0 Os O2}; do
    cc="$BIN/clang --target=i8086-pc-msdos -mcmodel=$m --sysroot=$SYSROOT $ldflag -$o"
    cxx="$BIN/clang++ --target=i8086-pc-msdos -mcmodel=$m --sysroot=$SYSROOT $ldflag -$o"
    for t in libc hello stdin textmode; do
      if ! $cc "$tests/libc/$t.c" -o "$t-$m-$o.$ext" >"$t-$m-$o.log" 2>&1; then
        fail=$((fail + 1))
        echo "FAIL $t-$m-$o (build)"
        sed 's/^/    /' "$t-$m-$o.log" | head -20
        continue
      fi
    done
    check "libc-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "libc-$m-$o.$ext"
    check "hello-$m-$o" "argv\[2\]=<b>" timeout 30 "$EMU2" "hello-$m-$o.$ext" a b
    printf 'first\nsecond line\n' >stdin.txt
    check "stdin-$m-$o" "lines=2" sh -c "timeout 30 '$EMU2' 'stdin-$m-$o.$ext' <stdin.txt"
    check "textmode-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "textmode-$m-$o.$ext"
    # Unoptimized C++ with libc++ does not fit in a 64 KiB .COM program.
    # Models without libc++ in the sysroot skip it too.
    if [ -z "$NO_CXX" ] && [ -f "$SYSROOT/lib/$m/libc++.a" ] &&
       ! { [ "$m" = tiny ] && [ "$o" = O0 ]; }; then
      if $cxx "$tests/cxx/cxx.cpp" -o "cxx-$m-$o.$ext" >"cxx-$m-$o.log" 2>&1; then
        check "cxx-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "cxx-$m-$o.$ext"
      else
        fail=$((fail + 1))
        echo "FAIL cxx-$m-$o (build)"
        sed 's/^/    /' "cxx-$m-$o.log" | head -20
      fi
    fi
    if $cc -I"$tests/builtins" "$tests/builtins/rtt.c" -o "rtt-$m-$o.$ext" >"rtt-$m-$o.log" 2>&1; then
      check "builtins-$m-$o" "ALL PASSED" timeout 60 "$EMU2" "rtt-$m-$o.$ext"
    else
      fail=$((fail + 1))
      echo "FAIL rtt-$m-$o (build)"
      sed 's/^/    /' "rtt-$m-$o.log" | head -20
    fi
  done
done
echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
