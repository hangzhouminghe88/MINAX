#
# Copyright (C) 2000-2018 Kern Sibbald
# License: BSD 2-Clause; see file LICENSE-FOSS
#
##{{NSIS_PLUS_BEGIN_PROJECT_SETTINGS}}##
#NAME "Release"
#       CMD -DSRC_DIR=release64 -DSRC32_DIR=..\win32_installer\release32 -DSRC64_DIR=..\win64_installer\release64 -DOUT_DIR=release64 -DWINVER=64 -DVERSION=6.7.9 -DBUILD_TOOLS=NSIS-3.0b0
#       EXE C:\Program Files (x86)\NSIS\makensis.exe
#       FLAGS 2
##{{NSIS_PLUS_END_PROJECT_SETTINGS}}##

; winbacula.nsi
;
; Began as a version written by Michel Meyers (michel@tcnnet.dyndns.org)
;
; Adapted by Kern Sibbald for native Win32 Bacula
;    added a number of elements from Christopher Hull's installer
;
; D. Scott Barninger Apr 17 2005
; Robert Nelson May 15 2006
; Eric Bollengier March 2009
;
; Kern Sibbald October 2010
; Remove server installs and
;   install into a single bacula directory
;
;
; Command line options:
;
; /service    - 
; /start
;
; netsh firewall add portopening protocol=tcp port=9102 name="Bacula-FD"


!define PRODUCT "Bacula"

;
; Include the Modern UI
;

!include "MUI.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "Sections.nsh"
!include "StrFunc.nsh"
!include "WinMessages.nsh"
!include "x64.nsh"
;
; Basics
;
; Name in Welcome screen
Name "Bacula 64 bit"
OutFile "${OUT_DIR}\bacula-win${WINVER}-${VERSION}.exe"
SetCompressor lzma
Caption "Bacula 64 bit ${VERSION}"
;Caption "Bacula 64 bit Edition"
VIProductVersion ${VERSION}.1
VIAddVersionKey CompanyName "Bacula Project"
VIAddVersionKey LegalCopyright "Kern Sibbald"
VIAddVersionKey FileDescription "Bacula network backup and restore"
VIAddVersionKey FileVersion win${WINVER}-${VERSION}
VIAddVersionKey ProductVersion win${WINVER}-${VERSION}
VIAddVersionKey ProductName "Bacula"
VIAddVersionKey InternalName "Bacula"
VIAddVersionKey LegalTrademarks "Bacula is a registered trademark of Kern Sibbald"
VIAddVersionKey OriginalFilename "bacula.exe"

InstallDir "C:\Program Files\Bacula"
InstallDirRegKey HKLM "Software\Bacula" "InstallLocation"

InstType "Client"
InstType "Server"
;InstType "Full"

!insertmacro GetParent

${StrCase}
${StrRep}
${StrTok}
${StrTrimNewLines}

;
; Pull in pages
;

!define      MUI_COMPONENTSPAGE_SMALLDESC

!define      MUI_HEADERIMAGE
!define      MUI_BGCOLOR                739AB9
!define      MUI_HEADERIMAGE_BITMAP     "bacula-logo.bmp"

!InsertMacro MUI_PAGE_WELCOME
!InsertMacro MUI_PAGE_LICENSE "${SRC_DIR}\LICENSE"
Page custom EnterInstallType
!define      MUI_PAGE_CUSTOMFUNCTION_SHOW PageComponentsShow
!InsertMacro MUI_PAGE_COMPONENTS
!define      MUI_PAGE_CUSTOMFUNCTION_PRE PageDirectoryPre
!InsertMacro MUI_PAGE_DIRECTORY
Page custom EnterConfigPage1 LeaveConfigPage1
Page custom EnterConfigPage2 LeaveConfigPage2
!Define      MUI_PAGE_CUSTOMFUNCTION_LEAVE LeaveInstallPage
!InsertMacro MUI_PAGE_INSTFILES
!Define      MUI_FINISHPAGE_SHOWREADME $INSTDIR\Readme.txt
!InsertMacro MUI_PAGE_FINISH

!InsertMacro MUI_UNPAGE_WELCOME
!InsertMacro MUI_UNPAGE_CONFIRM
!InsertMacro MUI_UNPAGE_INSTFILES
!InsertMacro MUI_UNPAGE_FINISH

!define      MUI_ABORTWARNING

!InsertMacro MUI_LANGUAGE "English"

!InsertMacro GetParameters
!InsertMacro GetOptions

DirText "Setup will install Bacula 64 bit ${VERSION} to the directory specified below. To install in a different folder, click Browse and select another folder."

!InsertMacro MUI_RESERVEFILE_INSTALLOPTIONS
;
; Global Variables
;
Var OptService
Var OptStart
Var OptSilent

Var CommonFilesDone

Var OsIsNT

Var HostName

Var ConfigClientName
Var ConfigClientPort
Var ConfigClientMaxJobs
Var ConfigClientPassword
Var ConfigClientInstallService
Var ConfigClientStartService

Var ConfigStorageName
Var ConfigStoragePort
Var ConfigStorageMaxJobs
Var ConfigStoragePassword
Var ConfigStorageInstallService
Var ConfigStorageStartService

Var ConfigDirectorName
Var ConfigDirectorPort
Var ConfigDirectorMaxJobs
Var ConfigDirectorPassword
Var ConfigDirectorAddress
Var ConfigDirectorMailServer
Var ConfigDirectorMailAddress
Var ConfigDirectorDB
Var ConfigDirectorInstallService
Var ConfigDirectorStartService

Var ConfigMonitorName
Var ConfigMonitorPassword

Var LocalDirectorPassword
Var LocalHostAddress

Var MySQLPath
Var MySQLVersion
Var PostgreSQLPath
Var PostgreSQLVersion

Var AutomaticInstall
Var InstallType

!define NewInstall      0
!define UpgradeInstall  1
!define MigrateInstall  2

Var OldInstallDir
Var PreviousComponents
Var NewComponents
Var TmpComponent

; Bit 0 = File Service
;     1 = Storage Service
;     2 = Director Service
;     3 = Command Console
;     4 = Bat Console
;     5 = wxWidgits Console
;     6 = Documentation (PDF)
;     7 = Documentation (HTML)
;     8 = CDP Plugin
;     9 = Tray Monitor
;    10 = alldrives Plugin

