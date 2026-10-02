#!/bin/bash
# Acceptance 2 (typing 2,000 words), 3 (focus, typewriter, night) and 7
# (1 MB file) on the win98 VM. Screenshots go to docs/shots/.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
. "$repo/tests/vm/lib.sh"

seconds() { python3 -c 'import time; print(time.time())'; }
elapsed() { python3 -c "print(round($(seconds) - $1, 2))"; }
under() { awk "BEGIN { exit !($1 < $2) }"; }

python3 "$repo/tests/vm/make-samples.py" typing "$work/typing.md"
python3 "$repo/tests/vm/make-samples.py" large "$work/LARGE.MD"
LC_ALL=C sed 's/$/\r/' "$work/typing.md" > "$work/expected-typed.md"
printf '@echo off\r\nif not exist C:\\MINUTA\\NUL mkdir C:\\MINUTA\r\ncopy D:\\MINUTA.EXE C:\\MINUTA > NUL\r\nif exist C:\\MINUTA\\MINUTA.INI del C:\\MINUTA\\MINUTA.INI\r\ncls\r\n' > "$work/SETUP.BAT"
printf 'transfer\r\n' > "$work/XFER.TXT"

"$bin/stop" win98 >/dev/null
"$bin/start" win98 --loadvm ready
"$bin/push-files" win98 "$repo/build/bin/MINUTA.EXE" "$work/LARGE.MD" "$work/SETUP.BAT" >/dev/null
"$bin/push-files" win98 --fd "$work/XFER.TXT" >/dev/null
sleep 2
"$bin/typeln" win98 'D:\SETUP'
sleep 6
"$bin/key" win98 meta_l-r
sleep 5
"$bin/typeln" win98 'C:\MINUTA\MINUTA.EXE'
"$repo/tests/vm/screen.py" last-change win98 30 3 >/dev/null
sleep 2

# Acceptance 2: type ~2,000 words line by line through bin/type. QEMU drops
# input events when too many are queued, so each line waits for the keys
# already sent to drain (~20 characters per second; a burst of one line
# is fine).
start=$(seconds)
while IFS= read -r line; do
  [[ -n $line ]] && "$bin/type" win98 "$line"
  "$bin/key" win98 ret
  sleep "$(python3 -c "print(round(${#line} / 20 + 0.3, 2))")"
done < "$work/typing.md"
"$repo/tests/vm/screen.py" last-change win98 1800 5 >/dev/null
words=$(wc -w < "$work/typing.md" | tr -d ' ')
pass "typed $words words in $(elapsed "$start") s (screen settled)"
"$bin/shot" win98 "$shots/m4-typing-end.png" >/dev/null
"$bin/key" win98 ctrl-home
sleep 3
"$bin/shot" win98 "$shots/m4-typing.png" >/dev/null
save_as 'A:\TYPED.MD'
expect_floppy_file "all $words typed words arrived (no dropped keys)" TYPED.MD "$work/expected-typed.md" 90 || true

# Acceptance 3: focus mode (sentence), typewriter, night theme.
"$bin/key" win98 down down down
sleep 1
"$bin/key" win98 ctrl-shift-f
sleep 4
"$bin/shot" win98 "$shots/m4-focus.png" >/dev/null
"$bin/key" win98 ctrl-shift-p
sleep 4
"$bin/shot" win98 "$shots/m4-focus-paragraph.png" >/dev/null
"$bin/key" win98 ctrl-shift-p
"$bin/key" win98 ctrl-shift-f
sleep 2
"$bin/key" win98 ctrl-shift-t
sleep 1
"$bin/key" win98 ctrl-end
"$bin/typeln" win98 ''
"$bin/type" win98 'The caret line stays in the *middle* of the window.'
sleep 4
"$bin/shot" win98 "$shots/m4-typewriter.png" >/dev/null
"$bin/key" win98 ctrl-shift-t
"$bin/key" win98 ctrl-home
"$bin/key" win98 ctrl-shift-n
sleep 4
"$bin/shot" win98 "$shots/m4-night.png" >/dev/null
"$bin/key" win98 ctrl-shift-n
sleep 2
"$bin/key" win98 ctrl-s
sleep 5

# Acceptance 7: 1 MB document.
"$bin/key" win98 ctrl-o
sleep 4
"$bin/type" win98 'D:\LARGE.MD'
start=$(seconds)
"$bin/key" win98 ret
"$repo/tests/vm/screen.py" last-change win98 60 3 >/dev/null
open_time=$(python3 -c "print(round($(seconds) - $start - 3, 2))")
if under "$open_time" 5; then pass "1 MB file open and styled in ${open_time} s (< 5 s)"
else fail "1 MB file took ${open_time} s to open"; fi
"$bin/shot" win98 "$shots/m4-large-top.png" >/dev/null
worst=0
for key in ctrl-end pgup pgup pgup pgup ctrl-home pgdn pgdn pgdn pgdn; do
  "$bin/key" win98 "$key"
  t=$("$repo/tests/vm/screen.py" last-change win98 30 2)
  worst=$(python3 -c "print(max($worst, $t))")
done
if under "$worst" 2; then pass "scrolling the 1 MB file: slowest step ${worst} s (< 2 s)"
else fail "scrolling the 1 MB file stalled ${worst} s"; fi
"$bin/key" win98 ctrl-end
sleep 3
"$bin/shot" win98 "$shots/m4-large-end.png" >/dev/null

# The scroll bar thumb follows the view in the 1 MB file: bottom at the end,
# top at the start, about halfway at chapter 500 of ~1,000. At the end it is
# the minimum 8 pixels flush with the down arrow, easy to miss by eye.
expect_thumb() {  # LABEL LOW HIGH
  local at
  at=$("$repo/tests/vm/screen.py" thumb win98 740 64 545) || true
  if [[ $at != none ]] && awk "BEGIN { exit !($at >= $2 && $at <= $3) }"; then pass "$1: thumb at $at"
  else fail "$1: thumb at $at, expected $2..$3"; fi
}
expect_thumb "scroll bar at the end of the 1 MB file" 0.97 1
"$bin/key" win98 ctrl-home
"$repo/tests/vm/screen.py" last-change win98 30 2 >/dev/null
expect_thumb "scroll bar at the start of the 1 MB file" 0 0.03
# Find searches down from the caret, so this starts at the top.
"$bin/key" win98 ctrl-f
sleep 3
"$bin/type" win98 'tulo 500'
"$bin/key" win98 ret
sleep 3
"$bin/key" win98 esc
"$repo/tests/vm/screen.py" last-change win98 30 2 >/dev/null
expect_thumb "scroll bar in the middle of the 1 MB file" 0.4 0.6

"$bin/stop" win98
((failures == 0)) || { echo "$failures check(s) failed"; exit 1; }
echo "editor acceptance passed"
