# VBRUN helpers — implementation status

**Content type:** Implementation record for `REQ-0002`–`REQ-0007`

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
3. **Test.** `TC-0002-helpers-memory` and `TC-0003-helpers-clock` calls the export of
   `msvbvm60.dll` by name and checks the same results. It runs on every
   architecture of the matrix; the exports use `__stdcall` on x86, the only
   architecture whose binaries import them.

An export is listed in `lib/wfc-vbrun/src/implemented.txt` when it has a real
implementation; `lib/wfc-vbrun/scripts/New-VbrunExports.ps1` then exports it under its own
name. Every export now has one (635 of 635).

## Status by requirement

| Requirement | Exports | Implemented | Called by a test |
| --- | ---: | ---: | ---: |
| `REQ-0002` engine and hosting | 58 | 58 | 58 |
| `REQ-0003` compiler helpers | 334 | 334 | 334 |
| `REQ-0004` language runtime | 198 | 198 | 198 |
| `REQ-0005` member and memory | 24 | 24 | 24 |
| `REQ-0006` numeric support | 21 | 21 | 21 |
| **All** | **635** | **635** | **635** |

"Called by a test" counts the exports that at least one test program or call
case calls by name. For REQ-0002 that includes the four `Dll*` entries, which
`tests/dll_check.h` calls. The behaviours not reproduced are listed in
[COMPATIBILITY.md](../COMPATIBILITY.md).

## Status by family

The families in the order they were written. The notes describe each batch
when it was added; a later row supersedes an earlier one.

| Family | Requirement | Exports | Implemented | Notes |
| --- | --- | ---: | ---: | --- |
| Member and memory helpers | `REQ-0005` | 24 | 14 | `GetMem1/2/4/8`, `PutMem1/2/4/8`, `VarPtr`, `GetMemStr`, `PutMemStr`, `GetMemObj`, `SetMemObj`, `GetMemVar`. The other 10 follow in the rows "Member assignment, event sinks, records", "Record copy and destroy" and "Engine exports" |

| String functions on Strings | `REQ-0004` | (part of 198) | 16 | `Left`, `LeftB`, `Right`, `RightB`, `UCase`, `LCase`, `Trim`, `LTrim`, `RTrim`, `Space`, `Asc`, `AscB`, `AscW`, `ChrB`, `ChrW`, `StrReverse` (`rtcLeftBstr` is `LeftB`: the names with `Char` count characters). Call cases `strings` |
| Type tests on Variants | `REQ-0004` | (part of 198) | 8 | `VarType`, `IsEmpty`, `IsNull`, `IsArray`, `IsObject`, `IsError`, `TypeName` (`rtcTypeName`) and `IsNumeric`. Call cases `variants`. `IsDate` is not yet written |
| String and number conversions | `REQ-0003` | (part of 334) | 7 | `__vbaLenBstr`, `__vbaLenBstrB`, `__vbaI4Str`, `__vbaI2Str`, `__vbaUI1Str`, `__vbaStrI4`, `__vbaStrUI1`. Call cases `convert` |

| Operators and comparisons on Variants | `REQ-0003` | (part of 334) | 41 | `__vbaVarAdd`, `Sub`, `Mul`, `Div`, `Idiv`, `Mod`, `Pow`, `And`, `Or`, `Xor`, `Eqv`, `Imp`, `Not`, `Neg`, `Abs`, `Fix`, `Int`; the tests `__vbaVarTst*`, `__vbaVarTextTst*` and the Variant results `__vbaVarCmp*`, `__vbaVarTextCmp*`. A compiled program passes the operands right first (`__vbaVarSub(result, right, left)`). Call cases `varops`, taken with `Probe-VbrunCases.ps1 -Initialize` |

| Register-call helpers and the Zombie interface | `REQ-0003`, `REQ-0005` | (part of 334) | 33 | `__vbaI2I4`, `__vbaUI1I2`, `__vbaUI1I4`, `__vbaI2Abs`, `__vbaI4Abs`, `__vbaI2Sgn`, `__vbaI4Sgn`, `__vbaUI1Sgn`, `__vbaStrCopy`, `__vbaStrMove`, `__vbaFreeStr`, `__vbaFreeObj`, `__vbaFreeVar`, `__vbaVarCopy`, `__vbaVarDup`, `__vbaVarZero`, `__vbaVargParmRef`, `__vbaVargVar`, `__vbaVargVarRef`, `__vbaVarMove`, `__vbaFreeVarg`, `__vbaVarVargNofree`, `BASIC_CLASS_AddRef`/`Release`/`QueryInterface`, `BASIC_DISPINTERFACE_GetTICount` and the seven `Zombie_*` methods. Compiled code passes the first operands in ECX and EDX, so on x86 they are `__fastcall`; written from the disassembly and checked by `vbrun_regcall_tests` (they cannot be called from the 32-bit PowerShell probe). Not yet: `__vbaVargVarMove`/`Copy` (a large type switch; `__vbaVarVargNofree` leaves arrays unread) and the x87 helpers (`__vbaFp*`, `_CI*`, `_adj_*`: the operand is on the FPU stack) |