!define ComponentFile                   1
!define ComponentStorage                2
!define ComponentDirector               4
!define ComponentTextConsole            8
!define ComponentBatConsole             16
!define ComponentGUIConsole             32
!define ComponentPDFDocs                64
!define ComponentHTMLDocs               128
!define MUI_PAGE_LICENSE "${SRC_DIR}\INSTALL"
!define ComponentCDPPlugin              256
!define ComponentTrayMonitor            512
!define ComponentAllDrivesPlugin        1024

!define ComponentsRequiringUserConfig           63
!define ComponentsFileAndStorage                3
!define ComponentsFileAndStorageAndDirector     7
!define ComponentsDirectorAndTextGuiConsoles    60
!define ComponentsTextAndGuiConsoles            56

Var HDLG
Var HCTL

# detect if a process is running
# usage: !insertmacro FindProc $R0 "bacula-tray-monitor.exe"
# then if $R0 == 0 then the process is running, else it is not running or the
# macro failed fo another raison
!define FindProc_FOUND 0
!macro FindProc result processName
    # I tried to "protect" binaries path with ", but it is not working anymore
    nsExec::Exec 'cmd /c %SystemRoot%\System32\tasklist.exe /NH /FI "IMAGENAME eq ${processName}" | %SystemRoot%\System32\find.exe /I "${processName}"'
    Pop ${result} ; The exit code 0 -> Found
!macroend

Function .onInit
  Push $R0
  Push $R1

  ; Process Command Line Options
  StrCpy $OptService 1
  StrCpy $OptStart 1
  StrCpy $OptSilent 0
  StrCpy $CommonFilesDone 0
  StrCpy $OsIsNT 0
  StrCpy $AutomaticInstall 0
  StrCpy $InstallType ${NewInstall}
  StrCpy $OldInstallDir ""
  StrCpy $PreviousComponents 0
  StrCpy $NewComponents 0
  StrCpy $MySQLPath ""
  StrCpy $MySQLVersion ""
  StrCpy $PostgreSQLPath ""
  StrCpy $PostgreSQLVersion ""
  StrCpy $LocalDirectorPassword ""

  ${GetParameters} $R0

  ClearErrors

  ${If} ${RunningX64}
  ${Else}
     MessageBox MB_OK "This is a 64 bit installer, but the OS is not an x64 -- Aborting ..." /SD IDOK
     Abort
  ${EndIf}

  !insertmacro FindProc $R0 "bacula-tray-monitor.exe"
  # if FindProc fail for any reason (like tasklisk.exe not found), FindProc return
  # a return code != 0, and the installer will continue.
  # if "bacula-tray-monitor.exe" is running the installer will faile later
  IntCmp $R0 0 BaculaTrayMonitorFound BaculaTrayMonitorNotFound BaculaTrayMonitorNotFound
  BaculaTrayMonitorFound:
     IfSilent +2 0
     MessageBox MB_OK|MB_ICONEXCLAMATION "bacula-tray-monitor.exe is running. Please close it first" /SD IDOK
     Abort
  BaculaTrayMonitorNotFound:

  ${GetOptions} $R0 "/noservice" $R1
  IfErrors +2
    StrCpy $OptService 0

  ClearErrors
  ${GetOptions} $R0 "/nostart" $R1
  IfErrors +2
    StrCpy $OptStart 0

  IfSilent 0 +2
    StrCpy $OptSilent 1

  ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  ${If} $R0 != ""
    StrCpy $OsIsNT 1
  ${EndIf}

  Call GetComputerName
  Pop $HostName

  Call GetHostName
  Pop $LocalHostAddress

  Call GetUserName

  ; Configuration Defaults

  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigClientName" $ConfigClientName
  IfErrors 0 +2
    StrCpy $ConfigClientName               "$HostName-fd"
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigClientPort" $ConfigClientPort
  IfErrors 0 +2
    StrCpy $ConfigClientPort               9102
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigClientMaxJobs" $ConfigClientMaxJobs
  IfErrors 0 +2
    StrCpy $ConfigClientMaxJobs            5
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigClientPassword" $ConfigClientPassword
  IfErrors 0 +2
    StrCpy $ConfigClientPassword          ""
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigClientInstallService" $ConfigClientInstallService
  IfErrors 0 +2
    StrCpy $ConfigClientInstallService     "$OptService"
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigClientStartService" $ConfigClientStartService
  IfErrors 0 +2
    StrCpy $ConfigClientStartService       "$OptStart"
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigStorageName" $ConfigStorageName
  IfErrors 0 +2
    StrCpy $ConfigStorageName              "$HostName-sd"
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigStoragePort" $ConfigStoragePort
  IfErrors 0 +2
    StrCpy $ConfigStoragePort              9103
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigStorageMaxJobs" $ConfigStorageMaxJobs
  IfErrors 0 +2
    StrCpy $ConfigStorageMaxJobs           10
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigStoragePassword" $ConfigStoragePassword
  IfErrors 0 +2
    StrCpy $ConfigStoragePassword          ""
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigStorageInstallService" $ConfigStorageInstallService
  IfErrors 0 +2
    StrCpy $ConfigStorageInstallService    "$OptService"
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigStorageStartService" $ConfigStorageStartService
  IfErrors 0 +2
    StrCpy $ConfigStorageStartService      "$OptStart"
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigDirectorName" $ConfigDirectorName
  IfErrors 0 +2
    StrCpy $ConfigDirectorName            "$HostName-dir"
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigDirectorPort" $ConfigDirectorPort
  IfErrors 0 +2
    StrCpy $ConfigDirectorPort             9101
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigDirectorMaxJobs" $ConfigDirectorMaxJobs
  IfErrors 0 +2
    StrCpy $ConfigDirectorMaxJobs          1
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigDirectorPassword" $ConfigDirectorPassword
  IfErrors 0 +2
    StrCpy $ConfigDirectorPassword         ""
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigDirectorDB" $ConfigDirectorDB
  IfErrors 0 +2
    StrCpy $ConfigDirectorDB               0
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigDirectorInstallService" $ConfigDirectorInstallService
  IfErrors 0 +2
    StrCpy $ConfigDirectorInstallService   "$OptService"
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigDirectorStartService" $ConfigDirectorStartService
  IfErrors 0 +2
    StrCpy $ConfigDirectorStartService     "$OptStart"
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigMonitorName" $ConfigMonitorName
  IfErrors 0 +2
    StrCpy $ConfigMonitorName              "$HostName-mon"
  ClearErrors
  ${GetOptions} $CMDLINE "-ConfigMonitorPassword" $ConfigMonitorPassword
  IfErrors 0 +2
    StrCpy $ConfigMonitorPassword          ""
  ClearErrors
  IfSilent 0 +2
    Call SilentSelectComponents

