#Requires -Version 5.1
<#
.SYNOPSIS
    Runs the call cases of tests/cases against a VBRUN runtime and
    writes the results (REQ-0100 to REQ-0105).
.DESCRIPTION
    Each case line is `export arg... => result` (see tests/cases/
    README.md). The script runs in a 32-bit PowerShell, calls the export of
    the DLL with the arguments as stdcall pointer-sized values and writes one
    line per case. With the default runtime (msvbvm60.dll) it produces the
    reference results that the tests compare wfcvbrun.dll with.
.PARAMETER Cases
    The case file.
.PARAMETER Output
    The result file (one line per case); written to the console when empty.
.PARAMETER Runtime
    The DLL to call (default: the installed msvbvm60.dll).
.PARAMETER Initialize
    Bind the Automation entry points of the reference runtime first (needed
    for the Variant operators).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Cases,
    [string]$Output = '',
    [string]$Runtime = 'msvbvm60.dll',
    [switch]$Initialize
)

if ([Environment]::Is64BitProcess) {
    $ps32 = Join-Path $env:SystemRoot `
        'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    $arguments = @('-NoProfile', '-File', $PSCommandPath, '-Cases', $Cases,
        '-Runtime', $Runtime)
    if ($Initialize) { $arguments += '-Initialize' }
    if ($Output -ne '') { $arguments += @('-Output', $Output) }
    & $ps32 @arguments
    exit $LASTEXITCODE
}

$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class Native {
  [DllImport("kernel32", CharSet = CharSet.Ansi)]
  public static extern IntPtr LoadLibrary(string name);
  [DllImport("kernel32", CharSet = CharSet.Ansi)]
  public static extern IntPtr GetProcAddress(IntPtr module, string name);
  [DllImport("oleaut32")] public static extern void SysFreeString(IntPtr b);
  [DllImport("oleaut32")] public static extern int VariantClear(IntPtr v);
  [DllImport("oleaut32")] public static extern uint SysStringByteLen(IntPtr b);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)]
  public delegate IntPtr F0();
  [UnmanagedFunctionPointer(CallingConvention.StdCall)]
  public delegate IntPtr F1(IntPtr a);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)]
  public delegate IntPtr F2(IntPtr a, IntPtr b);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)]
  public delegate IntPtr F3(IntPtr a, IntPtr b, IntPtr c);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)]
  public delegate IntPtr F4(IntPtr a, IntPtr b, IntPtr c, IntPtr d);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)]
  public delegate IntPtr F5(IntPtr a, IntPtr b, IntPtr c, IntPtr d,
      IntPtr e);
  public static IntPtr Call(IntPtr fn, IntPtr[] a) {
    switch (a.Length) {
      case 0: return Marshal.GetDelegateForFunctionPointer<F0>(fn)();
      case 1: return Marshal.GetDelegateForFunctionPointer<F1>(fn)(a[0]);
      case 2: return Marshal.GetDelegateForFunctionPointer<F2>(fn)(
          a[0], a[1]);
      case 3: return Marshal.GetDelegateForFunctionPointer<F3>(fn)(
          a[0], a[1], a[2]);
      case 4: return Marshal.GetDelegateForFunctionPointer<F4>(fn)(
          a[0], a[1], a[2], a[3]);
      case 5: return Marshal.GetDelegateForFunctionPointer<F5>(fn)(
          a[0], a[1], a[2], a[3], a[4]);
    }
    throw new ArgumentException("too many arguments");
  }
}
'@

# Text with the escapes of the case files: \\ \" \n \r \t \uXXXX.
function Read-CaseText($text) {
    $builder = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $text.Length; $i++) {
        $c = $text[$i]
        if ($c -ne '\') { [void]$builder.Append($c); continue }
        $i++
        switch ($text[$i]) {
            'n' { [void]$builder.Append("`n") }
            'r' { [void]$builder.Append("`r") }
            't' { [void]$builder.Append("`t") }
            'u' {
                $code = [Convert]::ToInt32($text.Substring($i + 1, 4), 16)
                [void]$builder.Append([char]$code)
                $i += 4
            }
            default { [void]$builder.Append($text[$i]) }
        }
    }
    $builder.ToString()
}

