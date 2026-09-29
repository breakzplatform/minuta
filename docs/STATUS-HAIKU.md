# Status: Haiku port

Progress of `SPEC-HAIKU.md` on the `haiku` branch. Evidence: screenshots in
`docs/shots/haiku/` and the test commands listed per item.

## Milestone 1: VM, scripts, SSH build loop (done)

- Haiku R1/beta6 x86_64 (the current stable release; the spec named beta5
  as "latest stable") under QEMU TCG, 2 vCPUs, 2 GB, in `~/vms/haiku`.
  Scripts in `tools/haiku-vm/` (see its README for the commands and how the
  disk was built). Snapshots `clean` and `ready`.
- `start --loadvm ready` answers SSH in about a second; a cold boot in about
  20 s. `tools/haiku-vm/build` rsyncs the repo and runs
  `make -C src/haiku` in the guest (full build about a minute under TCG).
- gcc 13.3, binutils, make and the BeAPI headers come with the Installer's
  default optional packages; `rsync` was added with `pkgman`.

Decisions:
- AHCI disk instead of virtio: beta6 lists the virtio-blk device but
  publishes no disk node for it.
- Guest keymap "US" (the default US-International dead keys break typing
  through `sendkey`); the screen saver is disabled.

## Milestone 2: window, BTextView, open/save (done)

- `src/haiku/`: `App.cpp` (single-launch BApplication, argv, refs, open
  panel, text/markdown handler registration), `MainWindow` (menus, save
  panel, title `name — Minuta` with a leading `•` when modified),
  `EditorView` (BTextView), `PreviewView`, `FindWindow`, `CountersWindow`,
  `Settings` (flattened BMessage in `~/config/settings/Minuta`), `Theme`.
- Acceptance 5: `tests/haiku/roundtrip.sh` opens LF UTF-8, CRLF UTF-8 with
  BOM, CP1252 and UTF-8 with characters outside Latin-1, types "x" and
  saves (the file must then hold the "x"), deletes it and saves again, and
  compares the bytes on the Mac: all four identical.

## Core changes on this branch (for the merge into `main`)

All additive except one line; `make test` stays green on macOS and the new
files compile without warnings under Open Watcom `-Wall -Werror`.

- New: `src/core/document_utf8.[ch]`, `src/core/stats_utf8.[ch]`,
  `src/core/styled.[ch]`, `tests/unit/test_document_utf8.c`,
  `tests/unit/test_stats_utf8.c`, `tests/unit/test_styled.c`,
  `tests/unit/test_list_utf8.h`.
- `tests/unit/test_list.h`: one `#include "test_list_utf8.h"` at the end.
- `src/core/rtf.c`: `(unsigned)byte & 0xFFu` in the `\'hh` escape, to
  silence gcc 13 `-Wformat-overflow` (output unchanged).
- The top-level `Makefile` builds every `src/core/*.c` into `MINUTA.EXE`,
  so the new files are linked there too (unused, dead code for the
  Win32 build) unless the Win build lists its core sources explicitly.
- `styled.c` duplicates rtf.c's small entity decoder; a shared helper
  would remove it once both branches are merged.

Unrelated to this branch: `make win` stops at
`vendor/md4c/md2html/cmdline.c` with "Invalid option '-wcd==107'" from
the `-Wno-n=107` flag with the installed owcc; the same Makefile is on
`main`.