; PLUGINSDIR refers to temporary helper programs and not Bacula plugins!
  InitPluginsDir
  File "/oname=$PLUGINSDIR\openssl.exe"  "${SRC_DIR}\openssl.exe"
  File "/oname=$PLUGINSDIR\libssl-1_1-x64.dll" "${SRC_DIR}\libssl-1_1-x64.dll"
  File "/oname=$PLUGINSDIR\libcrypto-1_1-x64.dll" "${SRC_DIR}\libcrypto-1_1-x64.dll"
  File "/oname=$PLUGINSDIR\sed.exe"      "${SRC_DIR}\sed.exe"

  !InsertMacro MUI_INSTALLOPTIONS_EXTRACT "InstallType.ini"
  !InsertMacro MUI_INSTALLOPTIONS_EXTRACT "WriteTemplates.ini"

  SetPluginUnload alwaysoff

; Generate random File daemon password
  ${If} $ConfigClientPassword == ""
    nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
    pop $R0
    ${If} $R0 = 0
    FileOpen $R1 "$PLUGINSDIR\pw.txt" r
    IfErrors +4
      FileRead $R1 $R0
      ${StrTrimNewLines} $ConfigClientPassword $R0
      FileClose $R1
    ${EndIf}
  ${EndIf}

; Generate random Storage daemon password
  ${If} $ConfigStoragePassword == ""
    nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
    pop $R0
    ${If} $R0 = 0
    FileOpen $R1 "$PLUGINSDIR\pw.txt" r
    IfErrors +4
      FileRead $R1 $R0
      ${StrTrimNewLines} $ConfigStoragePassword $R0
      FileClose $R1
    ${EndIf}
  ${EndIf}
  SetPluginUnload manual

; Generate random monitor password
  ${If} $ConfigMonitorPassword == ""
    nsExec::Exec '"$PLUGINSDIR\openssl.exe" rand -base64 -out $PLUGINSDIR\pw.txt 33'
    pop $R0
    ${If} $R0 = 0
    FileOpen $R1 "$PLUGINSDIR\pw.txt" r
    IfErrors +4
      FileRead $R1 $R0
      ${StrTrimNewLines} $ConfigMonitorPassword $R0
      FileClose $R1
    ${EndIf}
  ${EndIf}

  Pop $R1
  Pop $R0
FunctionEnd

Function .onSelChange
  Call UpdateComponentUI
FunctionEnd

Function InstallCommonFiles
  ${If} $CommonFilesDone = 0
    SetOutPath "$INSTDIR"
    File "Readme.txt"

    SetOutPath "$INSTDIR"
!if "${BUILD_TOOLS}" == "MinGW32"
    File "${SRC_DIR}\mingwm10.dll"
!endif
    File "${SRC_DIR}\libwinpthread-1.dll"
#    File "${SRC_DIR}\libgcc_s_dw2-1.dll"
    File "${SRC_DIR}\libgcc_s_seh-1.dll"
    File "${SRC_DIR}\libstdc++-6.dll"
    File "${SRC_DIR}\libssl-1_1-x64.dll"
    File "${SRC_DIR}\libcrypto-1_1-x64.dll"
    File "${SRC_DIR}\zlib1.dll"
    File "${SRC_DIR}\bacula.dll"

    File "/oname=$INSTDIR\openssl.cnf" "${SRC_DIR}\openssl.cnf"
    File "${SRC_DIR}\openssl.exe"
    File "${SRC_DIR}\bsleep.exe"
    File "${SRC_DIR}\bsmtp.exe"
    File "${SRC_DIR}\expr64.exe"
    File "${SRC_DIR}\snooze.exe"

    CreateShortCut "$SMPROGRAMS\Bacula\Documentation\View Readme.lnk" "write.exe" '"$INSTDIR\Readme.txt"'

    StrCpy $CommonFilesDone 1
  ${EndIf}
FunctionEnd

Section "-Initialize"

  WriteRegStr   HKLM Software\Bacula InstallLocation "$INSTDIR"

  Call GetSelectedComponents
  Pop $R2
  WriteRegDWORD HKLM Software\Bacula Components $R2

  ; remove start menu items
  SetShellVarContext all

  Delete /REBOOTOK "$SMPROGRAMS\Bacula\Configuration\*"
  Delete /REBOOTOK "$SMPROGRAMS\Bacula\Documentation\*"
  Delete /REBOOTOK "$SMPROGRAMS\Bacula\*"
  RMDir "$SMPROGRAMS\Bacula\Configuration"
  RMDir "$SMPROGRAMS\Bacula\Documentation"
  RMDir "$SMPROGRAMS\Bacula"
  CreateDirectory "$SMPROGRAMS\Bacula"
  CreateDirectory "$SMPROGRAMS\Bacula\Configuration"
  CreateDirectory "$SMPROGRAMS\Bacula\Documentation"

  CreateDirectory "$INSTDIR"
  CreateDirectory "$INSTDIR\working"
  CreateDirectory "$INSTDIR\plugins"

  SetOutPath "$INSTDIR"
  File "${SRC_DIR}\LICENSE"
  Delete /REBOOTOK "$INSTDIR\License.txt"

; Output a series of SED commands to configure the .conf file(s)
  FileOpen $R1 $PLUGINSDIR\config.sed w
  FileWrite $R1 "s;@VERSION@;${VERSION};g$\r$\n"
  FileWrite $R1 "s;@DATE@;${__DATE__};g$\r$\n"
  FileWrite $R1 "s;@DISTNAME@;Windows;g$\r$\n"

  StrCpy $R2 ${BUILD_TOOLS}

  Call GetHostName
  Exch $R3
  Pop $R3

  FileWrite $R1 "s;@DISTVER@;$R2;g$\r$\n"

  ${StrRep} $R2 "$INSTDIR\working" "\" "\\\\"
  FileWrite $R1 's;@working_dir@;$R2;g$\r$\n'
