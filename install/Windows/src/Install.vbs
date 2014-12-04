dim currentDirectory,WshShell 

' Trick to elevate UAC from http://www.winhelponline.com/articles/185/1/VBScripts-and-UAC-elevation.html
If WScript.Arguments.length =0 Then
  Set objShell = CreateObject("Shell.Application")
  'Pass a bogus argument with leading blank space, say [ uac]
  objShell.ShellExecute "wscript.exe", Chr(34) & _
  WScript.ScriptFullName & Chr(34) & " uac", "", "runas", 1
Else
  'Add your code here

currentDirectory = left(WScript.ScriptFullName,(Len(WScript.ScriptFullName))-(len(WScript.ScriptName)))

Set WshShell = WScript.CreateObject("WScript.Shell")
'WshShell.Run "cmd /k " &  Chr(34)  & currentDirectory & "agent.exe" &  Chr(34) & "  install"   , 1, TRUE 
WshShell.Run Chr(34) & currentDirectory & "agent.exe" &  Chr(34) & "  install"   , 1, TRUE 

wscript.sleep 1000
End if
