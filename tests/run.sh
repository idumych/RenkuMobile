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
# Terminal's prompt lines below the ls listing (16 pt text, 23 px lines): the
# checks of what was typed compare this band before and after.
PROMPT=700x150+0+190

$Q stop >/dev/null 2>&1 || true
# Dictation needs a microphone: with PipeWire's tools, a silent sink on the host
# whose monitor is the VM's microphone, so that a WAV played into it is heard.
SINK=
if command -v pactl >/dev/null && command -v pw-play >/dev/null; then
	SINK=$(pactl load-module module-null-sink sink_name=renku_mobile_test)
	$Q start -audiodev pa,id=snd0,in.name=renku_mobile_test.monitor \
		-device ich9-intel-hda -device hda-micro,audiodev=snd0
else
	$Q start
fi
trap '$Q stop >/dev/null; [ -z "$SINK" ] || pactl unload-module $SINK' EXIT

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

# A finger, unlike a mouse, doesn't hover where it is about to press: open the
# app drawer, leave the pointer where that swipe ended, and touch AboutSystem.
$Q drag 360 1000 360 500; sleep 2; shot appdrawer
$Q touch 88 200; sleep 5; shot touched
check_not touched 360 700 "$(pixel appdrawer 360 700)" "a finger's tap in the app drawer launches the app"
$Q touch 560 1408; sleep 3

# Launch Terminal from the dock (third icon) and check it fills the work area.
$Q tap 444 1290; sleep 6; shot terminal
check_not terminal 360 700 "$(pixel home 360 700)" "Terminal opened over the home screen"
check terminal 300 6 $PANEL "status bar stays above the app"
check terminal 4 1100 $PANEL "keyboard comes up for Terminal's text"

# Type into it with the on-screen keyboard: "ls" then Return.
$Q tap 645 1170; $Q tap 146 1170; $Q tap 629 1330; sleep 2; shot typed

# A finger's drag selects "config" from the listing, and a finger held still
# right-clicks, which in Terminal pastes the selection at the prompt. Partway
# through, a ring around the finger fills up; the press goes through it.
$Q flick 3 178 58 178; sleep 1
$Q hold 300 500 1 & sleep 0.35; shot ring; wait; sleep 2; shot pasted
check_not ring 332 500 "$(pixel typed 332 500)" "a ring shows around a held finger"
changed=$(magick compare -metric AE "$SHOTS/test-typed.png" "$SHOTS/test-pasted.png" \
	-crop $PROMPT null: 2>&1 || true)
if [ "${changed%% *}" != 0 ]; then echo "ok   a held finger right-clicks (Terminal pastes)"
else echo "FAIL a held finger right-clicks (Terminal pastes): the prompt is unchanged"; fail=1; fi

# The globe key opens a menu of layouts above it; Terminal adds a row of keys
# above the letters. Ctrl+C drops the pasted line; Tab completes "cd De".
$Q tap 174 1330; sleep 1; shot layout-menu
check layout-menu 300 1250 $SURFACE "the globe key opens the layout menu"
$Q tap 150 1092; sleep 1; shot terminal-keys
check terminal-keys 15 1018 $SURFACE "the menu's Terminal adds the terminal keys"
$Q tap 182 1018; $Q tap 288 1250; sleep 1
$Q tap 288 1250; $Q tap 218 1170; $Q tap 395 1330; $Q tap 59 1250; $Q tap 218 1170
$Q tap 182 1090; sleep 1; shot before-tab
$Q tap 111 1018; sleep 1; shot completed
changed=$(magick compare -metric AE "$SHOTS/test-before-tab.png" "$SHOTS/test-completed.png" \
	-crop $PROMPT null: 2>&1 || true)
if [ "${changed%% *}" != 0 ]; then echo "ok   Ctrl+C and Tab from the terminal keys"
else echo "FAIL Ctrl+C and Tab from the terminal keys: nothing was completed"; fail=1; fi
$Q tap 174 1330; sleep 1; $Q tap 150 1031; sleep 1