;  ${StrRep} $R2 "$INSTDIR\working" "\" "\\"
;  FileWrite $R1 's;@working_dir_cmd@;$R2;g$\r$\n'

  ${StrRep} $R2 "$INSTDIR\plugins" "\" "\\\\"
  FileWrite $R1 's;@fdplugins_dir@;$R2;g$\r$\n'

  ${StrRep} $R2 "$INSTDIR" "\" "/"
  FileWrite $R1 "s;@BUILD_DIR@;$R2;g$\r$\n"

  FileWrite $R1 "s;@client_address@;$LocalHostAddress;g$\r$\n"
  FileWrite $R1 "s;@client_name@;$ConfigClientName;g$\r$\n"
  FileWrite $R1 "s;@client_port@;$ConfigClientPort;g$\r$\n"
  FileWrite $R1 "s;@client_maxjobs@;$ConfigClientMaxJobs;g$\r$\n"
  FileWrite $R1 "s;@client_password@;$ConfigClientPassword;g$\r$\n"
  FileWrite $R1 "s;@storage_address@;$LocalHostAddress;g$\r$\n"
  FileWrite $R1 "s;@storage_name@;$ConfigStorageName;g$\r$\n"
  FileWrite $R1 "s;@storage_port@;$ConfigStoragePort;g$\r$\n"
  FileWrite $R1 "s;@storage_maxjobs@;$ConfigStorageMaxJobs;g$\r$\n"
  FileWrite $R1 "s;@storage_password@;$ConfigStoragePassword;g$\r$\n"
  FileWrite $R1 "s;@director_name@;$ConfigDirectorName;g$\r$\n"
  FileWrite $R1 "s;@director_port@;$ConfigDirectorPort;g$\r$\n"
  FileWrite $R1 "s;@director_password@;$ConfigDirectorPassword;g$\r$\n"
  FileWrite $R1 "s;@director_address@;$ConfigDirectorAddress;g$\r$\n"
  FileWrite $R1 "s;@monitor_name@;$ConfigMonitorName;g$\r$\n"
  FileWrite $R1 "s;@monitor_password@;$ConfigMonitorPassword;g$\r$\n"

  FileClose $R1

  ${If} ${FileExists} "$OldInstallDir\bacula-fd.exe"
    nsExec::ExecToLog '"$OldInstallDir\bacula-fd.exe" /kill'     ; Shutdown any bacula that could be running
    nsExec::ExecToLog '"$OldInstallDir\bacula-fd.exe" /kill'     ; Shutdown any bacula that could be running
    Sleep 1000
    nsExec::ExecToLog '"$OldInstallDir\bacula-fd.exe" /remove'   ; Remove existing service
  ${EndIf}

  ${If} ${FileExists} "$INSTDIR\bacula-fd.exe"
    nsExec::ExecToLog '"$INSTDIR\bacula-fd.exe" /kill'     ; Shutdown any bacula that could be running
    nsExec::Exec /TIMEOUT=200 'net stop bacula-fd'
  ${EndIf}

  ${If} ${FileExists} "$OldInstallDir\bin\bacula-sd.exe"
    nsExec::ExecToLog '"$OldInstallDir\bin\bacula-sd.exe" /kill'     ; Shutdown any bacula that could be running
    nsExec::ExecToLog '"$OldInstallDir\bin\bacula-sd.exe" /kill'     ; Shutdown any bacula that could be running
    Sleep 1000
    nsExec::ExecToLog '"$OldInstallDir\bin\bacula-sd.exe" /remove'   ; Remove existing service
  ${EndIf}

  ${If} ${FileExists} "$INSTDIR\bacula-sd.exe"
    nsExec::ExecToLog '"$INSTDIR\bacula-sd.exe" /kill'     ; Shutdown any bacula that could be running
    nsExec::Exec /TIMEOUT=200 'net stop bacula-sd'
  ${EndIf}
  Sleep 1000


SectionEnd

SectionGroup "Client" SecGroupClient

Section "File Service" SecFileDaemon
  SectionIn 1 2 3

  SetOutPath "$INSTDIR"

  File "${SRC_DIR}\bacula-fd.exe"

  File "/oname=$PLUGINSDIR\bacula-fd.conf" "bacula-fd.conf.in"

  StrCpy $0 "$INSTDIR"
  StrCpy $1 bacula-fd.conf
  Call ConfigEditAndCopy

  StrCpy $0 bacula-fd
  StrCpy $1 "File Service"
  StrCpy $2 $ConfigClientInstallService
  StrCpy $3 $ConfigClientStartService

  Call InstallDaemon

  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Client Configuration.lnk" "write.exe" '"$INSTDIR\bacula-fd.conf"'
SectionEnd

SectionGroupEnd

SectionGroup "Server" SecGroupServer

Section "Storage Service" SecStorageDaemon
  SectionIn 2 3

  SetOutPath "$INSTDIR"

  File "${SRC_DIR}\bacula-sd.exe"
  File "${SRC_DIR}\bcopy.exe"
  File "${SRC_DIR}\bextract.exe"
  File "${SRC_DIR}\bls.exe"

  File "/oname=$PLUGINSDIR\bacula-sd.conf" "bacula-sd.conf.in"

  StrCpy $0 "$INSTDIR"
  StrCpy $1 bacula-sd.conf
  Call ConfigEditAndCopy

# File "${SRC_DIR}\loaderinfo.exe"
# File "${SRC_DIR}\mt.exe"
# File "${SRC_DIR}\mtx.exe"
# File "${SRC_DIR}\scsitape.exe"
# File "${SRC_DIR}\tapeinfo.exe"
# File "${SRC_DIR}\bscan.exe"
# File "${SRC_DIR}\btape.exe"
# File "${SRC_DIR}\scsilist.exe"
# File "${SRC_DIR}\mkisofs.exe"
# File "${SRC_DIR}\growisofs.exe"

#  File "/oname=$PLUGINSDIR\mtx-changer.cmd" "scripts\mtx-changer.cmd"

#  StrCpy $0 "$INSTDIR\bin"
#  StrCpy $1 mtx-changer.cmd
#  Call ConfigEditAndCopy

#  File "/oname=$PLUGINSDIR\disk-changer.cmd" "scripts\disk-changer.cmd"

