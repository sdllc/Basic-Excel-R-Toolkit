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
; BERT 2 installer, 64-bit only. Installs over an existing BERT 2
; installation without touching the user's settings: bert-config.json,
; user-stylesheet.less and the files in Documents\BERT2 are created from
; templates only when they do not exist. The console, module and startup
; directories belong to the application and are replaced.
;
;   makensis /DVERSION=<version> install-script.nsi
;
; R is not bundled. The installer looks for an installed 64-bit R in the
; registry and writes it into bert-languages.json as the default R home,
; after the bundled R-3.5.0 that older installers left behind (the add-in
; uses the last candidate that exists). BERT.R.home in bert-config.json
; overrides both.
;
; Function help in the formula bar is an optional component, off by default.
; Selecting it installs the Excel-DNA IntelliSense add-in and a small carrier
; add-in, and registers both with Excel; clearing it on a later run removes
; them again. See docs/FUNCTION-HELP.md. For scripted installs, /HELP-FEATURE
; and /NO-HELP-FEATURE set it from the command line.

!ifndef VERSION
  !error "version is not defined"
!endif

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "Sections.nsh"
!include "StrFunc.nsh"
!include "WordFunc.nsh"
${StrRep}
!insertmacro VersionCompare
!insertmacro GetParameters
!insertmacro GetOptions

Unicode true

Var ExcelFlavor
Var ExcelPath
Var RHome
Var RVersion
Var ExcelKey

; the values written into Excel's add-in list for the optional component
!define IS_XLL_VALUE '/R "$INSTDIR\ExcelDna.IntelliSense64.xll"'
!define IS_XLAM_VALUE '"$INSTDIR\BERT-IntelliSense.xlam"'

; BERT's own add-in, in the form Excel's add-in list wants. normally the
; ribbon loads the xll for us (RegisterXLL, as it connects), so this is only
; written for an install without the ribbon. see docs/XLL-ONLY.md
!define BERT_XLL_VALUE '/R "$INSTDIR\BERT64.xll"'

Icon "bert2.ico"
UninstallIcon "bert2.ico"
Name "BERT ${VERSION}"
OutFile "BERT-Installer-${VERSION}-x64.exe"
BrandingText "BERT-Installer"
InstallDir "$LOCALAPPDATA\BERT2"
InstallDirRegKey HKCU "Software\BERT2" "InstallDir"
RequestExecutionLevel user

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN_TEXT "Open Excel and the BERT Console"
!define MUI_FINISHPAGE_RUN_PARAMETERS "/x:BERT"
!define MUI_FINISHPAGE_RUN $ExcelPath
!define MUI_ICON "bert2.ico"
!define MUI_UNICON "bert2.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Excel keeps its add-in list in the registry as OPEN, OPEN1, OPEN2 and so
; on, with no gaps: the first missing name ends the list. These add to and
; remove from that list without disturbing add-ins the user already has.
; Defined twice, once for the installer and once for the uninstaller.

!macro EXCEL_ADDIN_FUNCTIONS UN

; the Excel Options key for the installed Excel, or empty. 16.0 covers 2016
; and everything since, including Microsoft 365.
Function ${UN}FindExcelOptionsKey
  Push $0
  Push $1
  Push $2
  StrCpy $ExcelKey ""
  StrCpy $2 0
excelkey_loop:
  ${If} $2 == 0
    StrCpy $0 "16.0"
  ${ElseIf} $2 == 1
    StrCpy $0 "15.0"
  ${ElseIf} $2 == 2
    StrCpy $0 "14.0"
  ${Else}
    Goto excelkey_done
  ${EndIf}
  ClearErrors
  EnumRegKey $1 HKCU "Software\Microsoft\Office\$0\Excel" 0
  ${If} $1 != ""
    StrCpy $ExcelKey "Software\Microsoft\Office\$0\Excel\Options"
    Goto excelkey_done
  ${EndIf}
  IntOp $2 $2 + 1
  Goto excelkey_loop