# The menu's Emoji puts emoji in place of the letters, on the smileys, whose
# key in the bar under them is tinted; ABC brings the letters back.
$Q tap 174 1330; sleep 1; $Q tap 150 1195; sleep 1; shot emoji
check emoji 192 1300 48,91,114 "the menu's Emoji shows the emoji, on the smileys"
$Q tap 73 1350; sleep 1; shot letters
check letters 192 1300 $SURFACE "ABC brings the letters back"

# The Corners layout: q flicked towards its top right corner types its 1.
$Q tap 174 1330; sleep 1; $Q tap 150 1146; sleep 1
$Q down 40 1090; $Q move 53 1077; $Q move 66 1064; $Q up 66 1064; sleep 1; shot flicked
changed=$(magick compare -metric AE "$SHOTS/test-letters.png" "$SHOTS/test-flicked.png" \
	-crop $PROMPT null: 2>&1 || true)
if [ "${changed%% *}" != 0 ]; then echo "ok   a flick to a key's corner types its symbol"
else echo "FAIL a flick to a key's corner types its symbol: the prompt is unchanged"; fail=1; fi
# (the Corners layout's globe key is further right)
$Q tap 660 1250; sleep 0.5; $Q tap 226 1330; sleep 1; $Q tap 150 1038; sleep 1

# Swipe typing: a finger drawn through h, e, l and o types "hello", and the
# strip on top suggests it; backspace takes the whole word out again.
$Q swipe 431 1170 182 1090 645 1170 610 1090; sleep 1; shot swiped
changed=$(magick compare -metric AE "$SHOTS/test-letters.png" "$SHOTS/test-swiped.png" \
	-crop $PROMPT null: 2>&1 || true)
if [ "${changed%% *}" != 0 ]; then echo "ok   a swipe types a word"
else echo "FAIL a swipe types a word: the prompt is unchanged"; fail=1; fi
changed=$(magick compare -metric AE "$SHOTS/test-letters.png" "$SHOTS/test-swiped.png" \
	-crop 200x40+232+1004 null: 2>&1 || true)
if [ "${changed%% *}" != 0 ]; then echo "ok   the strip suggests words for the swipe"
else echo "FAIL the strip suggests words for the swipe: its middle is empty"; fail=1; fi
$Q tap 660 1250; sleep 1

# Dictation: the microphone opens the listening circle; whisper.cpp's JFK
# sample played into it is typed at the prompt once the quiet after it ends it.
if [ -n "$SINK" ]; then
	$Q tap 688 1026; sleep 1.5; shot listening
	check listening 405 1195 48,91,114 "the microphone opens the listening circle"
	pw-play --target renku_mobile_test third_party/whisper.cpp/samples/jfk.wav
	for i in $(seq 20); do
		sleep 1; shot dictated
		[ "$(pixel dictated 15 1018)" != "$(pixel listening 15 1018)" ] && break
	done
	sleep 1; shot dictated
	changed=$(magick compare -metric AE "$SHOTS/test-letters.png" "$SHOTS/test-dictated.png" \
		-crop $PROMPT null: 2>&1 || true)
	if [ "${changed%% *}" != 0 ]; then echo "ok   dictation types what was said"
	else echo "FAIL dictation types what was said: the prompt is unchanged"; fail=1; fi
else
	echo "skip dictation: no pactl/pw-play for a microphone"
fi

# Task switcher shows a card; then close the app from the navigation panel.
$Q tap 160 1408; sleep 2; shot tasks
check tasks 40 300 $SURFACE "task switcher shows a card"
$Q tap 360 1300; sleep 2
$Q tap 560 1408; sleep 3; shot closed
check closed 360 700 "$(pixel home 360 700)" "close button closed Terminal"
check_not closed 4 1100 $PANEL "keyboard went with Terminal"

[ $fail = 0 ] && echo "all checks passed" || echo "some checks failed"
exit $fail
