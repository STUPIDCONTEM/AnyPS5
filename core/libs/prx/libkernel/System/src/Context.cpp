#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <stdexcept>

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {

struct GuestSignalSet {
    std::uint32_t bits[4];
};

struct GuestMcontext {
    std::uint64_t onstack;
    std::uint64_t rdi;
    std::uint64_t rsi;
    std::uint64_t rdx;
    std::uint64_t rcx;
    std::uint64_t r8;
    std::uint64_t r9;
    std::uint64_t rax;
    std::uint64_t rbx;
    std::uint64_t rbp;
    std::uint64_t r10;
    std::uint64_t r11;
    std::uint64_t r12;
    std::uint64_t r13;
    std::uint64_t r14;
    std::uint64_t r15;
    std::uint32_t trapno;
    std::uint16_t fs;
    std::uint16_t gs;
    std::uint64_t addr;
    std::uint32_t flags;
    std::uint16_t es;
    std::uint16_t ds;
    std::uint64_t err;
    std::uint64_t rip;
    std::uint64_t cs;
    std::uint64_t rflags;
    std::uint64_t rsp;
    std::uint64_t ss;
    std::uint64_t len;
    std::uint64_t fpformat;
    std::uint64_t ownedfp;
    std::uint64_t lbrfrom;
    std::uint64_t lbrto;
    std::uint64_t aux1;
    std::uint64_t aux2;
    std::uint64_t fpstate[104];
    std::uint64_t fsbase;
    std::uint64_t gsbase;
    std::uint64_t spare[6];
};

struct GuestUcontext {
    GuestSignalSet sigmask;
    std::int32_t reserved[12];
    GuestMcontext mcontext;
    GuestUcontext* link;
    void* stackPointer;
    std::uint64_t stackSize;
    std::int32_t stackFlags;
    std::int32_t stackAlign;
    std::int32_t flags;
    std::int32_t spare[4];
    std::int32_t tail[3];
};

constexpr std::size_t McontextOffset = 0x40;
static_assert(offsetof(GuestUcontext, mcontext) == McontextOffset);
static_assert(McontextOffset + offsetof(GuestMcontext, rdi) == 0x48 && McontextOffset + offsetof(GuestMcontext, rsi) == 0x50);
static_assert(McontextOffset + offsetof(GuestMcontext, rdx) == 0x58 && McontextOffset + offsetof(GuestMcontext, rcx) == 0x60);
static_assert(McontextOffset + offsetof(GuestMcontext, r8) == 0x68 && McontextOffset + offsetof(GuestMcontext, r9) == 0x70);
static_assert(McontextOffset + offsetof(GuestMcontext, rax) == 0x78 && McontextOffset + offsetof(GuestMcontext, rbx) == 0x80);
static_assert(McontextOffset + offsetof(GuestMcontext, rbp) == 0x88 && McontextOffset + offsetof(GuestMcontext, r10) == 0x90);
static_assert(McontextOffset + offsetof(GuestMcontext, r11) == 0x98 && McontextOffset + offsetof(GuestMcontext, r12) == 0xa0);
static_assert(McontextOffset + offsetof(GuestMcontext, r13) == 0xa8 && McontextOffset + offsetof(GuestMcontext, r14) == 0xb0);
static_assert(McontextOffset + offsetof(GuestMcontext, r15) == 0xb8 && McontextOffset + offsetof(GuestMcontext, rip) == 0xe0);
static_assert(McontextOffset + offsetof(GuestMcontext, rsp) == 0xf8 && McontextOffset + offsetof(GuestMcontext, len) == 0x108);
static_assert(McontextOffset + offsetof(GuestMcontext, fpstate) == 0x140);
static_assert(sizeof(GuestMcontext) == 0x480);

constexpr int GuestInvalid = 22;
constexpr int GuestFault = 14;
constexpr std::uint64_t MinimumStack = 2048;
constexpr std::size_t RegisterArguments = 6;

}

extern "C" {

void APS5_VABI SignalJumpSaveMask_nid_no_patch(GuestSignalSet* saved);
void APS5_VABI SignalJumpRestoreMask_nid_no_patch(const GuestSignalSet* saved);
[[noreturn]] void APS5_VABI exit_nid_postfix(int code);

int APS5_VABI getcontext_nid_postfix(GuestUcontext* context);
int APS5_VABI setcontext_nid_postfix(const GuestUcontext* context);
void APS5_VABI makecontext_nid_postfix(GuestUcontext* context, void (APS5_VABI *function)(), int argc, ...);

}

#ifdef _WIN32

extern "C" {

int APS5_VABI getcontext_nid_postfix(GuestUcontext*) {
    throw std::runtime_error("getcontext: not implemented on Windows");
}

int APS5_VABI setcontext_nid_postfix(const GuestUcontext*) {
    throw std::runtime_error("setcontext: not implemented on Windows");
}

void APS5_VABI makecontext_nid_postfix(GuestUcontext*, void (APS5_VABI *)(), int, ...) {
    throw std::runtime_error("makecontext: not implemented on Windows");
}

}

#else

