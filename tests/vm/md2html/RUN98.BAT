@echo off
rem Minuta md2html acceptance on Windows 98 (CD in D:, transfer floppy in A:).
D:\MD2HTMLW.EXE --gfm D:\TESTE.MD -o A:\T1252.HTM
D:\MD2HTMLW.EXE --gfm --codepage utf8 D:\TESTEU8.MD -o A:\TUTF8.HTM
D:\MD2HTMLW.EXE --gfm --full-html --html-title Minuta D:\TESTE.MD -o C:\TESTE.HTM
copy C:\TESTE.HTM A:\TFULL.HTM > NUL
D:\MD2HTMLW.EXE --gfm --out-codepage 1252 D:\TESTE.MD
echo done> A:\DONE.TXT
