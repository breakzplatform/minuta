#!/bin/bash
# Typing latency: type TEXT (ASCII) into Minuta's focused editor in one
# burst through QEMU sendkey, then poll the caret with `hey` until all of it
# has arrived. Prints how long the keys took to send and how long after the
# last key the editor had processed them. QEMU delivers a sendkey batch at
# its own pace, so the second number is the backlog the app left, if any.
# usage: tests/haiku/typing.sh TEXT
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
vm="$repo/tools/haiku-vm"
text=${1:?usage: typing.sh TEXT}
selection='hey Minuta get selection of View editor of View "editor scroll" of Window 0'
before=$("$vm/ssh" "$selection" | awk '/"result"/ {print $4; exit}')
target=$((before + ${#text}))
start=$(python3 -c 'import time; print(time.time())')
"$vm/type" "$text"
sent=$(python3 -c 'import time; print(time.time())')
"$vm/ssh" "until [ \"\$($selection | awk '/\"result\"/ {print \$4; exit}')\" -ge $target ]; do :; done"
done_at=$(python3 -c 'import time; print(time.time())')
python3 -c "print(f'${#text} keys: sent in {$sent - $start:.1f} s, all in the editor {$done_at - $start:.1f} s after the first key')"