excelkey_done:
  Pop $2
  Pop $1
  Pop $0
FunctionEnd

; adds the value on the stack to the add-in list, if it is not already there
Function ${UN}RegisterExcelAddIn
  Exch $R0
  Push $R1
  Push $R2
  Push $R3
  StrCmp $ExcelKey "" reg_done
  StrCpy $R1 0
reg_loop:
  ${If} $R1 == 0
    StrCpy $R2 "OPEN"
  ${Else}
    StrCpy $R2 "OPEN$R1"
  ${EndIf}
  ClearErrors
  ReadRegStr $R3 HKCU $ExcelKey $R2
  ${If} ${Errors}
    WriteRegStr HKCU $ExcelKey $R2 $R0    ; first free slot ends the list
    Goto reg_done
  ${EndIf}
  StrCmp $R3 $R0 reg_done                 ; already registered
  IntOp $R1 $R1 + 1
  ${If} $R1 < 40
    Goto reg_loop
  ${EndIf}
reg_done:
  Pop $R3
  Pop $R2
  Pop $R1
  Pop $R0
FunctionEnd

; removes the value on the stack from the add-in list, keeping it contiguous
; by moving the last entry into the gap
Function ${UN}UnregisterExcelAddIn
  Exch $R0
  Push $R1
  Push $R2
  Push $R3
  Push $R4
  Push $R5
  StrCmp $ExcelKey "" unreg_done
  StrCpy $R4 -1
  StrCpy $R5 -1
  StrCpy $R1 0
unreg_loop:
  ${If} $R1 == 0
    StrCpy $R2 "OPEN"
  ${Else}
    StrCpy $R2 "OPEN$R1"
  ${EndIf}
  ClearErrors
  ReadRegStr $R3 HKCU $ExcelKey $R2
  ${If} ${Errors}
    Goto unreg_scanned
  ${EndIf}
  StrCpy $R5 $R1
  StrCmp $R3 $R0 0 +2
  StrCpy $R4 $R1
  IntOp $R1 $R1 + 1
  ${If} $R1 < 40
    Goto unreg_loop
  ${EndIf}
unreg_scanned:
  ${If} $R4 < 0
    Goto unreg_done
  ${EndIf}
  ${If} $R4 == 0
    StrCpy $R2 "OPEN"
  ${Else}
    StrCpy $R2 "OPEN$R4"
  ${EndIf}
  ${If} $R5 == 0
    StrCpy $R3 "OPEN"
  ${Else}
    StrCpy $R3 "OPEN$R5"
  ${EndIf}
  ${If} $R4 != $R5
    ReadRegStr $R1 HKCU $ExcelKey $R3
    WriteRegStr HKCU $ExcelKey $R2 $R1
  ${EndIf}
  DeleteRegValue HKCU $ExcelKey $R3
unreg_done:
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Pop $R1
  Pop $R0
FunctionEnd

!macroend

!insertmacro EXCEL_ADDIN_FUNCTIONS ""
!insertmacro EXCEL_ADDIN_FUNCTIONS "un."

;--------------------------------
; which Excel is installed (Win64 or Win32), and where

Function CheckExcelVersion
  StrCpy $ExcelFlavor ""
  StrCpy $ExcelPath ""
  EnumRegKey $1 HKCR "TypeLib\{00020813-0000-0000-C000-000000000046}" 0
  StrCmp $1 "" CheckExcel_end
  EnumRegKey $ExcelFlavor HKCR "TypeLib\{00020813-0000-0000-C000-000000000046}\$1\0" 0
  StrCmp $ExcelFlavor "" CheckExcel_end
  ReadRegStr $ExcelPath HKCR "TypeLib\{00020813-0000-0000-C000-000000000046}\$1\0\$ExcelFlavor" ""
CheckExcel_end:
FunctionEnd

;--------------------------------
; a BERT v1 installation is disabled (not removed), with the user's consent

