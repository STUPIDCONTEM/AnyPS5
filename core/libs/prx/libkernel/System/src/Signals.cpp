#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#ifndef _WIN32
#include <pthread.h>
#include <signal.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" int APS5_VABI getpid_nid_postfix(void);

struct GuestSignalSet {
    std::uint32_t bits[4];
};

struct GuestSigaction {
    std::uintptr_t handler;
    int flags;
    GuestSignalSet mask;
};
static_assert(sizeof(GuestSigaction) == 32 && offsetof(GuestSigaction, flags) == 8 && offsetof(GuestSigaction, mask) == 12);

namespace {
using GuestHandler = void (APS5_VABI *)(int);
constexpr int MaxSignal = 128;
constexpr int GuestSigkill = 9;
constexpr int GuestSigstop = 17;
constexpr int SaRestart = 0x2;
constexpr int SaSiginfo = 0x40;
std::atomic<GuestHandler> handlers[MaxSignal + 1]{};
static_assert(std::atomic<GuestHandler>::is_always_lock_free);
std::atomic<std::uint32_t> blockedMask{0};
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
std::mutex registration;
GuestSigaction dispositions[MaxSignal + 1]{};
int NativeSignal(int guest) {
    switch (guest) {
        case 2: return SIGINT;
        case 4: return SIGILL;
        case 6: return SIGABRT;
        case 8: return SIGFPE;
        case 11: return SIGSEGV;
        case 15: return SIGTERM;
        default: return 0;
    }
}
void Dispatch(int native) {
    int guest = 0;
    for (int candidate : {2, 4, 6, 8, 11, 15})
        if (NativeSignal(candidate) == native) { guest = candidate; break; }
    if (!guest) return;
#ifdef _WIN32
    // Preserve the guest's persistent registration across CRT delivery.
    std::signal(native, Dispatch);
#endif
    if ((blockedMask.load() & (1u << guest)) != 0) return;
    const auto callback = handlers[guest].load();
    if (reinterpret_cast<std::uintptr_t>(callback) > 1) callback(guest);
}
bool Delivered(const GuestSigaction& action) {
    return action.handler > 1 && (action.flags & SaSiginfo) == 0;
}
bool Install(int guest, const GuestSigaction& action) {
    const int native = NativeSignal(guest);
    if (!native) return true;
    const auto previous = handlers[guest].exchange(Delivered(action) ? reinterpret_cast<GuestHandler>(action.handler) : nullptr);
    auto hostHandler = action.handler == 1 ? SIG_IGN : Delivered(action) ? Dispatch : SIG_DFL;
    if (std::signal(native, hostHandler) == SIG_ERR) {
        handlers[guest].store(previous);
        return false;
    }
    return true;
}
}

struct GuestStack {
    void* sp;
    std::size_t size;
    int flags;
};
static_assert(sizeof(GuestStack) == 24 && offsetof(GuestStack, flags) == 16);
namespace {
constexpr int SsDisable = 4;
constexpr std::size_t MinSignalStackSize = 2048;
thread_local GuestStack alternateStack{nullptr, 0, SsDisable};
}

extern "C" {
GuestHandler APS5_VABI signal_nid_postfix(int guest, GuestHandler handler) {
    const auto invalid = reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(-1));
    const int native = NativeSignal(guest);
    if (!native || handler == invalid) { *__error_nid_postfix() = 22; return invalid; }
    std::lock_guard lock(registration);
    const GuestSigaction action{reinterpret_cast<std::uintptr_t>(handler), SaRestart, {}};
    if (!Install(guest, action)) { *__error_nid_postfix() = 22; return invalid; }
    const auto previous = reinterpret_cast<GuestHandler>(dispositions[guest].handler);
    dispositions[guest] = action;
    return previous;
}
int APS5_VABI sigaction_nid_postfix(int guest, const GuestSigaction* action, GuestSigaction* previous) {
    if (guest < 1 || guest > MaxSignal) { *__error_nid_postfix() = 22; return -1; }
    if (action && (guest == GuestSigkill || guest == GuestSigstop) && action->handler != 0) { *__error_nid_postfix() = 22; return -1; }
    std::lock_guard lock(registration);
    const GuestSigaction old = dispositions[guest];
    if (action) {
        if (!Install(guest, *action)) { *__error_nid_postfix() = 22; return -1; }
        dispositions[guest] = *action;
    }
    if (previous) *previous = old;
    return 0;
}
int APS5_VABI raise_nid_postfix(int guest) {
    const int native = NativeSignal(guest);
    if (!native) { *__error_nid_postfix() = 22; return -1; }
    {
        std::lock_guard lock(registration);
        if (dispositions[guest].handler > 1 && !Delivered(dispositions[guest])) throw std::runtime_error("raise: SA_SIGINFO handlers are not delivered");
    }
    const int result = std::raise(native);
    if (result) *__error_nid_postfix() = 22;
    return result ? -1 : 0;
}
int APS5_VABI kill_nid_postfix(int pid, int guest) {
    if (guest < 0 || guest > MaxSignal) { *__error_nid_postfix() = 22; return -1; }
    if (pid != getpid_nid_postfix()) throw std::runtime_error("kill: only the calling process can be signalled");
    if (guest == 0) return 0;
    if (!NativeSignal(guest)) throw std::runtime_error("kill: signal " + std::to_string(guest) + " is not delivered");
    return raise_nid_postfix(guest);
}
int APS5_VABI sigaltstack_nid_postfix(const GuestStack* stack, GuestStack* previous) {
    GuestStack replacement = alternateStack;
    if (stack != nullptr) {
        if ((stack->flags & ~SsDisable) != 0) { *__error_nid_postfix() = 22; return -1; }
        if (stack->flags & SsDisable) replacement.flags = SsDisable;
        else if (stack->size < MinSignalStackSize) { *__error_nid_postfix() = 12; return -1; }
        else replacement = {stack->sp, stack->size, 0};
    }
    if (previous != nullptr) *previous = alternateStack;
    alternateStack = replacement;
    return 0;
}
int APS5_VABI _sigprocmask_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    std::lock_guard lock(registration);
    if (previousSet != nullptr) {
        previousSet->bits[0] = blockedMask.load();
        previousSet->bits[1] = 0;
        previousSet->bits[2] = 0;
        previousSet->bits[3] = 0;
    }
    if (set != nullptr) {
        switch (how) {
            case 1: blockedMask.fetch_or(set->bits[0]); break;
            case 2: blockedMask.fetch_and(~set->bits[0]); break;
            case 3: blockedMask.store(set->bits[0]); break;
            default: throw std::invalid_argument("_sigprocmask: invalid how");
        }
    }
    return 0;
}

