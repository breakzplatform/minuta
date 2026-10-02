#!/usr/bin/env python3
"""Screen probes for the VM acceptance scripts (reads QEMU screendumps).

usage: screen.py wait-light VM X Y TIMEOUT   seconds until pixel (X,Y) is light
       screen.py pixel VM X Y                prints the pixel as RRGGBB
       screen.py thumb VM X TOP BOTTOM       where the vertical scroll bar thumb
           sits in its track, from 0.00 (top) to 1.00 (bottom); X is the
           bar's left edge, where buttons are a light run ending in black,
           and TOP..BOTTOM the rows of the bar
       screen.py last-change VM TIMEOUT QUIET [PIXELS]
           seconds from now to the last screen change before the screen
           stayed unchanged for QUIET seconds; changes of at most PIXELS
           pixels are ignored (default 64: the blinking caret; the mouse
           cursor changing shape is a few hundred)
"""
import os
import subprocess
import sys
import tempfile
import time

MON = os.path.expanduser("~/vms/retro/bin/mon")


def grab(vm):
    fd, path = tempfile.mkstemp(suffix=".ppm", dir=os.path.expanduser("~/vms/retro/run"))
    os.close(fd)
    try:
        subprocess.run([MON, vm, f"screendump {path}"], check=True, stdout=subprocess.DEVNULL)
        with open(path, "rb") as f:
            data = f.read()
    finally:
        os.remove(path)
    # P6 header: magic, width, height, maxval, each followed by whitespace.
    fields = []
    pos = 0
    while len(fields) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        start = pos
        while not data[pos:pos + 1].isspace():
            pos += 1
        fields.append(data[start:pos])
    width = int(fields[1])
    return width, data[pos + 1:]


def pixel(vm, x, y):
    width, pixels = grab(vm)
    offset = (y * width + x) * 3
    return tuple(pixels[offset:offset + 3])


def changed(a, b, pixels=64):
    if a is None or len(a) != len(b):
        return True
    if a == b:
        return False
    count = 0
    for i in range(0, len(a), 3):
        if a[i:i + 3] != b[i:i + 3]:
            count += 1
            if count > pixels:
                return True
    return False


def last_change(vm, timeout, quiet, pixels=64):
    start = time.time()
    previous = None
    last = 0.0
    while True:
        now = time.time()
        _, current = grab(vm)
        if changed(previous, current, pixels):
            last = now - start
        previous = current
        if now - start - last >= quiet:
            return last
        if now - start >= timeout:
            return None
        time.sleep(0.02)


def buttons(vm, x, first, last):
    """(top, bottom) of each 3D button (arrows and thumb) in column x."""
    width, pixels = grab(vm)
    found = []
    top = None
    for y in range(first, last + 1):
        offset = (y * width + x) * 3
        value = tuple(pixels[offset:offset + 3])
        if value == (216, 216, 216):
            if top is None:
                top = y
        elif value == (0, 0, 0) and top is not None:
            found.append((top, y))
            top = None
        else:
            top = None
    return found


def thumb(vm, x, first, last):
    """Thumb position in its track (0 top, 1 bottom), or None without one."""
    found = buttons(vm, x, first, last)
    if len(found) != 3:
        return None
    up, middle, down = found
    room = down[0] - up[1] - 1 - (middle[1] - middle[0] + 1)
    return (middle[0] - up[1] - 1) / room if room > 0 else None


def main():
    command, vm = sys.argv[1], sys.argv[2]
    if command == "last-change":
        pixels = int(sys.argv[5]) if len(sys.argv) > 5 else 64
        result = last_change(vm, float(sys.argv[3]), float(sys.argv[4]), pixels)
        print("timeout" if result is None else "%.2f" % result)
        sys.exit(0 if result is not None else 1)
    if command == "thumb":
        result = thumb(vm, int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]))
        print("none" if result is None else "%.2f" % result)
        sys.exit(0 if result is not None else 1)
    x, y = int(sys.argv[3]), int(sys.argv[4])
    if command == "pixel":
        print("%02X%02X%02X" % pixel(vm, x, y))
    elif command == "wait-light":
        timeout = float(sys.argv[5])
        start = time.time()
        while time.time() - start < timeout:
            if min(pixel(vm, x, y)) >= 0xE0:
                print("%.2f" % (time.time() - start))
                return
            time.sleep(0.05)
        print("timeout")
        sys.exit(1)


main()
