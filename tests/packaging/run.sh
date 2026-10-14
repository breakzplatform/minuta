#!/bin/bash
# Checks the example texts that go into the installers: what tools/examples.sh
# stages for each platform, and that the NSIS script, the Haiku Makefile and
# the HaikuPorts recipe install (and the uninstaller removes) every one.
# usage: tests/packaging/run.sh
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $1"; failures=$((failures + 1)); }
pass() { echo "ok: $1"; }

"$repo/tools/examples.sh" windows "$work/win"
"$repo/tools/examples.sh" haiku "$work/haiku"

expected=("Hino de Pernambuco" "Poema em linha reta" "Luar do Sertão" "About the examples")
for name in "${expected[@]}"; do
  [ -f "$repo/samples/$name.md" ] || fail "samples/$name.md is missing"
  [ -f "$work/haiku/$name.md" ] || fail "Haiku staging lacks $name.md"
  cmp -s "$repo/samples/$name.md" "$work/haiku/$name.md" || fail "Haiku copy of $name.md differs from the original"
done
[ "$(ls "$work/haiku" | wc -l)" -eq "${#expected[@]}" ] || fail "Haiku staging has unexpected files"
[ "$(ls "$work/win" | wc -l)" -eq "${#expected[@]}" ] || fail "Windows staging has unexpected files"
pass "Haiku staging is the UTF-8 originals"

for file in "$work"/win/*; do
  base=$(basename "$file")
  if LC_ALL=C grep -q '[^ -~]' <<< "$base"; then fail "Windows name is not ASCII: $base"; fi
  matched=
  for name in "${expected[@]}"; do
    cmp -s <(tr -d '\r' < "$file" | iconv -f CP1252 -t UTF-8) "$repo/samples/$name.md" && matched=$name
  done
  [ -n "$matched" ] || fail "$base does not decode (CP1252, CRLF) to any original"
  [ "$(grep -c $'\r$' "$file")" -eq "$(wc -l < "$file")" ] || fail "$base has lines without CRLF"
  # The accents must be single bytes, as RichEdit on Windows 98 reads them.
  if LC_ALL=C grep -q $'\xC3' "$file"; then fail "$base still has UTF-8 sequences"; fi
done
LC_ALL=C grep -q $'\xE3' "$work/win/Luar do Sertao.md" || fail "Luar do Sertao.md lost its a-tilde"
pass "Windows staging is Windows-1252 with CRLF and ASCII names"

nsi="$repo/dist/minuta.nsi"
for file in "$work"/win/*; do
  base=$(basename "$file")
  grep -qF "File \"..\\build\\dist\\win98\\Examples\\$base\"" "$nsi" || fail "installer does not install $base"
  grep -qF "Delete \"\$INSTDIR\\Examples\\$base\"" "$nsi" || fail "uninstaller does not delete $base"
done
grep -qF 'RMDir "$INSTDIR\Examples"' "$nsi" || fail "uninstaller does not remove the Examples folder"
grep -qF 'Examples.lnk' "$nsi" || fail "no Start menu shortcut to Examples"
pass "NSIS script installs and removes each example"

grep -q 'tools/examples.sh haiku $(STAGE)/data/Minuta/Examples' "$repo/src/haiku/Makefile" || fail "make package does not stage the examples"
grep -q 'tools/examples.sh haiku $(DATA_DIR)/Minuta/Examples' "$repo/src/haiku/Makefile" || fail "make install does not copy the examples"
grep -q 'tools/examples.sh haiku $dataDir/Minuta/Examples' "$repo"/haiku/recipe/*.recipe || fail "recipe does not install the examples"
grep -q 'tools/examples.sh windows' "$repo/Makefile" || fail "make dist does not stage the examples"
pass "Makefiles and recipe call tools/examples.sh"

# Needs a built package; `package` only exists on Haiku.
if [ -n "${1:-}" ]; then
  listing=$(mktemp)
  package list "$1" > "$listing"
  for name in "${expected[@]}"; do
    grep -qF "data/Minuta/Examples/$name.md" "$listing" || fail "$1 lacks $name.md"
  done
  rm -f "$listing"
  pass "$1 lists the examples"
fi

[ "$failures" -eq 0 ] || { echo "$failures check(s) failed"; exit 1; }
