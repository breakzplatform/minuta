#!/bin/bash
# Acceptance 4 (preview of the smoke sample) and 6 (exported HTML in IE 5),
# plus the word count overlay and Find, on the win98 VM.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
. "$repo/tests/vm/lib.sh"

printf '@echo off\r\nif not exist C:\\MINUTA\\NUL mkdir C:\\MINUTA\r\ncopy D:\\MINUTA.EXE C:\\MINUTA > NUL\r\nif exist C:\\MINUTA\\MINUTA.INI del C:\\MINUTA\\MINUTA.INI\r\ncls\r\n' > "$work/SETUP.BAT"
printf 'transfer\r\n' > "$work/XFER.TXT"

"$bin/stop" win98 >/dev/null
"$bin/start" win98 --loadvm ready
"$bin/push-files" win98 "$repo/build/bin/MINUTA.EXE" "$repo/samples/SMOKE.MD" "$repo/samples/TESTE.MD" \
  "$repo/samples/UNICODE.MD" "$work/SETUP.BAT" >/dev/null
"$bin/push-files" win98 --fd "$work/XFER.TXT" >/dev/null
sleep 2
"$bin/typeln" win98 'D:\SETUP'
sleep 6
open_run
sleep 5
"$bin/typeln" win98 'C:\MINUTA\MINUTA.EXE D:\SMOKE.MD'
"$repo/tests/vm/screen.py" last-change win98 30 3 >/dev/null
"$bin/shot" win98 "$shots/m5-smoke-editor.png" >/dev/null

# Acceptance 4: preview. The caret goes to "## Lists" first, so the
# preview has to open at the matching place.
"$bin/key" win98 down down down down down
"$bin/key" win98 ctrl-r
"$repo/tests/vm/screen.py" last-change win98 30 3 >/dev/null
"$bin/shot" win98 "$shots/m5-preview-lists.png" >/dev/null
"$bin/key" win98 ctrl-home
sleep 3
"$bin/shot" win98 "$shots/m5-preview.png" >/dev/null
"$bin/key" win98 pgdn
sleep 3
"$bin/shot" win98 "$shots/m5-preview-code.png" >/dev/null
"$bin/key" win98 pgdn
sleep 3
"$bin/shot" win98 "$shots/m5-preview-table.png" >/dev/null
"$bin/key" win98 esc
sleep 3
"$bin/shot" win98 "$shots/m5-back-to-editor.png" >/dev/null
pass "preview screenshots taken (check them)"

# Word count overlay and Find.
"$bin/key" win98 ctrl-shift-w
sleep 2
"$bin/shot" win98 "$shots/m5-counters.png" >/dev/null
"$bin/key" win98 ctrl-home
"$bin/key" win98 ctrl-f
sleep 3
"$bin/type" win98 'Nested'
"$bin/key" win98 ret
sleep 2
"$bin/key" win98 esc
sleep 2
"$bin/shot" win98 "$shots/m5-find.png" >/dev/null

# Acceptance 6: export HTML (CP1252 document -> windows-1252 page) and open
# it in IE 5.
open_file 'D:\TESTE.MD'
"$bin/key" win98 ctrl-e
sleep 3
"$bin/type" win98 'A:\TESTE.HTM'
"$bin/key" win98 ret
sleep 4
"$bin/shot" win98 "$shots/m5-export-question.png" >/dev/null
"$bin/key" win98 y
sleep 3
"$repo/tests/vm/screen.py" last-change win98 90 4 >/dev/null
"$bin/shot" win98 "$shots/m5-export-ie5.png" >/dev/null
if wait_floppy_file TESTE.HTM 30 && pull TESTE.HTM \
    && grep -q 'charset=windows-1252' "$work/pulled/TESTE.HTM" \
    && LC_ALL=C grep -q $'<h1>Ol\xe1, Windows 98!</h1>' "$work/pulled/TESTE.HTM"; then
  pass "exported TESTE.HTM is windows-1252 with the accents as bytes"
else
  fail "exported TESTE.HTM"
fi
"$bin/key" win98 alt-f4
sleep 4

# The UTF-8 file with characters outside CP1252 exports them intact.
open_file 'D:\UNICODE.MD'
"$bin/key" win98 ctrl-e
sleep 3
"$bin/type" win98 'A:\UNICODE.HTM'
"$bin/key" win98 ret
sleep 4
"$bin/key" win98 y
sleep 3
"$repo/tests/vm/screen.py" last-change win98 90 4 >/dev/null
"$bin/shot" win98 "$shots/m5-export-unicode-ie5.png" >/dev/null
if wait_floppy_file UNICODE.HTM 30 && pull UNICODE.HTM \
    && grep -q 'charset=utf-8' "$work/pulled/UNICODE.HTM" \
    && grep -q '日本語' "$work/pulled/UNICODE.HTM" && grep -q '😀' "$work/pulled/UNICODE.HTM"; then
  pass "exported UNICODE.HTM keeps the characters outside CP1252"
else
  fail "exported UNICODE.HTM"
fi

"$bin/stop" win98
((failures == 0)) || { echo "$failures check(s) failed"; exit 1; }
echo "preview/export acceptance passed"
