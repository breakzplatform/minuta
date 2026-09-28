# Minuta — spec

Minimal Markdown editor for Windows 98 SE, in the spirit of iA Writer: one
centered column of monospace text, Markdown syntax visible but dimmed, focus
mode, no chrome. Ships with `md2html` (Win32 console + DOS) as a companion CLI.

Out of scope: Win16 / Windows 3.11 (the DOS `md2html` covers 3.11), Windows 95
without IE 4+, tables editing UI, plugins, spell check, multiple documents per
window, cloud/sync.

## Targets

| Binary | Target | Toolchain |
|---|---|---|
| `MINUTA.EXE` | Win32 GUI, Windows 98 SE (must also run on 98 FE and ME) | Open Watcom 2.0, `owcc -bnt_win` |
| `MD2HTMLW.EXE` | Win32 console | same, `-bnt` |
| `MD2HTMLD.EXE` | DOS/32A, self-contained | same, `-bdos32a` |

Constraints: C99 subset accepted by Open Watcom, imports limited to
`KERNEL32`, `USER32`, `GDI32`, `COMDLG32`, `COMCTL32` (v4.72, the one in 98 SE)
and `RICHED20.DLL` loaded with `LoadLibrary`. No MSVCRT dependency, no C++ runtime.
Binary size goal: under 400 KB.

## Architecture

- `vendor/md4c/` — md4c with `toolchain/md4c-watcom.patch` applied (copied from
  `~/vms/retro/toolchain`). Untouched otherwise.
- `src/core/` — portable C, no Win32: document buffer helpers, Markdown span
  scanner for syntax dimming, md4c → RTF renderer, encoding conversion
  (UTF-8 ⇄ cp1252 with a replacement table), word/char counters.
  Builds natively on macOS too, so it is unit-tested on the Mac.
- `src/win/` — Win32 shell: window, RichEdit editor, preview pane, menus,
  accelerators, file dialogs, settings (INI next to the exe).
- `src/cli/` — md2html front end (existing `md2html.c` from md4c plus a
  `--codepage` flag).
- `tests/` — core unit tests (native, `make test`) and VM acceptance scripts
  that drive `~/vms/retro/bin/*`.

## Editor behavior

1. **Layout:** single RichEdit 2.0 control, text column centered with a fixed
   max width of 64 characters (configurable 50–80), generous line height,
   no toolbar, no status bar by default. Title bar shows `name — Minuta`,
   with `•` when modified.
2. **Font:** Courier New 12pt (decided); size configurable.
   Colors: near-black text on off-white (`#1E1E1E` on `#F7F7F2`) and a night
   theme (`#D6D6D6` on `#1B1B1B`), toggled with `Ctrl+Shift+N`.
3. **Syntax dimming (iA style):** Markdown markers stay in the text but are
   drawn in a light gray: `#`, `*`, `_`, `` ` ``, `>`, list bullets, link
   brackets and URLs. Headings in bold, emphasis in italic/bold, inline code
   and fenced blocks in the same font with a faint background.
   Restyling runs only on the paragraphs touched by an edit (and the visible
   range on scroll), never on the whole document, so typing stays instant on a
   Pentium II–class machine.
4. **Focus mode (`Ctrl+Shift+F`):** everything outside the current sentence
   (or paragraph; toggle with `Ctrl+Shift+P`) is dimmed.
5. **Typewriter scrolling (`Ctrl+Shift+T`):** the caret line stays vertically
   centered.
6. **Preview (`Ctrl+R`):** toggles the editor with a read-only RichEdit that
   shows the document rendered through md4c → RTF (headings, emphasis, code,
   quotes, lists, links underlined, horizontal rules; images shown as
   `[image: alt]`; tables as monospace text). `Esc` returns to the editor at
   the same position.
7. **Export HTML (`Ctrl+E`):** md4c HTML with a small embedded stylesheet and
   `<meta charset>` matching the chosen output encoding; optional "open in
   browser" via `ShellExecute`.
8. **Files:** New/Open/Save/Save As through the common dialogs; `.md`, `.markdown`,
   `.txt`. Encoding detection: UTF-8 BOM or valid UTF-8 → UTF-8; otherwise
   cp1252. Save keeps the file's encoding; new files default to UTF-8
   (decided: files usually travel to other machines). Line endings kept as found; new files CRLF.
   Characters that don't fit cp1252 are shown as `?` in the editor and
   **preserved** on save (the buffer keeps the original bytes of untouched runs).
9. **Counters:** `Ctrl+Shift+W` shows a transient overlay with words,
   characters and reading time.
10. **Search:** `Ctrl+F` / `F3` via the common Find dialog. Replace is optional.
11. **Settings:** `MINUTA.INI` beside the exe: font, size, column width, theme,
    focus mode, typewriter, last file, window placement.
12. **Crash safety:** autosave to `name.md~` every 30 s while modified; removed
    on clean save/close.

## md2html CLI

`MD2HTML [--codepage utf8|1252|850] [--full-html] [-o out.html] in.md`.
Default input codepage: DOS build 850, Win32 build 1252, both auto-accept
UTF-8 with BOM. Output is always UTF-8 with `<meta charset="utf-8">` unless
`--out-codepage` is given (IE 5 handles UTF-8).

## Acceptance (all must pass, with screenshots)

Run on the VMs in `~/vms/retro` using `--loadvm ready`:

1. `MINUTA.EXE` starts on win98 in under 2 s, opens `samples/TESTE.MD`
   (cp1252) and a UTF-8 sample; accents correct in both.
2. Typing 2,000 words (via `bin/type`) keeps up without dropped keys;
   screenshot shows dimmed markers, bold heading, italic span.
3. Focus mode, typewriter mode, night theme: one screenshot each.
4. Preview of the CommonMark smoke sample renders headings, lists, code block,
   quote and link; screenshot.
5. Save as UTF-8 and cp1252 round trip: pull the files back to the Mac and
   compare bytes with the expected output.
6. Export HTML opens in IE 5 with correct accents; screenshot.
7. A 1 MB Markdown file opens in under 5 s and scrolling doesn't stall.
8. md2html: same tests as today on win98, freedos and dos622.
9. `make test` (core unit tests) green on macOS; no compiler warnings at
   `-w4` in `src/`.

## Delivery

- Private GitHub repo `breakzplatform/minuta`, `main` branch; one commit per
  milestone, conventional commits.
- README (English) with build, test and usage; screenshots in `docs/`.
- Barsa note `20-professional/minuta.md` (product) once v0.1 passes.

## Milestones

1. Repo, vendored md4c, build scripts (`make win`, `make dos`, `make test`),
   md2html with codepage flags. Acceptance 8.
2. Core: encoding, span scanner, RTF renderer + unit tests. Acceptance 9.
3. Window + RichEdit + file I/O + settings. Acceptance 1, 5.
4. Syntax dimming, themes, focus, typewriter. Acceptance 2, 3, 7.
5. Preview + export + counters + search. Acceptance 4, 6.
6. Polish, README, screenshots, tag `v0.1.0`.

## Overnight run

Separate tmux session `minuta` (not touched by the 05:00 clear), one
orchestrator following these milestones, each milestone gated by its
acceptance items with screenshots saved in `docs/shots/`. On finish or on a
hard blocker: Winston notification with the result and the blocker, if any.

## Decisions (2026-09-28)

- Font: Courier New.
- New files: UTF-8.
- Hosting: private GitHub repo.
