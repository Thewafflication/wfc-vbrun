"""Writes the Visual Basic 6 harness that measures exports of MSVBVM60.

Some exports (the rtc*Var functions, ...) read the thread state that a running
Visual Basic program creates, so they fault when they are called from
PowerShell (scripts/Probe-VbrunCases.ps1). The harness declares them (Declare
Function ... Lib "msvbvm60.dll") and calls them from inside a program.

An experiment is an export, its signature, and a grid of arguments. The
program runs one experiment per start (its number is the command line), so a
signature that is wrong faults only that run; the driver
(vb6_harness_cases.py) starts the program for every experiment and turns the
results into tests/cases/<family>.cases and .expected.

Usage: python vb6_harness_gen.py <output folder>
"""

import os
import sys

# Argument kinds: V a Variant by reference, S a String (its BSTR), L a Long,
# O the output Variant (always first for the functions that give a Variant).
# Result kinds: V the output Variant, s a BSTR, i a Long, w an Integer.

VARIANTS = [
    ('CInt(-5)', 'v2:-5'), ('CLng(1234)', 'v3:1234'), ('CLng(0)', 'v3:0'),
    ('CSng(-2.5)', 'v4:-2.5'), ('CDbl(7.9)', 'v5:7.9'),
    ('"-5"', 'v8"-5"'), ('"Hello World"', 'v8"Hello World"'),
    ('""', 'v8""'), ('Empty', 'v0'), ('Null', 'v1'), ('True', 'v11:-1'),
    ('CCur(-3.5)', 'v6:-35000'), ('CByte(7)', 'v17:7'),
    ('CDate(36526.5)', 'v7:36526.5'),
]
TEXTS = [
    ('"hello world"', 's"hello world"'), ('"hello"', 's"hello"'),
    ('""', 's""'), ('"  pad  "', 's"  pad  "'), ('"abc"', 's"abc"'),
    ('"ABC"', 's"ABC"'),
]
COUNTS = ['0', '1', '2', '5', '100']
MODES = [(m, 'i' + m) for m in ('0', '1')]
DATES = [
    ('CDate(36526.5)', 'v7:36526.5'), ('CDate(40000.75)', 'v7:40000.75'),
    ('"2000-01-01 13:45:10"', 'v8"2000-01-01 13:45:10"'), ('Null', 'v1'),
    ('Empty', 'v0'),
]

EXPERIMENTS = []


def experiment(export, args, result, grid, isolate=False):
    """One experiment; with isolate, one for every row (a call that faults
    then costs only its own row)."""
    if isolate:
        for row in grid:
            EXPERIMENTS.append((export, args, result, [row]))
    else:
        EXPERIMENTS.append((export, args, result, grid))


def pair(x, y):
    return [(a, b) for a in x for b in y]


# Left/Right/Mid on Variants: (result, text, count) and (result, text, start,
# length).
# The byte forms count bytes: an odd count leaves a half character, which a
# Variant formatted from Visual Basic cannot show, so only even counts.
for name, counts in (('rtcLeftVar', ('0', '2', '4', '100')),
                     ('rtcRightVar', ('0', '2', '4', '100')),
                     ('rtcLeftCharVar', COUNTS),
                     ('rtcRightCharVar', COUNTS)):
    experiment(name, 'OVL', 'V',
               [(v, (c, 'i' + c)) for v in VARIANTS for c in counts])
for name in ('rtcStrFromVar',):
    experiment(name, 'V', 's', [(v,) for v in VARIANTS])
for name in ('rtcHexVarFromVar', 'rtcOctVarFromVar', 'rtcVarBstrFromChar'):
    experiment(name, 'OV' if 'From' in name and 'Bstr' not in name else 'OL',
               'V',
               [(v,) for v in VARIANTS[:5] + VARIANTS[10:13]]
               if 'Bstr' not in name else [((c, 'i' + c),) for c in
                                           ('65', '97', '9', '200')])
for name in ('rtcGetYear', 'rtcGetMonthOfYear', 'rtcGetDayOfMonth',
             'rtcGetHourOfDay', 'rtcGetMinuteOfHour',
             'rtcGetSecondOfMinute'):
    experiment(name, 'OV', 'V', [(d,) for d in DATES])

