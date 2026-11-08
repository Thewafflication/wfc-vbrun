unary=['rtcAbsVar','rtcFixVar','rtcIntVar','rtcSgnVar','rtcLenVar','rtcLenCharVar','rtcLowerCaseVar','rtcUpperCaseVar','rtcTrimVar','rtcLeftTrimVar','rtcRightTrimVar']
vals=['CInt(-5)','CLng(-5)','CLng(0)','CSng(-2.5)','CDbl(-2.5)','CDbl(7.9)','"-5"','"  Hi There  "','"abc"','""','Empty','Null','True','CCur(-3.5)','CByte(7)','CDate(36526.5)']
head=r'''Attribute VB_Name = "Harness"
Option Explicit
Declare Sub CopyMemory Lib "kernel32" Alias "RtlMoveMemory" (Dest As Any, Src As Any, ByVal n As Long)
'''
for n in unary: head+=f'Declare Function {n} Lib "msvbvm60.dll" (ByRef res As Variant, ByRef v As Variant) As Long\n'
head+=r'''
Private out As Integer

Private Function Esc(ByVal s As String) As String
    Dim i As Long, c As Long, r As String
    For i = 1 To Len(s)
        c = AscW(Mid$(s, i, 1))
        If c = 92 Then
            r = r & "\\"
        ElseIf c = 34 Then
            r = r & "\" & Chr$(34)
        ElseIf c < 32 Or c > 126 Then
            r = r & "\u" & Right$("0000" & Hex$(c And &HFFFF&), 4)
        Else
            r = r & Chr$(c)
        End If
    Next
    Esc = r
End Function

Private Function HexOf(ByRef v As Variant, ByVal bytes As Long) As String
    Dim b(0 To 7) As Byte, i As Long, s As String
    CopyMemory b(0), ByVal VarPtr(v) + 8, bytes
    For i = bytes - 1 To 0 Step -1
        s = s & Right$("0" & Hex$(b(i)), 2)
    Next
    HexOf = s
End Function

Public Function Fmt(ByRef v As Variant) As String
    Dim t As Long
    t = VarType(v)
    Select Case t
    Case 0: Fmt = "v0"
    Case 1: Fmt = "v1"
    Case 2, 3, 11: Fmt = "v" & t & ":" & CStr(CLng(v))
    Case 17: Fmt = "v17:" & CStr(CLng(v))
    Case 4: Fmt = "v4:x" & HexOf(v, 4)
    Case 5, 7: Fmt = "v" & t & ":x" & HexOf(v, 8)
    Case 6: Fmt = "v6:" & HexOf(v, 8)
    Case 8: Fmt = "v8""" & Esc(CStr(v)) & """"
    Case Else: Fmt = "v" & t & ":?"
    End Select
End Function

Private Sub Rec(ByVal s As String)
    Print #out, s
End Sub
'''
body='Public Sub Main()\n    Dim v As Variant, r As Variant\n    out = FreeFile\n    Open App.Path & "\vh.txt" For Output As #out\n'
for n in unary:
    for e in vals:
        body+=f'''    v = {e}
    r = Empty
    On Error Resume Next
    Err.Clear
    {n} r, v
    If Err.Number <> 0 Then
        Rec "{n} IN " & Fmt(v) & " => err" & Err.Number
    Else
        Rec "{n} IN " & Fmt(v) & " => " & Fmt(r)
    End If
    On Error GoTo 0
'''
body+='    Close #out\nEnd Sub\n'
open('Harness.bas','w',newline='').write((head+'\n'+body).replace('\n','\r\n'))
open('Harness.vbp','w',newline='').write('Type=Exe\r\nStartup="Sub Main"\r\nModule=Harness; Harness.bas\r\nName="Harness"\r\n')
open('../make-vh.ps1','w').write('''$src="$env:TEMP\wr\vh"
$out="$env:TEMP\wr\vb6vh"
Set-Location $src
Start-Process 'C:\Program Files (x86)\Microsoft Visual Studio\VB98\VB6.EXE' -Verb RunAs -Wait -ArgumentList '/make','Harness.vbp','/outdir',$out,'/out',"$out\make.log"
Get-Content "$out\make.log"
Get-ChildItem $out
''')
