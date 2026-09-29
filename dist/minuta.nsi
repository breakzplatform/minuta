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

!define SHCNE_ASSOCCHANGED 0x08000000

; Points .md or .markdown at Minuta.Document, keeping any previous handler in
; the value MinutaBackup so the uninstaller can put it back.
!macro Associate EXTENSION
  ReadRegStr $0 HKCR "${EXTENSION}" ""
  StrCmp $0 "Minuta.Document" +3
  StrCmp $0 "" +2
  WriteRegStr HKCR "${EXTENSION}" "MinutaBackup" $0
  WriteRegStr HKCR "${EXTENSION}" "" "Minuta.Document"
!macroend

!macro Unassociate EXTENSION
  ReadRegStr $0 HKCR "${EXTENSION}" ""
  StrCmp $0 "Minuta.Document" 0 +7
  ReadRegStr $1 HKCR "${EXTENSION}" "MinutaBackup"
  StrCmp $1 "" +4
  WriteRegStr HKCR "${EXTENSION}" "" $1
  DeleteRegValue HKCR "${EXTENSION}" "MinutaBackup"
  Goto +2
  DeleteRegKey HKCR "${EXTENSION}"
!macroend

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
  ; Markdown files open in Minuta. The icon index 1 is the second icon group
  ; in MINUTA.EXE, MINUTADOC.ICO (Win9x handles positive indexes reliably).
  WriteRegStr HKCR "Minuta.Document" "" "Markdown Document"
  WriteRegStr HKCR "Minuta.Document\DefaultIcon" "" "$INSTDIR\MINUTA.EXE,1"
  WriteRegStr HKCR "Minuta.Document\shell" "" "open"
  WriteRegStr HKCR "Minuta.Document\shell\open\command" "" '"$INSTDIR\MINUTA.EXE" "%1"'
  !insertmacro Associate ".md"
  !insertmacro Associate ".markdown"
  System::Call 'shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i 0, i 0, i 0)'
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
  !insertmacro Unassociate ".md"
  !insertmacro Unassociate ".markdown"
  DeleteRegKey HKCR "Minuta.Document"
  System::Call 'shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i 0, i 0, i 0)'
SectionEnd