# Exports whose signature was read from the disassembly but not confirmed:
# each shape is its own experiment, so a wrong one only fails its own run.
MISSING = ('CVErr(-2147352572)', 'v10:x80020004')
STARTS = [(c, 'i' + c) for c in ('1', '3', '7', '20')]
LENGTHS = [('CLng(3)', 'v3:3'), ('CInt(100)', 'v2:100'), MISSING]
TEXTV = [VARIANTS[6], VARIANTS[2], VARIANTS[8], VARIANTS[9]]
for name in ('rtcMidVar', 'rtcMidCharVar'):
    experiment(name, 'OVLV', 'V',
               [(v, st, ln) for v in TEXTV for st in STARTS for ln in LENGTHS])
CHARS = [('"x"', 'v8"x"'), ('65', 'v3:65'), ('"ab"', 'v8"ab"')]
experiment('rtcStringVar', 'OLV', 'V',
           [(('3', 'i3'), c) for c in CHARS] + [(('0', 'i0'), CHARS[0])])
# InStrB and InStr: (result, start, text, find, compare), all Variants; the
# compare argument may be missing. (Found by trying every placement of a
# missing argument, a number and two texts: the first is the start, which
# must be a number, then the text, the text to find and the compare mode.)
for name in ('rtcInStr', 'rtcInStrChar'):
    experiment(name, 'OVVVV', 'V',
               [(st, tx, fd, cm)
                for st in [('CLng(1)', 'v3:1'), ('CLng(6)', 'v3:6'),
                           ('CLng(20)', 'v3:20')]
                for tx in [VARIANTS[6], VARIANTS[7]]
                for fd in [('"o"', 'v8"o"'), ('"zz"', 'v8"zz"'),
                           ('""', 'v8""'), ('"L"', 'v8"L"'),
                           ('"l"', 'v8"l"')]
                for cm in [MISSING, ('CLng(0)', 'v3:0'),
                           ('CLng(1)', 'v3:1')]])
experiment('rtcStrConvVar', 'OVL', 'V',
           [(v, (c, 'i' + c)) for v in (VARIANTS[6], VARIANTS[1])
            for c in ('1', '2', '3', '64')])
experiment('rtcGetDayOfWeek', 'OVL', 'V',
           [(d, (f, 'i' + f)) for d in DATES[:3] for f in ('0', '1', '2')])

# The numeric helpers: a Double by value, a Double result.
DOUBLES = [('0', 'd0'), ('1', 'd1'), ('-1', 'd-1'), ('0.5', 'd0.5'),
           ('2.718281828459045', 'd2.718281828459045'), ('100', 'd100'),
           ('1E-10', 'd1e-10'), ('-7.25', 'd-7.25'),
           ('3.141592653589793', 'd3.141592653589793')]
for name in ('rtcSin', 'rtcCos', 'rtcTan', 'rtcAtn', 'rtcExp', 'rtcLog',
             'rtcSqr'):
    experiment(name, 'D', 'd', [(x,) for x in DOUBLES])
VALS = [('"12"', 's"12"'), ('" 12abc"', 's" 12abc"'), ('"&HFF"', 's"&HFF"'),
        ('"1e3"', 's"1e3"'), ('"abc"', 's"abc"'), ('""', 's""'),
        ('"-.5"', 's"-.5"'), ('"1,5"', 's"1,5"'), ('"12 34"', 's"12 34"'),
        ('"&O17"', 's"&O17"'), ('"3.25.5"', 's"3.25.5"'),
        ('"  -42  "', 's"  -42  "')]
experiment('rtcR8ValFromBstr', 'S', 'd', [(v,) for v in VALS])
experiment('rtcMonthName', 'LL', 's',
           [((str(m), 'i%d' % m), (a, 'i' + a)) for m in range(1, 13)
            for a in ('0', '-1')])
CONDS = [('True', 'v11:-1'), ('False', 'v11:0'), ('CLng(1)', 'v3:1'),
         ('CLng(0)', 'v3:0'), ('Null', 'v1'), ('"x"', 'v8"x"')]
experiment('rtcImmediateIf', 'OVVV', 'V',
           [(c, ('"yes"', 'v8"yes"'), ('CLng(7)', 'v3:7')) for c in CONDS])
ROUNDS = [('CDbl(2.5)', 'v5:2.5'), ('CDbl(3.5)', 'v5:3.5'),
          ('CDbl(-2.5)', 'v5:-2.5'), ('CDbl(1.2345)', 'v5:1.2345'),
          ('CDbl(0.285)', 'v5:0.285'), ('CLng(7)', 'v3:7'),
          ('CCur(-3.5)', 'v6:-35000'), ('"1.5"', 'v8"1.5"'),
          ('Null', 'v1'), ('Empty', 'v0')]
