# VBRUN helpers — implementation status

**Content type:** Implementation record for `REQ-0100`–`REQ-0105`

**Reference:** installed `MSVBVM60.DLL` 6.00.9848 (x86), the file the inventory
`evidence/reference/msvbvm60-6.0-runtime.json` was taken from.

## Method

The reference runtime is on the development machine, so the helpers are
written from what it does and checked against it:

1. **Signature and behavior** come from the disassembly of the export
   (`dumpbin /disasm`; the stack size of `ret n`, the registers and stack slots
   the code reads, the imports it calls). Nothing is taken from a name alone:
   `rtcLeftBstr`, for example, counts bytes (it is `LeftB`).
2. **Probe.** `lib/wfc-vbrun/scripts/Probe-Vbrun.ps1 -Family <family>` calls the reference
   export from a 32-bit PowerShell with the same arguments; the expected values
   in `lib/wfc-vbrun/tests/vbrun_helper_tests.cpp` are the output of that probe.
3. **Test.** `TC-MP0009-vbrun-helpers-<family>` calls the export of
   `wfcvbrun.dll` by name and checks the same results. It runs on every
   architecture of the matrix; the exports use `__stdcall` on x86, the only
   architecture whose binaries import them.

An export is listed in `lib/wfc-vbrun/src/implemented.txt` when it has a real
implementation; `lib/wfc-vbrun/scripts/New-VbrunExports.ps1` then exports it under its own
name. Every other export stays a stub that stops the process with a message
naming it.

## Status by family

| Family | Requirement | Exports | Implemented | Notes |
| --- | --- | ---: | ---: | --- |
| Member and memory helpers | `REQ-0103` | 24 | 14 | `GetMem1/2/4/8`, `PutMem1/2/4/8`, `VarPtr`, `GetMemStr`, `PutMemStr`, `GetMemObj`, `SetMemObj`, `GetMemVar`. Not yet: `PutMemVar`, `PutMemObj`, `SetMemVar` (default-property calls through `IDispatch`), the `*NewObj` and `*Event` helpers (they call into the engine's object creation), `CopyRecord` (reads private record descriptors) |

| String functions on Strings | `REQ-0102` | (part of 198) | 16 | `Left`, `LeftB`, `Right`, `RightB`, `UCase`, `LCase`, `Trim`, `LTrim`, `RTrim`, `Space`, `Asc`, `AscB`, `AscW`, `ChrB`, `ChrW`, `StrReverse` (`rtcLeftBstr` is `LeftB`: the names with `Char` count characters). Call cases `strings` |
| Type tests on Variants | `REQ-0102` | (part of 198) | 8 | `VarType`, `IsEmpty`, `IsNull`, `IsArray`, `IsObject`, `IsError`, `TypeName` (`rtcTypeName`) and `IsNumeric`. Call cases `variants`. `IsDate` is not yet written |
| String and number conversions | `REQ-0101` | (part of 334) | 7 | `__vbaLenBstr`, `__vbaLenBstrB`, `__vbaI4Str`, `__vbaI2Str`, `__vbaUI1Str`, `__vbaStrI4`, `__vbaStrUI1`. Call cases `convert` |

| Operators and comparisons on Variants | `REQ-0101` | (part of 334) | 41 | `__vbaVarAdd`, `Sub`, `Mul`, `Div`, `Idiv`, `Mod`, `Pow`, `And`, `Or`, `Xor`, `Eqv`, `Imp`, `Not`, `Neg`, `Abs`, `Fix`, `Int`; the tests `__vbaVarTst*`, `__vbaVarTextTst*` and the Variant results `__vbaVarCmp*`, `__vbaVarTextCmp*`. A compiled program passes the operands right first (`__vbaVarSub(result, right, left)`). Call cases `varops`, taken with `Probe-VbrunCases.ps1 -Initialize` |

| Register-call helpers and the Zombie interface | `REQ-0101`, `REQ-0103` | (part of 334) | 26 | `__vbaI2I4`, `__vbaUI1I2`, `__vbaUI1I4`, `__vbaI2Abs`, `__vbaI4Abs`, `__vbaI2Sgn`, `__vbaI4Sgn`, `__vbaUI1Sgn`, `__vbaStrCopy`, `__vbaStrMove`, `__vbaFreeStr`, `__vbaFreeObj`, `__vbaFreeVar`, `__vbaVarCopy`, `__vbaVarDup`, `__vbaVarZero`, `__vbaVargParmRef`, `__vbaVargVar`, `__vbaVargVarRef` and the seven `Zombie_*` methods. Compiled code passes the first operands in ECX and EDX, so on x86 they are `__fastcall`; written from the disassembly and checked by `vbrun_regcall_tests` (they cannot be called from the 32-bit PowerShell probe). Not yet: `__vbaVarMove`, `__vbaVargVarMove`/`Copy`, `__vbaVarVargNofree`, `__vbaFreeVarg` (large type switches) and the x87 helpers (`__vbaFp*`, `_CI*`, `_adj_*`: the operand is on the FPU stack) |

(Updated with each batch.)

## Probing the reference

The reference binds its Automation entry points (`VarAdd`, ...) in a routine
that a Visual Basic program runs at start-up; calling the Variant operators
without it crashes. `Probe-VbrunCases.ps1 -Initialize` runs that routine
(MSVBVM60 6.00.9848, RVA 0x2CE61) first, so those exports can be probed too.
Some exports still cannot be called from outside a program because they use the
frames of the running program (error raising, `On Error`, `Gosub`) or the
runtime's thread state.