Function CheckOldBERT
  StrCpy $1 ""
  SetRegView 32
  ReadRegStr $0 HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "Description"
  StrCmp $0 "" CheckOldBERT_Check64
  ReadRegDWORD $0 HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "LoadBehavior"
  IntCmp $0 3 +2
  Goto CheckOldBERT_Check64
  MessageBox MB_OKCANCEL|MB_ICONQUESTION "Installer found an old BERT (v1) installation.$\nOkay to disable it?" /SD IDOK IDCANCEL CheckOldBERT_Cancel
  WriteRegDWORD HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "LoadBehavior" 2
  StrCpy $1 "Disable"
CheckOldBERT_Check64:
  SetRegView 64
  ReadRegStr $0 HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "Description"
  StrCmp $0 "" CheckOldBERT_End
  ReadRegDWORD $0 HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "LoadBehavior"
  IntCmp $0 3 +2
  Goto CheckOldBERT_End
  MessageBox MB_OKCANCEL|MB_ICONQUESTION "Installer found an old BERT (v1) installation.$\nOkay to disable it?" /SD IDOK IDCANCEL CheckOldBERT_Cancel
  WriteRegDWORD HKCU "Software\Microsoft\Office\Excel\Addins\BERTRibbon.Connect" "LoadBehavior" 2
  StrCpy $1 "Disable"
  Goto CheckOldBERT_End
CheckOldBERT_Cancel:
  Abort "Install canceled. Please see the BERT website for more information about upgrading."
CheckOldBERT_End:
  SetRegView default
  StrCmp $1 "" CheckOldBERT_Exit
  MessageBox MB_OK|MB_ICONINFORMATION "Your old BERT installation was disabled, but not deleted. Please see the BERT website for more information on upgrading from BERT v1." /SD IDOK
CheckOldBERT_Exit:
FunctionEnd

;--------------------------------
; find an R to use. people install R in more ways than one, so look in
; three places, keeping the newest usable one:
;
;   1. the InstallPath each R installer records, per user or per machine,
;      under R64 for 64-bit builds and R for older ones
;   2. the per-version subkeys below those, which survive when the
;      top-level value is missing or points at an R that has been removed
;   3. the usual install directory, for an R that registered nothing
;
; a candidate counts only if it actually has a 64-bit R.dll. $RVersion ends
; up holding the version where we could tell, otherwise empty.

; considers one candidate: $0 = home, $1 = version (may be empty)
Function ConsiderR
  Push $2
  ${If} $0 != ""
  ${AndIf} ${FileExists} "$0\bin\x64\R.dll"
    ${If} $RHome == ""
      StrCpy $RHome $0
      StrCpy $RVersion $1
    ${ElseIf} $1 != ""
    ${AndIf} $RVersion != ""
      ${VersionCompare} $1 $RVersion $2
      ${If} $2 == 1                      ; the candidate is newer
        StrCpy $RHome $0
        StrCpy $RVersion $1
      ${EndIf}
    ${EndIf}
  ${EndIf}
  Pop $2
FunctionEnd

; every per-version subkey of one R-core key: $3 = root marker, $4 = subkey
Function ConsiderRSubkeys
  Push $5
  Push $6
  StrCpy $5 0
subkey_loop:
  ClearErrors
  ${If} $3 == "HKLM"
    EnumRegKey $6 HKLM "Software\R-core\$4" $5
  ${Else}
    EnumRegKey $6 HKCU "Software\R-core\$4" $5
  ${EndIf}
  ${If} ${Errors}
    Goto subkey_done
  ${EndIf}
  StrCmp $6 "" subkey_done
  ${If} $3 == "HKLM"
    ReadRegStr $0 HKLM "Software\R-core\$4\$6" "InstallPath"
  ${Else}
    ReadRegStr $0 HKCU "Software\R-core\$4\$6" "InstallPath"
  ${EndIf}
  StrCpy $1 $6                            ; the subkey name is the version
  Call ConsiderR
  IntOp $5 $5 + 1
  ${If} $5 < 40
    Goto subkey_loop
  ${EndIf}
subkey_done:
  Pop $6
  Pop $5