# The same escapes for a result: everything outside 32..126 is \uXXXX.
function Write-CaseText($text) {
    $builder = New-Object System.Text.StringBuilder
    foreach ($c in $text.ToCharArray()) {
        $code = [int]$c
        if ($c -eq '\') { [void]$builder.Append('\\') }
        elseif ($c -eq '"') { [void]$builder.Append('\"') }
        elseif ($code -lt 32 -or $code -gt 126) {
            [void]$builder.Append(('\u{0:X4}' -f $code))
        } else { [void]$builder.Append($c) }
    }
    $builder.ToString()
}

$m = [System.Runtime.InteropServices.Marshal]

# A Variant of the case grammar: v<vt>:<number> or v8"text" (vt 0 and 1 have
# no value).
# A whole number of the grammar: decimal, or x followed by hexadecimal.
function ConvertTo-CaseInteger($number) {
    if ($number.StartsWith('x')) {
        $unsigned = [Convert]::ToUInt32($number.Substring(1), 16)
        return [BitConverter]::ToInt32([BitConverter]::GetBytes($unsigned), 0)
    }
    [int]$number
}

function New-CaseVariant($vt, $number, $text) {
    $v = $m::AllocHGlobal(16)
    $m::WriteInt64($v, 0, 0)
    $m::WriteInt64($v, 8, 0)
    $m::WriteInt16($v, 0, [int16]$vt)
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    switch ($vt) {
        { $_ -in 2, 11 } {
            $m::WriteInt16($v, 8, [int16](ConvertTo-CaseInteger $number))
        }
        { $_ -in 3, 10 } {
            $m::WriteInt32($v, 8, (ConvertTo-CaseInteger $number))
        }
        17 { $m::WriteByte($v, 8, [byte]$number) }
        4 {
            $single = [single]::Parse($number, $culture)
            $bytes = [BitConverter]::GetBytes($single)
            $m::WriteInt32($v, 8, [BitConverter]::ToInt32($bytes, 0))
        }
        { $_ -in 5, 7 } {
            $bits = [BitConverter]::DoubleToInt64Bits(
                [double]::Parse($number, $culture))
            $m::WriteInt64($v, 8, $bits)
        }
        6 { $m::WriteInt64($v, 8, [int64]$number) }
        8 { $m::WriteIntPtr($v, 8, $m::StringToBSTR((Read-CaseText $text))) }
    }
    $v
}

function Format-CaseVariant($v) {
    $vt = [int]$m::ReadInt16($v, 0) -band 0xFFFF
    switch ($vt) {
        { $_ -in 0, 1 } { return "v$vt" }
        { $_ -in 2, 11 } { return "v${vt}:" + $m::ReadInt16($v, 8) }
        3 { return 'v3:' + $m::ReadInt32($v, 8) }
        10 { return ('v10:x{0:X8}' -f $m::ReadInt32($v, 8)) }
        17 { return 'v17:' + $m::ReadByte($v, 8) }
        4 { return ('v4:x{0:X8}' -f $m::ReadInt32($v, 8)) }
        { $_ -in 5, 7 } {
            return ("v${vt}:x{0:X16}" -f $m::ReadInt64($v, 8))
        }
        6 { return 'v6:' + $m::ReadInt64($v, 8) }
        8 {
            $b = $m::ReadIntPtr($v, 8)
            if ($b -eq [IntPtr]::Zero) { return 'v8:0' }
            $text = $m::PtrToStringBSTR($b)
            return 'v8"' + (Write-CaseText $text) + '"'
        }
    }
    "v${vt}:?"
}

$module = [Native]::LoadLibrary($Runtime)
if ($module -eq [IntPtr]::Zero) { throw "Cannot load $Runtime." }
if ($Initialize) {
    # The Automation entry points of the runtime are bound by a routine that
    # a Visual Basic program runs at start-up (MSVBVM60 6.00.9848, RVA
    # 0x2CE61); run it so that the exports that call them can be probed.
    $bind = [IntPtr]($module.ToInt64() + 0x2CE61)
    [void][Native]::Call($bind, @())
}

$results = New-Object System.Collections.Generic.List[string]
$echo = ($Output -eq '')
foreach ($line in [System.IO.File]::ReadAllLines($Cases)) {
    $line = $line.Trim()
    if ($line -eq '' -or $line.StartsWith('#')) { continue }
    $split = $line -split '\s=>\s'
    $head = $split[0].Trim()
    $kind = $split[1].Trim()
    # The export name, then arguments: s"text" s0 i123 x7F.
    $name = ($head -split '\s+', 2)[0]
    $rest = ''
    if ($head.Length -gt $name.Length) {
        $rest = $head.Substring($name.Length).Trim()
    }
    $arguments = New-Object System.Collections.Generic.List[IntPtr]
    $allocated = New-Object System.Collections.Generic.List[IntPtr]
    $variants = New-Object System.Collections.Generic.List[IntPtr]
    $outputs = New-Object System.Collections.Generic.List[IntPtr]
    while ($rest -ne '') {
        if ($rest -match '^s"((?:[^"\\]|\\.)*)"\s*(.*)$') {
            $b = $m::StringToBSTR((Read-CaseText $Matches[1]))
            $arguments.Add($b)
            $allocated.Add($b)
            $rest = $Matches[2]
        } elseif ($rest -match '^s0\s*(.*)$') {
            $arguments.Add([IntPtr]::Zero)
            $rest = $Matches[1]
        } elseif ($rest -match '^i(-?\d+)\s*(.*)$') {
            $arguments.Add([IntPtr][int]$Matches[1])
            $rest = $Matches[2]
        } elseif ($rest -match '^x([0-9A-Fa-f]+)\s*(.*)$') {
            $unsigned = [Convert]::ToUInt32($Matches[1], 16)
            $value = [BitConverter]::ToInt32(
                [BitConverter]::GetBytes($unsigned), 0)
            $arguments.Add([IntPtr]$value)
            $rest = $Matches[2]
        } elseif ($rest -match '^o\s*(.*)$') {
            $v = New-CaseVariant 0 '' ''
            $arguments.Add($v)
            $variants.Add($v)
            $outputs.Add($v)
            $rest = $Matches[1]
        } elseif ($rest -match
            '^v(\d+)(?::(\S+)|"((?:[^"\\]|\\.)*)")?\s*(.*)$') {
            $v = New-CaseVariant ([int]$Matches[1]) $Matches[2] $Matches[3]
            $arguments.Add($v)
            $variants.Add($v)
            $rest = $Matches[4]
        } else {
            throw "Bad argument in case: $line"
        }
    }
    $function = [Native]::GetProcAddress($module, $name)
    if ($function -eq [IntPtr]::Zero) { throw "$name is not exported." }
    $raw = [Native]::Call($function, $arguments.ToArray())
    switch ($kind) {
        's' {
            if ($raw -eq [IntPtr]::Zero) { $results.Add('s0') }
            else {
                $bytes = [Native]::SysStringByteLen($raw)
                $text = $m::PtrToStringBSTR($raw)
                $odd = ''
                if (($bytes % 2) -eq 1) { $odd = '+1' }
                $results.Add('s"' + (Write-CaseText $text) + '"' + $odd)
                [Native]::SysFreeString($raw)
            }
        }
        'i' { $results.Add('i' + [int]$raw) }
        'h' { $results.Add(('x{0:X8}' -f [int]$raw)) }
        'w' {
            $word = [int]$raw -band 0xFFFF
            if ($word -ge 32768) { $word -= 65536 }
            $results.Add('w' + $word)
        }
        'V' { $results.Add((Format-CaseVariant $raw)) }
        'O' { $results.Add((Format-CaseVariant $outputs[0])) }
        default { throw "Bad result kind in case: $line" }
    }
    if ($echo) { [Console]::Out.WriteLine($results[$results.Count - 1]) }
    foreach ($b in $allocated) { [Native]::SysFreeString($b) }
    foreach ($v in $variants) {
        [void][Native]::VariantClear($v)
        $m::FreeHGlobal($v)
    }
}

if ($Output -ne '') {
    [System.IO.File]::WriteAllLines($Output, $results)
}
