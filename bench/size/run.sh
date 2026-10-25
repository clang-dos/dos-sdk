#!/bin/sh
# Build each size benchmark with each 16-bit DOS C++ compiler that is
# available, in the tiny, small and large models, check that it runs, and
# record its size. See README.md.
#
# Toolchains (each one is skipped when its variable is unset):
#   BIN, SYSROOT   clang-dos: the directory with clang++ and a sysroot
#   WATCOM         Open Watcom 2 (wpp/wcl), installed at $WATCOM
#   DMC            Digital Mars C++: the dm directory (bin/dmc.exe, lib, include),
#                  run through Wine on non-Windows hosts
#   IA16           gcc-ia16: the directory with ia16-elf-g++
# Other settings:
#   TUNED=1        also build "clang-tuned": clang-dos with the size experiments
#                  in experiments/ (abort messages without vfprintf, and the
#                  integer-only printf for programs that call printf)
#   EMU2           emu2, to run each program and check its output (optional)
#   MODELS         default "tiny small large"
#   PROGS          default: every src/*.c and src/*.cpp
#   OUT            output directory, default ./out; results go to $OUT/results.csv
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${OUT:-$PWD/out}
MODELS=${MODELS:-"tiny small large"}
EMU2=${EMU2:-}
BIN=${BIN:-}
SYSROOT=${SYSROOT:-}
WATCOM=${WATCOM:-}
DMC=${DMC:-}
IA16=${IA16:-}
if [ -z "${PROGS:-}" ]; then
  PROGS=$(cd "$HERE/src" && ls *.c *.cpp | sed 's/\.[^.]*$//' | sort -u)
fi

mkdir -p "$OUT"
CSV=$OUT/results.csv
echo "toolchain,model,program,status,file,code,data,bss,flags" > "$CSV"

src_of() {
  if [ -f "$HERE/src/$1.cpp" ]; then echo "$HERE/src/$1.cpp"; else echo "$HERE/src/$1.c"; fi
}

# Programs that need exceptions turned on.
needs_eh() {
  case $1 in except) return 0;; esac
  return 1
}

# record TOOLCHAIN MODEL PROG EXE MAP FLAGS: check the program and log its size.
record() {
  tc=$1 m=$2 p=$3 exe=$4 map=$5 flags=$6
  if [ ! -f "$exe" ]; then
    echo "$tc,$m,$p,build-failed,,,,,\"$flags\"" >> "$CSV"
    echo "  $tc $m $p: build failed"
    return
  fi
  status=ok
  if [ -n "$EMU2" ]; then
    d=$(dirname "$exe")
    if (cd "$d" && "$EMU2" "$(basename "$exe")" < /dev/null 2>&1) | tr -d '\r' > "$exe.out" &&
       cmp -s "$exe.out" "$HERE/expected/$p.txt"; then
      status=ok
    else
      status=wrong-output
    fi
  fi
  com=; case $exe in *.com) com=--com;; esac
  sizes=$(python3 "$HERE/mapsize.py" $com "$map" 2>/dev/null || echo ", , ")
  set -- $sizes
  echo "$tc,$m,$p,$status,$(wc -c < "$exe" | tr -d ' '),${1:-},${2:-},${3:-},\"$flags\"" >> "$CSV"
  echo "  $tc $m $p: $status $(wc -c < "$exe" | tr -d ' ') bytes (code ${1:-?} data ${2:-?} bss ${3:-?})"
}

ext_of() { if [ "$1" = tiny ]; then echo com; else echo exe; fi; }

# clang-dos ------------------------------------------------------------------
# run_clang NAME [tuned]
run_clang() {
  [ -n "$BIN" ] && [ -n "$SYSROOT" ] || return 0
  tc=$1 tuned=${2:-}
  echo "$tc: $("$BIN/clang" --version | head -1)"
  for m in $MODELS; do
    d=$OUT/$tc/$m; mkdir -p "$d"
    for p in $PROGS; do
      s=$(src_of $p) e=$(ext_of $m)
      cc="$BIN/clang++"; case $s in *.c) cc="$BIN/clang";; esac
      flags="-Os"
      needs_eh $p && flags="$flags -fexceptions"
      extra=
      if [ -n "$tuned" ]; then
        case $s in *.cpp) extra="$HERE/experiments/quietabort.cpp";; esac
        # picolibc's integer printf (no floating point or long long), as
        # Open Watcom links when a program has no floating point.
        grep -q printf "$s" && flags="$flags -Wl,alias,_vfprintf=___i_vfprintf"
      fi
      rm -f "$d/$p.$e"
      $cc --target=i8086-pc-msdos --sysroot="$SYSROOT" -mcmodel=$m $flags \
        "$s" $extra -o "$d/$p.$e" -Wl,option,map="$d/$p.map" > "$d/$p.log" 2>&1
      record $tc $m $p "$d/$p.$e" "$d/$p.map" "$flags${extra:+ +quietabort.cpp}"
    done
  done
}

