; Minuta installer. ANSI (not Unicode) so it runs on Windows 98.
; Built by `make dist` with makensis; expects the files in build/dist/win98.
Unicode false
SetCompressor /SOLID lzma

!ifndef VERSION
  !define VERSION "0.1.2"
!endif

Name "Minuta ${VERSION}"
OutFile "..\build\dist\minuta-${VERSION}-setup.exe"
InstallDir "$PROGRAMFILES\Minuta"
InstallDirRegKey HKLM "Software\Minuta" "InstallDir"
Icon "..\res\MINUTA.ICO"
UninstallIcon "..\res\MINUTAUN.ICO"
RequestExecutionLevel admin
XPStyle off

!define SHCNE_ASSOCCHANGED 0x08000000
!define HWND_BROADCAST 0xFFFF
!define WM_SETTINGCHANGE 0x001A
!define SPI_SETNONCLIENTMETRICS 0x002A
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Minuta"

; Windows 98 keeps icons in ShellIconCache by path and index, so after an
; upgrade Explorer and the Start menu would go on showing the old ones. A
; change of the shell icon size makes Explorer rebuild the cache (what
; TweakUI's "Rebuild Icons" does); the size is then put back.
!macro RebuildIconCache
  ReadRegStr $0 HKCU "Control Panel\Desktop\WindowMetrics" "Shell Icon Size"
  StrCpy $1 $0
  StrCmp $1 "" 0 +2
  StrCpy $1 32
  IntOp $1 $1 - 1
  WriteRegStr HKCU "Control Panel\Desktop\WindowMetrics" "Shell Icon Size" $1
  SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} ${SPI_SETNONCLIENTMETRICS} 0 /TIMEOUT=5000
  StrCmp $0 "" 0 +3
  DeleteRegValue HKCU "Control Panel\Desktop\WindowMetrics" "Shell Icon Size"
  Goto +2
  WriteRegStr HKCU "Control Panel\Desktop\WindowMetrics" "Shell Icon Size" $0
  SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} ${SPI_SETNONCLIENTMETRICS} 0 /TIMEOUT=5000
!macroend

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
  SetOutPath "$INSTDIR\Examples"
  File "..\build\dist\win98\Examples\About the examples.md"
  File "..\build\dist\win98\Examples\Hino de Pernambuco.md"
  File "..\build\dist\win98\Examples\Luar do Sertao.md"
  File "..\build\dist\win98\Examples\Poema em linha reta.md"
  SetOutPath "$INSTDIR"
  WriteRegStr HKLM "Software\Minuta" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\UNINST.EXE"
  CreateDirectory "$SMPROGRAMS\Minuta"
  CreateShortCut "$SMPROGRAMS\Minuta\Minuta.lnk" "$INSTDIR\MINUTA.EXE"
  CreateShortCut "$SMPROGRAMS\Minuta\Read Me.lnk" "$INSTDIR\README.TXT"
  CreateShortCut "$SMPROGRAMS\Minuta\Examples.lnk" "$INSTDIR\Examples"
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
  !insertmacro RebuildIconCache
  ; Add/Remove Programs. Windows 98 shows only the name; later versions also
  ; show the icon, version and publisher.
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "Minuta ${VERSION}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\UNINST.EXE"'
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\MINUTA.EXE,0"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "Joselito Junior"
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\MINUTA.EXE"
  Delete "$INSTDIR\MD2HTMLW.EXE"
  Delete "$INSTDIR\README.TXT"
  Delete "$INSTDIR\LICENSE.TXT"
  Delete "$INSTDIR\UNINST.EXE"
  ; Only the files the installer put there; copies the user saved in
  ; Examples stay, and so does the folder.
  Delete "$INSTDIR\Examples\About the examples.md"
  Delete "$INSTDIR\Examples\Hino de Pernambuco.md"
  Delete "$INSTDIR\Examples\Luar do Sertao.md"
  Delete "$INSTDIR\Examples\Poema em linha reta.md"
  RMDir "$INSTDIR\Examples"
  ; Written by Minuta itself: settings, diagnostics, untitled autosave.
  Delete "$INSTDIR\MINUTA.INI"
  Delete "$INSTDIR\TRACE.TXT"
  Delete "$INSTDIR\UNTITLED.MD~"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\Minuta\*.lnk"
  RMDir "$SMPROGRAMS\Minuta"
  DeleteRegKey HKLM "${UNINSTALL_KEY}"
  DeleteRegKey HKLM "Software\Minuta"
  !insertmacro Unassociate ".md"
  !insertmacro Unassociate ".markdown"
  DeleteRegKey HKCR "Minuta.Document"
  System::Call 'shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i 0, i 0, i 0)'
  !insertmacro RebuildIconCache
SectionEnd