extern "C" {

void Aps5ContextRestore(const GuestUcontext* context);
void Aps5ContextStart();

[[noreturn]] void Aps5ContextDone(const GuestUcontext* context) {
    if (context->link == nullptr) exit_nid_postfix(0);
    setcontext_nid_postfix(context->link);
    throw std::runtime_error("makecontext: the linked context could not be resumed");
}

int APS5_VABI setcontext_nid_postfix(const GuestUcontext* context) {
    if (context == nullptr) {
        *__error_nid_postfix() = GuestFault;
        return -1;
    }
    if (context->mcontext.len != sizeof(GuestMcontext)) {
        *__error_nid_postfix() = GuestInvalid;
        return -1;
    }
    Aps5ContextRestore(context);
    __builtin_unreachable();
}

void APS5_VABI makecontext_nid_postfix(GuestUcontext* context, void (APS5_VABI *function)(), int argc, ...) {
    if (context == nullptr) return;
    if (context->stackPointer == nullptr || context->stackSize < MinimumStack) {
        context->mcontext.len = 0;
        return;
    }
    if (argc < 0) throw std::invalid_argument("makecontext: negative argument count");
    const auto count = static_cast<std::size_t>(argc);
    const std::size_t stackArguments = count > RegisterArguments ? count - RegisterArguments : 0;
    auto top = reinterpret_cast<std::uintptr_t>(context->stackPointer) + static_cast<std::uintptr_t>(context->stackSize);
    top = (top - stackArguments * sizeof(std::uint64_t)) & ~static_cast<std::uintptr_t>(15);
    auto* stack = reinterpret_cast<std::uint64_t*>(top);

    auto& registers = context->mcontext;
    std::uint64_t* const targets[RegisterArguments] = {&registers.rdi, &registers.rsi, &registers.rdx, &registers.rcx, &registers.r8, &registers.r9};
    std::va_list arguments;
    va_start(arguments, argc);
    for (std::size_t index = 0; index < count; ++index) {
        const auto value = va_arg(arguments, std::uint64_t);
        if (index < RegisterArguments) *targets[index] = value;
        else stack[index - RegisterArguments] = value;
    }
    va_end(arguments);

    registers.rbx = reinterpret_cast<std::uint64_t>(function);
    registers.r12 = reinterpret_cast<std::uint64_t>(context);
    registers.rip = reinterpret_cast<std::uint64_t>(&Aps5ContextStart);
    registers.rsp = reinterpret_cast<std::uint64_t>(stack);
    registers.len = sizeof(GuestMcontext);
}

}

asm(".text\n"
    ".globl getcontext_nid_postfix\n"
    ".type getcontext_nid_postfix, @function\n"
    "getcontext_nid_postfix:\n"
    "    mov %rdi, 0x48(%rdi)\n"
    "    mov %rsi, 0x50(%rdi)\n"
    "    mov %rdx, 0x58(%rdi)\n"
    "    mov %rcx, 0x60(%rdi)\n"
    "    mov %r8, 0x68(%rdi)\n"
    "    mov %r9, 0x70(%rdi)\n"
    "    movq $0, 0x78(%rdi)\n"
    "    mov %rbx, 0x80(%rdi)\n"
    "    mov %rbp, 0x88(%rdi)\n"
    "    mov %r10, 0x90(%rdi)\n"
    "    mov %r11, 0x98(%rdi)\n"
    "    mov %r12, 0xa0(%rdi)\n"
    "    mov %r13, 0xa8(%rdi)\n"
    "    mov %r14, 0xb0(%rdi)\n"
    "    mov %r15, 0xb8(%rdi)\n"
    "    mov (%rsp), %rax\n"
    "    mov %rax, 0xe0(%rdi)\n"
    "    lea 8(%rsp), %rax\n"
    "    mov %rax, 0xf8(%rdi)\n"
    "    movq $0x480, 0x108(%rdi)\n"
    "    fnstcw 0x140(%rdi)\n"
    "    stmxcsr 0x158(%rdi)\n"
    "    sub $8, %rsp\n"
    "    call SignalJumpSaveMask_nid_no_patch\n"
    "    add $8, %rsp\n"
    "    xor %eax, %eax\n"
    "    ret\n"
    ".size getcontext_nid_postfix, .-getcontext_nid_postfix\n"
    ".globl Aps5ContextRestore\n"
    ".type Aps5ContextRestore, @function\n"
    "Aps5ContextRestore:\n"
    "    push %rdi\n"
    "    call SignalJumpRestoreMask_nid_no_patch\n"
    "    pop %rdi\n"
    "    ldmxcsr 0x158(%rdi)\n"
    "    fldcw 0x140(%rdi)\n"
    "    mov 0x50(%rdi), %rsi\n"
    "    mov 0x58(%rdi), %rdx\n"
    "    mov 0x60(%rdi), %rcx\n"
    "    mov 0x68(%rdi), %r8\n"
    "    mov 0x70(%rdi), %r9\n"
    "    mov 0x80(%rdi), %rbx\n"
    "    mov 0x88(%rdi), %rbp\n"
    "    mov 0x90(%rdi), %r10\n"
    "    mov 0x98(%rdi), %r11\n"
    "    mov 0xa0(%rdi), %r12\n"
    "    mov 0xa8(%rdi), %r13\n"
    "    mov 0xb0(%rdi), %r14\n"
    "    mov 0xb8(%rdi), %r15\n"
    "    mov 0xf8(%rdi), %rsp\n"
    "    mov 0xe0(%rdi), %rax\n"
    "    push %rax\n"
    "    mov 0x78(%rdi), %rax\n"
    "    mov 0x48(%rdi), %rdi\n"
    "    ret\n"
    ".size Aps5ContextRestore, .-Aps5ContextRestore\n"
    ".globl Aps5ContextStart\n"
    ".type Aps5ContextStart, @function\n"
    "Aps5ContextStart:\n"
    "    call *%rbx\n"
    "    mov %r12, %rdi\n"
    "    call Aps5ContextDone\n"
    "    ud2\n"
    ".size Aps5ContextStart, .-Aps5ContextStart\n");

#endif
