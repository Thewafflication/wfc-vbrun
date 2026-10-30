# REQ-0105 — VB6 runtime export-directory layout

**Content type:** Project requirement

**Status:** Proposed

**Source:** Installed Visual Basic 6.0 runtime (`MSVBVM60.DLL`)

## Requirement

The compatibility DLL shall use ordinal base 100 and
preserve the reference mapping of 635 named exports within
1911 export-address-table slots. Slots that are empty in the
reference shall not resolve to an implementation. No symbol may silently move to a
different ordinal, even when normal linking uses its name.

The complete ordinal-to-name, RVA, presence, and category mapping is retained in
`evidence/reference/msvbvm60-6.0-runtime.json`.

## Rationale

Native-code VB6 programs import runtime entry points directly. Their
names, ordinals, calling conventions, data layouts, and behavior therefore
form a binary compatibility boundary independent of the VBA object model.

## Verification

**Method:** Inspection and test

**References:** `evidence/reference/msvbvm60-6.0-runtime.json`; test case TBD

Parse the built PE export directory and compare ordinal base, table size, holes, names, and ordinal assignments with the retained machine-readable inventory.

## Relationships

- **Derived from:** MP-0001 compatibility discovery objective
- **Depends on:** WCRT integration and VB execution-engine architecture
- **Conflicts with:** None known

## Tailoring

An export or behavior may be deferred only through an approved compatibility
profile that identifies the affected compiled VB6 programs.

## Implementation Record

`lib/wfc-vbrun/scripts/New-VbrunExports.ps1` generates `lib/wfc-vbrun/src/wfcvbrun.def` and
`vbrun_exports.inc` from the inventory: ordinal base 100, 1,911 slots, 635 named
exports at their reference ordinals, empty slots left empty (ordinals 100 to
2010). `TC-MP0007-vbrun-library` parses the built DLL's export directory and
compares base, slot count, names, ordinals and holes with the inventory.
