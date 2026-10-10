Minuta 0.1.2
============

Minuta is a small Markdown editor for Windows 98. It shows one column of
monospaced text, keeps the Markdown syntax on screen in a lighter gray, and
has a focus mode, a night theme and a full screen mode.

Files in this ZIP:

  MINUTA.EXE     the editor
  MD2HTMLW.EXE   Markdown to HTML converter for the Windows console
  README.TXT     this file
  LICENSE.TXT    GNU GPL v3 license

Unzip everything into a folder of your choice and run MINUTA.EXE. There is
nothing to install. Minuta saves its settings in MINUTA.INI in the same
folder. It needs RICHED20.DLL, which comes with Windows 98 SE.

Keys

  Ctrl+O, Ctrl+S, Ctrl+Shift+S   open, save, save as
  Ctrl+E                         export HTML
  Ctrl+R                         preview; Esc goes back
  Ctrl+Shift+F                   focus mode (Ctrl+Shift+P: whole paragraph)
  Ctrl+Shift+T                   typewriter scrolling
  Ctrl+Shift+N                   night theme
  F11                            full screen; F11 or Esc to leave
  Ctrl+Shift+W                   word count and reading time
  Ctrl+F, F3, Ctrl+H             find, find next, replace
  Ctrl+Z, Ctrl+Y                 undo, redo

Minuta opens UTF-8 and Windows-1252 files and saves them in the same
encoding, with the same line endings. Characters that Windows-1252 cannot
show appear as "?", and lines you don't edit are saved exactly as they were.
While a document has unsaved changes, a copy goes to NAME.MD~ every 30
seconds.

MD2HTMLW converts Markdown to HTML:

  MD2HTMLW [--codepage utf8|1252|850] [--full-html] [-o out.html] in.md

Run MD2HTMLW --help for all options. A DOS version is in the separate DOS
ZIP.

Source code: https://github.com/breakzplatform/minuta
Markdown parsing by md4c (MIT license), https://github.com/mity/md4c
