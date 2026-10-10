#!/bin/bash
# Acceptance 1 (start time, CP1252 and UTF-8 samples) and 5 (save round trips)
# on the win98 VM. Screenshots go to docs/shots/.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
. "$repo/tests/vm/lib.sh"

cp "$repo/samples/teste-utf8.md" "$work/TESTEU8.MD"
printf '@echo off\r\nif not exist C:\\MINUTA\\NUL mkdir C:\\MINUTA\r\ncopy D:\\MINUTA.EXE C:\\MINUTA > NUL\r\nif exist C:\\MINUTA\\MINUTA.INI del C:\\MINUTA\\MINUTA.INI\r\ncls\r\n' > "$work/SETUP.BAT"
printf 'transfer\r\n' > "$work/XFER.TXT"

"$bin/stop" win98 >/dev/null
"$bin/start" win98 --loadvm ready
"$bin/push-files" win98 "$repo/build/bin/MINUTA.EXE" "$repo/samples/TESTE.MD" "$work/TESTEU8.MD" \
  "$repo/samples/UNICODE.MD" "$work/SETUP.BAT" >/dev/null
"$bin/push-files" win98 --fd "$work/XFER.TXT" >/dev/null
sleep 2
"$bin/typeln" win98 'D:\SETUP'
sleep 6

# Acceptance 1: time from Enter in Start > Run to the last screen change
# (window painted with the document). Launching from the MS-DOS Prompt
# instead adds ~1.7 s of console overhead (Notepad pays it too).
open_run
sleep 3
"$bin/type" win98 'C:\MINUTA\MINUTA.EXE D:\TESTE.MD'
sleep 2
"$bin/key" win98 ret
# Changes under 2,000 pixels (mouse cursor turning from hourglass to I-beam,
# caret blink) don't count; the document paint is ~350,000.
elapsed=$("$repo/tests/vm/screen.py" last-change win98 20 3 2000)
if [[ $elapsed != timeout ]] && awk "BEGIN { exit !($elapsed < 2.0) }"; then
  pass "MINUTA.EXE shows TESTE.MD ${elapsed}s after Enter (last screen change; < 2 s)"
else
  fail "MINUTA.EXE start took ${elapsed}s"
fi
"$bin/settle" win98 20 2 "$shots/m3-open-cp1252.png" >/dev/null
open_file 'D:\TESTEU8.MD'
"$bin/settle" win98 20 2 "$shots/m3-open-utf8.png" >/dev/null
open_file 'D:\UNICODE.MD'
"$bin/settle" win98 20 2 "$shots/m3-open-unicode.png" >/dev/null

# Acceptance 5: save round trips, pulled back through the floppy.
LC_ALL=C iconv -f CP1252 -t UTF-8 "$repo/samples/TESTE.MD" > "$work/expected-conv8.md"
LC_ALL=C iconv -f UTF-8 -t CP1252 "$repo/samples/teste-utf8.md" > "$work/expected-conv1252.md"
{ cat "$repo/samples/UNICODE.MD"; printf 'Edited line.'; } > "$work/expected-edit8.md"

save_as 'A:\U8.MD'
expect_floppy_file "UTF-8 with characters outside CP1252 saved byte for byte" U8.MD "$repo/samples/UNICODE.MD" || true

"$bin/key" win98 ctrl-end
"$bin/type" win98 'Edited line.'
sleep 1
"$bin/shot" win98 "$shots/m3-modified.png" >/dev/null
# Crash safety: a modified document is copied to name.md~ every 30 s. The
# guest clock runs 3-4x slower than real time here, so allow 180 s.
expect_floppy_file "autosave wrote U8.MD~ with the unsaved edit" 'U8.MD~' "$work/expected-edit8.md" 180 || true
"$bin/key" win98 ctrl-s
expect_floppy_file "edited UTF-8 file keeps the untouched lines" U8.MD "$work/expected-edit8.md" || true
sleep 4
if mdir -b -i "$HOME/vms/retro/run/win98-xfer.img" '::/U8.MD~' >/dev/null 2>&1; then
  fail "U8.MD~ still there after saving"
else
  pass "saving removed U8.MD~"
fi

open_file 'D:\TESTE.MD'
save_as 'A:\C1252.MD'
expect_floppy_file "CP1252 file saved byte for byte" C1252.MD "$repo/samples/TESTE.MD" || true
save_as 'A:\CONV8.MD' up
expect_floppy_file "CP1252 file saved as UTF-8" CONV8.MD "$work/expected-conv8.md" || true

open_file 'D:\TESTEU8.MD'
save_as 'A:\UTF8.MD'
expect_floppy_file "UTF-8 (LF) file saved byte for byte" UTF8.MD "$repo/samples/teste-utf8.md" || true
save_as 'A:\CONV1252.MD' down
expect_floppy_file "UTF-8 file saved as CP1252 keeps LF" CONV1252.MD "$work/expected-conv1252.md" || true
"$bin/settle" win98 20 2 "$shots/m3-saved.png" >/dev/null

# Settings persist in MINUTA.INI beside the exe.
"$bin/key" win98 alt-f4
sleep 4
"$bin/typeln" win98 'type C:\MINUTA\MINUTA.INI'
"$bin/settle" win98 20 2 "$shots/m3-settings-ini.png" >/dev/null

"$bin/stop" win98
((failures == 0)) || { echo "$failures check(s) failed"; exit 1; }
echo "files acceptance passed"
