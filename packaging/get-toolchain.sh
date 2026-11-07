#!/bin/sh
# Download a published clang-dos toolchain package (or only its sysroots)
# from clang-retro/dos-sdk's releases, check it against SHA256SUMS and
# unpack it, so that CI and new checkouts need not build the suite.
#
#   packaging/get-toolchain.sh [-t TAG] [-H HOST] [-s NAMES] DIR
#
#   -t TAG     release tag (default: nightly, rebuilt every night from the
#              default branches; or a release such as v0.2.0)
#   -H HOST    linux (default) or windows: which package to take
#   -s NAMES   only these sysroots, comma or space separated, such as
#              "i8086-msdos,win2k-xp", each into DIR/NAME; no compiler
#   DIR        where to unpack. The package goes in DIR itself (DIR/bin,
#              DIR/sysroots, ...), replacing what was there.
#
#   REPO       GitHub repository of the releases (default clang-retro/dos-sdk)
#   URL        or the base URL of the files (such as file:///path/to/dist)
#
# Needs curl, tar, xz and sha256sum (or shasum). Prints DIR when done.
set -e
tag=nightly host=linux sysroots=
while getopts t:H:s: o; do
  case $o in
  t) tag=$OPTARG ;;
  H) host=$OPTARG ;;
  s) sysroots=$(echo "$OPTARG" | tr ',' ' ') ;;
  *) echo "usage: get-toolchain.sh [-t TAG] [-H linux|windows] [-s NAMES] DIR" >&2; exit 2 ;;
  esac
done
shift $((OPTIND - 1))
dir=${1:?usage: get-toolchain.sh [-t TAG] [-H linux|windows] [-s NAMES] DIR}
REPO=${REPO:-clang-retro/dos-sdk}
url=${URL:-https://github.com/$REPO/releases/download/$tag}

if command -v sha256sum >/dev/null; then sha() { sha256sum "$@"; }; else sha() { shasum -a 256 "$@"; }; fi
get() { curl -fsSL --retry 5 --retry-all-errors -o "$2" "$url/$1"; }
check() { # file
  want=$(awk -v f="$1" '$2 == f || $2 == "*" f { print $1 }' SHA256SUMS)
  [ -n "$want" ] || { echo "error: $1 is not in $tag's SHA256SUMS" >&2; exit 1; }
  echo "$want  $1" | sha -c - >/dev/null || { echo "error: $1 does not match SHA256SUMS" >&2; exit 1; }
}

mkdir -p "$dir"
dir=$(cd "$dir" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"
get SHA256SUMS SHA256SUMS

if [ -n "$sysroots" ]; then
  for s in $sysroots; do
    f=clang-dos-sysroot-$s.tar.xz
    echo "Downloading $tag/$f" >&2
    get "$f" "$f"
    check "$f"
    rm -rf "${dir:?}/$s"
    # Each archive holds one directory, NAME.
    tar -C "$dir" -xJf "$f"
    rm -f "$f"
  done
else
  case $host in
  linux) f=clang-dos-linux-x86_64.tar.xz ;;
  windows) f=clang-dos-windows-x86_64.zip ;;
  *) echo "error: host is linux or windows" >&2; exit 2 ;;
  esac
  echo "Downloading $tag/$f" >&2
  get "$f" "$f"
  check "$f"
  mkdir x
  case $f in
  *.zip) (cd x && unzip -q "../$f") ;;
  *) tar -C x -xJf "$f" ;;
  esac
  rm -f "$f"
  rm -rf "${dir:?}"/* 2>/dev/null || true
  # The archive holds one directory, clang-dos-HOST-x86_64.
  mv x/*/* "$dir/"
fi
[ -n "$sysroots" ] && { get BRANCHES.txt "$dir/BRANCHES.txt" 2>/dev/null || true; }
echo "$dir"
