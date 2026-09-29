# Minuta on Haiku

Minuta has a native Haiku version, written against the Haiku API. It shares
its Markdown code with the Windows 98 build, and it works the same way: one
centered column of monospace text, Markdown syntax that stays in the text
but is drawn in a lighter gray, and not much else on screen. You also get
focus mode, typewriter scrolling, a night theme, full screen, a preview,
HTML export, word count and find.

![Minuta on Haiku, editing the Hino de Pernambuco](shots/haiku/hino-editor.png)

| | |
|---|---|
| ![Preview](shots/haiku/hino-preview.png) | ![Focus mode](shots/haiku/focus-sentence.png) |
| ![Night theme](shots/haiku/night.png) | ![Full screen](shots/haiku/fullscreen.png) |

It runs on Haiku R1/beta6 and later, 64-bit (x86_64).

## Installing

Download `minuta-0.1.0-1-x86_64.hpkg` from the
[releases page](https://github.com/breakzplatform/minuta/releases), then do
one of these:

- Double-click the file. HaikuDepot opens it; click **Install minuta**.
- In Terminal: `pkgman install minuta-0.1.0-1-x86_64.hpkg`.
- Copy it into `~/config/packages` to install it for your user only. Haiku
  picks it up on its own within a few seconds.

Minuta then shows up in Deskbar under Applications, and you can also start
it from Terminal with `minuta` or `minuta notes.md`. The first time it runs,
it offers itself as the app for Markdown files (only if no other app has
that job) and registers its icons.

To remove it, run `pkgman uninstall minuta`, or delete the file from
`~/config/packages` if you installed it there.

## Building from source

You need a Haiku with the development tools. The installer includes them by
default (gcc, make and the system headers). From the top of the repository:

    make -C src/haiku            # builds src/haiku/objects/Minuta
    make -C src/haiku install    # copies it to ~/config/non-packaged/apps
                                 # and adds a `minuta` command
    make -C src/haiku package    # builds objects/minuta-0.1.0-1-x86_64.hpkg

The build uses plain `g++` and `gcc` with `-Wall -Wextra` and should finish
without warnings.

If you work on a Mac, `tools/haiku-vm/` has scripts that run Haiku in QEMU,
copy the sources in over SSH and build there. Its README explains how the VM
was set up.

## Keyboard

Haiku apps use the Command key for shortcuts, which is Alt unless you
changed it in the Keymap preferences. Wherever the Windows version uses
Ctrl, this one uses Alt.

| Keys | What it does |
|---|---|
| Alt+N, Alt+O | New window, open a file |
| Alt+S, Alt+Shift+S | Save, save as |
| Alt+W, Alt+Q | Close the window, quit |
| Alt+Z | Undo |
| Alt+F | Find |
| Alt+G, Alt+Shift+G | Find next, find previous |
| Alt+R | Preview. Esc or Alt+R takes you back to the same spot |
| Alt+Shift+F | Focus mode: everything but the current sentence fades |
| Alt+Shift+P | Focus on the whole paragraph instead of the sentence |
| Alt+Shift+T | Typewriter scrolling: the line you're on stays in the middle |
| Alt+Shift+N | Night theme |
| Alt+Enter or F11 | Full screen. Esc also leaves it |
| Alt+Shift+W | Word count, characters and reading time |
| Alt+E | Export to HTML |
| Alt+Shift+E | Export to HTML and open it in the browser |
| Alt+= and Alt+- | Bigger and smaller text |

The column width (50 to 80 characters) is under View > Column width. Your
choices, the window position and the last file you had open are saved in
`~/config/settings/Minuta`.

## Files

Minuta reads UTF-8 files, with or without a byte order mark. A file that
isn't valid UTF-8 is read as Windows-1252. When you save, the file keeps
its encoding, its byte order mark and its line endings, so a file you
open and save without changes comes out byte for byte the same. New files
are UTF-8 with Unix line endings.

You can open files by double-clicking them in Tracker, by dropping them on
a Minuta window, from Terminal, or with File > Open.

## Known limitations

- Haiku usually labels `.md` files as plain text, so a Markdown file you
  download will open in StyledEdit when you double-click it. Open it once
  in Minuta (drop it on the window, or use Tracker's "Open with") and save
  it: Minuta marks it as Markdown, and from then on it opens in Minuta.
  You can also set the type in Tracker with File > File type.
- Inline code and code blocks get their own color instead of a shaded
  background. The text view Haiku provides can't shade parts of the text.
  For the same reason there is no extra space between lines.
- There's no autosave and no find and replace yet.
- Undo doesn't clear the "modified" dot in the title, even when you undo
  back to the saved text.
- In very long documents the gray syntax markers are updated as parts of
  the file come into view, so you may see plain text for a moment while
  scrolling fast.
- Only the 64-bit build has been tested, on Haiku R1/beta6 in QEMU.