| The x87 helpers | `REQ-0003`, `REQ-0006` | (part of 334) | 18 | `__vbaFpI2`, `__vbaFpI4`, `__vbaFpUI1`, `__vbaFpR4`, `__vbaFpR8`, `__vbaFpCSngR4`/`R8`, `__vbaFpCDblR4`/`R8`, `__vbaFpCy`, `__vbaFpCmpCy`, `__vbaFPInt`, `__vbaFPFix`, `__vbaFPException`, `__vbaR8IntI2`/`I4`, `__vbaR8FixI2`/`I4`. The operand is on the x87 stack (`st0`). The reference runs at the control word 0x133F (64-bit precision, nearest even) and sets 0x123F, 0x103F, 0x173F and 0x1F3F for the conversions that need them. On x86 with MSVC the exports are the reference's own sequences (`src/helpers_x87.cpp`); on every other architecture they take a `double` and use the portable functions of `src/x87_core.hpp` (CCur is worked out exactly with integers: a double times 10,000 needs 63 bits). `vbrun_x87_tests` checks the portable functions everywhere and, on x86, compares each sequence with them value by value. Not yet: `_adj_fdiv_r` |
| The x87 math helpers and more | `REQ-0003`, `REQ-0006` | (part of 334) | 25 | `_CIsin`, `_CIcos`, `_CItan`, `_CIatan`, `_CIexp`, `_CIlog`, `_CIsqrt`, the `_adj_fdiv*` and `_adj_fdivr*` memory forms, `_adj_fprem`, `_adj_fprem1`, `_adj_fpatan`, `_adj_fptan`, `_allmul`, `__vbaVargVarMove`, `__vbaVargVarCopy`, `__vbaAryVarVarg`, `__vbaLdZeroAry`, `__vbaChkstk`. On x86 with MSVC the `_CI*` functions are the reference's sequences at the control word 0x027F (Exp at 0x133F) with the range reduction for a huge operand, and the `_adj_*` helpers are the single x87 instruction they stand for (the reference only adds a workaround for the Pentium FDIV flaw); elsewhere they take doubles (`src/helpers_x87math.cpp`, `vbrun_x87math_tests`) |
| Member assignment, event sinks, records | `REQ-0005` | (part of 334) | 15 | `GetMemNewObj`, `PutMemNewObj`, `PutMemVar`, `PutMemObj`, `SetMemVar`, `GetMemEvent`, `PutMemEvent`, `SetMemNewObj`, `EVENT_SINK_AddRef`/`Release`, `EVENT_SINK2_AddRef`/`Release`, `EbLibraryLoad`/`Unload`, `__vbaAryRecMove`. PutMemVar, PutMemObj and SetMemVar read or assign the default member of an object (`DISPID_VALUE`; flags 3 to read, `DISPATCH_PROPERTYPUT` to assign; Nothing is error 91, a number given to SetMemVar is 13). The Event and NewObj forms have two unused leading arguments and then are GetMemObj, PutMemObj and SetMemObj. The event-sink counts follow the vtable header of the reference (the offset of the interface in its object is found through a record just below the table; the plain sink counts at object offset 0x24). The other event-sink and class dispatch methods: see Engine exports |
| Record copy and destroy | `REQ-0005` | (part of 334) | 4 | `__vbaRecAssign`, `CopyRecord`, `__vbaRecDestruct`, `__vbaRecDestructAnsi`, from the descriptor the compiler emits (size at +2, field count at +6, variable-length entries from +0xc, by kind: string, Variant, object, nested record, raw). Copying fills the destination with the source bytes, then copies strings, Variants and nested records deeply and counts objects; destroying frees them. Array fields, kind 6 and the 0x400 pointer chain: see Engine exports. Checked against hand-built descriptors only. |
| Register divide, host identifier | `REQ-0005` | (part of 334) | 3 | `_adj_fdiv_r` (eax holds the form: register in bits 3-5, operation in bits 0-2; the six x87 register-divide forms, the two invalid ones trap as in the reference, whose 64-entry table was read entry by entry), `IID_IVbaHost` (the 16 bytes the reference exports as data) and `EbSetContextWorkerThread` (the value is only remembered) |
| GoSub list and error-handler frame | `REQ-0003` | (part of 334) | 5 | `__vbaGosub`, `__vbaGosubReturn`, `__vbaGosubFree`, `__vbaOnError`, `__vbaExitProc`. On x86 with MSVC these are the reference's sequences: GoSub saves its return address in a list node and GoSubReturn replaces its own return address with the saved one (Return without GoSub is error 3); OnError stores the handler target at +0x14 of the exception frame at fs:[0] (-3 clears the active flag, bit 1 of +0x10), ExitProc resets the flags. Elsewhere there is no frame to rewrite: the list and the error state are kept and GoSubReturn does not jump (`src/helpers_frames.cpp`, `vbrun_frames_tests`) |
| Engine exports | `REQ-0002`, `REQ-0003`, `REQ-0005` | (part of 635) | 50 | Records and arrays: array fields (kind 5: dynamic arrays, fixed arrays with their shape header), locked array references (kind 6), the 0x400 pointer chain, `__vbaRecUniToAnsi`/`__vbaRecAnsiToUni`, `__vbaAryRecCopy`, `__vbaCVarAryUdt`, `__vbaGetOwner3/4` and `__vbaPutOwner3/4` (Get and Put of records and arrays from the file layout the compiler emits; the file bytes measured with VB6.EXE /make probes). `__vbaPrintObj` (Print on an object, laid out as Print #, given to its Print member). Events: `EVENT_SINK_QueryInterface`/`GetIDsOfNames`/`Invoke`, `__vbaRaiseEvent`, `SetMemEvent`, `BASIC_CLASS_GetIDsOfNames`/`Invoke`, `BASIC_DISPINTERFACE_GetTypeInfo`. Program control: `DllFunctionCall` (errors 53, 452, 453 and their texts measured), `__vbaEnd`, `__vbaStopExe` (the measured message box), `__vbaResume`, `__vbaExceptHandler`. Entries and projects: `ThunRTMain` (Sub Main), `UserDllMain`, `VBDllRegisterServer`/`UnRegisterServer` (the classes of the COM registration data), `VBDllGetClassObject`, `VBDllCanUnloadNow`, the `Eb*` contexts and projects, the `Tip*` calls (`TipInvokeMethod` calls through the table as the FUNCDESC says), `CreateIExprSrvObj`, `DLLGetDocumentation`. Not reproduced: the object and p-code engines (see COMPATIBILITY.md) |
| Audit against the reference | `REQ-0002`–`REQ-0006` | — | — | Every export is now called by a test. The last 19 without one were checked against the reference: `__vbaAryConstruct2` raised error 5 for an array of records and dropped its third argument. It now stores the record information (through the pointer it is given), the interface identifier or the element type, as the reference does. `Error(n)` (`rtcVarFromError`, `rtcBstrFromError`) now follows a compiled program's results: without an argument it is the error of the moment; above 65535 it is error 6; for the current error number it is `Err.Description`; when it raises, the result Variant is left as it was. Checked in the same pass, already right: `Get`/`Put` of a fixed-length String without a record number (NUL characters past the end of the file), `Line Input #` into a Variant, `Reset`, `Timer`, the Variant text comparisons (`__vbaVarTextCmpGe/Gt/Lt`, `__vbaVarTextTstGe`, new call cases), `__vbaFpCSngR8`, `__vbaFpCDblR4`, `__vbaFPException`, `__vbaChkstk`, `__vbaErrorOverflow` and `__vbaGenerateBoundsError` |

(Updated with each batch.)

## Probing the reference

The reference binds its Automation entry points (`VarAdd`, ...) in a routine
that a Visual Basic program runs at start-up; calling the Variant operators
without it crashes. `Probe-VbrunCases.ps1 -Initialize` runs that routine
(MSVBVM60 6.00.9848, RVA 0x2CE61) first, so those exports can be probed too.
Some exports still cannot be called from outside a program because they use the
frames of the running program (error raising, `On Error`, `Gosub`) or the
runtime's thread state.
