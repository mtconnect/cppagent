

Dim strService, WshShell 

' Trick to elevate UAC from http://www.winhelponline.com/articles/185/1/VBScripts-and-UAC-elevation.html
If WScript.Arguments.length =0 Then
  Set objShell = CreateObject("Shell.Application")
  'Pass a bogus argument with leading blank space, say [ uac]
  objShell.ShellExecute "wscript.exe", Chr(34) & _
  WScript.ScriptFullName & Chr(34) & " uac", "", "runas", 1
Else
  'Add your code here


strService = Inputbox ("Service that you would like to remove:", "Service", "MTConnectAgent")
if strService = ""  Then
	Wscript.Quit(0)
End if

Set WshShell = WScript.CreateObject("WScript.Shell")
WshShell.Run "sc.exe stop" & strService 

WshShell.Run "sc.exe delete " & strService 
wscript.sleep 1000
End If