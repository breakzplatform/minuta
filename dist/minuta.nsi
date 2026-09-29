; Minuta installer. ANSI (not Unicode) so it runs on Windows 98.
; Built by `make dist` with makensis; expects the files in build/dist/win98.
Unicode false
SetCompressor /SOLID lzma

!ifndef VERSION
  !define VERSION "0.1.0"
!endif

Name "Minuta ${VERSION}"
OutFile "..\build\dist\minuta-${VERSION}-setup.exe"
InstallDir "$PROGRAMFILES\Minuta"
InstallDirRegKey HKLM "Software\Minuta" "InstallDir"
Icon "..\res\MINUTA.ICO"
UninstallIcon "..\res\MINUTA.ICO"
RequestExecutionLevel admin
XPStyle off

Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "Minuta"
  SetOutPath "$INSTDIR"
  File "..\build\dist\win98\MINUTA.EXE"
  File "..\build\dist\win98\MD2HTMLW.EXE"
  File "..\build\dist\win98\README.TXT"
  File "..\build\dist\win98\LICENSE.TXT"
  WriteRegStr HKLM "Software\Minuta" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\UNINST.EXE"
  CreateDirectory "$SMPROGRAMS\Minuta"
  CreateShortCut "$SMPROGRAMS\Minuta\Minuta.lnk" "$INSTDIR\MINUTA.EXE"
  CreateShortCut "$SMPROGRAMS\Minuta\Read Me.lnk" "$INSTDIR\README.TXT"
  CreateShortCut "$SMPROGRAMS\Minuta\Uninstall Minuta.lnk" "$INSTDIR\UNINST.EXE"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Minuta" "DisplayName" "Minuta ${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Minuta" "UninstallString" '"$INSTDIR\UNINST.EXE"'
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\MINUTA.EXE"
  Delete "$INSTDIR\MD2HTMLW.EXE"
  Delete "$INSTDIR\README.TXT"
  Delete "$INSTDIR\LICENSE.TXT"
  Delete "$INSTDIR\UNINST.EXE"
  ; Written by Minuta itself: settings, diagnostics, untitled autosave.
  Delete "$INSTDIR\MINUTA.INI"
  Delete "$INSTDIR\TRACE.TXT"
  Delete "$INSTDIR\UNTITLED.MD~"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\Minuta\*.lnk"
  RMDir "$SMPROGRAMS\Minuta"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Minuta"
  DeleteRegKey HKLM "Software\Minuta"
SectionEnd
