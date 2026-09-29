#!/bin/bash
# Acceptance 6 (Haiku), time to a settled screen: launch Minuta with a file
# and wait until the VM screen stops changing for 1 s (changes within a few
# pixel rows, like the blinking caret, don't count). Prints the seconds from
# launch to the last change (screen polled about every 0.3 s).
# usage: tests/haiku/open-visible.sh GUEST_FILE [out.png]
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
vm="$repo/tools/haiku-vm"
. "$vm/_common.sh"
file=${1:?usage: open-visible.sh GUEST_FILE [out.png]}
"$vm/ssh" 'hey Minuta quit >/dev/null 2>&1; sleep 1' || true
start=$(python3 -c 'import time; print(time.time())')
"$vm/launch" src/haiku/objects/Minuta "$file"
python3 - "$vm/mon" "$RUN/open-probe.ppm" "$start" <<'PY'
import os, subprocess, sys, time
mon, probe, start = sys.argv[1], sys.argv[2], float(sys.argv[3])
def grab():
    subprocess.run([mon, f"screendump {probe}"], check=True, stdout=subprocess.DEVNULL)
    data = open(probe, "rb").read(); os.remove(probe)
    header_end = 0
    for _ in range(3):
        header_end = data.index(b"\n", header_end) + 1
    width = int(data.split()[1])
    pixels = data[header_end:]
    return [pixels[i:i + width * 3] for i in range(0, len(pixels), width * 3)]
def rows_changed(a, b):
    return sum(1 for x, y in zip(a, b) if x != y)
previous, changed = grab(), time.time()
while time.time() - changed < 1.0 and time.time() - start < 120:
    time.sleep(0.2)
    current = grab()
    if rows_changed(current, previous) > 24:
        changed = time.time()
    previous = current
print(f"screen settled {changed - start:.1f} s after launch")
PY
[[ -n ${2:-} ]] && "$vm/shot" "$2" >/dev/null
exit 0
