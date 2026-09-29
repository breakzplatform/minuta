#!/bin/bash
# Regenerates the 8.3 copies of the literary samples used by the Windows 98
# and DOS tests: Windows-1252 and code page 850, CRLF line endings, from the
# UTF-8 originals (whose names have spaces).
# usage: tools/dos-samples.sh
set -euo pipefail
samples=$(cd "$(dirname "$0")/../samples" && pwd)
while IFS='|' read -r name cp1252 cp850; do
  iconv -f UTF-8 -t CP1252 "$samples/$name.md" | LC_ALL=C sed $'s/$/\r/' > "$samples/$cp1252.MD"
  iconv -f UTF-8 -t CP850 "$samples/$name.md" | LC_ALL=C sed $'s/$/\r/' > "$samples/$cp850.MD"
done <<'LIST'
Hino de Pernambuco|HINOPE|HINOP850
Poema em linha reta|POEMA|POEMA850
LIST
