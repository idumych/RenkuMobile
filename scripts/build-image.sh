#!/bin/sh
# build-image.sh [HPKG] — the Renku Mobile image: the nightly anyboot ISO with
# the renku_mobile package added and the first-boot prompt skipped, written
# with bfs_shell, no VM needed. Output: vm/renku-mobile.img (boot it with
# scripts/qemu.sh start).
set -eu
. "$(dirname "$0")/env.sh"
HPKG=${1:-$(ls -t "$ROOT"/build/renku_mobile-*-"$ARCH".hpkg | head -1)}
IMG=${IMG:-$ROOT/vm/renku-mobile.img}
mkdir -p "$(dirname "$IMG")"
cp "$ISO" "$IMG"
# The launch file starts the desktop target only once "Locale settings"
# exists on a writable disk; an empty one means the default locale.
EMPTY=$ROOT/build/empty
: > "$EMPTY"
"$BFS_SHELL" --start-offset $BFS_OFFSET "$IMG" <<CMDS
cp :$HPKG /myfs/system/packages/
cp :$EMPTY "/myfs/home/config/settings/Locale settings"
ls /myfs/home/config/settings
sync
quit
CMDS
echo "$IMG"
