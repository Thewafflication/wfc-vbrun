# VBRUN call cases

Each `<family>.cases` file lists calls of exports of the runtime, one per
line, and `<family>.expected` holds what the installed MSVBVM60 answered, one
line per case (made by `scripts/Probe-VbrunCases.ps1`, which calls the
reference from a 32-bit PowerShell). `TC-MP0009-vbrun-cases-<family>` calls the
export of `wfcvbrun.dll` with the same arguments and compares.

```
export arg... => result
```

Arguments (values are passed as pointer-sized stdcall arguments):

| Argument | Meaning |
| --- | --- |
| `s"text"` | a String (escapes `\ \" \n \r \t \uXXXX`) |
| `s0` | no String (a null pointer) |
| `i123`, `x7F` | a Long, in decimal or hexadecimal |
| `v3:5`, `v8"text"`, `v0` | a Variant: `v<VarType>:<number>` or a String; `v0` Empty, `v1` Null |
| `o` | an output Variant (zeroed) |

Results: `s` a String (`s0` for none; `+1` after the text for an odd byte
count), `i` a Long, `w` a 16-bit result, `h` an HRESULT, `V` the Variant the
call returned, `O` the first output Variant. A Single or Double in a Variant
is shown as its bits in hexadecimal.

Error paths are not cases: the reference raises a Visual Basic error through
the frames of a running program and fails outside one.

## Exports that need the state of a running program

The `rtc*Var` functions read the thread state that a Visual Basic program's
start-up creates, so they fault when `Probe-VbrunCases.ps1` calls them from
PowerShell, and the reference raises its errors through the frames of a running
program. These are measured from inside one instead: `scripts/vb6_harness_gen.py`
writes a Visual Basic 6 module that declares the exports (`Declare Function
rtcAbsVar Lib "msvbvm60.dll" (ByRef result As Variant, ByRef v As Variant)`),
calls them under `On Error Resume Next` and writes the result or the error
number of each call; `scripts/vb6_harness_cases.py` turns that output into
the `.cases` and `.expected` files (`varfuncs`). Errors are not cases: the
reference raises 13 (Type mismatch) for a text that is no number in Abs, Fix,
Int and Sgn.

Found with it: `rtcInStr` is InStrB (byte positions, never ignores case) and
`rtcInStrChar` is InStr; both take (result, start, text, find, compare) as
Variants, the start must be a number (else 13) and at least 1 (else 5), and
the compare argument may be missing. `rtcStrFromVar` raises 94 for a Null and
13 for a text that is no number. `rtcCompareBstr` and `rtcStringBstr` fault in
every signature tried and are not implemented.

`rtcSin`, `rtcCos`, `rtcTan` reproduce the 66-bit pi of the x87 instructions
near a multiple of pi/2 (Sin of the Double nearest to pi is 1.2246063538223773E-16
in the reference); Tan(-7.25) and Atn(pi) differ from the reference by one unit
in the last place (the instructions round twice) and are not cases. A Double
by value is `d1.5` and a Double result is `D` (the bits, `d:x...`).
