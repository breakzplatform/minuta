#!/bin/bash
# Acceptance 6 (Haiku), load time: launch Minuta with a file inside the
# guest and poll its window title with `hey` until the document is loaded.
# Prints the seconds from launch to the loaded title.
# usage: tests/haiku/open-time.sh GUEST_FILE
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
file=${1:?usage: open-time.sh GUEST_FILE}
name=$(basename "$file")
"$repo/tools/haiku-vm/ssh" "cd ~/minuta && start=\$(date +%s%N)
(src/haiku/objects/Minuta '$file' >/dev/null 2>&1 &)
until hey -o Minuta get Title of Window 0 2>/dev/null | grep -q '$name'; do
  (( (\$(date +%s%N) - start) / 1000000000 < 120 )) || { echo 'timed out'; exit 1; }
done
end=\$(date +%s%N)
echo \"$name loaded in \$(( (end - start) / 1000000 )) ms\""
