# REQ-0100 — VB6 execution-engine and hosting exports

**Content type:** Project requirement

**Status:** Proposed

**Source:** Installed Visual Basic 6.0 runtime (`MSVBVM60.DLL`)

## Requirement

WFC shall reproduce project and context loading, native EXE/DLL startup, COM class-factory and registration entry points, type-instance management, Automation dispatch, event-sink dispatch, and teardown semantics represented by these exports.

The compatibility DLL shall export all 58 symbols below at the
listed ordinals. Reference RVAs identify the inspected binary but are not
required implementation addresses.

| Ordinal | Export | Reference RVA |
| ---: | --- | ---: |
| 100 | `ThunRTMain` | `0x0002AC91` |
| 101 | `VBDllUnRegisterServer` | `0x000CABF3` |
| 102 | `VBDllCanUnloadNow` | `0x0004F754` |
| 103 | `VBDllRegisterServer` | `0x000CAB6B` |
| 104 | `VBDllGetClassObject` | `0x00059067` |
| 105 | `UserDllMain` | `0x00058FF8` |
| 106 | `DllRegisterServer` | `0x000E34BD` |
| 107 | `DllUnregisterServer` | `0x000E369B` |
| 153 | `DLLGetDocumentation` | `0x000E37A8` |
| 154 | `DllCanUnloadNow` | `0x000D4CAF` |
| 155 | `DllGetClassObject` | `0x000D4C2A` |
| 186 | `ProcCallEngine` | `0x00108BF1` |
| 187 | `DllFunctionCall` | `0x0003159C` |
| 195 | `TipGetAddressOfPredeclaredInstance` | `0x0003ECA7` |
| 199 | `MethCallEngine` | `0x0010C012` |
| 220 | `TipSetOption` | `0x000E5888` |
| 227 | `TipUnloadProject` | `0x00032C5A` |
| 230 | `TipCreateInstanceProject2` | `0x000E6AE0` |
| 231 | `EbResetProject` | `0x0003536A` |
| 232 | `EbGetHandleOfExecutingProject` | `0x00009A68` |
| 265 | `EbResetProjectNormal` | `0x000679B3` |
| 266 | `TipUnloadInstance` | `0x00063518` |
| 268 | `EbLibraryLoad` | `0x00020ACB` |
| 269 | `EbLibraryUnload` | `0x00055D06` |
| 271 | `EbLoadRunTime` | `0x0002F6CE` |
| 274 | `EbCreateContext` | `0x0002DD99` |
| 275 | `EbDestroyContext` | `0x00034290` |
| 276 | `EbSetContextWorkerThread` | `0x0002DDDA` |
| 284 | `EbGetErrorInfo` | `0x000E5FC9` |
| 298 | `EbIsProjectOnStack` | `0x00032A24` |
| 299 | `TipCreateInstanceEx` | `0x0003C77A` |
| 400 | `EVENT_SINK_QueryInterface` | `0x0001B8A5` |
| 401 | `EVENT_SINK_AddRef` | `0x0001755A` |
| 402 | `EVENT_SINK_Release` | `0x00018108` |
| 403 | `EVENT_SINK_GetIDsOfNames` | `0x000E5723` |
| 404 | `EVENT_SINK_Invoke` | `0x0000FF3B` |
| 410 | `BASIC_CLASS_QueryInterface` | `0x0000306E` |
| 411 | `BASIC_CLASS_AddRef` | `0x00002F45` |
| 412 | `BASIC_CLASS_Release` | `0x00002D18` |
| 413 | `BASIC_CLASS_GetIDsOfNames` | `0x0000F26C` |
| 414 | `BASIC_CLASS_Invoke` | `0x0000F141` |
| 420 | `BASIC_DISPINTERFACE_GetTICount` | `0x000B2FE1` |
| 421 | `BASIC_DISPINTERFACE_GetTypeInfo` | `0x00043ADC` |
| 430 | `Zombie_QueryInterface` | `0x000373D7` |
| 431 | `Zombie_AddRef` | `0x00002F45` |
| 432 | `Zombie_Release` | `0x00002D18` |
| 433 | `Zombie_GetTypeInfoCount` | `0x000E57E9` |
| 434 | `Zombie_GetTypeInfo` | `0x000E57E1` |
| 435 | `Zombie_GetIDsOfNames` | `0x000E57F1` |
| 436 | `Zombie_Invoke` | `0x000E57F9` |
| 440 | `EVENT_SINK2_AddRef` | `0x000E56FF` |
| 441 | `EVENT_SINK2_Release` | `0x000E5711` |
| 999 | `TipInvokeMethod2` | `0x000EBF2C` |
| 1016 | `TipInvokeMethod` | `0x000EC04C` |
| 1024 | `IID_IVbaHost` | `0x00099D90` |
| 1025 | `EbGetObjConnectionCounts` | `0x0003464B` |
| 2000 | `CreateIExprSrvObj` | `0x000F773C` |
| 2010 | `EbGetVBAObject` | `0x000E534C` |

## Rationale

Native-code VB6 programs import runtime entry points directly. Their
names, ordinals, calling conventions, data layouts, and behavior therefore
form a binary compatibility boundary independent of the VBA object model.

## Verification

**Method:** Inspection and test

**References:** `evidence/reference/msvbvm60-6.0-runtime.json`; test case TBD

Link representative VB6 native executables that import every listed helper family, compare import resolution by name and ordinal, and compare observable results, errors, state changes, and cleanup against the reference runtime.

## Relationships

- **Derived from:** MP-0001 compatibility discovery objective
- **Depends on:** WCRT integration and VB execution-engine architecture
- **Conflicts with:** None known

## Tailoring

An export or behavior may be deferred only through an approved compatibility
profile that identifies the affected compiled VB6 programs.

## Implementation Record

Not implemented.