#  StrCpy $0 "$INSTDIR"
#  StrCpy $1 disk-changer.cmd
#  Call ConfigEditAndCopy

  StrCpy $0 bacula-sd
  StrCpy $1 "Storage Service"
  StrCpy $2 $ConfigStorageInstallService
  StrCpy $3 $ConfigStorageStartService
  Call InstallDaemon

#  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\List Devices.lnk" "$INSTDIR\bin\scsilist.exe" "/pause"
  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Storage Configuration.lnk" "write.exe" '"$INSTDIR\bacula-sd.conf"'
SectionEnd

SectionGroupEnd


SectionGroup "Consoles" SecGroupConsoles

Section "Command Console" SecConsole
  SectionIn 1 2 3

  SetOutPath "$INSTDIR"

  File "${SRC_DIR}\bconsole.exe"
  Call InstallCommonFiles

  File "/oname=$PLUGINSDIR\bconsole.conf" "bconsole.conf.in"
  StrCpy $0 "$INSTDIR"
  StrCpy $1 bconsole.conf
  Call ConfigEditAndCopy

  CreateShortCut "$SMPROGRAMS\Bacula\bconsole.lnk" "$INSTDIR\bconsole.exe" '-c "$INSTDIR\bconsole.conf"' "$INSTDIR\bconsole.exe" 0
  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Command Console Configuration.lnk" "write.exe" '"$INSTDIR\bconsole.conf"'

SectionEnd

Section "Bat Console" SecBatConsole
  SectionIn 1 2 3

  SetOutPath "$INSTDIR"

!if "${BUILD_BAT}" == "yes"
  Call InstallCommonFiles
  File "${SRC64_DIR}\Qt5Core.dll"
  File "${SRC64_DIR}\Qt5Gui.dll"
  File "${SRC64_DIR}\Qt5Network.dll"
  File "${SRC64_DIR}\Qt5Widgets.dll"
  File "${SRC64_DIR}\libwinpthread-1.dll"
#  File "${SRC64_DIR}\libgcc_s_dw2-1.dll"
  File "${SRC64_DIR}\libgcc_s_seh-1.dll"
  File "${SRC64_DIR}\libssl-1_1-x64.dll"
  File "${SRC64_DIR}\libcrypto-1_1-x64.dll"
  File "${SRC64_DIR}\bat.exe"
  File "/oname=$INSTDIR\bacula.dll" "${SRC64_DIR}\bacula.dll"
  File "/oname=$INSTDIR\zlib1.dll" "${SRC64_DIR}\zlib1.dll"

  File "/oname=$PLUGINSDIR\bat.conf" "bat.conf.in"
  StrCpy $0 "$INSTDIR"
  StrCpy $1 bat.conf
  Call ConfigEditAndCopy

  SetOutPath "$INSTDIR\platforms"
  File "${SRC64_DIR}\qwindows.dll"
  SetOutPath "$INSTDIR"

  SetOutPath "$INSTDIR\help"
  File "${SRC64_DIR}\help\*"
  SetOutPath "$INSTDIR"

  ; Create Start Menu entry
  CreateShortCut "$SMPROGRAMS\Bacula\Bat.lnk" "$INSTDIR\bat.exe" '-c "$INSTDIR\bat.conf"' "$INSTDIR\bat.exe" 0
  CreateShortCut "$SMPROGRAMS\Bacula\Configuration\Edit Bat Configuration.lnk" "write.exe" '"$INSTDIR\bat.conf"'
  SetOutPath "$INSTDIR"
!endif

SectionEnd

Section "Tray Monitor" SecTrayMonitor
  SectionIn 1 2 3

  SetOutPath "$INSTDIR"

!if "${BUILD_BAT}" == "yes"
  Call InstallCommonFiles
  File "${SRC64_DIR}\Qt5Core.dll"
  File "${SRC64_DIR}\Qt5Gui.dll"
  File "${SRC64_DIR}\Qt5Network.dll"
  File "${SRC64_DIR}\Qt5Widgets.dll"
  File "${SRC64_DIR}\libssl-1_1-x64.dll"
  File "${SRC64_DIR}\libcrypto-1_1-x64.dll"
  File "${SRC64_DIR}\libwinpthread-1.dll"
#  File "${SRC64_DIR}\libgcc_s_dw2-1.dll"
  File "${SRC64_DIR}\libgcc_s_seh-1.dll"
  File "${SRC64_DIR}\bacula-tray-monitor.exe"
  File "/oname=$INSTDIR\bacula.dll" "${SRC64_DIR}\bacula.dll"
  File "/oname=$INSTDIR\zlib1.dll" "${SRC64_DIR}\zlib1.dll"

  SetOutPath "$INSTDIR\platforms"
  File "${SRC64_DIR}\qwindows.dll"
  SetOutPath "$INSTDIR"

  File "/oname=$PLUGINSDIR\bacula-tray-monitor.conf" "bacula-tray-monitor.conf.in"
  StrCpy $0 "$INSTDIR"
  StrCpy $1 bacula-tray-monitor.conf
  Call ConfigEditAndCopy

  ; Create Start Menu entry
  CreateShortCut "$SMPROGRAMS\Bacula\TrayMonitor.lnk" "$INSTDIR\bacula-tray-monitor.exe" "" "$INSTDIR\bacula-tray-monitor.exe" 0
  SetOutPath "$INSTDIR"
!endif

SectionEnd

; Deleted because wxconsole is deprecated
;Section "Graphical Console" SecWxConsole
;  SectionIn 1 2 3
  
;  SetOutPath "$INSTDIR"
;
;SectionEnd

SectionGroupEnd


SectionGroup "Plugins" SecGroupPlugins

Section "alldrives Plugin" SecAllDrivesPlugin
  SectionIn 1 2 3
 
  SetOutPath "$INSTDIR\plugins"
  File "${SRC_DIR}\alldrives-fd.dll"
  SetOutPath "$INSTDIR" 
SectionEnd


Section /o "CDP Plugin" SecCDPPlugin
  SectionIn 1 2 3

  SetOutPath "$INSTDIR\plugins"
  File "${SRC_DIR}\cdp-fd.dll"
  SetOutPath "$INSTDIR"
  File "${SRC_DIR}\cdp-client.exe"

