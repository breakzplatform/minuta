# shellcheck shell=bash
# Helpers for the win98 acceptance scripts. Source after setting repo.
bin=$HOME/vms/retro/bin
shots=$repo/docs/shots
work=$repo/build/vm
failures=0
mkdir -p "$shots" "$work"

pass() { echo "ok   $*"; }
fail() { echo "FAIL $*"; failures=$((failures + 1)); }

# Waits until the guest has written NAME to the transfer floppy and the
# file size stopped changing.
wait_floppy_file() {  # NAME [TIMEOUT]
  local image=$HOME/vms/retro/run/win98-xfer.img previous="" current probe=$work/probe
  for _ in $(seq "${2:-60}"); do
    current=""
    rm -f "$probe"
    if mcopy -n -o -i "$image" "::/$1" "$probe" 2>/dev/null; then
      current=$(wc -c < "$probe")
    fi
    if [[ -n $current && $current == "$previous" ]]; then return 0; fi
    previous=$current
    sleep 1
  done
  return 1
}

pull() {  # NAME -> $work/pulled/NAME
  mkdir -p "$work/pulled"
  rm -f "$work/pulled/$1"
  mcopy -n -o -i "$HOME/vms/retro/run/win98-xfer.img" "::/$1" "$work/pulled/$1" 2>/dev/null
}

# Waits until the floppy holds NAME with exactly the EXPECTED bytes (Windows
# writes the floppy lazily), then reports; on timeout shows the difference.
expect_floppy_file() {  # LABEL NAME EXPECTED [TIMEOUT]
  local waited=0
  while ((waited < ${4:-60})); do
    if pull "$2" && cmp -s "$work/pulled/$2" "$3"; then pass "$1"; return 0; fi
    sleep 2
    waited=$((waited + 2))
  done
  fail "$1"
  if [[ -f $work/pulled/$2 ]]; then cmp "$work/pulled/$2" "$3" || true; else echo "     $2 not found"; fi
  return 1
}

compare_file() {  # LABEL ACTUAL EXPECTED
  if cmp -s "$2" "$3"; then pass "$1"
  else fail "$1"; cmp "$2" "$3" || true; fi
}

# Opens Start > Run. Not Win+R: since the guest layout became United
# States-International, the Windows key sent by sendkey sometimes stays down,
# and the path typed next fires Win+E, Win+F and so on.
open_run() {
  "$bin/key" win98 ctrl-esc
  sleep 2
  "$bin/key" win98 r
}

# Opens a file through File > Open.
open_file() {  # GUEST_PATH
  "$bin/key" win98 ctrl-o
  sleep 3
  "$bin/type" win98 "$1"
  "$bin/key" win98 ret
  sleep 4
}

# File > Save As to GUEST_PATH; TYPE_KEYS moves the "Save as type" combo.
save_as() {  # GUEST_PATH [TYPE_KEYS...]
  local path=$1
  shift
  "$bin/key" win98 ctrl-shift-s
  sleep 3
  "$bin/type" win98 "$path"
  sleep 1
  if (($#)); then
    # Plain arrows don't move this combo; open the list, pick, close it.
    "$bin/key" win98 alt-t
    sleep 1
    "$bin/key" win98 alt-down
    sleep 1
    for k in "$@"; do "$bin/key" win98 "$k"; sleep 1; done
    "$bin/key" win98 ret
    sleep 1
  fi
  "$bin/key" win98 ret
}
