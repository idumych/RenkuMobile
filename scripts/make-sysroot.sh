#!/bin/sh
# make-sysroot.sh — the headers and libraries the cross compiler links the
# shell against, taken from the very build the image is made from: haiku and
# haiku_devel from the nightly's repository, gcc_syslibs from the ISO.
set -eu
. "$(dirname "$0")/env.sh"
rm -rf "$SYSROOT"; mkdir -p "$SYSROOT/hpkg"
tar -xf "$REPO_TAR" -C "$SYSROOT/hpkg" --strip-components=1 --wildcards \
	'packages/haiku-r*' 'packages/haiku_devel-*'
syslibs=$(printf 'ls /myfs/system/packages\nquit\n' | "$BFS_SHELL" --start-offset $BFS_OFFSET "$ISO" \
	| grep -o 'gcc_syslibs-[^ ]*\.hpkg' | grep -v devel | head -1)
printf 'cp /myfs/system/packages/%s :%s\nquit\n' "$syslibs" "$SYSROOT/hpkg/$syslibs" \
	| "$BFS_SHELL" --start-offset $BFS_OFFSET "$ISO" >/dev/null
mkdir -p "$SYSROOT/system"
for p in "$SYSROOT"/hpkg/*.hpkg; do
	"$PACKAGE" extract -C "$SYSROOT/system" "$p"
done
ls "$SYSROOT/system/lib/libbe.so" "$SYSROOT/system/lib/libstdc++.so" >/dev/null
echo "sysroot: $SYSROOT"