SectionEnd

SectionGroupEnd


Section "-Finish"
  Push $R0

  ${If} $OsIsNT = 1
    nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$INSTDIR\bacula-fd.conf" /G SYSTEM:F Administrators:F'
    nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$INSTDIR\bacula-sd.conf" /G SYSTEM:F Administrators:F'
    nsExec::ExecToLog 'cmd.exe /C echo Y|cacls "$INSTDIR\bat.conf" /G SYSTEM:F Administrators:F'
  ${EndIf}

  ; Write the uninstall keys for Windows & create Start Menu entry
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "DisplayName" "Bacula"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "DisplayVersion" "${VERSION}"
  ${StrTok} $R0 "${VERSION}" "." 0 0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "VersionMajor" $R0
  ${StrTok} $R0 "${VERSION}" "." 1 0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "VersionMinor" $R0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "NoRepair" 1
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "URLUpdateInfo" "http://www.bacula.org"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "URLInfoAbout" "http://www.bacula.org"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "HelpLink" "http://www.bacula.org?page=support"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortCut "$SMPROGRAMS\Bacula\Uninstall Bacula.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0

  ${If} $OsIsNT = 1
     nsExec::ExecToLog 'net start bacula-fd'
     nsExec::ExecToLog 'net start bacula-sd'
  ${Else}
     Exec '"$INSTDIR\bacula-fd.exe" /service -c "$INSTDIR\bacula-fd.conf"'
     Exec '"$INSTDIR\bacula-sd.exe" /service -c "$INSTDIR\bacula-sd.conf"'
  ${EndIf}

  Pop $R0
SectionEnd

; Extra Page descriptions

LangString DESC_SecFileDaemon ${LANG_ENGLISH} "Install Bacula 64 bit File Daemon on this system."
LangString DESC_SecStorageDaemon ${LANG_ENGLISH} "Install Bacula 64 bit Storage Daemon on this system."
LangString DESC_SecConsole ${LANG_ENGLISH} "Install command console program on this system."
LangString DESC_SecBatConsole ${LANG_ENGLISH} "Install Bat graphical console program on this system."
LangString DESC_SecTrayMonitor ${LANG_ENGLISH} "Install Tray Monitor graphical program on this system."
LangString DESC_SecAllDrivesPlugin ${LANG_ENGLISH} "Install alldrives Plugin on this system."
LangString DESC_SecCDPPlugin ${LANG_ENGLISH} "Install CDP Plugin and CDP client on this system."



LangString TITLE_ConfigPage1 ${LANG_ENGLISH} "Configuration"
LangString SUBTITLE_ConfigPage1 ${LANG_ENGLISH} "Set installation configuration."

LangString TITLE_ConfigPage2 ${LANG_ENGLISH} "Configuration (continued)"
LangString SUBTITLE_ConfigPage2 ${LANG_ENGLISH} "Set installation configuration."

LangString TITLE_InstallType ${LANG_ENGLISH} "Installation Type"
LangString SUBTITLE_InstallType ${LANG_ENGLISH} "Choose installation type."

LangString TITLE_WriteTemplates ${LANG_ENGLISH} "Create Templates"
LangString SUBTITLE_WriteTemplates ${LANG_ENGLISH} "Create a resource template for inclusion in the Director's configuration file."

!InsertMacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecFileDaemon} $(DESC_SecFileDaemon)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecStorageDaemon} $(DESC_SecStorageDaemon)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecConsole} $(DESC_SecConsole)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecBatConsole} $(DESC_SecBatConsole)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecAllDrivesPlugin} $(DESC_SecAllDrivesPlugin)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecTrayMonitor} $(DESC_SecTrayMonitor)
  !InsertMacro MUI_DESCRIPTION_TEXT ${SecCDPPlugin} $(DESC_SecCDPPlugin)
!InsertMacro MUI_FUNCTION_DESCRIPTION_END

; Uninstall section

UninstallText "This will uninstall Bacula. Click Uninstall to continue."

Section "Uninstall"
  ; Shutdown any baculum that could be running
  nsExec::ExecToLog '"$INSTDIR\bacula-fd.exe" /kill'
  nsExec::Exec /TIMEOUT=200 'net stop bacula-fd'
  nsExec::ExecToLog '"$INSTDIR\bacula-sd.exe" /kill'
  nsExec::Exec /TIMEOUT=200 'net stop bacula-sd'
  Sleep 1000

; ReadRegDWORD $R0 HKLM "Software\Bacula" "Service_Bacula-fd"
  ; Remove Bacula File Daemon service
  nsExec::ExecToLog '"$INSTDIR\bacula-fd.exe" /remove'

; Remove Bacula Storage Daemon service
  nsExec::ExecToLog '"$INSTDIR\bacula-sd.exe" /remove'

  ; remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bacula"
  DeleteRegKey HKLM "Software\Bacula"

  ; remove start menu items
  SetShellVarContext all
  Delete /REBOOTOK "$SMPROGRAMS\Bacula\*"
  RMDir /REBOOTOK "$SMPROGRAMS\Bacula"

  ; remove files and uninstaller (preserving config for now)
  Delete /REBOOTOK "$INSTDIR\doc\*"
  Delete /REBOOTOK "$INSTDIR\help\*"
  Delete /REBOOTOK "$PLUGINSDIR\pw.txt"
  Delete /REBOOTOK "$PLUGINSDIR\*.sed"
  Delete /REBOOTOK "$PLUGINSDIR\*.cmd"
  Delete /REBOOTOK "$PLUGINSDIR\*.sql"
  Delete /REBOOTOK "$INSTDIR\openssl.exe"
  Delete /REBOOTOK "$INSTDIR\bacula-fd.exe"
  Delete /REBOOTOK "$INSTDIR\bat.exe"
  Delete /REBOOTOK "$INSTDIR\bacula-tray-monitor.exe"
  Delete /REBOOTOK "$INSTDIR\bsleep.exe"
  Delete /REBOOTOK "$INSTDIR\bsmtp.exe"
  Delete /REBOOTOK "$INSTDIR\bconsole.exe"
  Delete /REBOOTOK "$INSTDIR\expr64.exe"
  Delete /REBOOTOK "$INSTDIR\snooze.exe"
  Delete /REBOOTOK "$INSTDIR\Uninstall.exe"
  Delete /REBOOTOK "$INSTDIR\LICENSE"
  Delete /REBOOTOK "$INSTDIR\Readme.txt"
  Delete /REBOOTOK "$INSTDIR\*.dll"
  Delete /REBOOTOK "$INSTDIR\plugins\*.dll"

  ; Check for existing installation
  IfSilent +2 0
    MessageBox MB_YESNO|MB_ICONQUESTION "Would you like to delete the current configuration files and the working state file?" /SD IDNO IDNO NoDel

    Delete /REBOOTOK "$INSTDIR\*"
    Delete /REBOOTOK "$INSTDIR\working\*"
    Delete /REBOOTOK "$INSTDIR\bat.conf"
    Delete /REBOOTOK "$PLUGINSDIR\bacula-*.conf"
    Delete /REBOOTOK "$PLUGINSDIR\*console.conf"
    Delete /REBOOTOK "$PLUGINSDIR\*conf.in"
    RMDir /REBOOTOK "$INSTDIR\plugins"
    RMDir /REBOOTOK "$INSTDIR\working"
    RMDir /REBOOTOK "$INSTDIR"
