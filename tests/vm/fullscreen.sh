#!/bin/bash
# Acceptance 10: F11 full screen on the win98 VM. Checked with pixels: in
# the normal window the top row of the screen is desktop and there is a
# taskbar; in full screen both are the editor background. Screenshots go to
# docs/shots/.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
. "$repo/tests/vm/lib.sh"

printf '@echo off\r\nif not exist C:\\MINUTA\\NUL mkdir C:\\MINUTA\r\ncopy D:\\MINUTA.EXE C:\\MINUTA > NUL\r\nif exist C:\\MINUTA\\MINUTA.INI del C:\\MINUTA\\MINUTA.INI\r\ncls\r\n' > "$work/SETUP.BAT"

light() { [[ $("$repo/tests/vm/screen.py" pixel win98 "$1" "$2") =~ ^[E-F][0-9A-F][E-F][0-9A-F][E-F][0-9A-F]$ ]]; }
check() {  # LABEL EXPECT(light|dark) X Y
  if { [[ $2 == light ]] && light "$3" "$4"; } || { [[ $2 == dark ]] && ! light "$3" "$4"; }; then pass "$1"
  else fail "$1 (pixel $3,$4 is $("$repo/tests/vm/screen.py" pixel win98 "$3" "$4"))"; fi
}

"$bin/stop" win98 >/dev/null
"$bin/start" win98 --loadvm ready
"$bin/push-files" win98 "$repo/build/bin/MINUTA.EXE" "$repo/samples/SMOKE.MD" "$work/SETUP.BAT" >/dev/null
sleep 2
"$bin/typeln" win98 'D:\SETUP'
sleep 6
"$bin/key" win98 meta_l-r
sleep 5
"$bin/typeln" win98 'C:\MINUTA\MINUTA.EXE D:\SMOKE.MD'
"$repo/tests/vm/screen.py" last-change win98 30 3 2000 >/dev/null
check "windowed: top of the screen is desktop" dark 400 8
check "windowed: taskbar visible" dark 620 588

"$bin/key" win98 f11
"$repo/tests/vm/screen.py" last-change win98 30 3 2000 >/dev/null
"$bin/shot" win98 "$shots/m7-fullscreen.png" >/dev/null
check "full screen covers the top edge (no caption, no menu)" light 400 8
check "full screen covers the taskbar" light 620 588
check "full screen covers the left edge" light 2 300

# Typing works and the column stays centered.
"$bin/key" win98 ctrl-end
"$bin/typeln" win98 ''
"$bin/type" win98 'Written in *full screen*, nothing else on the screen.'
sleep 4
"$bin/shot" win98 "$shots/m7-fullscreen-typing.png" >/dev/null

# Esc leaves the preview first, then full screen.
"$bin/key" win98 ctrl-r
"$repo/tests/vm/screen.py" last-change win98 30 3 2000 >/dev/null
"$bin/shot" win98 "$shots/m7-fullscreen-preview.png" >/dev/null
"$bin/key" win98 esc
sleep 3
check "Esc from the preview stays in full screen" light 400 8
"$bin/key" win98 esc
"$repo/tests/vm/screen.py" last-change win98 30 3 2000 >/dev/null
"$bin/shot" win98 "$shots/m7-restored.png" >/dev/null
check "second Esc restores the window (desktop at the top)" dark 400 8
check "second Esc restores the taskbar" dark 620 588

# F11 toggles too.
"$bin/key" win98 f11
sleep 4
check "F11 enters full screen again" light 400 8
"$bin/key" win98 f11
sleep 4
check "F11 leaves full screen" dark 400 8

"$bin/stop" win98
((failures == 0)) || { echo "$failures check(s) failed"; exit 1; }
echo "fullscreen acceptance passed"
