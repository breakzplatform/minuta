#!/bin/bash
# Acceptance 6 (Haiku), scrolling: with a document open and focused, send
# COUNT Page Down keys in one burst and time until the screen settles, then
# a jump to the end (Alt+End... BTextView: Page Down repeated) is checked on
# the screenshot. usage: tests/haiku/scroll.sh [COUNT=40] [out.png]
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
vm="$repo/tools/haiku-vm"
. "$vm/_common.sh"
count=${1:-40}
keys=()
for _ in $(seq "$count"); do keys+=(pgdn); done
start=$(python3 -c 'import time; print(time.time())')
"$vm/key" "${keys[@]}"
python3 - "$vm/mon" "$RUN/scroll-probe.ppm" "$start" "$count" <<'PY'
import os, subprocess, sys, time
mon, probe, start, count = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]
def grab():
    subprocess.run([mon, f"screendump {probe}"], check=True, stdout=subprocess.DEVNULL)
    data = open(probe, "rb").read(); os.remove(probe)
    header_end = 0
    for _ in range(3):
        header_end = data.index(b"\n", header_end) + 1
    width = int(data.split()[1])
    pixels = data[header_end:]
    return [pixels[i:i + width * 3] for i in range(0, len(pixels), width * 3)]
previous, changed = grab(), time.time()
while time.time() - changed < 1.0 and time.time() - start < 120:
    time.sleep(0.2)
    current = grab()
    if sum(1 for x, y in zip(current, previous) if x != y) > 24:
        changed = time.time()
    previous = current
print(f"{count} x Page Down: screen settled {changed - start:.1f} s after the first key")
PY
[[ -n ${2:-} ]] && "$vm/shot" "$2" >/dev/null
exit 0
