
;--------------------------------
;

!define VERSION 2.0.15

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"

;--------------------------------
;General

  ;Name and file
  Name "BERT ${VERSION}"
  OutFile "BERT-Installer-${VERSION}.exe"
  BrandingText "BERT-Installer"  

  ;Default installation folder
  InstallDir "$LOCALAPPDATA\BERT2"

  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\BERT2" "InstallDir"

  ;Request application privileges for Windows Vista
  RequestExecutionLevel user

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "${NSISDIR}\Docs\Modern UI\License.txt"
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Dummy Section" SecDummy

  SetOutPath "$INSTDIR"

  ; console 
  File /r ..\Build\console

  ; R module 
  File /r ..\Build\module

  ; excel modules
  File ..\Build\BERT64.xll
  File ..\Build\BERTRibbon2x64.dll

  ; controllers
  File ..\Build\ControlR.exe
  File ..\Build\ControlJulia.exe

  ;Store installation folder
  WriteRegStr HKCU "Software\BERT2" "InstallDir" $INSTDIR

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd

Section "Examples" SecExamples

  CreateDirectory "$DOCUMENTS\BERT2\functions"
  SetOutPath "$DOCUMENTS\BERT2\functions"

  File ..\Examples\functions.*

SectionEnd

Section "Docs" SecDocs

  CreateDirectory "$DOCUMENTS\BERT2\examples"
  SetOutPath "$DOCUMENTS\BERT2\examples"

  File ..\Examples\excel-scripting.r

  SetOutPath "$DOCUMENTS\BERT2"

  File Welcome.md

SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ; directories and files

  RMDir /r "$INSTDIR\console"
  RMDir /r "$INSTDIR\module"

  Delete "$INSTDIR\BERT64.xll"
  Delete "$INSTDIR\BERTRibbon2x64.dll"
  Delete "$INSTDIR\ControlR.exe"
  Delete "$INSTDIR\ControlJulia.exe"

  Delete "$DOCUMENTS\BERT2\Welcome.md"

  ; uninstaller

  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\BERT2"

SectionEnd
