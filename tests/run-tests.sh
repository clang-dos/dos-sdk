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
#   NO_CXX   set to skip the C++ tests (for a sysroot without libc++)
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

# build_run NAME PATTERN INPUT COMMAND...: build NAME.$ext with COMMAND,
# run it, with the file INPUT as standard input unless INPUT is empty, and
# check its output.
build_run() {
  name=$1 pattern=$2 input=$3
  shift 3
  if "$@" -o "$name.$ext" >"$name.log" 2>&1; then
    if [ -n "$input" ]; then
      check "$name" "$pattern" sh -c "timeout 30 '$EMU2' '$name.$ext' <'$input'"
    else
      check "$name" "$pattern" timeout 30 "$EMU2" "$name.$ext"
    fi
  else
    fail=$((fail + 1))
    echo "FAIL $name (build)"
    sed 's/^/    /' "$name.log" | head -20
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
    # The near and far heaps and string functions, as C and as C++.
    build_run "nearfar-$m-$o" "ALL PASSED" "" $cc "$tests/libc/nearfar.c"
    # emu2 does not set file attributes or times.
    build_run "fsys-$m-$o" "ALL PASSED" "" $cc -DEMU2 "$tests/libc/fsys.c"
    # Models without libc++ in the sysroot skip the C++ tests.
    if [ -z "$NO_CXX" ] && [ -f "$SYSROOT/lib/$m/libc++.a" ]; then
      build_run "nearfarxx-$m-$o" "ALL PASSED" "" $cxx -x c++ "$tests/libc/nearfar.c"
      build_run "cxx-$m-$o" "ALL PASSED" "" $cxx "$tests/cxx/cxx.cpp"
      build_run "oop-$m-$o" "ALL PASSED" "" $cxx -frtti "$tests/cxx/oop.cpp"
      if [ -f "$SYSROOT/lib/$m/eh/libc++.a" ]; then
        build_run "except-$m-$o" "ALL PASSED" "" $cxx -fexceptions "$tests/cxx/except.cpp"
      fi
      case $m in
      tiny | small | compact)
        # All of iostreams, <filesystem> and the demangler are more code
        # than these models have. Output with cout fits in each; input and
        # string streams too only in small with optimization (62 KiB of
        # code at -Os). Tiny has 64 KiB for code and data together, and in
        # compact far pointers make the code a third larger.
        build_run "iohello-$m-$o" "ALL PASSED" "" $cxx "$tests/cxx/iohello.cpp"
        if [ "$m" = small ] && [ "$o" != O0 ]; then
          build_run "iobasic-$m-$o" "ALL PASSED" stdin.txt $cxx "$tests/cxx/iobasic.cpp"
        fi
        ;;
      *)
        build_run "iostream-$m-$o" "ALL PASSED" stdin.txt $cxx "$tests/cxx/iostream.cpp"
        build_run "wchar-$m-$o" "ALL PASSED" "" $cxx "$tests/cxx/wchar.cpp"
        build_run "fs-$m-$o" "ALL PASSED" "" $cxx -DEMU2 "$tests/cxx/fs.cpp"
        # The demangler's parser takes about 5 KiB of stack, more than the
        # 4 KiB default.
        build_run "demangle-$m-$o" "ALL PASSED" "" $cxx -frtti "$tests/cxx/demangle.cpp" -Wl,option,stack=8192
        ;;
      esac
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
      if $cc "$tests/libc/halloc.c" -o "halloc-$m-$o.$ext" >"halloc-$m-$o.log" 2>&1; then
        check "halloc-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "halloc-$m-$o.$ext"
      else
        fail=$((fail + 1))
        echo "FAIL halloc-$m-$o (build)"
        sed 's/^/    /' "halloc-$m-$o.log" | head -20
      fi
      # A .COM program's stack is the top of its segment, not a STACK
      # segment the linker can make larger.
      if $cc "$tests/libc/bigstack.c" -Wl,option,stack=16384 \
           -o "bigstack-$m-$o.$ext" >"bigstack-$m-$o.log" 2>&1; then
        check "bigstack-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "bigstack-$m-$o.$ext"
      else
        fail=$((fail + 1))
        echo "FAIL bigstack-$m-$o (build)"
        sed 's/^/    /' "bigstack-$m-$o.log" | head -20
      fi
      for t in farkw farfn pascal winprol; do
        flags=
        [ $t = winprol ] && flags=-mwindows-prologue=exported
        if $cc $flags "$tests/fardata/$t.c" -o "$t-$m-$o.$ext" >"$t-$m-$o.log" 2>&1; then
          check "$t-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "$t-$m-$o.$ext"
        else
          fail=$((fail + 1))
          echo "FAIL $t-$m-$o (build)"
          sed 's/^/    /' "$t-$m-$o.log" | head -20
        fi
      done
    fi
    if $cc "$tests/libc/intr.c" -o "intr-$m-$o.$ext" >"intr-$m-$o.log" 2>&1; then
      check "intr-$m-$o" "ALL PASSED" timeout 30 "$EMU2" "intr-$m-$o.$ext"
    else
      fail=$((fail + 1))
      echo "FAIL intr-$m-$o (build)"
      sed 's/^/    /' "intr-$m-$o.log" | head -20
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
