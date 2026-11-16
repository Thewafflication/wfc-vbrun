// The GoSub stack and the error-handler frame helpers: __vbaGosub,
// __vbaGosubReturn, __vbaGosubFree, __vbaOnError and __vbaExitProc.
//
// The compiled procedure keeps a list head (one pointer) for its GoSub calls
// and registers an exception frame at fs:[0] whose fields the error helpers
// set: flags at +0x10 (bit 1: an error handler is active), the handler's
// target at +0x14. On 32-bit x86 with MSVC the exports are the reference's own
// sequences, written from the disassembly of the installed MSVBVM60
// 6.00.9848: GoSub saves its own return address in a node and GoSubReturn
// replaces its return address with the saved one, so the `ret` that ends it
// continues after the GoSub call. Elsewhere there is no frame to rewrite, so
// the helpers keep the list and the error state only (GoSubReturn returns 1
// and does not jump).

#include "vbrun_helpers.hpp"

#include <cstdint>

namespace {

struct GosubNode {
    GosubNode* next;
    void* return_address;
};

GosubNode* node_alloc() {
    return static_cast<GosubNode*>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(GosubNode)));
}

void node_free(GosubNode* const node) {
    HeapFree(GetProcessHeap(), 0, node);
}

}  // namespace

#if defined(_MSC_VER) && defined(_M_IX86)

extern "C" {

void* __cdecl vbrun_node_alloc() { return node_alloc(); }
void __cdecl vbrun_node_free(void* const node) {
    node_free(static_cast<GosubNode*>(node));
}
void __cdecl vbrun_raise_error(const int error) {
    static_cast<void>(vbrun::raise(error));
}
void __cdecl vbrun_clear_error() { vbrun::clear_error(); }

// GoSub: a node {next, return address} is pushed on the list; returns 0 (error
// 7, out of memory, when the node cannot be allocated).
__declspec(naked) void __vbaGosub() {
    __asm {
        push ebx
        call vbrun_node_alloc
        test eax, eax
        je failed
        mov ebx, dword ptr [esp + 8]
        mov ecx, dword ptr [ebx]
        mov dword ptr [eax], ecx
        mov ecx, dword ptr [esp + 4]
        mov dword ptr [eax + 4], ecx
        mov dword ptr [ebx], eax
        xor eax, eax
        pop ebx
        ret 4
    failed:
        push 7
        call vbrun_raise_error
        add esp, 4
        xor eax, eax
        pop ebx
        ret 4
    }
}

// Return: the newest node is popped and its address becomes the return address
// of this call; returns 1. An empty list is error 3 (Return without GoSub).
__declspec(naked) void __vbaGosubReturn() {
    __asm {
        push esi
        mov esi, dword ptr [esp + 8]
        mov eax, dword ptr [esi]
        test eax, eax
        je none
        mov ecx, dword ptr [eax]
        mov dword ptr [esi], ecx
        mov ecx, dword ptr [eax + 4]
        mov dword ptr [esp + 4], ecx
        push eax
        call vbrun_node_free
        add esp, 4
        mov eax, 1
        pop esi
        ret 4
    none:
        push 3
        call vbrun_raise_error
        add esp, 4
        xor eax, eax
        pop esi
        ret 4
    }
}

// Free: every node of the list is released and the list emptied.
__declspec(naked) void __vbaGosubFree() {
    __asm {
        push esi
        push ebx
        mov esi, dword ptr [esp + 12]
        mov ebx, dword ptr [esi]
        mov dword ptr [esi], 0
    again:
        test ebx, ebx
        je done
        mov eax, dword ptr [ebx]
        push eax
        push ebx
        call vbrun_node_free
        add esp, 4
        pop ebx
        jmp again
    done:
        pop ebx
        pop esi
        ret 4
    }
}

// On Error: the error is cleared; -3 (On Error GoTo 0) turns the handler off,
// any other value is the handler's target in the frame at fs:[0].
__declspec(naked) void __vbaOnError() {
    __asm {
        call vbrun_clear_error
        mov eax, dword ptr fs:[0]
        mov ecx, dword ptr [esp + 4]
        cmp ecx, -3
        je off
        mov dword ptr [eax + 0x14], ecx
        ret 4
    off:
        and dword ptr [eax + 0x10], 0xFFFFFFFD
        ret 4
    }
}

// Exit of a procedure: an active handler's error is cleared, the frame's
// flags are reset.
__declspec(naked) void __vbaExitProc() {
    __asm {
        push esi
        mov esi, dword ptr fs:[0]
        test byte ptr [esi + 0x10], 2
        je skip
        call vbrun_clear_error
    skip:
        and dword ptr [esi + 0x10], 0
        pop esi
        ret
    }
}

}  // extern "C"

#else

extern "C" {

LONG WINAPI __vbaGosub(void** const head) {
    GosubNode* const node = node_alloc();
    if (node == nullptr) {
        static_cast<void>(vbrun::raise(7));
        return 0;
    }
    node->next = static_cast<GosubNode*>(*head);
    node->return_address = nullptr;
    *head = node;
    return 0;
}

LONG WINAPI __vbaGosubReturn(void** const head) {
    GosubNode* const node = static_cast<GosubNode*>(*head);
    if (node == nullptr) {
        static_cast<void>(vbrun::raise(3));
        return 0;
    }
    *head = node->next;
    node_free(node);
    return 1;
}

void WINAPI __vbaGosubFree(void** const head) {
    GosubNode* node = static_cast<GosubNode*>(*head);
    *head = nullptr;
    while (node != nullptr) {
        GosubNode* const next = node->next;
        node_free(node);
        node = next;
    }
}

// There is no exception frame to hold the handler: the error is cleared.
void WINAPI __vbaOnError(const LONG /*target*/) { vbrun::clear_error(); }

void __vbaExitProc() { vbrun::clear_error(); }

}  // extern "C"

#endif
