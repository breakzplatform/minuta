@echo off
rem Minuta md2html acceptance on DOS (everything on the transfer floppy in A:).
A:\MD2HTMLD.EXE --gfm A:\TESTE850.MD -o A:\T850.HTM
A:\MD2HTMLD.EXE --gfm --codepage 1252 A:\TESTE.MD -o A:\T1252.HTM
A:\MD2HTMLD.EXE --gfm --out-codepage 850 A:\TESTE850.MD
echo done> A:\DONE.TXT
