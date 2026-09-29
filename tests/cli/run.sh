#!/bin/bash
# md2html regression tests against the samples; run by `make test`.
# usage: tests/cli/run.sh path/to/md2html
set -euo pipefail
md2html=${1:?usage: run.sh path/to/md2html}
here=$(cd "$(dirname "$0")" && pwd)
samples="$here/../../samples"
expected="$here/expected"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0

check() {  # check NAME FILE EXPECTED_FILE
  if cmp -s "$2" "$3"; then echo "ok   $1"
  else echo "FAIL $1"; diff <(xxd "$3") <(xxd "$2") | head -20; failures=$((failures + 1)); fi
}

"$md2html" --gfm --codepage 1252 "$samples/TESTE.MD" -o "$work/1252.html"
check "cp1252 input" "$work/1252.html" "$expected/teste.html"

"$md2html" --gfm "$samples/TESTE.MD" -o "$work/default.html"
check "cp1252 is the default outside DOS" "$work/default.html" "$expected/teste.html"

"$md2html" --gfm --codepage 850 "$samples/TESTE850.MD" -o "$work/850.html"
check "cp850 input" "$work/850.html" "$expected/teste.html"

"$md2html" --gfm --codepage utf8 "$samples/teste-utf8.md" -o "$work/utf8.html"
check "utf8 input" "$work/utf8.html" "$expected/teste.html"

printf '\xEF\xBB\xBF' | cat - "$samples/teste-utf8.md" > "$work/bom.md"
"$md2html" --gfm --codepage 850 "$work/bom.md" -o "$work/bom.html"
check "BOM overrides --codepage" "$work/bom.html" "$expected/teste.html"

"$md2html" --gfm --codepage 850 --out-codepage 1252 "$samples/TESTE850.MD" -o "$work/out1252.html"
iconv -f UTF-8 -t CP1252 "$expected/teste.html" > "$work/expected1252.html"
check "--out-codepage 1252" "$work/out1252.html" "$work/expected1252.html"

printf '# \xE2\x82\xAC \xE6\x97\xA5\n' > "$work/wide.md"
"$md2html" --codepage utf8 --out-codepage 850 "$work/wide.md" -o "$work/wide.html"
printf '<h1>&#8364; &#26085;</h1>\n' > "$work/wide-expected.html"
check "unmappable output becomes character references" "$work/wide.html" "$work/wide-expected.html"

"$md2html" --gfm --full-html --out-codepage 1252 "$samples/TESTE.MD" -o "$work/full.html"
if grep -q 'content="text/html; charset=windows-1252"' "$work/full.html"; then echo "ok   full html declares the output charset"
else echo "FAIL full html declares the output charset"; failures=$((failures + 1)); fi

if "$md2html" --codepage latin9 "$samples/TESTE.MD" >/dev/null 2>&1; then
  echo "FAIL unknown code page is rejected"; failures=$((failures + 1))
else echo "ok   unknown code page is rejected"; fi

# The literary samples: the UTF-8 original and its CP1252 and CP850 copies
# (CRLF) must render the same HTML. The originals have spaces in their names.
for sample in "Hino de Pernambuco|HINOPE|HINOP850" "Poema em linha reta|POEMA|POEMA850" \
    "Luar do Sertão|LUAR|LUAR850"; do
  IFS='|' read -r name cp1252 cp850 <<< "$sample"
  "$md2html" --codepage utf8 "$samples/$name.md" -o "$work/$cp1252-utf8.html"
  "$md2html" --codepage 1252 "$samples/$cp1252.MD" -o "$work/$cp1252.html"
  "$md2html" --codepage 850 "$samples/$cp850.MD" -o "$work/$cp850.html"
  check "$cp1252.MD (CP1252) renders like $name.md" "$work/$cp1252.html" "$work/$cp1252-utf8.html"
  check "$cp850.MD (CP850) renders like $name.md" "$work/$cp850.html" "$work/$cp1252-utf8.html"
done

((failures == 0)) || { echo "$failures md2html test(s) failed"; exit 1; }
echo "md2html tests passed"
