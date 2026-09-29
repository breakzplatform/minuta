# Minuta for Haiku — spec

Native Haiku (BeAPI, C++) port of Minuta: the same iA Writer–style editor as the
Windows 98 build (see `SPEC.md`), sharing the portable core. Minuta's direction is
"one native editor per retro platform"; Haiku is the second target.

Work happens on the `haiku` branch (worktree `~/git/100-joselito/minuta-haiku`)
while the Win98 run continues on `main`. Merge back when both are green.

## Environment (set up first)

- VM: Haiku R1/beta5 x86_64 (latest stable; nightly only if beta5 blocks) under
  QEMU on this Mac, in `~/vms/haiku/` with scripts mirroring `~/vms/retro/bin`
  (start/stop/shot/settle/type/key, snapshots `clean` and `ready`). TCG on Apple
  Silicon is slow: give it 2 GB RAM, 2 vCPUs, virtio disk if supported, VNC on
  127.0.0.1:5904, user-mode network with SSH forwarded to localhost:2222.
- Build and test **inside the VM** over SSH (`pkgman install` gcc/cmake/md4c
  devel as needed; `haiku_devel` is preinstalled). Sync sources with rsync/scp.
- Screenshots via the QEMU monitor (`screendump`), same as the retro VMs.
- Only this VM plus at most the Win98 VM running at once; the Mac has 8 GB.

## Scope (MVP)

Same editor behavior as `SPEC.md` "Editor behavior", adapted:

1. `BTextView` editor in a `BWindow`, text column centered with a max width of
   64 characters (insets recomputed in `FrameResized`), no toolbar.
2. Font: the system fixed font (`be_fixed_font`, Noto Sans Mono) 12pt.
3. Syntax dimming through `text_run_array`, restyling only edited paragraphs and
   the visible range. Reuse `src/core/scanner.*` (must stay free of Win32; if it
   isn't, refactor it on this branch so both ports use the same code).
4. Focus mode, typewriter scrolling, night theme — same shortcuts (Alt instead
   of Ctrl, as Haiku convention).
5. Preview: second read-only `BTextView` filled from md4c callbacks (headings,
   emphasis, code, quotes, lists, links). No WebKit.
6. Files: open/save via `BFilePanel`, UTF-8 native (Haiku is UTF-8 everywhere),
   MIME type `text/markdown`, drag and drop from Tracker, `B_REFS_RECEIVED`.
7. Export HTML (md4c), word count, find.
8. Settings in `~/config/settings/Minuta`.

## Acceptance (screenshots from the VM, committed under `docs/shots/haiku/`)

1. App launches from Terminal and from Tracker (double-click a `.md`).
2. Dimmed markers, bold heading, italic span visible.
3. Focus mode, typewriter, night theme.
4. Preview renders the CommonMark smoke sample.
5. Save/reopen round trip byte-identical for UTF-8 with accents.
6. 1 MB file opens and scrolls without stalls.
7. Core unit tests still green on macOS (`make test`), and build with no
   warnings at `-Wall` inside Haiku.

## Delivery

- `src/haiku/` + a `Makefile` target or `haiku/Makefile` using Haiku's makefile-engine or plain g++.
- README section "Haiku" (English).
- Optional, if time allows: a HaikuPorts-style recipe draft in `haiku/recipe/`.
