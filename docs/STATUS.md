# Status

Progress of the milestones in `SPEC.md`. Evidence lives in `docs/shots/`
(VM screenshots) and in the test commands listed per milestone.

## Milestone 1 — repo, vendored md4c, build, md2html codepages: done

- `vendor/md4c/` is md4c 0.6.0 (`c7ba975`) with `md4c-watcom.patch` applied;
  `src/cli/md2html.c` is md4c's front end plus `--codepage`/`--out-codepage`.
- `make win` / `make dos` build `MD2HTMLW.EXE` (162 KB, imports only
  KERNEL32/USER32) and `MD2HTMLD.EXE` (DOS/32A, 217 KB).
- Acceptance 8: `tests/vm/md2html.sh` runs the samples on win98, freedos and
  dos622, pulls the HTML back through the transfer floppy and compares it
  byte for byte with `tests/cli/expected/teste.html`: all 7 checks pass.
  Screenshots: `md2html-win98-console.png`, `md2html-win98-ie5.png` (IE 5
  showing the UTF-8 full-HTML output with correct accents),
  `md2html-freedos.png`, `md2html-dos622.png`.
- `make test` runs the native md2html regression tests (`tests/cli/run.sh`).

Decisions:
- `--full-html` declares the charset with `<meta http-equiv="Content-Type">`
  instead of `<meta charset>`: IE 4/5 ignore the short form.
- `--name value` is accepted besides md4c's `--name=value`.
- Only a BOM switches input to UTF-8 (as the spec says); a UTF-8 file without
  BOM needs `--codepage utf8`.

## Milestone 2 — core: encoding, span scanner, RTF renderer: done

- `src/core/`: `encoding` (UTF-8/CP1252/CP850), `document` (file decode and
  encode: encoding detection, line endings kept, original bytes of untouched
  lines with characters outside CP1252 preserved), `scanner` (per-line
  Markdown span scanner for syntax dimming, with fenced-code state across
  lines), `rtf` (md4c → RTF for the preview, with source ↔ rendered position
  anchors), `stats` (words, characters, reading time, sentence bounds for
  focus mode), `buffer`.
- Acceptance 9: `make test` → `40 tests, 0 failed checks` plus the md2html
  regression tests, built with clang `-Wall -Wextra -Wpedantic -Werror`. All
  of `src/core` and `src/cli` compile with Open Watcom `-Wall` (level 4)
  `-Werror` without diagnostics.

Decisions:
- The span scanner works one line at a time; emphasis spanning a soft line
  break is not dimmed. Setext headings dim the underline but do not bold the
  text above it; indented code blocks are not styled (they can't be told
  apart from list continuations without the whole document).
- Fenced code is only recognized at the top level (not inside quotes/lists).
- Saving keeps the file's most frequent line ending for every line.
- Preservation of characters outside CP1252 works per line: an unchanged line
  is written back byte for byte; a line that was edited is written from the
  editor text, so its `?` placeholders become literal `?`.