# Open Watcom 2 ---------------------------------------------------------------
run_watcom() {
  [ -n "$WATCOM" ] || return 0
  PATH=$WATCOM/binl64:$WATCOM/binl:$PATH INCLUDE=$WATCOM/h EDPATH=$WATCOM/eddat
  export PATH INCLUDE EDPATH WATCOM
  echo "Open Watcom: $(wpp 2>&1 | sed -n 2p)"
  for m in $MODELS; do
    d=$OUT/watcom/$m; mkdir -p "$d"
    case $m in
      tiny) mf="-mt -bcl=com";;
      small) mf="-ms -bt=dos";;
      medium) mf="-mm -bt=dos";;
      compact) mf="-mc -bt=dos";;
      large) mf="-ml -bt=dos";;
      *) continue;;
    esac
    for p in $PROGS; do
      s=$(src_of $p) e=$(ext_of $m)
      flags="-os -s"
      # Open Watcom's STL headers need exceptions too.
      case $p in except|strvec) flags="$flags -xs";; esac
      rm -f "$d/$p.$e"
      (cd "$d" && wcl -q $mf $flags -fe="$p.$e" -fm="$p.map" "$s") > "$d/$p.log" 2>&1
      record watcom $m $p "$d/$p.$e" "$d/$p.map" "$flags"
    done
  done
}

# Digital Mars ----------------------------------------------------------------
run_dmc() {
  [ -n "$DMC" ] || return 0
  if command -v wine > /dev/null 2>&1; then
    WINE=wine; WINEDEBUG=-all; export WINEDEBUG
    win() { winepath -w "$1" 2>/dev/null; }
  else
    WINE=; win() { echo "$1"; }
  fi
  echo "Digital Mars: $($WINE "$DMC/bin/dmc.exe" 2>&1 | head -1 | tr -d '\r')"
  inc=$(win "$HERE/compat/dmc16")
  for m in $MODELS; do
    d=$OUT/dmc/$m; mkdir -p "$d"
    case $m in tiny) mf=-mt;; small) mf=-ms;; medium) mf=-mm;; compact) mf=-mc;; large) mf=-ml;; *) continue;; esac
    for p in $PROGS; do
      s=$(src_of $p) e=$(ext_of $m)
      flags="-o+space"
      case $p in except|strvec) flags="$flags -Ae";; esac
      rm -f "$d/$p.$e"
      cp "$s" "$d/"
      (cd "$d" && $WINE "$DMC/bin/dmc.exe" $mf $flags "-I$inc" "$(basename "$s")" \
        -o"$p.$e" -L/MAP) > "$d/$p.log" 2>&1
      [ -f "$d/$p.MAP" ] && mv "$d/$p.MAP" "$d/$p.map"
      # The linker leaves an .EXE; Digital Mars runs the DOS EXE2BIN on it.
      if [ $m = tiny ] && [ -f "$d/$p.$e" ]; then
        mv "$d/$p.$e" "$d/$p.mz"
        python3 "$HERE/exe2com.py" "$d/$p.mz" "$d/$p.$e" >> "$d/$p.log" 2>&1
      fi
      record dmc $m $p "$d/$p.$e" "$d/$p.map" "$flags"
    done
  done
}

# gcc-ia16 --------------------------------------------------------------------
run_ia16() {
  [ -n "$IA16" ] || return 0
  echo "gcc-ia16: $("$IA16/ia16-elf-gcc" --version | head -1)"
  for m in $MODELS; do
    d=$OUT/ia16/$m; mkdir -p "$d"
    # gcc-ia16 has no large model (far data pointers).
    case $m in tiny|small|medium) ;; *) continue;; esac
    for p in $PROGS; do
      s=$(src_of $p) e=$(ext_of $m)
      cc="$IA16/ia16-elf-g++"; case $s in *.c) cc="$IA16/ia16-elf-gcc";; esac
      flags="-Os -ffunction-sections -fdata-sections -Wl,--gc-sections"
      case $s in *.cpp)
        if needs_eh $p; then flags="$flags -fexceptions"; else flags="$flags -fno-exceptions -fno-rtti"; fi;;
      esac
      rm -f "$d/$p.$e"
      $cc -mcmodel=$m $flags "$s" -o "$d/$p.$e" -Wl,-Map="$d/$p.map" > "$d/$p.log" 2>&1
      record ia16 $m $p "$d/$p.$e" "$d/$p.map" "$flags"
    done
  done
}

run_clang clang
[ -n "${TUNED:-}" ] && run_clang clang-tuned tuned
run_watcom
run_dmc
run_ia16
echo "Results: $CSV"
