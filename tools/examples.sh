#!/bin/bash
# Stages the example texts that ship with the installers.
#   windows  Windows-1252, CRLF and ASCII file names, for the Windows 98 installer
#   haiku    the UTF-8 originals, for the Haiku package and `make install`
# usage: tools/examples.sh windows|haiku DEST_DIR
set -euo pipefail
platform=${1:?usage: examples.sh windows|haiku DEST_DIR}
dest=${2:?usage: examples.sh windows|haiku DEST_DIR}
samples=$(cd "$(dirname "$0")/../samples" && pwd)

# source name|name on Windows 98 (no characters outside ASCII, whatever the
# code page of the machine)
manifest='Hino de Pernambuco.md|Hino de Pernambuco.md
Poema em linha reta.md|Poema em linha reta.md
Luar do Sertão.md|Luar do Sertao.md
About the examples.md|About the examples.md'

mkdir -p "$dest"
while IFS='|' read -r source windows; do
  case $platform in
    windows) iconv -f UTF-8 -t CP1252 "$samples/$source" | LC_ALL=C sed $'s/$/\r/' > "$dest/$windows" ;;
    haiku) cp "$samples/$source" "$dest/$source" ;;
    *) echo "unknown platform: $platform" >&2; exit 2 ;;
  esac
done <<< "$manifest"
