#!/bin/bash
# Takes the Haiku acceptance screenshots (SPEC-HAIKU.md items 2, 3, 4 and 6)
# into OUT_DIR (default docs/shots/haiku). Needs the VM running, the app
# installed (tools/haiku-vm/build install) and the big test file in the
# guest at /tmp/big.md. Settings are reset first; coordinates assume the
# window frame set below and the guest's 1280x800 screen.
# usage: tests/haiku/shots.sh [OUT_DIR]
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
vm="$repo/tools/haiku-vm"
out=$(cd "${1:-$repo/docs/shots/haiku}" && pwd)

quit_app() { "$vm/ssh" 'hey Minuta quit >/dev/null 2>&1; sleep 1' || true; }
open_file() {
  "$vm/launch" minuta "$1"
  sleep 3
  "$vm/ssh" 'hey Minuta set Frame of Window 0 to "BRect(180,50,1100,770)" >/dev/null'
  sleep "${2:-2}"
}
shot() { "$vm/settle" 20 1 "$out/$1" >/dev/null; echo "$out/$1"; }

quit_app
"$vm/ssh" 'rm -f ~/config/settings/Minuta'

open_file samples/prose.md
shot dimming.png
"$vm/click" 600 285                 # "The rest of the paragraph is still there."
"$vm/key" alt-shift-f
shot focus-sentence.png
"$vm/key" alt-shift-p
shot focus-paragraph.png
"$vm/key" alt-shift-p alt-shift-f
"$vm/key" alt-shift-t
"$vm/key" down down down down down down down down down down down down down down
"$vm/key" shift-end                 # selects the caret line, so it shows
shot typewriter.png
"$vm/key" alt-shift-t alt-shift-n
shot night.png
"$vm/key" alt-shift-n alt-shift-w
shot counters.png
sleep 4
"$vm/key" alt-f
sleep 1
"$vm/type" 'caret'
"$vm/key" ret
shot find.png
"$vm/key" esc
quit_app

open_file samples/SMOKE.MD
shot smoke-editor.png
"$vm/key" alt-r
shot smoke-preview.png
"$vm/key" pgdn
shot smoke-preview-end.png
"$vm/key" esc
shot smoke-back-to-editor.png
quit_app

open_file /tmp/big.md 4
"$repo/tests/haiku/scroll.sh" 40
shot big-file-scrolled.png
quit_app
