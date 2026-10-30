# REQ-0103 — VB6 member and memory helper exports

**Content type:** Project requirement

**Status:** Proposed

**Source:** Installed Visual Basic 6.0 runtime (`MSVBVM60.DLL`)

## Requirement

WFC shall reproduce compiled field access for scalar, string, variant, object, event, and newly-created object fields, together with record copy and address-observation behavior.

The compatibility DLL shall export all 24 symbols below at the
listed ordinals. Reference RVAs identify the inspected binary but are not
required implementation addresses.

| Ordinal | Export | Reference RVA |
| ---: | --- | ---: |
| 190 | `CopyRecord` | `0x000ED452` |
| 300 | `GetMem2` | `0x0004FC71` |
| 301 | `GetMem4` | `0x000FEAEB` |
| 302 | `GetMem8` | `0x000FEAFC` |
| 303 | `GetMemStr` | `0x000FEB2E` |
| 304 | `GetMemVar` | `0x000FEB68` |
| 305 | `GetMemObj` | `0x000FEB13` |
| 306 | `PutMem2` | `0x00014454` |
| 307 | `PutMem4` | `0x000FEC11` |
| 308 | `PutMem8` | `0x000FEC20` |
| 309 | `PutMemStr` | `0x0001266C` |
| 310 | `PutMemVar` | `0x000FED06` |
| 311 | `PutMemObj` | `0x000FEC36` |
| 312 | `SetMemVar` | `0x000FEE48` |
| 313 | `SetMemObj` | `0x000FEE21` |
| 314 | `GetMemNewObj` | `0x000FEB87` |
| 315 | `PutMemNewObj` | `0x000FEDB9` |
| 316 | `SetMemNewObj` | `0x000FEED6` |
| 317 | `GetMem1` | `0x000FEADA` |
| 318 | `PutMem1` | `0x000FEC02` |
| 319 | `GetMemEvent` | `0x000FEBE7` |
| 320 | `PutMemEvent` | `0x000FEE11` |
| 321 | `SetMemEvent` | `0x00059B4D` |
| 644 | `VarPtr` | `0x000EEC2C` |

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
