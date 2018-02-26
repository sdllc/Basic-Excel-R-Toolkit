
;--------------------------------
;

!ifndef VERSION
  !error "version is not defined"
!endif

;--------------------------------
;Includes

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
  !insertmacro MUI_PAGE_LICENSE "license.txt"
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

  ; default prefs
  File ..\Build\bert-config-default.json

  ; copy unless there is an existing prefs file
  IfFileExists "$INSTDIR\bert-config.json" +2
  CopyFiles "$INSTDIR\bert-config-default.json" "$INSTDIR\bert-config.json"

  ; FIXME: for 32-bit Excel, we'll need to switch registration based
  ; on bitness. have to move a bitness check into this script somewhere.

  ; NOTE: we stopped using the install lib macro because that requires
  ; global defines, which will prohibit us from doing 32/64 bit switching.

  ; register
  ExecWait 'regsvr32 /s "$INSTDIR\BERTRibbon2x64.dll"'

  ; controllers
  File ..\Build\ControlR.exe
  File ..\Build\ControlJulia.exe

  ;Store installation folder
  WriteRegStr HKCU "Software\BERT2" "InstallDir" "$INSTDIR"

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd

Section "Docs" SecDocs

  ; files are installed to the root directory,
  ; then we can copy them if they don't already exist

  SetOutPath "$INSTDIR\files"
  File ..\Examples\functions.*
  File ..\Examples\excel-scripting.r
  File ..\Examples\excel-scripting.jl

  CreateDirectory "$DOCUMENTS\BERT2\examples"
  CreateDirectory "$DOCUMENTS\BERT2\functions"

  ; FIXME: make this a function

  IfFileExists "$DOCUMENTS\BERT2\examples\excel-scripting.r" +2
  CopyFiles "$INSTDIR\files\excel-scripting.r" "$DOCUMENTS\BERT2\examples\excel-scripting.r"

  IfFileExists "$DOCUMENTS\BERT2\examples\excel-scripting.jl" +2
  CopyFiles "$INSTDIR\files\excel-scripting.jl" "$DOCUMENTS\BERT2\examples\excel-scripting.jl"

  IfFileExists "$DOCUMENTS\BERT2\functions\functions.r" +2
  CopyFiles "$INSTDIR\files\functions.r" "$DOCUMENTS\BERT2\functions\functions.r"

  IfFileExists "$DOCUMENTS\BERT2\functions\functions.jl" +2
  CopyFiles "$INSTDIR\files\functions.jl" "$DOCUMENTS\BERT2\functions\functions.jl"

  ; intro/release notes

  File Welcome.md 


SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ; directories and files

  RMDir /r "$INSTDIR\console"
  RMDir /r "$INSTDIR\module"
  RMDir /r "$INSTDIR\files"

  ; unregister. see note above re:32-bit Excel

  ExecWait 'regsvr32 /s /u "$INSTDIR\BERTRibbon2x64.dll"'

  Delete "$INSTDIR\BERT64.xll"
  Delete "$INSTDIR\BERTRibbon2x64.dll"
  Delete "$INSTDIR\ControlR.exe"
  Delete "$INSTDIR\ControlJulia.exe"
  Delete "$INSTDIR\bert-config-default.json"

  Delete "$DOCUMENTS\BERT2\Welcome.md"

  ; uninstaller

  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\BERT2"

SectionEnd
