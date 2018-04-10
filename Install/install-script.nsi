;
; Copyright (c) 2017-2018 Structured Data, LLC
; 
; This file is part of BERT.
;
; BERT is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; BERT is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with BERT.  If not, see <http://www.gnu.org/licenses/>.
;

;--------------------------------
;

!ifndef VERSION
  !error "version is not defined"
!endif

;--------------------------------
;Check bitness and path

Function CheckExcelVersion

  Var /GLOBAL ExcelFlavor
  Var /GLOBAL ExcelPath

  EnumRegKey $1 HKCR "TypeLib\{00020813-0000-0000-C000-000000000046}" 0
  StrCmp $1 "" CheckExcel_end

  EnumRegKey $ExcelFlavor HKCR "TypeLib\{00020813-0000-0000-C000-000000000046}\$1\0" 0
  StrCmp $ExcelFlavor "" CheckExcel_end

  ReadRegStr $ExcelPath HKCR "TypeLib\{00020813-0000-0000-C000-000000000046}\$1\0\$ExcelFlavor" ""

  CheckExcel_end:

FunctionEnd

;--------------------------------
;Check for BERT1

Function CheckOldBERT

  StrCpy $1 ""

  SetRegView 32
  ReadRegStr $0 HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "Description"
  StrCmp $0 "" CheckOldBERT_Check64
  ReadRegDWORD $0 HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "LoadBehavior"
  IntCmp $0 3 +2
  Goto CheckOldBERT_Check64
  MessageBox MB_OKCANCEL|MB_ICONQUESTION "Installer found an old BERT (v1) installation.$\nOkay to disable it?" IDCANCEL CheckOldBERT_Cancel
  WriteRegDWORD HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "LoadBehavior" 2
  StrCpy $1 "Disable"

CheckOldBERT_Check64:

  SetRegView 64
  ReadRegStr $0 HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "Description"
  StrCmp $0 "" CheckOldBERT_End
  ReadRegDWORD $0 HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "LoadBehavior"
  IntCmp $0 3 +2
  Goto CheckOldBERT_End
  MessageBox MB_OKCANCEL|MB_ICONQUESTION "Installer found an old BERT (v1) installation.$\nOkay to disable it?" IDCANCEL CheckOldBERT_Cancel
  WriteRegDWORD HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "LoadBehavior" 2
  StrCpy $1 "Disable"
  Goto CheckOldBERT_End

CheckOldBERT_Cancel:
  Abort "Install canceled. Please see the BERT website for more information about upgrading."

CheckOldBERT_End:
  SetRegView default
  StrCmp $1 "" CheckOldBERT_Exit

  MessageBox MB_OK|MB_ICONINFORMATION "Your old BERT installation was disabled, but not deleted. Please see the BERT website for more information on upgrading from BERT v1."

CheckOldBERT_Exit:
FunctionEnd

;--------------------------------
;Delete old versions of R

!macro DeleteRVersions

  ; this happens both on uninstall and on update. we are breaking
  ; it out so there's only one list to maintain. it has to be a macro
  ; and not a function, otherwise we would still need separate functions.

  RMDir /r "$INSTDIR\R-3.4.1"
  RMDir /r "$INSTDIR\R-3.4.2"
  RMDir /r "$INSTDIR\R-3.4.3"
  RMDir /r "$INSTDIR\R-3.4.4"

!macroend

;--------------------------------
;Includes

  !include "MUI2.nsh"
  
  ; for GetTime

  !include "FileFunc.nsh"

;--------------------------------
;General

  Icon "bert2.ico"
  UninstallIcon "bert2.ico"

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
  !define MUI_FINISHPAGE_RUN_TEXT "Open Excel and the BERT Console"
  !define MUI_FINISHPAGE_RUN_PARAMETERS "/x:BERT"
  !define MUI_FINISHPAGE_RUN $ExcelPath

  !define MUI_ICON "bert2.ico"
  !define MUI_UNICON "bert2.ico"

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

Section "Main" SecMain

  CheckExcelRunning:
    FindWindow $0 "XLMAIN"
    StrCmp $0 0 +4
    MessageBox MB_OKCANCEL|MB_ICONINFORMATION "Please close Excel before running the installer." IDCANCEL +2
    Goto CheckExcelRunning
    Abort "Install canceled"

  Call CheckExcelVersion

  Call CheckOldBERT

