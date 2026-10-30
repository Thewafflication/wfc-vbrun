# REQ-0104 — VB6 native numeric support exports

**Content type:** Project requirement

**Status:** Proposed

**Source:** Installed Visual Basic 6.0 runtime (`MSVBVM60.DLL`)

## Requirement

WFC shall reproduce the x86 floating-point adjustment, transcendental, square-root, and wide-multiply helper ABI with reference precision, rounding, exception, and stack behavior.

The compatibility DLL shall export all 21 symbols below at the
listed ordinals. Reference RVAs identify the inspected binary but are not
required implementation addresses.

| Ordinal | Export | Reference RVA |
| ---: | --- | ---: |
| 156 | `_CIatan` | `0x0004883F` |
| 157 | `_CIcos` | `0x00048803` |
| 158 | `_CIexp` | `0x00041824` |
| 159 | `_CIlog` | `0x000485E0` |
| 160 | `_CIsin` | `0x000487AD` |
| 161 | `_CIsqrt` | `0x0004864A` |
| 162 | `_CItan` | `0x00101D82` |
| 659 | `_adj_fdiv_m16i` | `0x000FA81F` |
| 688 | `_adj_fdiv_m32` | `0x000FA787` |
| 701 | `_adj_fdiv_m32i` | `0x000FA853` |
| 718 | `_adj_fdiv_m64` | `0x000FA7D3` |
| 719 | `_adj_fdiv_r` | `0x000FA2C2` |
| 720 | `_adj_fdivr_m16i` | `0x000FA91F` |
| 721 | `_adj_fdivr_m32` | `0x000FA887` |
| 722 | `_adj_fdivr_m32i` | `0x000FA953` |
| 723 | `_adj_fdivr_m64` | `0x000FA8D3` |
| 724 | `_adj_fpatan` | `0x000FAF0F` |
| 725 | `_adj_fprem` | `0x000FABA2` |
| 726 | `_adj_fprem1` | `0x000FAE5A` |
| 727 | `_adj_fptan` | `0x000FAF12` |
| 728 | `_allmul` | `0x000F9631` |

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
