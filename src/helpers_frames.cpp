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

#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include <cstring>

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

void* __cdecl vbrun_node_alloc() {
    return node_alloc();
}
void __cdecl vbrun_node_free(void* const node) {
    node_free(static_cast<GosubNode*>(node));
}
void __cdecl vbrun_raise_error(const int error) {
    static_cast<void>(vbrun::raise(error));
}
void __cdecl vbrun_clear_error() {
    vbrun::clear_error();
}

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

namespace {

// The exception frame of a compiled procedure (at fs:[0] while it runs): the
// next frame, the handler, the stack pointer of the procedure's body (+8),
// its descriptor (+0xc), the flags (+0x10: 2 an error is being handled), the
// On Error target (+0x14: 0 none, -1 Resume Next, else a label id), the
// statement to resume (+0x18) and the current statement (+0x1c). The
// descriptor: flags (+0), the offset of the procedure's frame pointer from
// frame+0xc (Int16 at +2, 4 to 0x14), the label table (+0x10: a count, then
// pairs of id and address) and the address of every statement (+0x14).
struct VbFrame {
    VbFrame* next;
    void* handler;
    void* body_stack;
    const unsigned char* procedure;
    DWORD flags;
    LONG target;
    LONG resume_statement;
    LONG statement;
};

constexpr DWORD kHandling = 2;

short frame_offset(const VbFrame* const frame) {
    short offset = 0;
    std::memcpy(&offset, frame->procedure + 2, sizeof offset);
    return offset;
}

bool valid_frame(const VbFrame* const frame) {
    if (frame == nullptr || frame->procedure == nullptr) {
        return false;
    }
    const short offset = frame_offset(frame);
    return offset >= 4 && offset <= 0x14;
}

void* statement_address(const VbFrame* const frame, const LONG index) {
    void* const* table = nullptr;
    std::memcpy(&table, frame->procedure + 0x14, sizeof table);
    return table != nullptr && index >= 0 ? table[index] : nullptr;
}

void* label_address(const VbFrame* const frame, const LONG id) {
    const LONG* table = nullptr;
    std::memcpy(&table, frame->procedure + 0x10, sizeof table);
    if (table == nullptr) {
        return nullptr;
    }
    for (LONG i = 0; i < table[0]; ++i) {
        if (table[1 + 2 * i] == id) {
            return reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(table[2 + 2 * i]));
        }
    }
    return nullptr;
}

// Continues the procedure at `address` with its frame and stack pointers.
__declspec(naked) void __cdecl continue_at(void* /*address*/,
                                           void* /*frame_pointer*/,
                                           void* /*stack_pointer*/) {
    __asm {
        mov eax, dword ptr [esp + 4]
        mov ecx, dword ptr [esp + 8]
        mov edx, dword ptr [esp + 12]
        mov esp, edx
        mov ebp, ecx
        fnclex
        jmp eax
    }
}

// The frames above `frame` are unwound (their handlers run); on x86 RtlUnwind
// returns to its caller with the frame at fs:[0].
void unwind_to(VbFrame* const frame, EXCEPTION_RECORD* const record) {
    RtlUnwind(frame, nullptr, record, nullptr);
}

[[noreturn]] void resume_at(VbFrame* const frame, void* const address) {
    continue_at(
        address,
        reinterpret_cast<char*>(&frame->procedure) + frame_offset(frame),
        frame->body_stack);
    for (;;) {
    }
}

// The Visual Basic error of an exception, or 0 when it is not one Visual
// Basic handles (an error the runtime raised is 0xC000008F with two
// parameters; its number is the thread's).
int exception_error(const EXCEPTION_RECORD* const record) {
    switch (record->ExceptionCode) {
        case EXCEPTION_STACK_OVERFLOW:
            return 28;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            return 11;
        case EXCEPTION_INT_OVERFLOW:
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_INVALID_OPERATION:
        case EXCEPTION_FLT_UNDERFLOW:
        case EXCEPTION_FLT_STACK_CHECK:
        case EXCEPTION_FLT_DENORMAL_OPERAND:
            return 6;
        case EXCEPTION_FLT_INEXACT_RESULT:
            return record->NumberParameters == 2 && vbrun::last_error() != 0
                       ? vbrun::last_error()
                       : 6;
        default:
            return 0;
    }
}

}  // namespace

extern "C" {

// Resume (-2), Resume Next (-1) or Resume label (any other value, the
// compiled code jumps itself): only inside an error handler (else error 20);
// the error is cleared and the handler left.
void WINAPI __vbaResume(const LONG target) {
    VbFrame* const frame = reinterpret_cast<VbFrame*>(__readfsdword(0));
    if ((frame->flags & kHandling) == 0) {
        static_cast<void>(vbrun::raise(20));
        return;
    }
    vbrun::clear_error();
    frame->flags &= ~kHandling;
    if (target == -2 || target == -1) {
        void* const address = statement_address(
            frame, frame->resume_statement + (target == -1 ? 1 : 0));
        if (address != nullptr) {
            resume_at(frame, address);
        }
    }
}

// The exception handler of every compiled procedure: an error in a procedure
// with an active On Error goes to its handler (the frames above are unwound,
// the error is the thread's, the statement is kept for Resume); anything
// else, an unwind, or an error inside the handler, is left to the next frame.
// On Error Resume Next goes on with the next statement, the error kept.
int __cdecl __vbaExceptHandler(EXCEPTION_RECORD* const record,
                               void* const frame_pointer, CONTEXT* /*context*/,
                               void* /*dispatcher*/) {
    VbFrame* const frame = static_cast<VbFrame*>(frame_pointer);
    if ((record->ExceptionFlags & 6) != 0 || !valid_frame(frame)) {
        return ExceptionContinueSearch;
    }
    const int error = exception_error(record);
    if (error == 0 || frame->target == 0 || (frame->flags & kHandling) != 0) {
        return ExceptionContinueSearch;
    }
    void* const address =
        frame->target == -1   ? statement_address(frame, frame->statement + 1)
        : frame->target == -2 ? statement_address(frame, frame->statement)
                              : label_address(frame, frame->target);
    if (address == nullptr) {
        return ExceptionContinueSearch;
    }
    if (error != vbrun::last_error()) {
        static_cast<void>(vbrun::raise(error));
    }
    unwind_to(frame, record);
    frame->resume_statement = frame->statement;
    if (frame->target != -1) {
        frame->flags |= kHandling;
    }
    resume_at(frame, address);
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
void WINAPI __vbaOnError(const LONG /*target*/) {
    vbrun::clear_error();
}

void __vbaExitProc() {
    vbrun::clear_error();
}

// Without the frames of compiled code: Resume needs an error to resume from
// (error 20 otherwise) and clears it; the exception handler leaves every
// exception to the next frame.
void WINAPI __vbaResume(const LONG /*target*/) {
    if (vbrun::last_error() == 0) {
        static_cast<void>(vbrun::raise(20));
        return;
    }
    vbrun::clear_error();
}

int __cdecl __vbaExceptHandler(EXCEPTION_RECORD* /*record*/, void* /*frame*/,
                               CONTEXT* /*context*/, void* /*dispatcher*/) {
    return ExceptionContinueSearch;
}

}  // extern "C"

#endif
