#!/bin/sh
# Build the resident program test (tests/tsr) for each memory model and
# run it in DOSBox, which keeps a program resident as DOS does: tsr stays
# resident with _dos_keep, and check (small model) fills the free memory
# and calls the resident INT 60h handler twice.
#
#   BIN      directory with clang
#   SYSROOT  sysroot from scripts/build-sysroot.sh
#   LD       wlink or JWlink (default: found by clang on its own path)
#   DOSBOX   dosbox (default: dosbox on PATH)
#   MODELS   memory models (default: tiny small medium compact large huge)
#   OPTS     optimization levels (default: O0 Os)
: "${BIN:?set BIN}" "${SYSROOT:?set SYSROOT}"
DOSBOX=${DOSBOX:-dosbox}
tests=$(cd "$(dirname "$0")" && pwd)
out=${WORK:-$PWD/build-tsr-tests}
mkdir -p "$out"
cd "$out" || exit 1
ldflag=${LD:+--ld-path=$LD}
cc="$BIN/clang --target=i8086-pc-msdos --sysroot=$SYSROOT $ldflag"
pass=0
fail=0

if ! $cc -Os "$tests/tsr/check.c" -o check.exe >check.log 2>&1; then
  echo "FAIL check (build)"
  sed 's/^/    /' check.log | head -20
  exit 1
fi

for m in ${MODELS:-tiny small medium compact large huge}; do
  ext=exe
  [ "$m" = tiny ] && ext=com
  for o in ${OPTS:-O0 Os}; do
    name=tsr-$m-$o
    rm -rf "$name"
    mkdir "$name"
    if ! $cc -mcmodel=$m -$o "$tests/tsr/tsr.c" -o "$name/tsr.$ext" \
         >"$name.log" 2>&1; then
      fail=$((fail + 1))
      echo "FAIL $name (build)"
      sed 's/^/    /' "$name.log" | head -20
      continue
    fi
    cp check.exe "$name/"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 60 "$DOSBOX" \
      -noconsole -c "mount c $PWD/$name" -c "c:" \
      -c "tsr.$ext > out1.txt" -c "check > out2.txt" -c "check >> out2.txt" \
      -c "exit" >"$name.dosbox.log" 2>&1
    cat "$name"/OUT1.TXT "$name"/OUT2.TXT >"$name.out" 2>/dev/null
    # resident, then calls 1 and 2 answered, with the clock counted.
    if grep -q "^resident" "$name.out" &&
       grep -q "^ax=0 calls=1 " "$name.out" &&
       grep -q "^ax=0 calls=2 " "$name.out" &&
       ! grep -q "ticks=0 " "$name.out"; then
      pass=$((pass + 1))
    else
      fail=$((fail + 1))
      echo "FAIL $name"
      sed 's/^/    /' "$name.out" | head -20
    fi
  done
done
echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
