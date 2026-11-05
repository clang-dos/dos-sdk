#!/bin/sh
# Smoke test for a clang-dos package: builds hello world (and C++ where the
# target has it) for every target, checks the outputs with the package's
# checkers, and runs them in emu2 (DOS), Wine (Windows), vamos (AmigaOS)
# and Hatari with EmuTOS (Atari) when available.
#
#   sh smoke-test.sh                 test the package this script is in
#   PKG=/path/to/package sh smoke-test.sh
#   HOSTRUN=wine sh smoke-test.sh    test a Windows-hosted package under Wine
#   EMU2=/path/to/emu2 WINE=wine VAMOS=vamos
#   EMUTOS_DIR=dir (with hatari on PATH)
#                                    runners for the outputs (optional)
#   OUT=dir                          where to build (default: ./smoke-out)
set -u
PKG=${PKG:-$(cd "$(dirname "$0")" && pwd)}
HOSTRUN=${HOSTRUN:-}
EMU2=${EMU2:-$(command -v emu2 || true)}
WINE=${WINE:-$(command -v wine || true)}
VAMOS=${VAMOS:-$(command -v vamos || true)}
OUT=${OUT:-$PWD/smoke-out}
PY=${PYTHON:-python3}
export WINEDEBUG=${WINEDEBUG:--all}
ex=$PKG/examples
tools=$PKG/share/clang-dos
mkdir -p "$OUT"
cp "$ex"/* "$OUT/"
cp "$tools"/amiga-examples/window.c "$tools"/amiga-examples/alibtest.c "$OUT/"
cd "$OUT" || exit 1
if [ -n "$HOSTRUN" ]; then
  CC="$HOSTRUN $PKG/bin/clang.exe" CXX="$HOSTRUN $PKG/bin/clang++.exe"
else
  CC="$PKG/bin/clang" CXX="$PKG/bin/clang++"
fi
pass=0 fail=0 skip=0
ok() { pass=$((pass + 1)); echo "PASS $1"; }
bad() { fail=$((fail + 1)); echo "FAIL $1"; [ -f "$2" ] && sed 's/^/    /' "$2" | head -15; }
# build NAME OUTPUT COMPILER ARGS...
build() {
  n=$1 o=$2; shift 2
  if "$@" -o "$o" >"$n.log" 2>&1 && [ -s "$o" ]; then ok "build $n"; return 0; fi
  bad "build $n" "$n.log"; return 1
}
# check NAME EXPECTED COMMAND...: the output must contain EXPECTED
check() {
  n=$1 want=$2; shift 2
  if timeout 60 "$@" >"$n.out" 2>&1 && grep -q "$want" "$n.out"; then ok "$n"; else bad "$n" "$n.out"; fi
}
rundos() {
  if [ -n "$EMU2" ]; then check "run $1" "$2" "$EMU2" "$3" smoke; else skip=$((skip + 1)); fi
}
runamiga() {
  if [ -n "$VAMOS" ]; then check "run $1" "$2" "$VAMOS" -q "$3" smoke; else skip=$((skip + 1)); fi
}
# Hatari needs EmuTOS (free, GPL); no Atari ROM is used.
runatari() {
  if command -v hatari >/dev/null && [ -n "${EMUTOS_DIR:-}" ]; then
    check "run $1" "$2" sh "$tools/hatari-run.sh" -m "$4" -w "$2" -T 50 "$3"
  else
    skip=$((skip + 1))
  fi
}
# prgcheck NAME FILE: a GEMDOS executable starts with 0x601A
prgcheck() {
  if [ "$(od -An -tx1 -N2 "$2" | tr -d ' ')" = 601a ]; then ok "prg $1"; else bad "prg $1" /dev/null; fi
}
runwin() {
  if [ -n "$WINE" ]; then check "run $1" "$2" "$WINE" "$3" smoke; else skip=$((skip + 1)); fi
}

echo "== 16-bit DOS (i8086-msdos)"
for m in tiny small medium compact large huge huge16; do
  case $m in tiny) e=com ;; *) e=exe ;; esac
  case $m in huge16) mf="-mcmodel=huge -msize-t=16" ;; *) mf="-mcmodel=$m" ;; esac
  build "dos-$m" "dos-$m.$e" $CC --config=dos.cfg $mf -Os hello.c &&
    rundos "dos-$m" "Hello, world from smoke! 42 123456789 3.25" "dos-$m.$e"
done
# PDCurses writes to video memory, which emu2 keeps only with a terminal:
# run it under script(1), where its output has cursor movement in it.
for m in small large; do
  build "dos-$m-curses" "dos-$m-curses.exe" $CC --config=dos.cfg -mcmodel=$m -Os curses.c -lcurses || continue
  if [ -n "$EMU2" ] && command -v script >/dev/null; then
    check "run dos-$m-curses" "curses:.*ok" \
      sh -c "script -qec '$EMU2 dos-$m-curses.exe' /dev/null </dev/null"
  else
    skip=$((skip + 1))
  fi
done
build "dos-small-cxx" dos-small-cxx.exe $CXX --config=dos.cfg -mcmodel=small -Os hello.cpp &&
  rundos "dos-small-cxx" "Hello, world from smoke! 42" dos-small-cxx.exe
# std::atomic on an 8086: calls to compiler-rt that turn interrupts off.
for m in small large; do
  build "dos-$m-atomic" "dos-$m-atomic.exe" $CXX --config=dos.cfg -mcmodel=$m -Os atomic.cpp &&
    rundos "dos-$m-atomic" "atomics ok 42 123456789" "dos-$m-atomic.exe"
done
# Turbo Vision waits for the keyboard, so it is only built.
for m in medium large huge huge16; do
  case $m in huge16) mf="-mcmodel=huge -msize-t=16" ;; *) mf="-mcmodel=$m" ;; esac
  build "dos-$m-tvision" "dos-$m-tvision.exe" $CXX --config=dos.cfg $mf -Os tvhello.cpp -ltvision
done
build "dos-large-eh" dos-large-eh.exe $CXX --config=dos.cfg -mcmodel=large -Os -fexceptions -frtti except.cpp &&
  rundos "dos-large-eh" "caught boom, 1 destructor ran" dos-large-eh.exe

echo "== 16-bit Windows (i8086-win16)"
for v in win16 win31 win30 win2 win1; do
  build "$v" "$v.exe" $CC --config=$v.cfg -Os hello.c || continue
  case $v in
  win30) chk="--real-mode --version 3.00" ;;
  win2) chk="--real-mode --version 2.03" ;;
  win1) chk="--real-mode --version 1.01" ;;
  win31) chk="--protmode --version 3.10" ;;
  *) chk="--protmode" ;;
  esac
  check "ne-check $v" "" $PY "$tools/ne-check.py" $chk "$v.exe"
  runwin "$v" "Hello, world from smoke! 42 123456789 3.25" "$v.exe"
done
build "win16-large-cxx" win16-large-cxx.exe $CXX --config=win16.cfg -mcmodel=large -Os hello.cpp &&
  runwin "win16-large-cxx" "Hello, world from smoke! 42" win16-large-cxx.exe
build "win16-large-atomic" win16-large-atomic.exe $CXX --config=win16.cfg -mcmodel=large -Os atomic.cpp &&
  runwin "win16-large-atomic" "atomics ok 42 123456789" win16-large-atomic.exe

echo "== 32-bit Windows"
for v in win32s nt31 nt35 nt351 nt4 win95 win95-crtdll win98 winme win2000 winxp; do
  build "$v" "$v.exe" $CC --config=$v.cfg -O2 hello.c || continue
  case $v in
  win32s | nt31) chk="--nt 3.10" ;;
  nt35) chk="--nt 3.50" ;;
  nt351) chk="--nt 3.51" ;;
  win2000) chk="--nt 5.0" ;;
  winxp) chk="--nt 5.1" ;;
  *) chk="" ;;
  esac
  check "check-imports $v" "" $PY "$tools/check-imports.py" $chk "$v.exe"
  runwin "$v" "Hello, world from smoke! 42 123456789 3.25" "$v.exe"
done
# C++ (libc++) on every 32-bit Windows before Vista.
for v in win32s nt31 nt35 nt351 nt4 win95 win95-crtdll win98 winme win2000 winxp; do
  case $v in
  win32s | nt31) chk="--nt 3.10" ;;
  nt35) chk="--nt 3.50" ;;
  nt351) chk="--nt 3.51" ;;
  win2000) chk="--nt 5.0" ;;
  winxp) chk="--nt 5.1" ;;
  *) chk="" ;;
  esac
  build "$v-cxx" "$v-cxx.exe" $CXX --config=$v.cfg -O2 hello.cpp &&
    { check "check-imports $v-cxx" "" $PY "$tools/check-imports.py" $chk "$v-cxx.exe"; runwin "$v-cxx" "Hello, world from smoke! 42" "$v-cxx.exe"; }
  build "$v-eh" "$v-eh.exe" $CXX --config=$v.cfg -O2 except.cpp &&
    { check "check-imports $v-eh" "" $PY "$tools/check-imports.py" $chk "$v-eh.exe"; runwin "$v-eh" "caught boom, 1 destructor ran" "$v-eh.exe"; }
  build "$v-threads" "$v-threads.exe" $CXX --config=$v.cfg -O2 threads.cpp &&
    { check "check-imports $v-threads" "" $PY "$tools/check-imports.py" $chk "$v-threads.exe"; runwin "$v-threads" "threads total 10000 OK" "$v-threads.exe"; }
done

echo "== 32-bit Windows with picolibc (no C runtime DLL)"
# nocrt NAME FILE: the program imports no C runtime DLL
nocrt() {
  if grep -qiE 'msvcrt|crtdll|ucrtbase' "$2"; then bad "no C runtime DLL $1" /dev/null; else ok "no C runtime DLL $1"; fi
}
for v in win32-picolibc nt31-picolibc; do
  chk=; [ $v = nt31-picolibc ] && chk="--nt 3.10"
  for p in hello.c:"Hello, world from smoke! 42 123456789 3.25" \
      hello.cpp:"Hello, world from smoke! 42" \
      except.cpp:"caught boom, 1 destructor ran" \
      threads.cpp:"threads total 10000 OK"; do
    src=${p%%:*} want=${p#*:} t=$v-${src%.*}-${src##*.}
    case $src in *.cpp) cc=$CXX ;; *) cc=$CC ;; esac
    build "$t" "$t.exe" $cc --config=$v.cfg -O2 "$src" || continue
    nocrt "$t" "$t.exe"
    check "check-imports $t" "" $PY "$tools/check-imports.py" $chk "$t.exe"
    runwin "$t" "$want" "$t.exe"
  done
done

echo "== Windows Vista, 7, 8, 8.1, 10 (32 and 64-bit)"
for v in vista win7 win8 win81 win10 vista-x64 win7-x64 win8-x64 win81-x64 win10-x64; do
  build "$v" "$v.exe" $CC --config=$v.cfg -O2 hello.c &&
    runwin "$v" "Hello, world from smoke! 42 123456789 3.25" "$v.exe"
  build "$v-cxx" "$v-cxx.exe" $CXX --config=$v.cfg -O2 hello.cpp &&
    runwin "$v-cxx" "Hello, world from smoke! 42" "$v-cxx.exe"
done
for v in win7 win10-x64; do
  build "$v-eh" "$v-eh.exe" $CXX --config=$v.cfg -O2 except.cpp &&
    runwin "$v-eh" "caught boom, 1 destructor ran" "$v-eh.exe"
  build "$v-threads" "$v-threads.exe" $CXX --config=$v.cfg -O2 threads.cpp &&
    runwin "$v-threads" "threads total 10000 OK" "$v-threads.exe"
done

echo "== AmigaOS (m68k-amigaos)"
for cpu in 68000 68020 68040; do
  build "amiga-$cpu" "amiga-$cpu" $CC --config=amiga.cfg -mcpu=$cpu -O2 hello.c &&
    runamiga "amiga-$cpu" "Hello, world from smoke! 42 123456789 3.25" "amiga-$cpu"
done

# OS calls through the AmigaOS headers and libamiga.a.
build "amiga-window" "amiga-window" $CC --config=amiga.cfg -O2 window.c
build "amiga-alibtest" "amiga-alibtest" $CC --config=amiga.cfg -O2 alibtest.c

echo "== Atari TOS (m68k-atari-tos)"
for t in 68000:st 68030:falcon; do
  cpu=${t%%:*} m=${t#*:}
  build "atari-$cpu" "atari-$cpu.tos" $CC --config=atari.cfg -mcpu=$cpu -O2 hello.c &&
    prgcheck "atari-$cpu" "atari-$cpu.tos" &&
    runatari "atari-$cpu" "Hello, world from .! 42 123456789 3.25" "atari-$cpu.tos" "$m"
done
build "atari-gem" atari-gem.prg $CC --config=atari.cfg -O2 "$tools/atari-examples/gemhello.c" -lgem &&
  prgcheck "atari-gem" atari-gem.prg

echo "== Microsoft SDKs and DDKs (import-ms-sdk.py with fake kits)"
if [ -z "$HOSTRUN" ] && [ -f "$tools/ms-sdk/run-test.sh" ]; then
  check "ms-sdk import" "ms-sdk import test passed" \
    env BIN="$PKG/bin" WORK="$OUT/ms-sdk" PYTHON="$PY" sh "$tools/ms-sdk/run-test.sh"
else
  skip=$((skip + 1))
fi

echo "== $pass passed, $fail failed, $skip runs skipped (no emu2, Wine, vamos or Hatari)"
[ "$fail" = 0 ]