FunctionEnd

!macro CONSIDER_R_KEY ROOT SUB
  ReadRegStr $0 ${ROOT} "Software\R-core\${SUB}" "InstallPath"
  ReadRegStr $1 ${ROOT} "Software\R-core\${SUB}" "Current Version"
  Call ConsiderR
  StrCpy $3 "${ROOT}"
  StrCpy $4 "${SUB}"
  Call ConsiderRSubkeys
!macroend

Function DetectR

  StrCpy $RHome ""
  StrCpy $RVersion ""
  SetRegView 64

  !insertmacro CONSIDER_R_KEY HKCU "R64"
  !insertmacro CONSIDER_R_KEY HKLM "R64"
  !insertmacro CONSIDER_R_KEY HKCU "R"
  !insertmacro CONSIDER_R_KEY HKLM "R"

  SetRegView default

  ; an R that registered nothing, in the usual place. the directory is
  ; named for the version, so R-4.5.2 gives 4.5.2.

  ${If} $RHome == ""
    FindFirst $5 $6 "$PROGRAMFILES64\R\R-*"
rdir_loop:
    StrCmp $6 "" rdir_done
    StrCpy $0 "$PROGRAMFILES64\R\$6"
    StrCpy $1 $6 "" 2                     ; drop the "R-"
    Call ConsiderR
    FindNext $5 $6
    Goto rdir_loop
rdir_done:
    FindClose $5
  ${EndIf}

  ${If} $RHome == ""
    MessageBox MB_OK|MB_ICONINFORMATION "No 64-bit R installation was found. BERT needs R 4.2 or later (https://cran.r-project.org).$\n$\nBERT will still install. Once R is installed, either run this installer again or set BERT.R.home in bert-config.json." /SD IDOK
    Return
  ${EndIf}

  ; found one: only say something if it is older than BERT is built for

  StrCmp $RVersion "" detectr_done
  ${VersionCompare} $RVersion "4.2.0" $0
  ${If} $0 == 2
    MessageBox MB_OK|MB_ICONINFORMATION "The R found on this machine is $RVersion, in $RHome.$\n$\nBERT is built for R 4.2 or later: with an older R the BERT R package may not load, and text outside the Windows code page may not survive the round trip. BERT will still install and use this R.$\n$\nTo use a different R, set BERT.R.home in bert-config.json." /SD IDOK
  ${EndIf}
detectr_done:

FunctionEnd

;--------------------------------
; bert-languages.json from its template, with the detected R filled in
; (backslashes doubled for JSON)

Function WriteLanguagesFile
  ${StrRep} $1 $RHome "\" "\\"
  ClearErrors
  FileOpen $2 "$INSTDIR\bert-languages.template.json" r
  FileOpen $3 "$INSTDIR\bert-languages.json" w
WriteLanguages_loop:
  FileRead $2 $4
  IfErrors WriteLanguages_end
  ${StrRep} $4 $4 "@R_HOME@" $1
  FileWrite $3 $4
  Goto WriteLanguages_loop
WriteLanguages_end:
  FileClose $3
  FileClose $2
  ClearErrors
FunctionEnd

;--------------------------------

Section "BERT" SecMain

  SectionIn RO

