#Requires -Version 5.1
<#
.SYNOPSIS
    Calls helpers of the installed MSVBVM60 runtime so that their behavior
    can be compared with wfcvbrun.dll (REQ-0100 to REQ-0105).
.DESCRIPTION
    Runs in a 32-bit PowerShell (the runtime is an x86 DLL; on a 64-bit
    Windows the script starts the SysWOW64 PowerShell itself). Each family
    prints one line per probe. The expected values in
    tests/vbrun_helper_tests.cpp were taken from this output.
.PARAMETER Family
    memory, or another family name defined below.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Family,
    [string]$Runtime = 'msvbvm60.dll'
)

if ([Environment]::Is64BitProcess) {
    $ps32 = Join-Path $env:SystemRoot `
        'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    & $ps32 -NoProfile -File $PSCommandPath -Family $Family `
        -Runtime $Runtime
    exit $LASTEXITCODE
}

$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class Vb {
  const string D = "$Runtime";
  const string O = "oleaut32.dll";
  [DllImport(D)] public static extern int GetMem1(IntPtr a, IntPtr b);
  [DllImport(D)] public static extern int GetMem2(IntPtr a, IntPtr b);
  [DllImport(D)] public static extern int GetMem4(IntPtr a, IntPtr b);
  [DllImport(D)] public static extern int GetMem8(IntPtr a, IntPtr b);
  [DllImport(D)] public static extern int PutMem1(IntPtr a, byte v);
  [DllImport(D)] public static extern int PutMem2(IntPtr a, short v);
  [DllImport(D)] public static extern int PutMem4(IntPtr a, int v);
  [DllImport(D)] public static extern int PutMem8(IntPtr a, int lo, int hi);
  [DllImport(D)] public static extern IntPtr VarPtr(IntPtr a);
  [DllImport(D)] public static extern int GetMemStr(IntPtr a, IntPtr b);
  [DllImport(D)] public static extern int PutMemStr(IntPtr a, IntPtr b);
  [DllImport(D)] public static extern int GetMemVar(IntPtr a, IntPtr b);
  [DllImport(O)] public static extern IntPtr SysAllocStringByteLen(
      IntPtr s, uint n);
  [DllImport(O)] public static extern uint SysStringByteLen(IntPtr b);
}
"@
$m = [System.Runtime.InteropServices.Marshal]

function Show($text) { Write-Output $text }

function Clear-Memory($address) { $m::WriteInt64($address, 0) }

function Invoke-MemoryProbe {
    $a = $m::AllocHGlobal(32)
    $b = $m::AllocHGlobal(32)
    $m::WriteInt64($a, 0x89ABCDEF01234567L)
    Clear-Memory $b
    $r = [Vb]::GetMem1($a, $b)
    Show ('GetMem1 = {0} out={1:X2}' -f $r, $m::ReadByte($b))
    Clear-Memory $b
    $r = [Vb]::GetMem2($a, $b)
    Show ('GetMem2 = {0} out={1:X4}' -f $r, $m::ReadInt16($b))
    Clear-Memory $b
    $r = [Vb]::GetMem4($a, $b)
    Show ('GetMem4 = {0} out={1:X8}' -f $r, $m::ReadInt32($b))
    Clear-Memory $b
    $r = [Vb]::GetMem8($a, $b)
    Show ('GetMem8 = {0} out={1:X16}' -f $r, $m::ReadInt64($b))
    Clear-Memory $b
    $r = [Vb]::PutMem1($b, 0x7F)
    Show ('PutMem1 = {0} out={1:X2}' -f $r, $m::ReadByte($b))
    Clear-Memory $b
    $r = [Vb]::PutMem2($b, 0x1234)
    Show ('PutMem2 = {0} out={1:X4}' -f $r, $m::ReadInt16($b))
    Clear-Memory $b
    $r = [Vb]::PutMem4($b, 0x7AFEF00D)
    Show ('PutMem4 = {0} out={1:X8}' -f $r, $m::ReadInt32($b))
    Clear-Memory $b
    $r = [Vb]::PutMem8($b, 0x11223344, 0x55667788)
    Show ('PutMem8 = {0} out={1:X16}' -f $r, $m::ReadInt64($b))
    Show ('VarPtr same = {0}' -f ([Vb]::VarPtr($a) -eq $a))

    # Strings are copied by byte length (an odd length survives).
    $text = $m::StringToCoTaskMemAnsi('abc')
    $odd = [Vb]::SysAllocStringByteLen($text, 3)
    $m::WriteIntPtr($a, $odd)
    $m::WriteIntPtr($b, [IntPtr]::Zero)
    $r = [Vb]::GetMemStr($a, $b)
    $copy = $m::ReadIntPtr($b)
    Show ('GetMemStr odd = {0} bytes={1} distinct={2}' -f `
        $r, [Vb]::SysStringByteLen($copy), ($copy -ne $odd))
    $m::WriteIntPtr($a, [IntPtr]::Zero)
    $m::WriteIntPtr($b, $copy)
    $r = [Vb]::GetMemStr($a, $b)
    Show ('GetMemStr empty = {0} out={1}' -f $r, $m::ReadIntPtr($b))
    $m::WriteIntPtr($b, $odd)
    $r = [Vb]::PutMemStr($b, $copy)
    $now = $m::ReadIntPtr($b)
    Show ('PutMemStr = {0} distinct={1} bytes={2}' -f `
        $r, ($now -ne $copy), [Vb]::SysStringByteLen($now))
    $r = [Vb]::PutMemStr($b, [IntPtr]::Zero)
    $now = $m::ReadIntPtr($b)
    Show ('PutMemStr empty = {0} allocated={1} bytes={2}' -f `
        $r, ($now -ne [IntPtr]::Zero), [Vb]::SysStringByteLen($now))

    # A Variant is copied (VariantCopy).
    $v1 = $m::AllocHGlobal(16)
    $v2 = $m::AllocHGlobal(16)
    $m::WriteInt64($v1, 0)
    $m::WriteInt64($v1, 8, 0)
    $m::WriteInt64($v2, 0)
    $m::WriteInt64($v2, 8, 0)
    $m::WriteInt16($v1, 0, 3)
    $m::WriteInt32($v1, 8, 42)
    $r = [Vb]::GetMemVar($v1, $v2)
    Show ('GetMemVar I4 = {0} vt={1} value={2}' -f `
        $r, $m::ReadInt16($v2, 0), $m::ReadInt32($v2, 8))
}

switch ($Family) {
    'memory' { Invoke-MemoryProbe }
    default { throw "Unknown family '$Family'." }
}