NoDel:
  ; remove directories used
  RMDir "$INSTDIR\plugins"
  RMDir "$INSTDIR\working"
  RMDir "$INSTDIR\doc"
  RMDir "$INSTDIR\help"
  RMDir "$INSTDIR"
SectionEnd

;
; $0 - Service Name (ie Bacula-FD)
; $1 - Service Description (ie Bacula File Daemon)
; $2 - Install as Service
; $3 - Start Service now
;
Function InstallDaemon
  Call InstallCommonFiles

  WriteRegDWORD HKLM "Software\Bacula" "Service_$0" $2
  
  ${If} $2 = 1
    nsExec::ExecToLog '"$INSTDIR\bacula-fd.exe" /kill'
    nsExec::Exec /TIMEOUT=200 'net stop bacula-fd'
    nsExec::ExecToLog '"$INSTDIR\bacula-sd.exe" /kill'
    nsExec::Exec /TIMEOUT=200 'net stop bacula-sd'
    nsExec::ExecToLog '"$INSTDIR\$0.exe" /remove'
    nsExec::ExecToLog '"$INSTDIR\$0.exe" /install -c "$INSTDIR\$0.conf"'

    ${If} $OsIsNT <> 1
      File "Start.bat"
      File "Stop.bat"
    ${EndIf}

  ${EndIf}

FunctionEnd

Function GetComputerName
  Push $R0
  Push $R1
  Push $R2

  System::Call "kernel32::GetComputerNameA(t .R0, *i ${NSIS_MAX_STRLEN} R1) i.R2"

  ${StrCase} $R0 $R0 "L"

  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

!define ComputerNameDnsFullyQualified   3

Function GetHostName
  Push $R0
  Push $R1
  Push $R2

  ${If} $OsIsNT = 1
    System::Call "kernel32::GetComputerNameExA(i ${ComputerNameDnsFullyQualified}, t .R0, *i ${NSIS_MAX_STRLEN} R1) i.R2 ?e"
    ${If} $R2 = 0
      Pop $R2
      DetailPrint "GetComputerNameExA failed - LastError = $R2"
      Call GetComputerName
      Pop $R0
    ${Else}
      Pop $R2
    ${EndIf}
  ${Else}
    Call GetComputerName
    Pop $R0
  ${EndIf}

  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

!define NameUserPrincipal 8

Function GetUserName
  Push $R0
  Push $R1
  Push $R2

  ${If} $OsIsNT = 1
    System::Call "secur32::GetUserNameExA(i ${NameUserPrincipal}, t .R0, *i ${NSIS_MAX_STRLEN} R1) i.R2 ?e"
    ${If} $R2 = 0
      Pop $R2
      DetailPrint "GetUserNameExA failed - LastError = $R2"
      Pop $R0
      StrCpy $R0 ""
    ${Else}
      Pop $R2
    ${EndIf}
  ${Else}
      StrCpy $R0 ""
  ${EndIf}

  ${If} $R0 == ""
    System::Call "advapi32::GetUserNameA(t .R0, *i ${NSIS_MAX_STRLEN} R1) i.R2 ?e"
    ${If} $R2 = 0
      Pop $R2
      DetailPrint "GetUserNameA failed - LastError = $R2"
      StrCpy $R0 ""
    ${Else}
      Pop $R2
    ${EndIf}
  ${EndIf}

  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

Function ConfigEditAndCopy
  Push $R1

  ${If} ${FileExists} "$0\$1"
    StrCpy $R1 ".new"
  ${Else}
    StrCpy $R1 ""
  ${EndIf}

  nsExec::ExecToLog '$PLUGINSDIR\sed.exe -f "$PLUGINSDIR\config.sed" -i.bak "$PLUGINSDIR\$1"'
  CopyFiles "$PLUGINSDIR\$1" "$0\$1$R1"

  Pop $R1
FunctionEnd

Function GetSelectedComponents
  Push $R0
  StrCpy $R0 0
  ${If} ${SectionIsSelected} ${SecFileDaemon}
    IntOp $R0 $R0 | ${ComponentFile}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecStorageDaemon}
    IntOp $R0 $R0 | ${ComponentStorage}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecConsole}
    IntOp $R0 $R0 | ${ComponentTextConsole}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecBatConsole}
    IntOp $R0 $R0 | ${ComponentBatConsole}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecTrayMonitor}
    IntOp $R0 $R0 | ${ComponentTrayMonitor}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecAllDrivesPlugin}
     IntOp $R0 $R0 | ${ComponentAllDrivesPlugin}
  ${EndIf}
  ${If} ${SectionIsSelected} ${SecCDPPlugin}
    IntOp $R0 $R0 | ${ComponentCDPPlugin}
  ${EndIf}
  Exch $R0
FunctionEnd

Function PageComponentsShow
  Call SelectPreviousComponents
  Call UpdateComponentUI
FunctionEnd

Function PageDirectoryPre
  ${If} $AutomaticInstall = 1
  ${OrIf} $InstallType = ${UpgradeInstall}
    Abort
  ${EndIf}
FunctionEnd

Function LeaveInstallPage
  Push "$INSTDIR\install.log"
  Call DumpLog
FunctionEnd

