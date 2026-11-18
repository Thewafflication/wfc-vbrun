// The GoSub list and error-handler frame helpers. On 32-bit x86 (MSVC) the
// real sequences run: GoSubReturn continues after the GoSub call and
// OnError/ExitProc work on an exception frame at fs:[0]; elsewhere the list
// and the error state are checked.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstdio>

#include "../src/vbrun_helpers.hpp"

namespace {

int failures = 0;

void expect(const bool ok, const char* const what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

}  // namespace

#if defined(_MSC_VER) && defined(_M_IX86)

extern "C" {
void __vbaGosub();
void __vbaGosubReturn();
void __vbaGosubFree();
void __vbaOnError();
void __vbaExitProc();
void WINAPI __vbaResume(LONG target);
int __cdecl __vbaExceptHandler(EXCEPTION_RECORD* record, void* frame,
                               CONTEXT* context, void* dispatcher);
}

namespace {

// Runs GoSub, then GoSubReturn, which must continue right after the GoSub
// call: the pass counter reaches 2 and the list is empty again.
int gosub_passes(void** head) {
    int passes = 0;
    __asm {
        push ebx
        push esi
        mov ebx, head
        xor esi, esi
        push ebx
        call __vbaGosub
        inc esi
        cmp esi, 1
        jne finished
        push ebx
        call __vbaGosubReturn
    finished:
        mov passes, esi
        pop esi
        pop ebx
    }
    return passes;
}

#pragma warning(push)
// The test installs its own frame at fs:[0], on purpose.
#pragma warning(disable : 4733)
void frames() {
    void* head = nullptr;
    expect(gosub_passes(&head) == 2 && head == nullptr,
           "GoSubReturn continues after the GoSub call");

    // Free releases a list of two nodes.
    void* list = nullptr;
    __asm {
        lea eax, list
        push eax
        call __vbaGosub
        lea eax, list
        push eax
        call __vbaGosub
    }
    expect(list != nullptr, "two GoSubs leave a list");
    __asm {
        lea eax, list
        push eax
        call __vbaGosubFree
    }
    expect(list == nullptr, "GoSubFree empties the list");

    // A frame at fs:[0] receives the handler and the flags.
    struct Frame {
        void* next;
        void* handler;
        unsigned unused[2];
        unsigned flags;
        unsigned target;
        unsigned more[2];
    } frame = {};
    void* previous = nullptr;
    __asm {
        mov eax, dword ptr fs:[0]
        mov previous, eax
        lea eax, frame
        mov dword ptr fs:[0], eax
    }
    frame.flags = 2;
    __asm {
        push 0x1234
        call __vbaOnError
    }
    expect(frame.target == 0x1234 && (frame.flags & 2) != 0,
           "OnError stores the handler");
    __asm {
        push -3
        call __vbaOnError
    }
    expect((frame.flags & 2) == 0, "OnError GoTo 0 turns the handler off");
    frame.flags = 2;
    __asm {
        call __vbaExitProc
    }
    expect(frame.flags == 0, "ExitProc resets the flags");
    // Resume outside an error handler is error 20; Resume label inside one
    // clears the error and leaves the handler (the code jumps itself).
    vbrun::clear_error();
    __vbaResume(-2);
    expect(vbrun::last_error() == 20, "Resume without an error is error 20");
    static_cast<void>(vbrun::raise(5));
    frame.flags = 2;
    __vbaResume(3);
    expect(vbrun::last_error() == 0 && (frame.flags & 2) == 0,
           "Resume label clears the error and leaves the handler");
    // An unwind, or a frame without a procedure, is the next frame's.
    EXCEPTION_RECORD record = {};
    record.ExceptionCode = EXCEPTION_INT_DIVIDE_BY_ZERO;
    record.ExceptionFlags = 2;
    expect(__vbaExceptHandler(&record, &frame, nullptr, nullptr) ==
               ExceptionContinueSearch,
           "the handler leaves an unwind alone");
    record.ExceptionFlags = 0;
    frame.unused[1] = 0;
    expect(__vbaExceptHandler(&record, &frame, nullptr, nullptr) ==
               ExceptionContinueSearch,
           "the handler leaves a frame without a procedure alone");
    __asm {
        mov eax, previous
        mov dword ptr fs:[0], eax
    }
}

#pragma warning(pop)

}  // namespace

#else

extern "C" {
LONG WINAPI __vbaGosub(void** head);
LONG WINAPI __vbaGosubReturn(void** head);
void WINAPI __vbaGosubFree(void** head);
void WINAPI __vbaOnError(LONG target);
void __vbaExitProc();
void WINAPI __vbaResume(LONG target);
int __cdecl __vbaExceptHandler(EXCEPTION_RECORD* record, void* frame,
                               CONTEXT* context, void* dispatcher);
}

namespace {

void frames() {
    void* head = nullptr;
    expect(__vbaGosub(&head) == 0 && head != nullptr, "GoSub pushes a node");
    expect(__vbaGosub(&head) == 0, "GoSub pushes another");
    expect(__vbaGosubReturn(&head) == 1 && head != nullptr,
           "GoSubReturn pops one");
    expect(__vbaGosubReturn(&head) == 1 && head == nullptr,
           "GoSubReturn pops the last");
    vbrun::clear_error();
    expect(__vbaGosubReturn(&head) == 0 && vbrun::last_error() == 3,
           "Return without GoSub is error 3");
    vbrun::clear_error();
    __vbaGosub(&head);
    __vbaGosub(&head);
    __vbaGosubFree(&head);
    expect(head == nullptr, "GoSubFree empties the list");
    static_cast<void>(vbrun::raise(5));
    __vbaOnError(0x1234);
    expect(vbrun::last_error() == 0, "OnError clears the error");
    static_cast<void>(vbrun::raise(5));
    __vbaExitProc();
    expect(vbrun::last_error() == 0, "ExitProc clears the error");
    __vbaResume(-2);
    expect(vbrun::last_error() == 20, "Resume without an error is error 20");
    static_cast<void>(vbrun::raise(5));
    __vbaResume(-1);
    expect(vbrun::last_error() == 0, "Resume clears the error");
    EXCEPTION_RECORD record = {};
    record.ExceptionCode = EXCEPTION_INT_DIVIDE_BY_ZERO;
    expect(__vbaExceptHandler(&record, nullptr, nullptr, nullptr) ==
               ExceptionContinueSearch,
           "without frames every exception is the next frame's");
}

}  // namespace

#endif

int main() {
    frames();
    if (failures == 0) {
        std::printf("vbrun frames: all checks passed\n");
        return 0;
    }
    return 1;
}
