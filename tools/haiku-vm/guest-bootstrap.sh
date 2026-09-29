#!/bin/sh
# Run inside Haiku (typed into Terminal once) to allow SSH logins from the
# Mac. Served over HTTP by the "bootstrap" script; the guest reaches the host
# at 10.0.2.2 with QEMU user networking.
#   curl -s http://10.0.2.2:8765/guest-bootstrap.sh | sh -s [TARGET_ROOT]
# TARGET_ROOT is the mount point of an installed system (e.g. /HaikuHD);
# default is the running system.
set -e
root=${1:-}
dir="$root/boot/home/config/settings/ssh"
[ -n "$root" ] && dir="$root/home/config/settings/ssh"
mkdir -p "$dir"
curl -sf http://10.0.2.2:8765/id_ed25519.pub > "$dir/authorized_keys"
chmod 700 "$dir"
chmod 600 "$dir/authorized_keys"
echo "authorized_keys installed in $dir"
