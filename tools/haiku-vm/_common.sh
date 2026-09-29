# shellcheck shell=bash
# Shared settings for the Haiku VM scripts. The scripts live in the repo; the
# disk, ISO, sockets and screenshots live in $HAIKU_VM_ROOT (~/vms/haiku).
ROOT="${HAIKU_VM_ROOT:-$HOME/vms/haiku}"
BIN="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUN="$ROOT/run"
mkdir -p "$RUN" "$ROOT/shots"
VM=haiku
DISK="$ROOT/disks/haiku.qcow2"
MON="$RUN/$VM.mon"
QMP="$RUN/$VM.qmp"
PIDFILE="$RUN/$VM.pid"
VNC_DISPLAY=4            # vnc://127.0.0.1:5904
SSH_PORT=2222
SSH_KEY="$ROOT/ssh/id_ed25519"
SSH_COMMON=(-i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null
            -o LogLevel=ERROR -o ConnectTimeout=10 -o ServerAliveInterval=15)
SSH_OPTS=("${SSH_COMMON[@]}" -p "$SSH_PORT")
SCP_OPTS=("${SSH_COMMON[@]}" -P "$SSH_PORT")
SSH_TARGET=user@127.0.0.1
is_running() { [[ -f $PIDFILE ]] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; }
