#!/bin/bash
# The installer on the win98 VM: installs, associates .md with Minuta
# (Enter on a .md file in Explorer, like a double-click, opens it in
# Minuta), and the uninstaller removes the association. Screenshots and
# registry exports go to build/vm/installer/.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
. "$repo/tests/vm/lib.sh"
out=$work/installer
mkdir -p "$out"
cp "$repo/build/dist/minuta-0.1.1-setup.exe" "$work/SETUP.EXE"
printf '@echo off\r\nmkdir C:\\DOCS\r\ncopy D:\\POEMA.MD C:\\DOCS > NUL\r\ncls\r\n' > "$work/DOCS.BAT"
printf 'transfer\r\n' > "$work/XFER.TXT"
run() { "$bin/key" win98 meta_l-r; sleep 5; "$bin/typeln" win98 "$1"; }
settle() { "$repo/tests/vm/screen.py" last-change win98 30 3 2000 >/dev/null; }
open_poem_from_explorer() {
  run 'explorer C:\DOCS'
  settle
  "$bin/key" win98 p        # select POEMA.MD in the file list
  sleep 1
  "$bin/key" win98 ret      # same as double-clicking it
  sleep 8
}
export_md_key() {  # FILE on the floppy
  run "regedit /e A:\\$1 HKEY_CLASSES_ROOT\\.md"
  sleep 6
}

"$bin/stop" win98 >/dev/null
"$bin/start" win98 --loadvm ready
"$bin/push-files" win98 "$work/SETUP.EXE" "$repo/samples/POEMA.MD" "$work/DOCS.BAT" >/dev/null
"$bin/push-files" win98 --fd "$work/XFER.TXT" >/dev/null
sleep 2
"$bin/typeln" win98 'D:\DOCS'
sleep 4

run 'D:\SETUP.EXE'
sleep 8
"$bin/key" win98 ret        # Install
sleep 10
"$bin/key" win98 ret        # Close
sleep 4

open_poem_from_explorer
"$bin/shot" win98 "$out/opened.png" >/dev/null
# A spot of the editor away from the mouse cursor, which rests at 400,300.
if [[ $("$repo/tests/vm/screen.py" pixel win98 700 460) =~ ^[E-F] ]]; then
  pass "Enter on POEMA.MD in Explorer opened a light editor window"
else
  fail "Enter on POEMA.MD did not open Minuta"
fi
"$bin/key" win98 alt-f4    # Minuta
sleep 3
"$bin/shot" win98 "$out/explorer-installed.png" >/dev/null
"$bin/key" win98 alt-f4    # Explorer
sleep 3
export_md_key INST.REG
if wait_floppy_file INST.REG 30 && pull INST.REG && iconv -f UTF-16 -t UTF-8 "$work/pulled/INST.REG" 2>/dev/null | grep -q 'Minuta.Document' \
    || grep -qa 'Minuta.Document' "$work/pulled/INST.REG"; then
  pass "installed: HKCR\\.md points to Minuta.Document"
else
  fail "installed: HKCR\\.md not set"
fi

run '"C:\Program Files\Minuta\UNINST.EXE"'
sleep 8
"$bin/key" win98 ret        # Uninstall
sleep 10
"$bin/key" win98 ret        # Close
sleep 4
export_md_key UNINST.REG
# regedit writes nothing when the key doesn't exist; give the floppy time.
if wait_floppy_file UNINST.REG 30; then
  fail "uninstalled: HKCR\\.md still exists"
else
  pass "uninstalled: HKCR\\.md is gone (regedit had nothing to export)"
fi
open_poem_from_explorer
"$bin/shot" win98 "$out/after-uninstall.png" >/dev/null

"$bin/stop" win98
((failures == 0)) || { echo "$failures check(s) failed"; exit 1; }
echo "installer acceptance passed"
