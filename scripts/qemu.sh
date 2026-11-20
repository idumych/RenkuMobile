#!/bin/sh
# qemu.sh CMD … — the Renku Mobile x86_64 test phone in QEMU/KVM.
#
#   IMG   disk image (default vm/renku-mobile.img)   SSH_PORT (default 2240)
#   DISPLAY_MODE  none (default, headless) | gtk | sdl
#   start [extra qemu args]  boot it; screen is 720x1440, the original PinePhone's
#   stop                     hard power-off (shut down from inside first)
#   shot FILE.png            screenshot
#   tap X Y | down X Y | up X Y | drag X1 Y1 X2 Y2   pointer via QMP (usb-tablet)
#   touch X Y | hold X Y [S] | flick X1 Y1 X2 Y2 | swipe X1 Y1 X2 Y2 …
#                            the same as a finger: no hover before the press
#   key KEYS… | type TEXT    keyboard via the monitor
#
# The screen comes from virtio-vga's EDID (xres/yres): Renku's VESA driver can only
# set modes the VBE BIOS lists, which doesn't include 720x1440; the virtio_gpu
# driver takes the EDID's mode.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
IMG=${IMG:-$ROOT/vm/renku-mobile.img}
VMDIR=$(dirname "$IMG")
SSH_PORT=${SSH_PORT:-2240}
MON=$VMDIR/monitor.sock
QMP=$VMDIR/qmp.sock
cmd=${1:-}; [ -n "$cmd" ] && shift
running() { [ -S "$MON" ] && echo info status | socat - "UNIX-CONNECT:$MON" >/dev/null 2>&1; }
hmp() { echo "$1" | socat - "UNIX-CONNECT:$MON" >/dev/null; }
case $cmd in
start)
	running && { echo "already running"; exit 0; }
	rm -f "$MON" "$QMP"
	# shellcheck disable=SC2086
	( cd "$VMDIR" && setsid qemu-system-x86_64 -name "Renku Mobile" -enable-kvm -cpu host \
		-smp 2 -m 2048 -rtc base=localtime \
		-drive file="$IMG",format=raw,if=none,id=hd0 -device ahci,id=ahci \
		-device ide-hd,drive=hd0,bus=ahci.0 \
		-netdev "user,id=n0,hostfwd=tcp:127.0.0.1:$SSH_PORT-:22" -device e1000,netdev=n0 \
		-device virtio-vga,xres=720,yres=1440 -device qemu-xhci -device usb-tablet \
		-display "${DISPLAY_MODE:-none}" \
		-monitor unix:monitor.sock,server,nowait -qmp unix:qmp.sock,server,nowait \
		-serial file:serial.log "$@" > qemu.log 2>&1 & )
	sleep 2; running && echo "started (serial: $VMDIR/serial.log)" || { cat "$VMDIR/qemu.log"; exit 1; } ;;
stop)
	running || { echo "not running"; exit 0; }
	hmp quit; for i in $(seq 20); do running || break; sleep 1; done; echo stopped ;;
shot)
	hmp "screendump $VMDIR/.shot.ppm"; sleep 1; magick "$VMDIR/.shot.ppm" "$1"; echo "$1" ;;
tap|down|up|move|touch|hold|swipe)
	python3 "$ROOT/scripts/qmp-pointer.py" "$QMP" "$cmd" "$@" ;;
drag|flick)
	python3 "$ROOT/scripts/qmp-pointer.py" "$QMP" "$cmd" "$@" ;;
key)
	for k in "$@"; do hmp "sendkey $k"; sleep 0.15; done ;;
type)
	printf '%s\n' "$*" | sed 's/./&\n/g' | while IFS= read -r c; do
		case $c in
			' ') k=spc ;; .) k=dot ;; /) k=slash ;; -) k=minus ;; _) k=shift-minus ;;
			'|') k=shift-backslash ;; "'") k=apostrophe ;; '"') k=shift-apostrophe ;;
			:) k=shift-semicolon ;; '&') k=shift-7 ;; '>') k=shift-dot ;; '~') k=shift-grave_accent ;;
			=) k=equal ;; ';') k=semicolon ;; ',') k=comma ;;
			[A-Z]) k=shift-$(echo "$c" | tr A-Z a-z) ;;
			'') continue ;; *) k=$c ;;
		esac
		hmp "sendkey $k"; sleep 0.05
	done ;;
*) sed -n '2,16p' "$0"; exit 2 ;;
esac
