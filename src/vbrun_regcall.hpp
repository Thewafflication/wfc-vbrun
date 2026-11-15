// The calling convention of the helpers the compiler calls with their first
// operands in ECX and EDX: `__fastcall` on x86, nothing elsewhere.

#pragma once

#if defined(_M_IX86) || defined(__i386__)
#define VBRUN_FASTCALL __fastcall
#else
#define VBRUN_FASTCALL
#endif
