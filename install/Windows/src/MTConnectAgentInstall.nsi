;
; MTConnectAgent.nsi
;
; This script will install an MTConnect Agent that reads Adapter SHDR. 
; It has uninstall support.
; 

; installer properties
;XPStyle on

; The name of the installer
!define VERSION 1.2

Name "MTConnectAgentFromShdr${VERSION}"
OutFile "MTConnectAgentInstall${VERSION}.exe"

!include MUI2.nsh
!include x64.nsh
!include Sections.nsh
!include "logiclib.nsh"

!include "MTCAgentConfig.nsdinc"
!include NSISList.nsh

ReserveFile "${NSISDIR}\Plugins\NSISList.dll"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; CONFIGURATION variables
var ServiceName
var AgentPort
Var AgentDelay
var DebugType
var AgentserviceStart



; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
;InstallDirRegKey HKLM "Software\MTConnectAgent32Bit${VERSION}" "$Install_Dir"

AddBrandingImage right 100
brandingtext "MTConnect"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Images - icon and bmp

; MUI Settings / Icons
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\orange-uninstall.ico"
 
; MUI Settings / Header
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
;!define MUI_HEADERIMAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Header\orange-r.bmp"
;!define MUI_HEADERIMAGE_UNBITMAP "${NSISDIR}\Contrib\Graphics\Header\orange-uninstall-r.bmp"
!define MUI_HEADERIMAGE_BITMAP "mtconnect.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "mtconnect.bmp"

; MUI Settings / Wizard
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange-uninstall.bmp"


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Pages

;Page custom fnc_MTConnectSplash_Show 

;; Welcome Page

!define MUI_WELCOMEPAGE
!define MUI_WELCOMEPAGE_TITLE  "$(MyWelcomeTitle)"
!insertmacro MUI_PAGE_WELCOME

;!define MUI_HEADERIMAGE
;!define MUI_HEADERIMAGE_BITMAP "C:\Users\michalos\Documents\GitHub\MTConnectSolutions\MTConnectAgentFromShdr32Bit\DistributionX64\orange.bmp"
;!define MUI_HEADERIMAGE_RIGHT


;; LicensePage
!define MUI_PAGE_HEADER_TEXT "MTConnect Licensing"
!define MUI_PAGE_HEADER_SUBTEXT  "Please Review"
!insertmacro MUI_PAGE_LICENSE "license.txt"


!define MUI_PAGE_HEADER_TEXT "MTConnect Destination"
!define MUI_DIRECTORYPAGE_TEXT_TOP  "Please Choose MTConnect Agent Destination Folder"
!insertmacro MUI_PAGE_DIRECTORY

  ShowInstDetails show

!define MUI_PAGE_HEADER_TEXT "MTConnect Agent Installation"
!define MUI_INSTFILESPAGE_FINISHHEADER_TEXT "MTConnect Agent Installation Completed!"
!insertmacro MUI_PAGE_INSTFILES


Page custom fnc_MTCAgentConfig_Show AgentConfigPageLeave

; Finish Page
!define MUI_PAGE_CUSTOMFUNCTION_SHOW finish_enter
!insertmacro MUI_PAGE_FINISH

;UninstPage uninstConfirm
;UninstPage instfiles

; Uninstaller Pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH 

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Languate specific
!insertmacro MUI_LANGUAGE English
LangString MyWelcomeTitle ${LANG_ENGLISH} "Welcome"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function .onInit
  ;  The file to write
  ; The default installation directory
  ${If} ${RunningX64}
    ;InstallDir $PROGRAMFILES64\MTConnect\MTConnectAgent
  	StrCpy $INSTDIR "$PROGRAMFILES64\MTConnect\MTConnectAgent"
  ${Else}
     ;InstallDir $PROGRAMFILES32\MTConnect\MTConnectAgent
  	StrCpy $INSTDIR "$PROGRAMFILES32\MTConnect\MTConnectAgent"
  ${EndIf}  
  ${List.Create} AgentCfgList                        ; Create a liste for Agent.cfg output
      
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function finish_enter
  ;; insure back button not enables
  GetDlGItem $0 $HWNDPARENT 3
  EnableWindow $0 0  
FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; The stuff to install

Section "MTConnectAgent (required)"

  SectionIn RO ;Section Index - If you specify RO as a parameter, then the section will be Read-Only, meaning it will always be set to install. 

  ;SetBrandingImage  /RESIZETOFIT "mtconnect.bmp"
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  SetOverwrite on
	; Put files there
	File "Agent.cfg"
	File "Agent.log"
	File "Devices.xml"
	File "Install.bat"
	File "KillAgent.bat"
	File "license.txt"  
	File "MTConnectPage-1.4.vbs"
	File "RunAgent.bat"
	File "SetFilePermission.exe"
	File "SuperUser.bat"
	File "Uninstall.bat"
 	File "Uninstall.vbs"
	File "AdaptersConfiguration.exe"

	${If} ${RunningX64}
	    File "x64\agent.exe"
	     ;File "x64\vc2010redist_x64.exe"
 	     ;ExecWait '$INSTDIR\vc2010redist_x64.exe' $0
	    File "x64\msvcrt.dll"
	    File "x64\msvcr100.dll"
 	${Else}
	     File "x32\agent.exe"
	     ;File "x32\vc2010redist_x86.exe"
  	     ;ExecWait '$INSTDIR\vc2008redist_x86.exe' $0
 	    File "x32\msvcrt.dll"
	    File "x32\msvcr100.dll"
 	${EndIf}    
  	SetOutPath $INSTDIR\Devices
	File  /r  "Devices\*.*"

	ExecWait '"$INSTDIR\SetFilePermission.exe" "$INSTDIR"' $0

  ; Write the installation path into the registry
  ;WriteRegStr HKLM SOFTWARE\NSIS_Example2 "Install_Dir" "$INSTDIR"
  ;ReadINIStr $0 $INSTDIR\winamp.ini winamp outname	
  WriteUninstaller "uninstall.exe"

SectionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MTConnectAgent${VERSION}"
  DeleteRegKey HKLM SOFTWARE\MTConnectAgent${VERSION}
  ExecWait '"sc.exe  stop $ServiceName "'
  ExecWait '"sc.exe  delete $ServiceName "'

   ; Remove files and uninstaller
   Delete "$INSTDIR\Agent.cfg"
   Delete "$INSTDIR\Agent.exe"
   Delete "$INSTDIR\Agent.log"
   Delete "$INSTDIR\Devices.xml"
   Delete "$INSTDIR\Install.bat"
   Delete "$INSTDIR\KillAgent.bat"
   Delete "$INSTDIR\License.txt"
   Delete "$INSTDIR\MTConnectPage-1.4.vbs"
   Delete "$INSTDIR\RunAgent.bat"
   Delete "$INSTDIR\SetFilePermission.exe"
   Delete "$INSTDIR\SuperUser.bat"
   Delete "$INSTDIR\Uninstall.bat"
   Delete "$INSTDIR\Uninstall.vbs"
   Delete "$INSTDIR\AdaptersConfiguration.exe"

   Delete "$INSTDIR\msvcrt.dll"
   Delete "$INSTDIR\msvcr100.dll"
 
   Delete $INSTDIR\uninstall.exe

    RMDir /r "$INSTDIR\Devices"

  ; Remove shortcuts, if any
  ;Delete "$SMPROGRAMS\Example2\*.*"

  ; Remove directories used
  ;RMDir "$SMPROGRAMS\Example2"
  RMDir "$INSTDIR"

SectionEnd
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function AdapterBuilder
   Banner::show /set 76 "AdapterConfiguration" "Please wait"  
     ExecWait '"$INSTDIR\AdaptersConfiguration.exe" /p "$INSTDIR"'
    Banner::destroy

FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function AgentConfigPageLeave

  ;Read Textboxes values
  ${NSD_GetText} $hCtl_MTCAgentConfig_TextBox2  $AgentPort ; port
  ${NSD_GetText} $hCtl_MTCAgentConfig_TextBox4  $ServiceName ; service name
  ${NSD_GetText} $hCtl_MTCAgentConfig_TextBox5  $AgentDelay ; delay in seconds
  ${NSD_LB_GetSelection} $hCtl_MTCAgentConfig_DropList1 $DebugType

  ;; Read Check box
  ${NSD_GetState} $hCtl_MTCAgentConfig_CheckBox2 $AgentserviceStart

  ;; Sample string concatenation
  strcpy $9 "000"
  strcpy $4 "$4$9"

  ;; Sample debugging message box
  ;MessageBox mb_ok $0
  ;MessageBox mb_ok $4


  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$ServiceName${VERSION}" "DisplayName" "MTConnectAgent${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$ServiceName${VERSION}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$ServiceName${VERSION}" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$ServiceName${VERSION}" "NoRepair" 1

  ; No adapters specified, use AgentBuilder instead
  Call AdapterBuilder
  ExecWait '"$INSTDIR\SetFilePermission.exe" "$INSTDIR\Devices.xml"' $0
  Call GenerateAgentCfg
 

FunctionEnd



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function GenerateAgentCfg
   Var /GLOBAL Line

   ClearErrors
   GetTempFileName $R0                            ; get new temp file name
   FileOpen $1 $R0 "w"                            ; open temp file for writing
   FileWrite $1 "ServiceName = $ServiceName$\r$\n"
   FileWrite $1 "Devices = Devices.xml$\r$\n"
   FileWrite $1 "Port = $AgentPort$\r$\n"
   FileWrite $1 "CheckpointFrequency=10000$\r$\n"
   FileWrite $1 "AllowPut=true$\r$\n"
   FileWrite $1 "Adapters $\r$\n"
   FileWrite $1 "{$\r$\n"

   FileOpen $4 "$INSTDIR\Agent.cfg" "r"
 Loop:
   IfErrors done
   FileRead $4 $Line
   IfErrors done
   FileWrite $1 $Line	        
   IfErrors 0 Loop 
 done:
   FileClose $4
   FileWrite $1 "}$\r$\n"
   FileWrite $1 "# Logger Configuration$\r$\n"
   FileWrite $1 "logger_config$\r$\n"
   FileWrite $1 "{$\r$\n"
   FileWrite $1 "	logging_level = $DebugType$\r$\n"
   FileWrite $1 "	output = cout$\r$\n"
   FileWrite $1 "}$\r$\n"

   FileClose $1                                ; close temp file
   Delete "$INSTDIR\Agent.cfg"                 ; delete target file
   CopyFiles /SILENT $R0 "$INSTDIR\Agent.cfg"  ; copy temp file to target file
   Delete $R0       
   ExecWait '"$INSTDIR\SetFilePermission.exe" "$INSTDIR\Agent.cfg"' $0
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function .onGUIEnd

  ;ExecWait 'sc.exe  create $ServiceName start= auto binpath= "$INSTDIR\Agent.exe"'
  ExecWait '"$INSTDIR\Agent.exe" install ' 
  ${If} $AgentserviceStart <> 0
  	;MessageBox mb_ok "sc.exe  start $ServiceName"
  	ExecWait 'sc.exe  start $ServiceName' $0 
  ${EndIf}

  ${List.Unload}     
FunctionEnd