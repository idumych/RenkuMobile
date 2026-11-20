#!/bin/sh
# build-image.sh [HPKG] — the Renku Mobile image: the nightly anyboot ISO with
# the renku_mobile package added, the first-boot prompt skipped and the
# composited desktop on, written with bfs_shell, no VM needed. Output:
# vm/renku-mobile.img (boot it with scripts/qemu.sh start). The package needs
# HaikuPorts' noto_emoji_color for the keyboard's emoji, which the nightly
# doesn't ship: it goes in too, from downloads/ (see README).
set -eu
. "$(dirname "$0")/env.sh"
HPKG=${1:-$(ls -t "$ROOT"/build/renku_mobile-*-"$ARCH".hpkg | head -1)}
IMG=${IMG:-$ROOT/vm/renku-mobile.img}
EMOJI=$(ls "$ROOT"/downloads/noto_emoji_color-*-any.hpkg 2>/dev/null | head -1)
[ -n "$EMOJI" ] || { echo "no downloads/noto_emoji_color-*-any.hpkg (see README)" >&2; exit 1; }
mkdir -p "$(dirname "$IMG")"
cp "$ISO" "$IMG"
# The launch file starts the desktop target only once "Locale settings"
# exists on a writable disk; an empty one means the default locale.
EMPTY=$ROOT/build/empty
: > "$EMPTY"
# app_server reads whether to composite when it starts.
APPEARANCE=$ROOT/build/appearance
"$ROOT"/scripts/app-server-appearance.py "$APPEARANCE"
"$BFS_SHELL" --start-offset $BFS_OFFSET "$IMG" <<CMDS
cp :$HPKG /myfs/system/packages/
cp :$EMOJI /myfs/system/packages/
cp :$EMPTY "/myfs/home/config/settings/Locale settings"
mkdir /myfs/home/config/settings/system
mkdir /myfs/home/config/settings/system/app_server
cp :$APPEARANCE /myfs/home/config/settings/system/app_server/appearance
ls /myfs/home/config/settings
sync
quit
CMDS
echo "$IMG"
