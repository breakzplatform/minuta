# Haiku VM

Scripts for the Haiku R1/beta6 x86_64 VM used to build and test the Haiku
port. The scripts live here; the disk, ISO, SSH key, sockets and screenshots
live in `~/vms/haiku` (override with `HAIKU_VM_ROOT`). Only one Haiku VM runs
at a time (the build Mac has 8 GB).

| Setting | Value |
|---|---|
| QEMU | `qemu-system-x86_64`, TCG (multi-threaded), `-cpu max`, 2 vCPUs, 2 GB |
| Disk | `disks/haiku.qcow2`, 16 GB, AHCI (beta6 detects virtio-blk but publishes no disk for it) |
| Display | `-vga std`, 1280x800, VNC on `127.0.0.1:5904` |
| Input | USB tablet, so `click` can use absolute coordinates |
| Network | user mode, virtio-net, guest SSH on `127.0.0.1:2222`, host is `10.0.2.2` |
| Snapshots | `clean` (disk only, fresh install + rsync) and `ready` (RAM + disk, idle desktop) |

## Commands

| Command | What it does |
|---|---|
| `start [--loadvm TAG] [--cd ISO] [--boot c\|d] [--volatile]` | Start headless. `--loadvm ready` resumes the desktop in about a second. |
| `stop [--guest]` | Power off; `--guest` shuts Haiku down over SSH first. |
| `wait-ssh [TIMEOUT]` | Wait until SSH answers. |
| `ssh [cmd...]` | Run a command in the guest as `user`. |
| `push [REPO] [DEST]` | Sync the clock and rsync the repo to `~/minuta` in the guest. |
| `pull GUEST_PATH... HOST_DIR` | Copy files back with scp. |
| `push-file FILE... GUEST_DIR` | Copy single files into the guest. |
| `launch COMMAND...` | Start a GUI program in `~/minuta`, detached from SSH (log in `/tmp/launch.log`). |
| `build [make args]` | `push`, then `make -C src/haiku` in the guest. |
| `shot [--after S] [out.png]` | Screenshot (QEMU `screendump`). |
| `settle [TIMEOUT] [QUIET] [out.png]` | Wait for the screen to stop changing, then screenshot. |
| `type TEXT` / `typeln TEXT` / `key KEY...` | Keyboard through `sendkey` (US layout). |
| `click X Y [left\|right\|middle] [--double] [--move]` | Pointer through QMP `input-send-event`. |
| `mon CMD...` | Raw HMP monitor commands. |
| `snapshot TAG` / `reset [TAG]` | Save a snapshot (full state if running, disk only if stopped) / revert the disk. |
| `bootstrap [ROOT]` | Serve `guest-bootstrap.sh` and the public key over HTTP and type the command that installs `authorized_keys`. |

## Typical loop

    tools/haiku-vm/start --loadvm ready
    tools/haiku-vm/build
    tools/haiku-vm/launch src/haiku/objects/Minuta samples/teste-utf8.md
    tools/haiku-vm/settle 30 3
    tools/haiku-vm/stop

## How the disk was made

1. `qemu-img create -f qcow2 disks/haiku.qcow2 16G`, then
   `start --cd iso/haiku-r1beta6-x86_64-anyboot.iso --boot d`
   (ISO SHA-256 `bb7743474fd175aea511be1799764ba054465285bbdf54c4722ca61d1fb4f5c9`).
2. In the welcome window: keymap "US" (the default US-International has dead
   keys that break `type`), then "Try Haiku".
3. In Terminal: `mkfs -t bfs /dev/disk/scsi/2/0/0/raw HaikuHD` and
   `mountvolume HaikuHD`. Do not name the volume "Haiku": the live CD's boot
   volume has that name, and formatting a second one broke the live session.
4. `Installer`, from "Haiku" onto "HaikuHD", optional packages as offered
   (they include gcc, binutils, make and the headers).
5. `bootstrap /HaikuHD` to install the SSH key, then shut down and boot from
   the disk.
6. The Installer skips the SSH host keys, so in a Terminal on the installed
   system: `ssh-keygen -A`, reboot.
7. `ssh 'pkgman install -y rsync'`, `stop --guest`, `snapshot clean`.
8. Boot, disable the screen saver (it blanks the screen after 10 minutes
   and spoils screenshots): write `~/config/settings/ScreenSaver_settings`
   as a flattened BMessage with `timeflags = 0` and `lockenable = false`,
   reboot, wait for the desktop, `snapshot ready`.
