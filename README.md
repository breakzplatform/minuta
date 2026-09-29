# Minuta

Minuta is a small Markdown editor for Windows 98. It shows one column of
monospaced text, keeps the Markdown syntax on screen in a lighter gray, and
has a focus mode, a night theme and a full screen mode for writing without
distractions. It comes with `md2html`, a converter for the Win32 console and
for DOS.

![Minuta editing a poem on Windows 98](docs/shots/editor.png)

| | |
|---|---|
| ![Focus mode fades everything but the current sentence](docs/shots/focus.png) | ![Night theme](docs/shots/night.png) |
| ![Preview of the rendered document](docs/shots/preview.png) | ![Full screen](docs/shots/fullscreen.png) |

![The Minuta icon in Explorer](docs/shots/explorer.png)

The texts in the screenshots are in `samples/`: "Poema em linha reta" by
Álvaro de Campos and the anthem of Pernambuco, both in the public domain.

## Download

Get the ZIP files from the
[releases page](https://github.com/breakzplatform/minuta/releases):

- `minuta-0.1.0-win98.zip` has `MINUTA.EXE`, `MD2HTMLW.EXE`, `README.TXT`
  and `LICENSE.TXT`. Unzip it into any folder and run `MINUTA.EXE`. There is
  nothing to install. Settings are saved in `MINUTA.INI` in the same folder.
- `minuta-0.1.0-dos.zip` has `MD2HTMLD.EXE`, which runs on plain DOS (the
  DOS/32A extender is built in).

Minuta runs on Windows 98 SE and should also run on Windows 98 and ME, which
I have not tested. It needs `RICHED20.DLL`, which Windows 98 SE includes.

## Using Minuta

Open a file from the command line or with File > Open. Minuta reopens the
last file when you start it without one.

| Keys | Action |
|---|---|
| Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Shift+S | New, open, save, save as |
| Ctrl+E | Export HTML, and open it in the browser if you like |
| Ctrl+R | Preview the rendered document; Esc or Ctrl+R goes back |
| Ctrl+Shift+F | Focus mode: fade everything but the current sentence |
| Ctrl+Shift+P | Focus on the whole paragraph instead |
| Ctrl+Shift+T | Typewriter scrolling: keep the caret line in the middle |
| Ctrl+Shift+N | Night theme |
| F11 | Full screen, without window frame, menu or taskbar; F11 or Esc to leave |
| Ctrl+Shift+W | Word count, characters and reading time |
| Ctrl+F, F3, Ctrl+H | Find, find next, replace |
| Ctrl+Z, Ctrl+Y | Undo and redo, a word at a time |
| Ctrl+= and Ctrl+- | Larger or smaller text |
| Ctrl+] and Ctrl+[ | Wider or narrower column, from 50 to 80 characters |

Files that start with a UTF-8 byte order mark, or that are valid UTF-8, open
as UTF-8. Anything else opens as Windows-1252. Minuta saves a file in the
encoding it was read in and keeps its line endings. New files are UTF-8 with
CRLF line endings. Save As lets you pick UTF-8 or Windows-1252.

The editor works in Windows-1252, so characters outside it, such as Greek,
Japanese or emoji, appear as `?`. Lines you don't edit are written back
exactly as they were, so those characters survive a save.

While a document has unsaved changes, Minuta writes a copy to `name.md~`
every 30 seconds and deletes it when you save or close.

`MINUTA.INI` keeps the font (`Font`, `FontSize`), the column width
(`ColumnWidth`), the modes (`Night`, `Focus`, `FocusParagraph`,
`Typewriter`), the last file and the window position. `Trace=1` writes a
diagnostic log to `TRACE.TXT`.

### md2html

    MD2HTMLD [--codepage utf8|1252|850] [--out-codepage utf8|1252|850]
             [--full-html] [--gfm] [-o out.html] in.md

The DOS build reads code page 850 by default and the Win32 build reads 1252.
A file with a UTF-8 byte order mark is always read as UTF-8. The output is
UTF-8 unless you ask for another code page with `--out-codepage`; characters
that code page can't hold become `&#NNNN;` references. `--full-html` writes a
complete page with a `<meta http-equiv>` charset, which Internet Explorer 4
and 5 understand. The other options are md4c's; see `--help`.

## Building

You need [Open Watcom 2.0](https://github.com/open-watcom/open-watcom-v2),
GNU make, and clang for the tests. The Makefile looks for Open Watcom in
`~/vms/retro/toolchain/ow2`; set `WATCOM` to use another location.

    make win    # build/bin/MINUTA.EXE and build/bin/MD2HTMLW.EXE
    make dos    # build/bin/MD2HTMLD.EXE
    make test   # unit tests and md2html tests, built natively
    make dist   # the release ZIPs in build/dist

The code in `src/core` is plain C and is tested on the host. `src/win` is the
Windows program and `src/cli` is md2html. `tests/vm` has the scripts that
drive the Windows 98, FreeDOS and MS-DOS 6.22 virtual machines used to test
the program and take these screenshots. The Markdown parser is
[md4c](https://github.com/mity/md4c) 0.6.0, in `vendor/md4c`.

## License

GPL-3.0-only with additional terms, see [LICENSE](LICENSE) and [NOTICE](NOTICE). md4c is also under the MIT license, in
`vendor/md4c/LICENSE.md`.
