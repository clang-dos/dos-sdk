#!/bin/sh
# Build the Mac OS X tests for PowerPC, run them in QEMU on Mac OS X 10.4
# (or 10.0) and report which fail. See macosx/README.md for how to make
# the guest image and the SDK.
#
#   BIN       directory with clang
#   SDK       SDK or sysroot for that version (MacOSX10.4.0.sdk for 10.4)
#   OSX_IMAGE disk image with Mac OS X and the test runner installed
#   OSX_VERSION  10.4 (default), 10.0 or 10.5. For 10.5 the tests are
#             built for 10.4 with the 10.4 SDK, and run on a 10.5 image
#   LD        ld64 for PowerPC (default: clang finds powerpc-apple-darwin8-ld
#             next to itself or on PATH)
#   QEMU      qemu-system-ppc (default: on PATH)
#   OPTS      optimization levels (default: O0 Os O2)
#   RELOCS    relocation models (default: pic dynamic-no-pic)
#   TIMEOUT   seconds QEMU may run (default: 900)
#   LIBCXX    install directory of scripts/build-libcxx-macosx.sh; when set
#             (10.4 and 10.5 only), the C++ test is built and run too
: "${BIN:?set BIN}" "${SDK:?set SDK}" "${OSX_IMAGE:?set OSX_IMAGE}"
QEMU=${QEMU:-qemu-system-ppc}
version=${OSX_VERSION:-10.4}
target=$version
case $version in
10.4) machine="-M mac99,via=pmu -m 1024" ;;
# The 10.5 installer can't bless the disk in QEMU either.
10.5) target=10.4
  machine="-M mac99,via=pmu -m 1024 -prom-env boot-device=hd:,\\System\\Library\\CoreServices\\BootX" ;;
# 10.0 is set up on the cuda machine (its console needs the ADB keyboard),
# and its disk is not blessed, so name BootX for OpenBIOS.
10.0) machine="-M mac99 -m 512 -prom-env boot-device=hd:,\\System\\Library\\CoreServices\\BootX" ;;
*) echo "OSX_VERSION must be 10.4, 10.0 or 10.5" >&2; exit 2 ;;
esac
tests=$(cd "$(dirname "$0")" && pwd)
out=${WORK:-$PWD/build-macosx-tests}
rm -rf "$out"
mkdir -p "$out/disk"
cd "$out" || exit 1
pass=0
fail=0

# The guest runs run.sh, which prints "=== NAME" before each test's
# output.
run=disk/run.sh
echo 'PATH=/bin:/usr/bin:/sbin:/usr/sbin; export PATH' >$run
names=

# add NAME SOURCE FLAGS... [-- RUN-COMMAND-TAIL]: build NAME from SOURCE
# and add a line to run.sh that runs it.
add() {
  name=$1 src=$2 tail=$3
  shift 3
  if "$BIN/clang" --target=powerpc-apple-macosx$target -isysroot "$SDK" \
       ${LD:+--ld-path="$LD"} "$@" "$src" -o "disk/$name" >"$name.log" 2>&1; then
    echo "echo '=== $name'; ./$name $tail; echo \"=== rc=\$?\"" >>$run
    names="$names $name"
  else
    fail=$((fail + 1))
    echo "FAIL $name (build)"
    sed 's/^/    /' "$name.log" | head -20
  fi
}

printf 'first\nsecond line\n' >disk/stdin.txt
for r in ${RELOCS:-pic dynamic-no-pic}; do
  case $r in
  pic) rf= ;;
  *) rf=-mdynamic-no-pic ;;
  esac
  for o in ${OPTS:-O0 Os O2}; do
    for cpu in G3 G4; do
      cf=-mcpu=$cpu
      [ $cpu = G3 ] && cf=-mcpu=750
      [ $cpu = G4 ] && cf=-mcpu=7400
      s=$r-$o-$cpu
      add "abi-$s" "$tests/macosx/abi.c" "" -$o $rf $cf
      add "builtins-$s" "$tests/macosx/builtins.c" "" -$o $rf $cf
      add "objc-$s" "$tests/macosx/objc.m" "" -$o $rf $cf \
        -framework Foundation -lobjc
      [ -n "$LIBCXX" ] && [ "$target" != 10.0 ] &&
        add "cxx-$s" "$tests/macosx/cxx.cpp" "" -$o $rf $cf -std=c++17 \
          --driver-mode=g++ -nostdinc++ -isystem "$LIBCXX/include/c++/v1" \
          -nostdlib++ -L"$LIBCXX/lib" -lc++ -lc++abi -lunwind
      add "hello-$s" "$tests/libc/hello.c" "a b" -$o $rf $cf
      add "stdin-$s" "$tests/libc/stdin.c" "<stdin.txt" -$o $rf $cf
    done
  done
done

# A whole-disk FAT volume named CDTEST, which Mac OS X mounts on its own.
img=cdtest.img
dd if=/dev/zero of=$img bs=1M count=64 2>/dev/null
mkfs.vfat -n CDTEST $img >/dev/null
mcopy -i $img -s disk/* ::
timeout "${TIMEOUT:-900}" "$QEMU" $machine -cpu G4 \
  -drive file="$OSX_IMAGE",media=disk,snapshot=on \
  -drive file=$img,format=raw,media=disk \
  -boot c -display none -serial none -monitor none >qemu.log 2>&1
if ! mcopy -i $img ::DONE.TXT done.txt 2>/dev/null; then
  echo "FAIL guest (the tests did not finish; see $out/qemu.log)"
  exit 1
fi
mcopy -i $img ::OUT.TXT out.txt
# Split the output by test.
awk '/^=== / && !/^=== rc=/ { f = substr($0, 5) ".out"; next } { print > f }' out.txt

check() {
  # check NAME PATTERN
  if [ -f "$1.out" ] && grep -q -- "$2" "$1.out" && grep -q '^=== rc=0' "$1.out"; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1))
    echo "FAIL $1"
    sed 's/^/    /' "$1.out" 2>/dev/null | head -20
  fi
}
for n in $names; do
  case $n in
  abi-*|builtins-*|objc-*|cxx-*) check "$n" "ALL PASSED" ;;
  hello-*) check "$n" "argv\[2\]=<b>" ;;
  stdin-*) check "$n" "lines=2" ;;
  esac
done
echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
