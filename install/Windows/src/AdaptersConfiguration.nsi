;
; AdaptersConfiguration.nsi
;
; This script will install an MTConnect Agent that reads Adapter SHDR. 
; It has uninstall support.
; 

; installer properties
;XPStyle on

; The name of the installer
!define VERSION 1.0

Name "AdaptersConfiguration"
OutFile "AdaptersConfiguration.exe"
!include MUI2.nsh
!include x64.nsh
!include Sections.nsh
!include "logiclib.nsh"
!include FileFunc.nsh
!include NSISList.nsh

ReserveFile "${NSISDIR}\Plugins\NSISList.dll"

;; Custom pages
!include "MTCAdapterInstall.nsdinc"
!include "MTCAdapterSelection.nsdinc"


var AdapterPort
var AdapterName
var AdapterIP
var variable
var AdapterVendor
var i
var MachineType


brandingtext "MTConnect"
RequestExecutionLevel admin

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Images - icon and bmp

; MUI Settings / Icons
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\orange-uninstall.ico"
 
; MUI Settings / Header
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Header\mtconnect.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "${NSISDIR}\Contrib\Graphics\Header\mtconnect.bmp"
; MUI Settings / Wizard
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange-uninstall.bmp"


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Pages

;; Welcome Page
!define MUI_WELCOMEPAGE
!define MUI_WELCOMEPAGE_TITLE  "Welcome to Adapter Configuration"
!insertmacro MUI_PAGE_WELCOME

!define MUI_PAGE_HEADER_TEXT "MTConnect Adapter Configuration"

Page custom fnc_MTCAdapterInstall_Show   AdapterVendorPageLeave
;!insertmacro  MUI_PAGE_CUSTOMFUNCTION_SHOW fnc_MTCAdapterInstall_Show 

Page custom fnc_MTCAdapterSelection_Show  AdapterSelectionPageLeave
;!insertmacro  MUI_PAGE_CUSTOMFUNCTION_SHOW fnc_MTCAdapterSelection_Show 


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Languate specific
!insertmacro MUI_LANGUAGE English
LangString MyWelcomeTitle ${LANG_ENGLISH} "Welcome"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function .onInit
    ${List.Create} DevicesXmlList                        ; Create a list for Devices,Xml output
    ${List.Create} AgentCfgList                        ; Create a liste for Agent.cfg output
 
    ;; Also http://nsis.sourceforge.net/Get_command_line_parameter_by_name 
    ;;MessageBox MB_OK "Value of CMDLINE is $CMDLINE"
    ${GetOptions} $CMDLINE "/p" $variable
    ;;MessageBox MB_OK "Value of CMDLINE parameter is '$variable'"
    StrCpy $INSTDIR "$variable"
 
    ;; For standalone debugging
    ;StrCpy $INSTDIR "$PROGRAMFILES64\MTConnect\MTConnectAgent"
FunctionEnd
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Section "MTConnectAgent (required)"

  SectionIn RO 

SectionEnd
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Section "Uninstall"
SectionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; StrReplace 
;; ${StrReplace} "C:\my documents\" "\" "//"
;; MessageBox MB_OK $0

!define StrReplace '!insertmacro "_StrReplaceConstructor"'

!macro _StrReplaceConstructor ORIGINAL_STRING TO_REPLACE REPLACE_BY
  Push "${ORIGINAL_STRING}"
  Push "${TO_REPLACE}"
  Push "${REPLACE_BY}"
  Call StrRep
  Pop $0
!macroend

Function StrRep
  Exch $R4 ; $R4 = Replacement String
  Exch
  Exch $R3 ; $R3 = String to replace (needle)
  Exch 2
  Exch $R1 ; $R1 = String to do replacement in (haystack)
  Push $R2 ; Replaced haystack
  Push $R5 ; Len (needle)
  Push $R6 ; len (haystack)
  Push $R7 ; Scratch reg
  StrCpy $R2 ""
  StrLen $R5 $R3
  StrLen $R6 $R1
