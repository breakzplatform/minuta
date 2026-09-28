# Minuta

Minimal Markdown editor for Windows 98 SE, plus `md2html` for Win32 and DOS.
See `SPEC.md` for the design and `docs/STATUS.md` for progress.

## Build

Requires Open Watcom 2.0 (defaults to `~/vms/retro/toolchain/ow2`; override
with `WATCOM=...`) and clang for the native tests.

    make win    # build/bin/MINUTA.EXE, build/bin/MD2HTMLW.EXE
    make dos    # build/bin/MD2HTMLD.EXE
    make test   # native unit tests and md2html regression tests
