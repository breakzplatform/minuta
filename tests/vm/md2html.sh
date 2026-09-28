#!/bin/bash
# Acceptance 8: md2html on the win98, freedos and dos622 VMs.
# Converts the samples in each VM, pulls the HTML back through the transfer
# floppy and compares it byte for byte with tests/cli/expected (CRLF, since
# the DOS/Win32 C runtime writes text mode). Screenshots go to docs/shots/.
# usage: tests/vm/md2html.sh [win98|freedos|dos622]...   (default: all three)
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
bin=$HOME/vms/retro/bin
shots=$repo/docs/shots
work=$repo/build/vm-md2html
rm -rf "$work"; mkdir -p "$work"

mkdir -p "$shots"
failures=0

LC_ALL=C sed 's/$/\r/' "$repo/tests/cli/expected/teste.html" > "$work/utf8.html"
iconv -f UTF-8 -t CP850 "$repo/tests/cli/expected/teste.html" | LC_ALL=C sed 's/$/\r/' > "$work/cp850.html"

wait_done() {  # wait_done VM: until the batch file writes DONE.TXT to the floppy
  local image=$HOME/vms/retro/run/$1-xfer.img
  for _ in $(seq 120); do
    mdir -b -i "$image" ::/DONE.TXT >/dev/null 2>&1 && { sleep 2; return; }
    sleep 1
  done
  echo "FAIL $1: batch file did not finish"; failures=$((failures + 1))
}

compare() {  # compare VM FILE EXPECTED
  if cmp -s "$2" "$3"; then echo "ok   $1 $(basename "$2")"
  else echo "FAIL $1 $(basename "$2")"; cmp "$2" "$3" || true; failures=$((failures + 1)); fi
}

run_win98() {
  cp "$repo/samples/teste-utf8.md" "$work/TESTEU8.MD"
  printf 'transfer\r\n' > "$work/XFER.TXT"
  "$bin/start" win98 --loadvm ready
  "$bin/push-files" win98 "$repo/build/bin/MD2HTMLW.EXE" "$repo/samples/TESTE.MD" \
    "$work/TESTEU8.MD" "$repo/tests/vm/md2html/RUN98.BAT"
  "$bin/push-files" win98 --fd "$work/XFER.TXT"
  sleep 2
  "$bin/typeln" win98 'D:\RUN98.BAT'
  wait_done win98
  "$bin/settle" win98 60 3 "$shots/md2html-win98-console.png"
  "$bin/pull-files" win98 "$work/win98" T1252.HTM TUTF8.HTM TFULL.HTM >/dev/null
  compare win98 "$work/win98/T1252.HTM" "$work/utf8.html"
  compare win98 "$work/win98/TUTF8.HTM" "$work/utf8.html"
  if grep -q 'charset=utf-8' "$work/win98/TFULL.HTM"; then echo "ok   win98 TFULL.HTM declares utf-8"
  else echo "FAIL win98 TFULL.HTM declares utf-8"; failures=$((failures + 1)); fi
  "$bin/typeln" win98 'start C:\TESTE.HTM'
  "$bin/settle" win98 90 4 "$shots/md2html-win98-ie5.png"
  "$bin/stop" win98
}

run_dos() {  # run_dos VM
  "$bin/start" "$1" --loadvm ready
  "$bin/push-files" "$1" "$repo/build/bin/MD2HTMLD.EXE" "$repo/samples/TESTE850.MD" \
    "$repo/samples/TESTE.MD" "$repo/tests/vm/md2html/RUNDOS.BAT"
  sleep 1
  "$bin/typeln" "$1" 'A:\RUNDOS.BAT'
  wait_done "$1"
  "$bin/settle" "$1" 60 3 "$shots/md2html-$1.png"
  "$bin/pull-files" "$1" "$work/$1" T850.HTM T1252.HTM >/dev/null
  compare "$1" "$work/$1/T850.HTM" "$work/utf8.html"
  compare "$1" "$work/$1/T1252.HTM" "$work/utf8.html"
  "$bin/stop" "$1"
}

for vm in "${@:-win98 freedos dos622}"; do
  for v in $vm; do
    case $v in
      win98) run_win98 ;;
      freedos|dos622) run_dos "$v" ;;
      *) echo "unknown vm $v" >&2; exit 2 ;;
    esac
  done
done
((failures == 0)) || { echo "$failures check(s) failed"; exit 1; }
echo "md2html VM acceptance passed"
