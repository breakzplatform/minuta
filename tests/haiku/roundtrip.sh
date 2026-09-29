#!/bin/bash
# Acceptance 5 (Haiku): open a file in Minuta, type "x" at the start and
# save with Alt+S (the file must now hold the extra "x", proving the save
# happened), delete it and save again, close, and compare the bytes with the
# original on the Mac. Needs the VM running with the app built
# (tools/haiku-vm/build).
# usage: tests/haiku/roundtrip.sh [SHOTS_DIR]
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
vm="$repo/tools/haiku-vm"
shots=${1:-}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cp "$repo/samples/teste-utf8.md" "$work/lf.md"
printf '\xEF\xBB\xBF' | cat - "$repo/samples/teste-utf8.md" | sed 's/$/\r/' > "$work/crlf-bom.md"
cp "$repo/samples/TESTE.MD" "$work/cp1252.md"
cp "$repo/samples/UNICODE.MD" "$work/unicode.md"

"$vm/ssh" 'rm -rf /tmp/rt && mkdir -p /tmp/rt'
for f in "$work"/*.md; do "$vm/push-file" "$f" /tmp/rt/; done

failures=0
for name in lf crlf-bom cp1252 unicode; do
  "$vm/launch" src/haiku/objects/Minuta "/tmp/rt/$name.md"
  sleep 4
  "$vm/type" 'x'
  sleep 1
  "$vm/key" alt-s
  sleep 2
  "$vm/pull" "/tmp/rt/$name.md" "$work/edited"
  python3 - "$work/$name.md" "$work/edited/$name.md" <<'PY' || { echo "FAIL $name: edit not saved"; failures=$((failures + 1)); }
import sys
original, edited = open(sys.argv[1], "rb").read(), open(sys.argv[2], "rb").read()
bom = b"\xef\xbb\xbf" if original.startswith(b"\xef\xbb\xbf") else b""
sys.exit(edited != bom + b"x" + original[len(bom):])
PY
  "$vm/key" backspace
  sleep 1
  "$vm/key" alt-s
  sleep 2
  [[ -n $shots ]] && "$vm/shot" "$shots/roundtrip-$name.png" >/dev/null
  "$vm/key" alt-w
  sleep 1
  "$vm/pull" "/tmp/rt/$name.md" "$work/out"
  if cmp -s "$work/$name.md" "$work/out/$name.md"; then
    echo "ok   $name: edit saved, then bytes identical ($(wc -c < "$work/$name.md" | tr -d ' ') bytes)"
  else
    echo "FAIL $name"; cmp "$work/$name.md" "$work/out/$name.md" || true
    failures=$((failures + 1))
  fi
done
((failures == 0)) && echo "round trip passed"
exit $failures