experiment('rtcRound', 'OVL', 'V',
           [(v, (c, 'i' + c)) for v in ROUNDS for c in ('0', '1', '2')])
experiment('rtcPartition', 'OVVVV', 'V',
           [(n, ('CLng(0)', 'v3:0'), ('CLng(99)', 'v3:99'),
             ('CLng(10)', 'v3:10')) for n in
            [('CLng(5)', 'v3:5'), ('CLng(15)', 'v3:15'),
             ('CLng(99)', 'v3:99'), ('CLng(-1)', 'v3:-1'),
             ('CLng(100)', 'v3:100'), ('Null', 'v1')]])


def vb_call(kind, index, row):
    """The Visual Basic statements of one grid row; returns (code, case)."""
    code = []
    case = []
    arguments = []
    cursor = 0
    for letter in kind:
        if letter == 'O':
            arguments.append('r')
            case.append('o')
            continue
        item = row[cursor]
        cursor += 1
        if letter == 'V':
            code.append('    v%d = %s' % (cursor, item[0]))
            arguments.append('v%d' % cursor)
            case.append(item[1])
        elif letter == 'S':
            arguments.append('StrPtr(%s)' % item[0])
            case.append(item[1])
        elif letter == 'D':
            arguments.append('CDbl(%s)' % item[0])
            case.append(item[1])
        else:
            arguments.append('CLng(%s)' % item[0])
            case.append(item[1])
    return code, arguments, case


def declare(number, export, kind, result):
    parameters = []
    for place, letter in enumerate(kind):
        if letter in 'OV':
            parameters.append('ByRef a%d As Variant' % place)
        elif letter == 'D':
            parameters.append('ByVal a%d As Double' % place)
        else:
            parameters.append('ByVal a%d As Long' % place)
    if kind.startswith('O'):
        ret = 'Long'
    else:
        ret = {'s': 'Long', 'i': 'Long', 'w': 'Integer',
               'd': 'Double'}.get(result, 'Long')
    return ('Declare Function E%d_%s Lib "msvbvm60.dll" Alias "%s" (%s) '
            'As %s' % (number, export, export, ', '.join(parameters), ret))


PROLOGUE = r'''Attribute VB_Name = "Hmod"
Option Explicit
Declare Sub CopyMemory Lib "kernel32" Alias "RtlMoveMemory" (Dest As Any, Src As Any, ByVal n As Long)
'''

SUPPORT = r'''
Private out As Integer

Private Function Esc(ByVal c As Long) As String
    If c = 92 Then
        Esc = "\\"
    ElseIf c = 34 Then
        Esc = "\" & Chr$(34)
    ElseIf c < 32 Or c > 126 Then
        Esc = "\u" & Right$("0000" & Hex$(c And &HFFFF&), 4)
    Else
        Esc = Chr$(c)
    End If
End Function

Private Function EscText(ByVal s As String) As String
    Dim i As Long
    For i = 1 To Len(s)
        EscText = EscText & Esc(AscW(Mid$(s, i, 1)))
    Next
End Function

Private Function HexOf(ByRef v As Variant, ByVal bytes As Long) As String
    Dim b(0 To 7) As Byte, i As Long, s As String
    CopyMemory b(0), ByVal VarPtr(v) + 8, bytes
    For i = bytes - 1 To 0 Step -1
        s = s & Right$("0" & Hex$(b(i)), 2)
    Next
    HexOf = s
End Function

Private Function Dec64(ByRef v As Variant) As String
    Dim lo As Long, hi As Long, c As Currency
    c = v
    Dec64 = CStr(c * 10000@)
End Function

Public Function Fmt(ByRef v As Variant) As String
    Dim t As Long
    t = VarType(v)
    Select Case t
    Case 0: Fmt = "v0"
    Case 1: Fmt = "v1"
    Case 10: Fmt = "v10:x" & HexOf(v, 4)
    Case 2, 3, 11: Fmt = "v" & t & ":" & CStr(CLng(v))
    Case 17: Fmt = "v17:" & CStr(CLng(v))
    Case 4: Fmt = "v4:x" & HexOf(v, 4)
    Case 5, 7: Fmt = "v" & t & ":x" & HexOf(v, 8)
    Case 6: Fmt = "v6:" & Dec64(v)
    Case 8: Fmt = "v8""" & EscText(CStr(v)) & """"
    Case Else: Fmt = "v" & t & ":?"
    End Select
End Function

Private Function HexD(ByVal d As Double) As String
    Dim b(0 To 7) As Byte, i As Long, s As String
    CopyMemory b(0), d, 8
    For i = 7 To 0 Step -1
        s = s & Right$("0" & Hex$(b(i)), 2)
    Next
    HexD = s
End Function

Private Function BstrFmt(ByVal p As Long) As String
    Dim n As Long, i As Long, b() As Byte, u As Long, s As String
    If p = 0 Then
        BstrFmt = "s0"
        Exit Function
    End If
    CopyMemory n, ByVal p - 4, 4
    s = "s"""
    If n > 0 Then
        ReDim b(0 To n - 1)
        CopyMemory b(0), ByVal p, n
        For i = 0 To n - 2 Step 2
            s = s & Esc(b(i) + 256& * b(i + 1))
        Next
        If n Mod 2 = 1 Then s = s & Esc(b(n - 1))
    End If
    s = s & """"
    If n Mod 2 = 1 Then s = s & "+1"
    BstrFmt = s
End Function

Private Sub Rec(ByVal s As String)
    Print #out, s
End Sub

Private Sub Trace(ByVal s As String)
    Dim t As Integer
    t = FreeFile
    Open App.Path & Chr$(92) & "trace" & Command$ & ".txt" For Append As #t
    Print #t, s
    Close #t
End Sub
'''


