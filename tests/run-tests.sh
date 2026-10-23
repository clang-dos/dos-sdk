#!/bin/sh
# Build the tests for each memory model and optimization level, run them
# in emu2 and report which fail.
#
#   BIN      directory with clang
#   SYSROOT  sysroot from scripts/build-sysroot.sh
#   LD       wlink or JWlink (default: found by clang on its own path)
#   EMU2     emu2 (default: emu2 on PATH)
#   MODELS   memory models (default: tiny small medium compact large huge
#            huge16)
#   OPTS     optimization levels (default: O0 Os O2)
#   NO_CXX   set to skip the C++ test (for a sysroot without libc++)
: "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
EMU2=${EMU2:-emu2}
tests=$(cd "$(dirname "$0")" && pwd)
out=${WORK:-$PWD/build-tests}
mkdir -p "$out"
cd "$out" || exit 1
ldflag=${LD:+--ld-path=$LD}
# The compiler flags for a memory model. huge16 is the huge model with a
# 16-bit size_t, as in Open Watcom.
model_flags() {
  case $1 in
    huge16) echo "-mcmodel=huge -msize-t=16" ;;
    *) echo "-mcmodel=$1" ;;
  esac
}
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

for m in ${MODELS:-tiny small medium compact large huge huge16}; do
  ext=exe
  [ "$m" = tiny ] && ext=com
  for o in ${OPTS:-O0 Os O2}; do
    cc="$BIN/clang --target=i8086-pc-msdos $(model_flags $m) --sysroot=$SYSROOT $ldflag -$o"
    cxx="$BIN/clang++ --target=i8086-pc-msdos $(model_flags $m) --sysroot=$SYSROOT $ldflag -$o"
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
    # Unoptimized C++ with libc++ does not fit in 64 KiB of code, which is
    # all the tiny and compact models have. Models without libc++ in the
    # sysroot skip it too.
    if [ -z "$NO_CXX" ] && [ -f "$SYSROOT/lib/$m/libc++.a" ] &&
       ! { [ "$o" = O0 ] && { [ "$m" = tiny ] || [ "$m" = compact ]; }; }; then
      if $cxx "$tests/cxx/cxx.cpp" -o "cxx-$m-$o.$ext" >"cxx-$m-$o.log" 2>&1; then
        check "cxx-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "cxx-$m-$o.$ext"
      else
        fail=$((fail + 1))
        echo "FAIL cxx-$m-$o (build)"
        sed 's/^/    /' "cxx-$m-$o.log" | head -20
      fi
      if $cxx -frtti "$tests/cxx/oop.cpp" -o "oop-$m-$o.$ext" >"oop-$m-$o.log" 2>&1; then
        check "oop-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "oop-$m-$o.$ext"
      else
        fail=$((fail + 1))
        echo "FAIL oop-$m-$o (build)"
        sed 's/^/    /' "oop-$m-$o.log" | head -20
      fi
    fi
    # Variables over 32 KiB go outside DGROUP only in the models with far
    # data.
    case $m in
    compact | large | huge*)
      if $cc "$tests/fardata/fardata.c" -o "fardata-$m-$o.$ext" >"fardata-$m-$o.log" 2>&1; then
        check "fardata-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "fardata-$m-$o.$ext"
      else
        fail=$((fail + 1))
        echo "FAIL fardata-$m-$o (build)"
        sed 's/^/    /' "fardata-$m-$o.log" | head -20
      fi
      ;;
    esac
    # __far and friends need segment fixups, which a .COM program lacks.
    if [ "$m" != tiny ]; then
      for t in farkw farfn pascal; do
        if $cc "$tests/fardata/$t.c" -o "$t-$m-$o.$ext" >"$t-$m-$o.log" 2>&1; then
          check "$t-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "$t-$m-$o.$ext"
        else
          fail=$((fail + 1))
          echo "FAIL $t-$m-$o (build)"
          sed 's/^/    /' "$t-$m-$o.log" | head -20
        fi
      done
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