;  StrCmp $ExcelFlavor "Win64" +3
;  MessageBox MB_ICONSTOP|MB_OK "Sorry, 32-bit Excel is not supported in this release."
;  Abort "Install error - invalid Excel version detected"

  SetOutPath "$INSTDIR"

  ; console 
  File /r ..\Build\console

  ; R module 
  File /r ..\Build\module

  ; config
  File /r ..\Build\startup
  File ..\Build\bert-languages.json

  ; excel modules
  ;
  ; FIXME: since we know, we only need to install the appropriate version.
  ; (although we still need the File command to build the installer)
  ;
  File ..\Build\BERT64.xll
  File ..\Build\BERT32.xll
  File ..\Build\BERTRibbon2x64.dll
  File ..\Build\BERTRibbon2x86.dll

  ; default prefs
  File ..\Build\bert-config-template.json

  ; copy unless there is an existing prefs file
  IfFileExists "$INSTDIR\bert-config.json" +2
  CopyFiles "$INSTDIR\bert-config-template.json" "$INSTDIR\bert-config.json"

  ; default stylesheet
  File "..\Build\user-stylesheet-template.less"

  ; copy unless there is an existing stylesheet
  IfFileExists "$INSTDIR\user-stylesheet.less" +2
  CopyFiles "$INSTDIR\user-stylesheet-template.less" "$INSTDIR\user-stylesheet.less"

  ; register, depends on bitness
  StrCmp $ExcelFlavor "Win64" 0 +3
  ExecWait 'regsvr32 /s "$INSTDIR\BERTRibbon2x64.dll"'
  Goto +2
  ExecWait 'regsvr32 /s "$INSTDIR\BERTRibbon2x86.dll"'

  ; controllers
  File ..\Build\ControlR.exe
  File ..\Build\ControlJulia.exe
  File ..\Build\ControlJulia07.exe

  ;Store installation folder
  WriteRegStr HKCU "Software\BERT2" "InstallDir" "$INSTDIR"

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; create a lib dir for R libs
  CreateDirectory "$INSTDIR\lib"

  ; files are installed to the root directory,
  ; then we can copy them if they don't already exist

  SetOutPath "$INSTDIR\files"
  File ..\Examples\functions.*
  File ..\Examples\excel-scripting.r
  File ..\Examples\excel-scripting.jl
  File ..\Examples\excel-functions.r

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

  IfFileExists "$DOCUMENTS\BERT2\examples\excel-functions.r" +2
  CopyFiles "$INSTDIR\files\excel-functions.r" "$DOCUMENTS\BERT2\examples\excel-functions.r"

  ; intro/release notes

  SetOutPath "$INSTDIR"
  File ..\Build\Welcome.md 

  ; icon for add/remove programs

  File "bert2.ico"

  ; delete old versions of R, if we are updating

  !insertmacro DeleteRVersions

!ifdef R

  ; R. this is somewhat convoluted because I can't figure out how to get 
  ; NSIS to install a directory without using a containing directory.
  ; this will install whatever is in the passed directory, which should 
  ; contain the current version of R (and nothing else).

  File /r ${R}\*.*

!endif

  ; add uninstall info

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "DisplayName" "BERT Tookit"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "DisplayIcon" "$\"$INSTDIR\bert2.ico$\""
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "DisplayVersion" ${VERSION}
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "Publisher" "Structured Data LLC"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "NoRepair" 1

  ${GetTime} "" "L" $0 $1 $2 $3 $4 $5 $6
	; $0="01"      day
	; $1="04"      month
	; $2="2005"    year
	; $3="Friday"  day of week name
	; $4="16"      hour
	; $5="05"      minute
	; $6="50"      seconds

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "InstallDate" "$2$1$0"


SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  UninstallCheckExcelRunning:
    FindWindow $0 "XLMAIN"
    StrCmp $0 0 +4
    MessageBox MB_OKCANCEL|MB_ICONINFORMATION "Please close Excel before running the uninstaller." IDCANCEL +2
    Goto UninstallCheckExcelRunning
    Abort "Uninstall canceled"

  ; directories and files

  RMDir /r "$INSTDIR\console"
  RMDir /r "$INSTDIR\module"
  RMDir /r "$INSTDIR\files"
  RMDir /r "$INSTDIR\startup"

  ; clean up R
  
  !insertmacro DeleteRVersions

  ; this is data for the console

  RMDir /r "$APPDATA\bert2-console"

  ; unregister, brute force
  ExecWait 'regsvr32 /s /u "$INSTDIR\BERTRibbon2x64.dll"'
  ExecWait 'regsvr32 /s /u "$INSTDIR\BERTRibbon2x86.dll"'

  Delete "$INSTDIR\BERT64.xll"
  Delete "$INSTDIR\BERT32.xll"
  Delete "$INSTDIR\BERTRibbon2x64.dll"
  Delete "$INSTDIR\BERTRibbon2x86.dll"
  Delete "$INSTDIR\ControlR.exe"
  Delete "$INSTDIR\ControlJulia.exe"
  Delete "$INSTDIR\ControlJulia07.exe"
  Delete "$INSTDIR\bert-config-template.json"
  Delete "$INSTDIR\user-stylesheet-template.json"
  Delete "$INSTDIR\bert-languages.json"
  Delete "$INSTDIR\Welcome.md"
  Delete "$INSTDIR\bert2.ico"

  ; uninstaller

  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  ; DeleteRegKey /ifempty HKCU "Software\BERT2"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2"

SectionEnd