def build(folder):
    os.makedirs(folder, exist_ok=True)
    text = PROLOGUE
    for number, (export, kind, result, grid) in enumerate(EXPERIMENTS):
        text += declare(number, export, kind, result) + '\n'
    text += SUPPORT
    body = ('Public Sub Main()\n    Dim v1 As Variant, v2 As Variant, '
            'v3 As Variant, v4 As Variant, r As Variant, p As Long\n'
            '    out = FreeFile\n'
            '    Open App.Path & Chr$(92) & "vh" & Command$ & ".txt" For '
            'Output As #out\n    Select Case Val(Command$)\n')
    plan = []
    for number, (export, kind, result, grid) in enumerate(EXPERIMENTS):
        body += '    Case %d\n        Run%d\n' % (number, number)
    body += '    End Select\n    Close #out\nEnd Sub\n'
    subs = ''
    for number, (export, kind, result, grid) in enumerate(EXPERIMENTS):
        subs += '\nPrivate Sub Run%d()\n' % number
        subs += '    Dim v1 As Variant, v2 As Variant, v3 As Variant, '
        subs += 'v4 As Variant, r As Variant, p As Long, q As Double\n'
        subs += '    On Error Resume Next\n'
        for row in grid:
            code, arguments, case = vb_call(kind, number, row)
            label = '%s %s' % (export, ' '.join(case))
            subs += '\n'.join(code) + '\n' if code else ''
            subs += '    Trace "%s"\n' % label.replace('"', '""')
            subs += '    r = Empty\n    Err.Clear\n'
            call = 'E%d_%s(%s)' % (number, export, ', '.join(arguments))
            subs += '    %s = %s\n' % ('q' if result == 'd' else 'p', call)
            subs += '    If Err.Number <> 0 Then\n'
            subs += '        Rec "%s => err" & Err.Number\n' % label.replace(
                '"', '""')
            subs += '    Else\n'
            if result == 'V':
                subs += '        Rec "%s => " & Fmt(r)\n' % label.replace(
                    '"', '""')
            elif result == 'd':
                subs += '        Rec "%s => d:x" & HexD(q)\n' % label.replace(
                    '"', '""')
            elif result == 's':
                subs += '        Rec "%s => " & BstrFmt(p)\n' % label.replace(
                    '"', '""')
            else:
                subs += '        Rec "%s => " & CStr(p)\n' % label.replace(
                    '"', '""')
            subs += '    End If\n'
        subs += 'End Sub\n'
    text += '\n' + body + subs
    open(os.path.join(folder, 'Hmod.bas'), 'w', newline='').write(
        text.replace('\n', '\r\n'))
    open(os.path.join(folder, 'Harness.vbp'), 'w', newline='').write(
        'Type=Exe\r\nStartup="Sub Main"\r\nModule=Hmod; Hmod.bas\r\n'
        'Name="Harness"\r\n')
    open(os.path.join(folder, 'experiments.txt'), 'w').write(
        '\n'.join('%d %s %s %s' % (n, e[0], e[1], e[2])
                  for n, e in enumerate(EXPERIMENTS)) + '\n')


if __name__ == '__main__':
    build(sys.argv[1])
