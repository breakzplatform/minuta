# Status: Haiku port

Progress of `SPEC-HAIKU.md` on the `haiku` branch. Evidence: screenshots in
`docs/shots/haiku/` and the test commands listed per item. All numbers are
from the QEMU VM below (TCG emulation on an Apple Silicon Mac, so a real
x86 machine will be faster).

## Acceptance

| # | Item | Result | Evidence |
|---|---|---|---|
| 1 | Launches from Terminal and from Tracker | pass | `launch-terminal.png` (`minuta samples/teste-utf8.md` in Terminal), `launch-tracker.png` (double-click in Tracker), `drop-from-tracker.png` (file dropped on a Minuta window opens in a new one) |
| 2 | Dimmed markers, bold heading, italic span | pass | `dimming.png`, `smoke-editor.png` |
| 3 | Focus mode, typewriter, night theme | pass | `focus-sentence.png`, `focus-paragraph.png`, `typewriter.png` (the selected caret line sits at the vertical center, y≈421 of 72–772), `night.png` |
| 4 | Preview renders the CommonMark smoke sample | pass | `smoke-preview.png`, `smoke-preview-end.png` (headings, emphasis, inline code, links, nested and ordered lists, tasks, code block, quote, rule, table, image placeholder, strikeout), `smoke-back-to-editor.png` (Esc returns to the same place) |
| 5 | Save/reopen byte-identical for UTF-8 with accents | pass | `tests/haiku/roundtrip.sh`, below |
| 6 | 1 MB file opens and scrolls without stalls | pass | `tests/haiku/open-visible.sh`, `scroll.sh`, `typing.sh`, below; `big-file-scrolled.png` |
| 7 | `make test` green on macOS; no warnings at `-Wall` in Haiku | pass | below |

Also shown: `counters.png` (Alt+Shift+W overlay), `find.png`,
`export-webpositive.png` (Alt+Shift+E: exported HTML opened in WebPositive
with the accents right).

### 5: save round trip