int APS5_VABI sigprocmask_nid_postfix(int how, const void* set, void* previousSet) {
    return _sigprocmask_nid_postfix(how, static_cast<const GuestSignalSet*>(set),
                                    static_cast<GuestSignalSet*>(previousSet));
}

void APS5_VABI SignalJumpSaveMask_nid_no_patch(GuestSignalSet* saved) {
    _sigprocmask_nid_postfix(1, nullptr, saved);
}

void APS5_VABI SignalJumpRestoreMask_nid_no_patch(const GuestSignalSet* saved) {
    _sigprocmask_nid_postfix(3, saved, nullptr);
#ifndef _WIN32
    sigset_t native;
    sigemptyset(&native);
    for (int guest : {2, 4, 6, 8, 11, 15}) sigaddset(&native, NativeSignal(guest));
    pthread_sigmask(SIG_UNBLOCK, &native, nullptr);
#endif
}
}

#ifdef _WIN32
#define APS5_SIGNAL_JUMP_FUNCTION(name) ".globl " name "\n.def " name "; .scl 2; .type 32; .endef\n" name ":\n"
#else
#define APS5_SIGNAL_JUMP_FUNCTION(name) ".globl " name "\n.type " name ", @function\n" name ":\n"
#endif

asm(".text\n"
    APS5_SIGNAL_JUMP_FUNCTION("sigsetjmp_nid_postfix")
    "    mov (%rsp), %rax\n"
    "    mov %rax, 0(%rdi)\n"
    "    mov %rbx, 8(%rdi)\n"
    "    lea 8(%rsp), %rax\n"
    "    mov %rax, 16(%rdi)\n"
    "    mov %rbp, 24(%rdi)\n"
    "    mov %r12, 32(%rdi)\n"
    "    mov %r13, 40(%rdi)\n"
    "    mov %r14, 48(%rdi)\n"
    "    mov %r15, 56(%rdi)\n"
    "    stmxcsr 64(%rdi)\n"
    "    fnstcw 68(%rdi)\n"
    "    movl %esi, 88(%rdi)\n"
    "    test %esi, %esi\n"
    "    jz 1f\n"
    "    sub $8, %rsp\n"
    "    lea 72(%rdi), %rdi\n"
    "    call SignalJumpSaveMask_nid_no_patch\n"
    "    add $8, %rsp\n"
    "1:\n"
    "    xor %eax, %eax\n"
    "    ret\n"
    APS5_SIGNAL_JUMP_FUNCTION("siglongjmp_nid_postfix")
    "    cmpl $0, 88(%rdi)\n"
    "    je 2f\n"
    "    push %rdi\n"
    "    push %rsi\n"
    "    sub $8, %rsp\n"
    "    lea 72(%rdi), %rdi\n"
    "    call SignalJumpRestoreMask_nid_no_patch\n"
    "    add $8, %rsp\n"
    "    pop %rsi\n"
    "    pop %rdi\n"
    "2:\n"
    "    mov %esi, %eax\n"
    "    test %eax, %eax\n"
    "    jnz 3f\n"
    "    inc %eax\n"
    "3:\n"
    "    mov 8(%rdi), %rbx\n"
    "    mov 16(%rdi), %rsp\n"
    "    mov 24(%rdi), %rbp\n"
    "    mov 32(%rdi), %r12\n"
    "    mov 40(%rdi), %r13\n"
    "    mov 48(%rdi), %r14\n"
    "    mov 56(%rdi), %r15\n"
    "    ldmxcsr 64(%rdi)\n"
    "    fldcw 68(%rdi)\n"
    "    jmp *0(%rdi)\n");

extern "C" {

int APS5_VABI _is_signal_return_nid_postfix(std::uint64_t programCounter) {
    (void)programCounter;
    return 0;
}

}
