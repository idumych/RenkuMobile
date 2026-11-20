#!/bin/sh
# tests/run.sh — boots vm/renku-mobile.img headless and checks the shell by
# tapping it and sampling screen pixels. Screenshots go to shots/test-*.png.
set -eu
cd "$(dirname "$0")/.."
Q=scripts/qemu.sh
SHOTS=shots
mkdir -p "$SHOTS"
fail=0

pixel() {	# pixel NAME X Y -> "r,g,b"
	magick "$SHOTS/test-$1.png" -format '%[fx:int(255*p{'"$2,$3"'}.r)],%[fx:int(255*p{'"$2,$3"'}.g)],%[fx:int(255*p{'"$2,$3"'}.b)]' info:
}
shot() { $Q shot "$SHOTS/test-$1.png" >/dev/null; }
check() {	# check NAME X Y EXPECTED WHAT
	got=$(pixel "$1" "$2" "$3")
	if [ "$got" = "$4" ]; then echo "ok   $5"
	else echo "FAIL $5: ($2,$3) is $got, expected $4"; fail=1; fi
}
check_not() {
	got=$(pixel "$1" "$2" "$3")
	if [ "$got" != "$4" ]; then echo "ok   $5"
	else echo "FAIL $5: ($2,$3) is still $4"; fail=1; fi
}

PANEL=22,25,27
SURFACE=42,46,50

$Q stop >/dev/null 2>&1 || true
$Q start
trap '$Q stop >/dev/null' EXIT

# Boot: wait for the status bar.
for i in $(seq 60); do
	sleep 3; shot boot
	[ "$(pixel boot 300 6)" = "$PANEL" ] && break
done
sleep 5; shot home
check home 300 6 $PANEL "status bar is up"
check home 300 1436 $PANEL "navigation panel is up"
check_not home 360 900 $PANEL "home screen shows the wallpaper"

# Action drawer: tap the status bar; the Keyboard tile is off (surface).
$Q tap 360 20; sleep 2; shot drawer
check drawer 274 360 $SURFACE "action drawer opens with its tiles"
$Q tap 360 1200; sleep 2; shot drawer-closed
check_not drawer-closed 274 360 $SURFACE "tapping below the drawer closes it"

# Keyboard: toggle from the navigation panel.
$Q tap 690 1408; sleep 2; shot keyboard
check keyboard 4 1100 $PANEL "keyboard shows above the navigation panel"
$Q tap 690 1408; sleep 2; shot keyboard-hidden
check_not keyboard-hidden 4 1100 $PANEL "keyboard hides again"

# Launch Terminal from the dock (third icon) and check it fills the work area.
$Q tap 444 1290; sleep 6; shot terminal
check_not terminal 360 700 "$(pixel home 360 700)" "Terminal opened over the home screen"
check terminal 300 6 $PANEL "status bar stays above the app"

# Type into it with the on-screen keyboard: "ls" then Return.
$Q tap 690 1408; sleep 2
$Q tap 645 1170; $Q tap 146 1170; $Q tap 629 1330; sleep 2; shot typed
$Q tap 690 1408; sleep 1

# Task switcher shows a card; then close the app from the navigation panel.
$Q tap 160 1408; sleep 2; shot tasks
check tasks 100 300 $SURFACE "task switcher shows a card"
$Q tap 360 1300; sleep 2
$Q tap 560 1408; sleep 3; shot closed
check closed 360 700 "$(pixel home 360 700)" "close button closed Terminal"

[ $fail = 0 ] && echo "all checks passed" || echo "some checks failed"
exit $fail