loop:
  StrCpy $R7 $R1 $R5
  StrCmp $R7 $R3 found
  StrCpy $R7 $R1 1 ; - optimization can be removed if U know len needle=1
  StrCpy $R2 "$R2$R7"
  StrCpy $R1 $R1 $R6 1
  StrCmp $R1 "" done loop
found:
  StrCpy $R2 "$R2$R4"
  StrCpy $R1 $R1 $R6 $R5
  StrCmp $R1 "" done loop
done:
  StrCpy $R3 $R2
  Pop $R7
  Pop $R6
  Pop $R5
  Pop $R2
  Pop $R1
  Pop $R4
  Exch $R3
FunctionEnd
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
!define GenerateDevicesXML '!insertmacro "_GenerateDevicesXML"'

!macro _GenerateDevicesXML FILENAME DEVICENAME
  Push "${FILENAME}"
  Push "${DEVICENAME}"
  Call FcnGenerateOneDevicesXML
!macroend

Function FcnGenerateOneDevicesXML
    Var /GLOBAL DeviceName
    Var /GLOBAL FileTitle
    Var /GLOBAL Line
    Pop $DeviceName
    Pop $FileTitle
    FileOpen $4 "$INSTDIR\Devices\$AdapterVendor\$MachineType\$FileTitle.txt" r
Loop:
    IfErrors done
 
    FileRead $4 $Line
    DetailPrint $Line

    ${StrReplace} $Line "####" "$DeviceName"
    StrCpy $Line $0
    ${StrReplace} $Line "NNNNNN" "$DeviceName"
    StrCpy $Line $0
   
    ${List.Add} DevicesXmlList  $Line 

    IfErrors 0 Loop 
 done:
    FileClose $4
 FunctionEnd

Function FcnGenerateAllDevicesXMLFile
    
    FileOpen $1 "$INSTDIR\Devices.xml" "w"
                            
    FileWrite $1 "<?xml version=$\"1.0$\" encoding=$\"UTF-8$\"?>$\r$\n"
    FileWrite $1 "<MTConnectDevices xmlns=$\"urn:mtconnect.org:MTConnectDevices:1.1$\" xmlns:xsi=$\"http://www.w3.org/2001/XMLSchema-instance$\" $\r$\n";
    FileWrite $1 " xsi:schemaLocation=$\"urn:mtconnect.org:MTConnectDevices:1.1 http://www.mtconnect.org/schemas/MTConnectDevices_1.1.xsd$\">$\r$\n";
    FileWrite $1 "<Header bufferSize=$\"130000$\" instanceId=$\"1$\" creationTime=$\"2014-11-06T19:21:45$\" sender=$\"local$\" version=$\"1.1$\"/>$\r$\n"
    FileWrite $1 "<Devices>$\r$\n" 
 
    ${List.Count} $2 DevicesXmlList  
    IntOp $2 $2 - 1
    ${For} $i 0 $2 
      ${List.Get} $3 DevicesXmlList $i
      FileWrite $1 "$3"	        
   ${Next}
 
    FileWrite $1 "</Devices>$\r$\n"
    FileWrite $1 "</MTConnectDevices>$\r$\n";

    FileClose $1           
 
FunctionEnd



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function AdapterVendorPageLeave
  ;Read Sheet values
  ${NSD_GetText} $hCtl_MTCAdapterInstall_ComboBox2 $AdapterVendor 
  ${NSD_GetText} $hCtl_MTCAdapterInstall_ComboBox3  $MachineType
  ;; MessageBox mb_ok "Vendor $AdapterVendor" 
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function ConfigPreCreate
  GetDlgItem $8 $HWNDPARENT 1 ;  next, close == 1
  SendMessage $8 ${WM_SETTEXT} 0 "STR:&Done"
  EnableWindow $8 1 
  ShowWindow $8 1  

  GetDlgItem $8 $HWNDPARENT 2 ;  cancel == 2
  SendMessage $8 ${WM_SETTEXT} 0 "STR:&Cancel"
  EnableWindow $8 1 
  ShowWindow $8 1  

  GetDlgItem $8 $HWNDPARENT 3 ; back==3 
  SendMessage $8 ${WM_SETTEXT} 0 "STR:&Apply"
  EnableWindow $8 1 
  ShowWindow $8 1  

FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function FindDevicesFiles
    ClearErrors
    SendMessage $hCtl_MTCAdapterSelection_AdapterDropList ${CB_RESETCONTENT} 0 0

    ; FindFirst  var_handle_output var_filename_output filespec
    FindFirst $R0 $R1 "$INSTDIR\Devices\$AdapterVendor\*.http" 
    ${StrReplace} $R1 ".http" ""
    StrCpy $R1 $0
   ${StrReplace} $R1 "@" "/"
    SendMessage  $hCtl_MTCAdapterSelection_Link1 ${WM_SETTEXT} 0 "STR:$0"


    ClearErrors
    FindFirst $R0 $R1 "$INSTDIR\Devices\$AdapterVendor\$MachineType\*.txt" 
    ;MessageBox mb_ok "FindFirst $R1"
    Loop_jobs:
        IfErrors Loop_jobs_end
  	    ${StrReplace} $R1 ".txt" ""
            SendMessage $hCtl_MTCAdapterSelection_AdapterDropList ${CB_ADDSTRING} 0 "STR:$0"; "$R1"
         FindNext $R0 $R1
      	 ;MessageBox mb_ok "FindNext $R1"
         IfErrors 0 Loop_jobs
    Loop_jobs_end:

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function AddAdapterConfig
  ;Read Textboxes values
  ${NSD_GetText} $hCtl_MTCAdapterSelection_PortTextBox  $AdapterPort ; adapter port, e.g., 7878
  ${NSD_GetText} $hCtl_MTCAdapterSelection_IPTextBox  $AdapterIP ; adapter IPe.g., 127.0.0.1

  ${NSD_GetText} $hCtl_MTCAdapterSelection_DeviceNameTextBox  $AdapterName ; adaptername

  ${NSD_GetText} $hCtl_MTCAdapterSelection_AdapterDropList $6  ; cnc type general
  ;MessageBox mb_ok "Selection $6" 

  ;;  This creates the agent.cfg entry
   ${List.Add} AgentCfgList  "$\t$AdapterName$\r$\n"
   ${List.Add} AgentCfgList  "$\t{$\r$\n"
   ${List.Add} AgentCfgList  "$\t$\tHost=$AdapterIP$\r$\n"
   ${List.Add} AgentCfgList  "$\t$\tPort=$AdapterPort$\r$\n"
   ${List.Add} AgentCfgList  "$\t$\tDeviceType=$6$\r$\n"
   ${List.Add} AgentCfgList  "$\t}$\r$\n"


  ${GenerateDevicesXML} $6 $AdapterName

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function AdapterSelectionPageLeave
;	MessageBox mb_ok "AdapterConfigPageLeave"
	Call  AddAdapterConfig
 	Call  FcnGenerateAllDevicesXMLFile
        Call WriteAgentCfgFile
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function WriteAgentCfgFile
  FileOpen $1 "$INSTDIR\Agent.cfg" "w"
  ${List.Count} $2 AgentCfgList  
  IntOp $2 $2 - 1
  ${For} $i 0 $2 
      ${List.Get} $3 AgentCfgList $i
      FileWrite $1 "$3"	        
  ${Next}
  FileClose $1 
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CustomButtonPushed ; back button of adapter config page
	Call AddAdapterConfig
	;Call ResetAdapterConfig
	;Abort 
	StrCpy $R9 "2" ;Relative page number. See below.
	Call RelGotoPage
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function .onGUIEnd
     ${List.Unload}                               ; Don't forget to unload the NSISList plugin at the end !!!!!	
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RelGotoPage
  IntCmp $R9 0 0 Move Move
    StrCmp $R9 "X" 0 Move
      StrCpy $R9 "120"
 
  Move:
  SendMessage $HWNDPARENT "0x408" "$R9" ""
FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function OnVendorUrlLinkClick

  ${NSD_GetText} $hCtl_MTCAdapterSelection_Link1 $0
  ExecShell "open" "$0"

FunctionEnd

 