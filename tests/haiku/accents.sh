#!/bin/bash
# Accented typing on Haiku. The guest keymap has dead keys
# (tools/haiku-vm/us-dead-keys.keymap), so tools/haiku-vm/type sends dead key
# + letter, the way noVNC passes on ' then e from a Mac with Brazilian Pro or
# US International. Minuta must save exactly what was typed, in UTF-8.
# Needs the VM running with the app built (tools/haiku-vm/build).
# usage: tests/haiku/accents.sh [SHOTS_DIR]
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
vm="$repo/tools/haiku-vm"
shots=${1:-}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

lines=("Ação e coração: é, à, ê, õ, ü." "Copo d'água, \"aspas\", ^ e ~ sozinhos." "ÉRAMOS À BEIRA-MAR.")
printf '%s\n' "${lines[@]}" > "$work/expected.md"

"$vm/ssh" 'rm -rf /tmp/ac && mkdir -p /tmp/ac && : > /tmp/ac/acentos.md'
"$vm/launch" src/haiku/objects/Minuta /tmp/ac/acentos.md
sleep 4
for line in "${lines[@]}"; do
  "$vm/type" "$line"
  "$vm/key" ret
  sleep 1
done
"$vm/key" alt-s
sleep 2
[[ -n $shots ]] && "$vm/shot" "$shots/accents.png" >/dev/null
"$vm/key" alt-w
sleep 1
"$vm/pull" /tmp/ac/acentos.md "$work/out"
if cmp -s "$work/expected.md" "$work/out/acentos.md"; then
  echo "ok   accented text typed with dead keys saved as UTF-8"
  echo "accents passed"
else
  echo "FAIL accented text"; diff "$work/expected.md" "$work/out/acentos.md" || true
  exit 1
fi