`tests/haiku/roundtrip.sh` opens each file, types "x" at the start and saves
with Alt+S (the saved file must then hold the extra "x", so a save that
silently failed can't pass), deletes it, saves again, closes, and compares
the bytes on the Mac:

    ok   lf: edit saved, then bytes identical (354 bytes)
    ok   crlf-bom: edit saved, then bytes identical (377 bytes)
    ok   cp1252: edit saved, then bytes identical (345 bytes)
    ok   unicode: edit saved, then bytes identical (161 bytes)
    round trip passed

(`lf` is `samples/teste-utf8.md`; `crlf-bom` the same text with a BOM and
CRLF; `cp1252` is `samples/TESTE.MD`; `unicode` is `samples/UNICODE.MD`,
UTF-8 with characters outside Latin-1.)

### 6: 1 MB file

`/tmp/big.md` is `samples/prose.md` repeated to 1,049,828 bytes (about 900
headings and 900 fenced code blocks).

- Load: the screen settles 1.5–2.0 s after launch (`open-visible.sh`;
  0.7 s for the 1 KB sample). `BTextView::SetText` itself takes most of it;
  measured once with a temporary timer: 2.25 s before the fix below, when
  the text was laid out at a tiny width and then again at the real one.
- Scrolling: 40 Page Downs sent in one burst settle 1.4–1.6 s after the
  first key (`scroll.sh`), restyled as they come into view; Alt+End jumps to
  the end with the fenced blocks there styled correctly.
- Typing: 57 keys sent in one burst are all in the editor 1.9 s after the
  first key, the same at the start and at the end of the 1 MB file
  (`typing.sh`, caret polled with `hey`), which is QEMU's own key pacing.

### 7: tests and warnings

- macOS: `make test` → `51 tests, 0 failed checks` and `md2html tests
  passed` (clang `-Wall -Wextra -Wpedantic -Werror`).
- Haiku: `make -C src/haiku clean all` → 0 lines with "warning" or "error"
  in the log; our C and C++ build with `-Wall -Wextra` (gcc 13.3). Vendored
  md4c is built with `-w`, as in the main Makefile.

## Milestones

1. VM, scripts, SSH build loop: done (`tools/haiku-vm/`, see its README).
2. Window, BTextView, open/save: done.
3. Dimming, column, themes, focus, typewriter: done.
4. Preview, export, counters, find: done.
5. Acceptance screenshots, README, status: done.

## Environment

- Haiku R1/beta6 x86_64 (the current stable release; the spec named beta5
  as "latest stable") under QEMU TCG, 2 vCPUs, 2 GB, in `~/vms/haiku`.
  Snapshots `clean` (fresh install + rsync) and `ready` (idle desktop).
- `start --loadvm ready` answers SSH in about a second, a cold boot in about
  20 s; `tools/haiku-vm/build` rsyncs the repo and runs `make -C src/haiku`
  in the guest (a full build takes about a minute).
- gcc 13.3, binutils, make and the headers come with the Installer's
  default optional packages; `rsync` was added with `pkgman`.

## Decisions

- AHCI disk instead of virtio: beta6 lists the virtio-blk device but
  publishes no disk node for it.
- Guest keymap "US" (US-International dead keys break `sendkey` typing);
  the guest's screen saver is off (it blanked the screen in screenshots).
- Font: `be_fixed_font` (Noto Sans Mono) at 12 pt; italics use the
  family's italic style when there is one, a sheared regular otherwise.
- Code spans and blocks get a color of their own: `text_run` has no
  background color, so the faint background of the Windows build isn't
  possible in `BTextView` (short of drawing it ourselves under the text).
- No extra line spacing: `BTextView` has no setting for it.
- Styling is incremental: an edit restyles the paragraphs it touched,
  scrolling restyles the visible ones (runs identical to the current ones
  are skipped, so scrolling over styled text doesn't redraw). Lines that can
  open or close a fenced block are tracked in a sorted list updated on each
  edit, so the scanner state at any paragraph is replayed from those lines
  only. Paragraphs out of sight can keep stale styles until they scroll in.
- Windows are shown before they load a file and the insets are set in
  `DoLayout`, so a document is laid out once, at the real width.
- New files: UTF-8, no BOM, LF (the Haiku convention); existing files keep
  their encoding, BOM and dominant line ending (as in the Windows build).
- Haiku's sniffer types most `.md` files as `text/plain` (content rules
  win over the extension), so they'd open in StyledEdit. Saving a `.md` or
  `.markdown` file that is untyped or `text/plain` sets `text/markdown`;
  Minuta registers as preferred app for `text/markdown` only when none is
  set. The Tracker test file got its type that way.
- Single-launch app: a second `minuta file.md` opens the file in the
  running instance (argv with its working directory).
- The find panel closes after a search (like StyledEdit's) because
  `BTextView` hides the selection while its window is inactive; Alt+G and
  Alt+Shift+G repeat it.
- Alt+= and Alt+Shift+= also make the text bigger: the "+" menu shortcut
  alone never matched, since "+" needs Shift on US keyboards.
- Word and character counts count code points (`stats_utf8.c`), not bytes.
- The preview uses the plain system font for text and the fixed font for
  code and tables, headings at 1.6/1.35/1.15× size.

## Not done

- Autosave (`name.md~`, `SPEC.md` item 12) and replace: not in the Haiku
  MVP list.
- "Undo" doesn't clear the modified marker when the text is back to what
  was saved.
- A HaikuPorts recipe (optional in the spec).

## Core changes on this branch (for the merge into `main`)

All additive except one line; `make test` stays green on macOS and the new
files compile without warnings under Open Watcom `-Wall -Werror`.

- New: `src/core/document_utf8.[ch]` (UTF-8 codec reusing
  `struct document_format`), `src/core/stats_utf8.[ch]`,
  `src/core/styled.[ch]` (md4c → text + style runs + anchors, reusing
  `struct rtf_anchors` and its lookups), `tests/unit/test_document_utf8.c`,
  `tests/unit/test_stats_utf8.c`, `tests/unit/test_styled.c`,
  `tests/unit/test_list_utf8.h`.
- `tests/unit/test_list.h`: one `#include "test_list_utf8.h"` at the end.
- `src/core/rtf.c`: `(unsigned)byte & 0xFFu` in the `\'hh` escape, to
  silence gcc 13 `-Wformat-overflow` (output unchanged).
- `samples/SMOKE.MD`: copied unchanged from `main`.
- `.gitignore`: `src/haiku/objects/`.
- `README.md`: a "Haiku" section appended at the end (`main` rewrote the
  README since this branch started; the section is self-contained).
- The top-level `Makefile` builds every `src/core/*.c` into `MINUTA.EXE`,
  so the new core files are linked there too (unused by the Win32 build)
  unless the Win build lists its core sources explicitly.
- `styled.c` duplicates `rtf.c`'s small entity decoder; a shared helper
  would remove it once both branches are merged.

Unrelated to this branch: on this Mac `make win` stops at
`vendor/md4c/md2html/cmdline.c` with "Invalid option '-wcd==107'" from the
`-Wno-n=107` flag; the same Makefile line is on `main`.