CheckExcelRunning:
  FindWindow $0 "XLMAIN"
  StrCmp $0 0 +4
  MessageBox MB_OKCANCEL|MB_ICONINFORMATION "Please close Excel before running the installer." /SD IDCANCEL IDCANCEL +2
  Goto CheckExcelRunning
  Abort "Install canceled"

  Call CheckExcelVersion
  StrCmp $ExcelFlavor "Win64" +3
  MessageBox MB_OK|MB_ICONSTOP "This installer is for 64-bit Excel, which was not found (found: '$ExcelFlavor')." /SD IDOK
  Abort "Install canceled: 64-bit Excel not found"

  Call CheckOldBERT
  Call DetectR

  SetOutPath "$INSTDIR"

  ; application-owned directories are replaced outright, so nothing from an
  ; older layout is left behind
  RMDir /r "$INSTDIR\console"
  RMDir /r "$INSTDIR\module"
  RMDir /r "$INSTDIR\startup"
  File /r ..\Build\Console
  File /r ..\Build\module
  File /r ..\Build\startup

  File ..\Build\BERT64.xll
  File ..\Build\ControlR.exe

  ; protobuf is linked statically now; remove the DLLs an earlier build
  ; shipped, which needed the Visual C++ redistributable
  Delete "$INSTDIR\libprotobuf.dll"
  Delete "$INSTDIR\abseil_dll.dll"

  ; the language list, with the detected R filled in
  File "bert-languages.template.json"
  Call WriteLanguagesFile

  ; settings: created from the templates only when they do not exist
  File ..\Build\bert-config-template.json
  IfFileExists "$INSTDIR\bert-config.json" +2
  CopyFiles "$INSTDIR\bert-config-template.json" "$INSTDIR\bert-config.json"

  File "..\Build\user-stylesheet-template.less"
  IfFileExists "$INSTDIR\user-stylesheet.less" +2
  CopyFiles "$INSTDIR\user-stylesheet-template.less" "$INSTDIR\user-stylesheet.less"

  WriteRegStr HKCU "Software\BERT2" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$INSTDIR\lib"

  ; examples and starter functions, only where the user has none
  SetOutPath "$INSTDIR\files"
  File ..\Examples\functions.*
  File ..\Examples\excel-scripting.r
  File ..\Examples\excel-functions.r

  CreateDirectory "$DOCUMENTS\BERT2\examples"
  CreateDirectory "$DOCUMENTS\BERT2\functions"

  IfFileExists "$DOCUMENTS\BERT2\examples\excel-scripting.r" +2
  CopyFiles "$INSTDIR\files\excel-scripting.r" "$DOCUMENTS\BERT2\examples\excel-scripting.r"
  IfFileExists "$DOCUMENTS\BERT2\functions\functions.r" +2
  CopyFiles "$INSTDIR\files\functions.r" "$DOCUMENTS\BERT2\functions\functions.r"
  IfFileExists "$DOCUMENTS\BERT2\examples\excel-functions.r" +2
  CopyFiles "$INSTDIR\files\excel-functions.r" "$DOCUMENTS\BERT2\examples\excel-functions.r"

  SetOutPath "$INSTDIR"
  File ..\Build\Welcome.md
  File ..\Build\CHANGELOG.md

  ; the project README, from the repository root rather than Build -- the one
  ; in Build describes the build directory and is not for shipping
  File ..\README.md

  File "bert2.ico"


  ; bundled R from installers before 2.4 (2.4 bundled R-3.5.0, which stays
  ; as a fallback for anyone who has not installed R themselves)
  RMDir /r "$INSTDIR\R-3.4.1"
  RMDir /r "$INSTDIR\R-3.4.2"
  RMDir /r "$INSTDIR\R-3.4.3"
  RMDir /r "$INSTDIR\R-3.4.4"

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "DisplayName" "BERT Toolkit"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "DisplayIcon" "$\"$INSTDIR\bert2.ico$\""
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "DisplayVersion" ${VERSION}
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "Publisher" "Structured Data LLC"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "NoRepair" 1
  ${GetTime} "" "L" $0 $1 $2 $3 $4 $5 $6
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2" "InstallDate" "$2$1$0"

SectionEnd

;--------------------------------
; optional: the BERT tab on the ribbon. it is a COM add-in, so it has to be
; registered, and excel can disable it independently of the xll. everything
; that reaches R -- functions in cells, the console, graphics, the EXCEL
; object -- works without it; what you lose is the tab itself and the
; buttons R adds with BERT$AddUserButton. see docs/XLL-ONLY.md.

Section "BERT tab on the ribbon" SecRibbon

  SetOutPath "$INSTDIR"
  File ..\Build\BERTRibbon2x64.dll
  ExecWait 'regsvr32 /s "$INSTDIR\BERTRibbon2x64.dll"'

