#!/usr/bin/env python3
"""qmp-pointer.py SOCK tap|down|up|move X Y | drag X1 Y1 X2 Y2 [STEPS]
               | touch X Y | hold X Y [SECONDS] | flick X1 Y1 X2 Y2 [STEPS]
               | swipe X1 Y1 X2 Y2 [X3 Y3 ...]

Absolute pointer (QEMU usb-tablet) for the 720x1440 test phone. Coordinates are
screen pixels; QMP's absolute axes run 0..32767. "drag" presses, moves in steps
and releases, like a finger swipe.

"touch" and "flick" are a finger instead of a mouse: the pointer jumps to where
the finger lands in the same event as the press, without hovering there first,
a hold stays put (a second by default) before it lets go, a flick moves
fast and lets go while still moving, and a swipe goes through its points at
a finger's pace, as when swipe typing.
"""
import json, socket, sys, time

W, H = 720, 1440

def main():
    sock_path, cmd, *args = sys.argv[1:]
    s = socket.socket(socket.AF_UNIX)
    s.connect(sock_path)
    f = s.makefile("rw")
    f.readline()
    def send(obj):
        f.write(json.dumps(obj) + "\n"); f.flush()
        while True:
            r = json.loads(f.readline())
            if "return" in r or "error" in r:
                return r
    send({"execute": "qmp_capabilities"})
    def move(x, y):
        send({"execute": "input-send-event", "arguments": {"events": [
            {"type": "abs", "data": {"axis": "x", "value": int(x * 32767 / (W - 1))}},
            {"type": "abs", "data": {"axis": "y", "value": int(y * 32767 / (H - 1))}}]}})
    def btn(down):
        send({"execute": "input-send-event", "arguments": {"events": [
            {"type": "btn", "data": {"down": down, "button": "left"}}]}})
    def press_at(x, y):
        send({"execute": "input-send-event", "arguments": {"events": [
            {"type": "abs", "data": {"axis": "x", "value": int(x * 32767 / (W - 1))}},
            {"type": "abs", "data": {"axis": "y", "value": int(y * 32767 / (H - 1))}},
            {"type": "btn", "data": {"down": True, "button": "left"}}]}})
    a = [float(v) for v in args]
    if cmd == "move":
        move(a[0], a[1])
    elif cmd in ("tap", "down", "up"):
        move(a[0], a[1]); time.sleep(0.05)
        if cmd in ("tap", "down"):
            btn(True)
        if cmd == "tap":
            time.sleep(0.08)
        if cmd in ("tap", "up"):
            btn(False)
    elif cmd == "drag":
        x1, y1, x2, y2 = a[:4]
        steps = int(a[4]) if len(a) > 4 else 20
        move(x1, y1); time.sleep(0.05); btn(True); time.sleep(0.05)
        for i in range(1, steps + 1):
            move(x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps)
            time.sleep(0.015)
        time.sleep(0.05); btn(False)
    elif cmd == "touch":
        press_at(a[0], a[1]); time.sleep(0.08); btn(False)
    elif cmd == "hold":
        press_at(a[0], a[1]); time.sleep(a[2] if len(a) > 2 else 1.0); btn(False)
    elif cmd == "swipe":
        points = list(zip(a[0::2], a[1::2]))
        press_at(*points[0]); time.sleep(0.03)
        for (x1, y1), (x2, y2) in zip(points, points[1:]):
            for i in range(1, 9):
                move(x1 + (x2 - x1) * i / 8, y1 + (y2 - y1) * i / 8)
                time.sleep(0.012)
        btn(False)
    elif cmd == "flick":
        x1, y1, x2, y2 = a[:4]
        steps = int(a[4]) if len(a) > 4 else 6
        press_at(x1, y1); time.sleep(0.03)
        for i in range(1, steps + 1):
            move(x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps)
            time.sleep(0.012)
        btn(False)

main()
