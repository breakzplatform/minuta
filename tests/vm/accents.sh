#!/bin/bash
# Accented typing on the win98 VM. The guest layout is United States-
# International (KEYMAP in ~/vms/retro/vms/win98.conf), so bin/type sends
# dead key + letter, the way noVNC passes on ' then e from a Mac with
# Brazilian Pro or US International. Minuta must save what was typed: as
# UTF-8 for a new file, and as Windows-1252 through Save As.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
. "$repo/tests/vm/lib.sh"

lines=("Ação e coração: é, à, ê, õ, ü." "Copo d'água, \"aspas\", ^ e ~ sozinhos." "ÉRAMOS À BEIRA-MAR.")
printf '%s\r\n' "${lines[@]}" > "$work/expected-accents.md"
iconv -f UTF-8 -t CP1252 "$work/expected-accents.md" > "$work/expected-accents-1252.md"
printf '@echo off\r\nif not exist C:\\MINUTA\\NUL mkdir C:\\MINUTA\r\ncopy D:\\MINUTA.EXE C:\\MINUTA > NUL\r\nif exist C:\\MINUTA\\MINUTA.INI del C:\\MINUTA\\MINUTA.INI\r\ncls\r\n' > "$work/SETUP.BAT"
printf 'transfer\r\n' > "$work/XFER.TXT"

"$bin/stop" win98 >/dev/null
"$bin/start" win98 --loadvm ready
"$bin/push-files" win98 "$repo/build/bin/MINUTA.EXE" "$work/SETUP.BAT" >/dev/null
"$bin/push-files" win98 --fd "$work/XFER.TXT" >/dev/null
sleep 2
"$bin/typeln" win98 'D:\SETUP'
sleep 6
# From the MS-DOS Prompt: Win+R through sendkey sometimes leaves the
# Windows key down in the guest, and the typed path then fires Win+E etc.
"$bin/typeln" win98 'start C:\MINUTA\MINUTA.EXE'
"$repo/tests/vm/screen.py" last-change win98 30 3 >/dev/null
sleep 2

for line in "${lines[@]}"; do
  "$bin/type" win98 "$line"
  "$bin/key" win98 ret
  sleep "$(python3 -c "print(round(${#line} / 10 + 0.5, 2))")"
done
"$repo/tests/vm/screen.py" last-change win98 60 3 >/dev/null
"$bin/shot" win98 "$work/accents.png" >/dev/null
save_as 'A:\ACENTOS.MD'
expect_floppy_file "accented text typed with dead keys saved as UTF-8" ACENTOS.MD "$work/expected-accents.md" || true
save_as 'A:\AC1252.MD' down
expect_floppy_file "the same text saved as Windows-1252" AC1252.MD "$work/expected-accents-1252.md" || true

"$bin/stop" win98
((failures == 0)) || { echo "$failures check(s) failed"; exit 1; }
echo "accents acceptance passed"