SectionEnd

;--------------------------------
; optional: the argument tooltip in the formula bar. excel draws that only
; for its own functions, so this installs the Excel-DNA IntelliSense add-in,
; which draws one for user-defined functions, plus an empty carrier add-in
; it can find BERT's function descriptions beside. see docs/FUNCTION-HELP.md

Section /o "Function help in the formula bar" SecIntelliSense

  SetOutPath "$INSTDIR"

  File "BERT-IntelliSense.xlam"
  File "ExcelDna.IntelliSense64.xll"
  File "ExcelDna.IntelliSense-License.txt"

  ; the help add-in reads the descriptions as Excel starts and will not pick
  ; the file up later in that session, so there has to be one before BERT
  ; has run once. BERT overwrites it whenever it registers functions, so
  ; only lay it down when it is not already here.

  IfFileExists "$INSTDIR\BERT-IntelliSense.intellisense.xml" +2
  File "BERT-IntelliSense.intellisense.xml"

  Call FindExcelOptionsKey
  ${If} $ExcelKey == ""
    MessageBox MB_OK|MB_ICONINFORMATION "The function help add-ins were installed, but Excel's add-in list could not be found, so they were not enabled. Load them from File > Options > Add-ins > Manage Excel Add-ins > Browse:$\n$\n$INSTDIR\BERT-IntelliSense.xlam$\n$INSTDIR\ExcelDna.IntelliSense64.xll" /SD IDOK
  ${Else}
    Push '${IS_XLAM_VALUE}'
    Call RegisterExcelAddIn
    Push '${IS_XLL_VALUE}'
    Call RegisterExcelAddIn
  ${EndIf}

SectionEnd

;--------------------------------
; runs after the sections above: takes the optional component back out if it
; was cleared on a re-install

Section "-post"

  ; the xll goes in Excel's own add-in list whether the ribbon is installed
  ; or not. the ribbon loads the xll itself when it connects (RegisterXLL),
  ; and used to be the only thing that did -- so clearing "BERT2 Ribbon
  ; Menu" from Excel's COM add-ins dialog took the whole add-in with it,
  ; functions and all. registered here as well, turning the ribbon off in
  ; Excel leaves everything else working. the ribbon's own RegisterXLL then
  ; never runs: it is only reached when the xll is not already loaded.

  Call FindExcelOptionsKey
  ${If} $ExcelKey == ""
    MessageBox MB_OK|MB_ICONEXCLAMATION "Excel's add-in list could not be found, so BERT was not enabled in Excel. Load it from File > Options > Add-ins > Manage Excel Add-ins > Browse:$\n$\n$INSTDIR\BERT64.xll" /SD IDOK
  ${Else}
    Push '${BERT_XLL_VALUE}'
    Call RegisterExcelAddIn
  ${EndIf}

  ${IfNot} ${SectionIsSelected} ${SecRibbon}
    ExecWait 'regsvr32 /s /u "$INSTDIR\BERTRibbon2x64.dll"'
    Delete "$INSTDIR\BERTRibbon2x64.dll"
  ${EndIf}

  ${IfNot} ${SectionIsSelected} ${SecIntelliSense}
    Call FindExcelOptionsKey
    Push '${IS_XLAM_VALUE}'
    Call UnregisterExcelAddIn
    Push '${IS_XLL_VALUE}'
    Call UnregisterExcelAddIn
    Delete "$INSTDIR\BERT-IntelliSense.xlam"
    Delete "$INSTDIR\ExcelDna.IntelliSense64.xll"
    Delete "$INSTDIR\ExcelDna.IntelliSense-License.txt"
    Delete "$INSTDIR\BERT-IntelliSense.intellisense.xml"
  ${EndIf}

SectionEnd

