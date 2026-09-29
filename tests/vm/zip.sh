#!/bin/bash
# The release ZIPs on the win98 VM: unpack them in the MS-DOS Prompt with
# FreeDOS's UNZIP.EXE (Windows 98 can't open ZIPs by itself), run the
# unpacked md2html builds on the samples and start the unpacked MINUTA.EXE.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
. "$repo/tests/vm/lib.sh"
version=$(sed -n 's/^VERSION := //p' "$repo/Makefile")

bsdtar -xOf "$HOME/vms/retro/iso/fd13/FD13LGCY.iso" packages/archiver/unzip.zip > "$work/fd-unzip.zip"
unzip -o -q -j "$work/fd-unzip.zip" BIN/UNZIP.EXE -d "$work"
cp "$repo/build/dist/minuta-$version-win98.zip" "$work/WIN98.ZIP"
cp "$repo/build/dist/minuta-$version-dos.zip" "$work/DOS.ZIP"
printf '@echo off\r\nmkdir C:\\MZIP\r\nD:\\UNZIP.EXE -o D:\\WIN98.ZIP -d C:\\MZIP\r\nD:\\UNZIP.EXE -o D:\\DOS.ZIP -d C:\\MZIP\\DOS\r\nC:\\MZIP\\DOS\\MD2HTMLD.EXE --codepage 850 D:\\POEMA850.MD -o A:\\P850.HTM\r\nC:\\MZIP\\MD2HTMLW.EXE --codepage 1252 D:\\POEMA.MD -o A:\\P1252.HTM\r\ncopy C:\\MZIP\\README.TXT A:\\ > NUL\r\necho done> A:\\DONE.TXT\r\n' > "$work/UNPACK.BAT"
printf 'transfer\r\n' > "$work/XFER.TXT"
"$repo/build/host/md2html" --codepage utf8 "$repo/samples/Poema em linha reta.md" | LC_ALL=C sed 's/$/\r/' > "$work/poema-expected.html"
iconv -f UTF-8 -t CP1252 "$repo/dist/README.TXT" | LC_ALL=C sed 's/$/\r/' > "$work/readme-expected.txt"

"$bin/stop" win98 >/dev/null
"$bin/start" win98 --loadvm ready
"$bin/push-files" win98 "$work/WIN98.ZIP" "$work/DOS.ZIP" "$work/UNZIP.EXE" "$work/UNPACK.BAT" \
  "$repo/samples/POEMA.MD" "$repo/samples/POEMA850.MD" >/dev/null
"$bin/push-files" win98 --fd "$work/XFER.TXT" >/dev/null
sleep 2
"$bin/typeln" win98 'D:\UNPACK'
wait_floppy_file DONE.TXT 120 || fail "UNPACK.BAT did not finish"
expect_floppy_file "MD2HTMLD.EXE from the DOS ZIP converts the CP850 sample" P850.HTM "$work/poema-expected.html" || true
expect_floppy_file "MD2HTMLW.EXE from the Win98 ZIP converts the CP1252 sample" P1252.HTM "$work/poema-expected.html" || true
expect_floppy_file "README.TXT in the ZIP is CP1252 with CRLF" README.TXT "$work/readme-expected.txt" || true
"$bin/key" win98 meta_l-r
sleep 5
"$bin/typeln" win98 'C:\MZIP\MINUTA.EXE D:\POEMA.MD'
"$repo/tests/vm/screen.py" last-change win98 30 3 2000 >/dev/null
if [[ $("$repo/tests/vm/screen.py" pixel win98 700 460) =~ ^[E-F] ]]; then pass "MINUTA.EXE from the ZIP opens the sample"
else fail "MINUTA.EXE from the ZIP did not open"; fi
"$bin/shot" win98 "$work/zip-minuta.png" >/dev/null
"$bin/stop" win98
((failures == 0)) || { echo "$failures check(s) failed"; exit 1; }
echo "zip acceptance passed"
