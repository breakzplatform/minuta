#!/bin/bash
# Takes the screenshots used by README.md (docs/shots/*.png) on the win98 VM.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
. "$repo/tests/vm/lib.sh"

printf '@echo off\r\nif not exist C:\\MINUTA\\NUL mkdir C:\\MINUTA\r\ncopy D:\\MINUTA.EXE C:\\MINUTA > NUL\r\ncopy D:\\*.MD C:\\MINUTA > NUL\r\nif exist C:\\MINUTA\\MINUTA.INI del C:\\MINUTA\\MINUTA.INI\r\ncls\r\n' > "$work/SETUP.BAT"
settle() { "$repo/tests/vm/screen.py" last-change win98 30 3 2000 >/dev/null; }
launch() {
  open_run
  sleep 5
  "$bin/typeln" win98 "$1"
  settle
}

"$bin/stop" win98 >/dev/null
"$bin/start" win98 --loadvm ready
"$bin/push-files" win98 "$repo/build/bin/MINUTA.EXE" "$repo/samples/POEMA.MD" "$repo/samples/HINOPE.MD" \
  "$work/SETUP.BAT" >/dev/null
sleep 2
"$bin/typeln" win98 'D:\SETUP'
sleep 6

launch 'explorer C:\MINUTA'
"$bin/shot" win98 "$shots/explorer.png" >/dev/null
"$bin/key" win98 alt-f4
sleep 3

launch 'C:\MINUTA\MINUTA.EXE C:\MINUTA\POEMA.MD'
"$bin/key" win98 ctrl-home down down down down down down down down down down
settle
"$bin/shot" win98 "$shots/editor.png" >/dev/null
"$bin/key" win98 ctrl-shift-f
settle
"$bin/shot" win98 "$shots/focus.png" >/dev/null
"$bin/key" win98 ctrl-shift-f
"$bin/key" win98 f11
settle
"$bin/shot" win98 "$shots/fullscreen.png" >/dev/null
"$bin/key" win98 f11
settle

"$bin/key" win98 ctrl-o
sleep 4
"$bin/type" win98 'C:\MINUTA\HINOPE.MD'
"$bin/key" win98 ret
settle
"$bin/key" win98 ctrl-shift-n
settle
"$bin/shot" win98 "$shots/night.png" >/dev/null
"$bin/key" win98 ctrl-shift-n
settle
"$bin/key" win98 ctrl-r
settle
"$bin/shot" win98 "$shots/preview.png" >/dev/null

"$bin/stop" win98
echo "README screenshots taken"
