#!/bin/sh
# build-hpkg.sh ARCH VERSION — packages build/ARCH into build/renku_mobile-VERSION-1-ARCH.hpkg
# with the package tool built for the host (README § Host tools).
set -eu
cd "$(dirname "$0")/.."
. scripts/env.sh
ARCH=$1 VERSION=$2
OUT=build/$ARCH
TREE=build/hpkg-$ARCH
rm -rf "$TREE"
mkdir -p "$TREE/apps" "$TREE/add-ons/input_server/devices" \
	"$TREE/add-ons/input_server/filters" "$TREE/data/user_launch"
cp "$OUT/MobileShell" "$TREE/apps/"
cp "$OUT/renku_virtual_keyboard" "$TREE/add-ons/input_server/devices/"
cp "$OUT/renku_touch" "$TREE/add-ons/input_server/filters/"
cp packaging/user_launch "$TREE/data/user_launch/renku_mobile"
cat > "$TREE/.PackageInfo" <<INFO
name			renku_mobile
version			$VERSION-1
architecture	$ARCH
summary			"Phone shell for Renku OS"
description		"A touch UI after Plasma Mobile: home screen and app drawer, status bar, navigation panel, action drawer, task switcher and on-screen keyboard. It replaces Tracker's desktop and the Deskbar."
packager		"Renku Mobile <idumych@gmail.com>"
vendor			"Renku Mobile"
copyrights		{ "2026 Renku Mobile contributors" }
licenses		{ "MIT" }
provides		{ renku_mobile = $VERSION  app:MobileShell = $VERSION }
requires		{ haiku >= r1~beta6  noto_emoji_color }
INFO
PKG=build/renku_mobile-$VERSION-1-$ARCH.hpkg
rm -f "$PKG"
"$PACKAGE" create -C "$TREE" "$PKG"
echo "$PKG"