;--------------------------------

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "The BERT add-in, the R language controller and the console."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecRibbon} "The BERT tab on Excel's ribbon, with the console button and any buttons your R code adds. A COM add-in, registered with Excel separately from BERT itself. Everything else -- functions, the console, graphics -- works without it. Clear this box on a later run to remove it."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecIntelliSense} "Shows a description of your R functions and their arguments as you type a formula, using the Excel-DNA IntelliSense add-in (MIT licensed, installed and enabled in Excel alongside BERT). Clear this box on a later run to remove it."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
; the optional component can be set from the command line, for scripted
; installs: /HELP-FEATURE or /NO-HELP-FEATURE

Function .onInit
  Push $R0
  Push $R1
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/HELP-FEATURE" $R1
  ${IfNot} ${Errors}
    !insertmacro SelectSection ${SecIntelliSense}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/NO-HELP-FEATURE" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SecIntelliSense}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/RIBBON" $R1
  ${IfNot} ${Errors}
    !insertmacro SelectSection ${SecRibbon}
  ${EndIf}
  ClearErrors
  ${GetOptions} $R0 "/NO-RIBBON" $R1
  ${IfNot} ${Errors}
    !insertmacro UnselectSection ${SecRibbon}
  ${EndIf}
  Pop $R1
  Pop $R0
FunctionEnd

;--------------------------------
; uninstall leaves the user's settings (bert-config.json, user-stylesheet.less)
; and Documents\BERT2 in place

Section "Uninstall"

UninstallCheckExcelRunning:
  FindWindow $0 "XLMAIN"
  StrCmp $0 0 +4
  MessageBox MB_OKCANCEL|MB_ICONINFORMATION "Please close Excel before running the uninstaller." /SD IDCANCEL IDCANCEL +2
  Goto UninstallCheckExcelRunning
  Abort "Uninstall canceled"

  RMDir /r "$INSTDIR\console"
  RMDir /r "$INSTDIR\module"
  RMDir /r "$INSTDIR\files"
  RMDir /r "$INSTDIR\startup"
  RMDir /r "$INSTDIR\R-3.4.1"
  RMDir /r "$INSTDIR\R-3.4.2"
  RMDir /r "$INSTDIR\R-3.4.3"
  RMDir /r "$INSTDIR\R-3.4.4"
  RMDir /r "$INSTDIR\R-3.5.0"
  RMDir /r "$APPDATA\bert2-console"

  ExecWait 'regsvr32 /s /u "$INSTDIR\BERTRibbon2x64.dll"'

  ; take the optional function help add-ins out of Excel's add-in list
  Call un.FindExcelOptionsKey
  Push '${IS_XLAM_VALUE}'
  Call un.UnregisterExcelAddIn
  Push '${IS_XLL_VALUE}'
  Call un.UnregisterExcelAddIn
  Push '${BERT_XLL_VALUE}'
  Call un.UnregisterExcelAddIn

  Delete "$INSTDIR\BERT64.xll"
  Delete "$INSTDIR\BERT32.xll"
  Delete "$INSTDIR\BERTRibbon2x64.dll"
  Delete "$INSTDIR\BERTRibbon2x86.dll"
  Delete "$INSTDIR\ControlR.exe"
  Delete "$INSTDIR\ControlJulia.exe"
  Delete "$INSTDIR\ControlJulia07.exe"
  Delete "$INSTDIR\libprotobuf.dll"
  Delete "$INSTDIR\abseil_dll.dll"
  Delete "$INSTDIR\bert-config-template.json"
  Delete "$INSTDIR\user-stylesheet-template.less"
  Delete "$INSTDIR\bert-languages.json"
  Delete "$INSTDIR\bert-languages.template.json"
  Delete "$INSTDIR\BERT-IntelliSense.xlam"
  Delete "$INSTDIR\BERT-IntelliSense.intellisense.xml"
  Delete "$INSTDIR\ExcelDna.IntelliSense64.xll"
  Delete "$INSTDIR\ExcelDna.IntelliSense-License.txt"
  Delete "$INSTDIR\Welcome.md"
  Delete "$INSTDIR\CHANGELOG.md"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\bert2.ico"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BERT2"

SectionEnd