; in silent mode, we manually select or deselect modules, depending on cmdline parameter
Function SilentSelectComponents
  ; by default fd and plugins are selected
  !InsertMacro SelectSection ${SecFileDaemon}
  !InsertMacro SetSectionFlag ${SecFileDaemon} ${SF_RO}
  !InsertMacro SelectSection ${SecCDPPlugin}
  !InsertMacro SetSectionFlag ${SecCDPPlugin} ${SF_RO}
  ClearErrors
  ${GetOptions} $CMDLINE "-ComponentStorage" $TmpComponent
  ${IfNot} ${Errors}
    !InsertMacro SelectSection ${SecStorageDaemon}
    !InsertMacro SetSectionFlag ${SecStorageDaemon} ${SF_RO}
  ${Else}
    !InsertMacro UnselectSection ${SecStorageDaemon}
    !InsertMacro ClearSectionFlag ${SecStorageDaemon} ${SF_RO}
  ${EndIf}
  ClearErrors
  ${GetOptions} $CMDLINE "-ComponentTextConsole" $TmpComponent
  ${IfNot} ${Errors}
    !InsertMacro SelectSection ${SecConsole}
    !InsertMacro SetSectionFlag ${SecConsole} ${SF_RO}
  ${Else}
    !InsertMacro UnselectSection ${SecConsole}
    !InsertMacro ClearSectionFlag ${SecConsole} ${SF_RO}
  ${EndIf}
  ClearErrors
  ${GetOptions} $CMDLINE "-ComponentBatConsole" $TmpComponent
  ${IfNot} ${Errors}
    !InsertMacro SelectSection ${SecBatConsole}
    !InsertMacro SetSectionFlag ${SecBatConsole} ${SF_RO}
  ${Else}
    !InsertMacro UnselectSection ${SecBatConsole}
    !InsertMacro ClearSectionFlag ${SecBatConsole} ${SF_RO}
  ${EndIf}
  ClearErrors
  ${GetOptions} $CMDLINE "-ComponentTrayMonitor" $TmpComponent
  ${IfNot} ${Errors}
    !InsertMacro SelectSection ${SecTrayMonitor}
    !InsertMacro SetSectionFlag ${SecTrayMonitor} ${SF_RO}
  ${Else}
    !InsertMacro UnselectSection ${SecTrayMonitor}
    !InsertMacro ClearSectionFlag ${SecTrayMonitor} ${SF_RO}
  ${EndIf}
  ClearErrors
FunctionEnd

Function SelectPreviousComponents
  ${If} $InstallType <> ${NewInstall}
    IntOp $R1 $PreviousComponents & ${ComponentFile}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecFileDaemon}
      !InsertMacro SetSectionFlag ${SecFileDaemon} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecFileDaemon}
      !InsertMacro ClearSectionFlag ${SecFileDaemon} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentStorage}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecStorageDaemon}
      !InsertMacro SetSectionFlag ${SecStorageDaemon} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecStorageDaemon}
      !InsertMacro ClearSectionFlag ${SecStorageDaemon} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentTextConsole}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecConsole}
      !InsertMacro SetSectionFlag ${SecConsole} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecConsole}
      !InsertMacro ClearSectionFlag ${SecConsole} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentBatConsole}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecBatConsole}
      !InsertMacro SetSectionFlag ${SecBatConsole} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecBatConsole}
      !InsertMacro ClearSectionFlag ${SecBatConsole} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentTrayMonitor}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecTrayMonitor}
      !InsertMacro SetSectionFlag ${SecTrayMonitor} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecTrayMonitor}
      !InsertMacro ClearSectionFlag ${SecTrayMonitor} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentAllDrivesPlugin}
    ${If} $R1 <> 0
       !InsertMacro SelectSection ${SecAllDrivesPlugin}
       !InsertMacro SetSectionFlag ${SecAllDrivesPlugin} ${SF_RO}
     ${Else}
       !InsertMacro UnselectSection ${SecAllDrivesPlugin}
       !InsertMacro ClearSectionFlag ${SecAllDrivesPlugin} ${SF_RO}
    ${EndIf}
    IntOp $R1 $PreviousComponents & ${ComponentCDPPlugin}
    ${If} $R1 <> 0
      !InsertMacro SelectSection ${SecCDPPlugin}
      !InsertMacro SetSectionFlag ${SecCDPPlugin} ${SF_RO}
    ${Else}
      !InsertMacro UnselectSection ${SecCDPPlugin}
      !InsertMacro ClearSectionFlag ${SecCDPPlugin} ${SF_RO}
    ${EndIf}
  ${EndIf}
FunctionEnd

Function UpdateComponentUI
  Push $R0
  Push $R1

  Call GetSelectedComponents
  Pop $R0

  IntOp $R1 $R0 ^ $PreviousComponents
  IntOp $NewComponents $R0 & $R1

  ${If} $InstallType <> ${NewInstall}
    IntOp $R1 $NewComponents & ${ComponentFile}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecFileDaemon} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecFileDaemon} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentStorage}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecStorageDaemon} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecStorageDaemon} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentTextConsole}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecConsole} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecConsole} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentBatConsole}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecBatConsole} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecBatConsole} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentTrayMonitor}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecTrayMonitor} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecTrayMonitor} ${SF_BOLD}
    ${EndIf}
    IntOp $R1 $NewComponents & ${ComponentCDPPlugin}
    ${If} $R1 <> 0
      !InsertMacro SetSectionFlag ${SecCDPPlugin} ${SF_BOLD}
    ${Else}
      !InsertMacro ClearSectionFlag ${SecCDPPlugin} ${SF_BOLD}
    ${EndIf}
  ${EndIf}

  GetDlgItem $R0 $HWNDPARENT 1

  IntOp $R1 $NewComponents & ${ComponentsRequiringUserConfig}
  ${If} $R1 = 0
    SendMessage $R0 ${WM_SETTEXT} 0 "STR:Install"
  ${Else}
    SendMessage $R0 ${WM_SETTEXT} 0 "STR:&Next >"
  ${EndIf}

  Pop $R1
  Pop $R0
FunctionEnd

!include "InstallType.nsh"
!include "ConfigPage1.nsh"
!include "ConfigPage2.nsh"
!include "DumpLog.nsh"
