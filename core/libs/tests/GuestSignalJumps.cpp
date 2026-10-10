#include "prx/libc/include/general/VabiMacros.hpp"
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>

struct GuestJumpBuffer {
    std::uint64_t words[12];
};
static_assert(sizeof(GuestJumpBuffer) == 96);
struct GuestSignalSet {
    std::uint32_t bits[4];
};
using Handler = void (APS5_VABI *)(int);
extern "C" {
int APS5_VABI sigsetjmp_nid_postfix(GuestJumpBuffer*, int);
[[noreturn]] void APS5_VABI siglongjmp_nid_postfix(GuestJumpBuffer*, int);
int APS5_VABI setjmp_nid_postfix(GuestJumpBuffer*);
int APS5_VABI sigprocmask_nid_postfix(int, const void*, void*);
Handler APS5_VABI signal_nid_postfix(int, Handler);
int APS5_VABI raise_nid_postfix(int);
}

static void Require(bool value) { if (!value) std::abort(); }

static std::uint32_t Blocked() {
    GuestSignalSet current{};
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    return current.bits[0];
}

static void SetBlocked(std::uint32_t bits) {
    const GuestSignalSet set{{bits, 0, 0, 0}};
    Require(sigprocmask_nid_postfix(3, &set, nullptr) == 0);
}

static GuestJumpBuffer handlerJump;
static volatile std::sig_atomic_t handled = 0;

static void APS5_VABI EscapingHandler(int value) {
    handled = handled + 1;
    siglongjmp_nid_postfix(&handlerJump, value);
}

[[noreturn]] static void JumpBack(GuestJumpBuffer* buffer, int value) {
    siglongjmp_nid_postfix(buffer, value);
}

int main() {
    GuestJumpBuffer buffer;
    volatile int passes = 0;

    std::memset(&buffer, 0xa5, sizeof(buffer));
    SetBlocked(0x20);
    int result = sigsetjmp_nid_postfix(&buffer, 0);
    passes = passes + 1;
    if (result == 0) {
        Require(buffer.words[11] == 0xa5a5a5a500000000ull);
        SetBlocked(0x400);
        JumpBack(&buffer, 7);
    }
    Require(result == 7 && passes == 2);
    Require(Blocked() == 0x400);

    passes = 0;
    result = sigsetjmp_nid_postfix(&buffer, 0);
    passes = passes + 1;
    if (result == 0) JumpBack(&buffer, 0);
    Require(result == 1 && passes == 2);

    passes = 0;
    SetBlocked(0x20);
    result = sigsetjmp_nid_postfix(&buffer, 1);
    passes = passes + 1;
    if (result == 0) {
        Require(static_cast<std::uint32_t>(buffer.words[11]) == 1);
        Require(static_cast<std::uint32_t>(buffer.words[9]) == 0x20);
        SetBlocked(0x400);
        JumpBack(&buffer, 3);
    }
    Require(result == 3 && passes == 2);
    Require(Blocked() == 0x20);

    passes = 0;
    std::memset(&buffer, 0, sizeof(buffer));
    SetBlocked(0x400);
    result = setjmp_nid_postfix(&buffer);
    passes = passes + 1;
    if (result == 0) {
        SetBlocked(0x20);
        JumpBack(&buffer, 9);
    }
    Require(result == 9 && passes == 2);
    Require(Blocked() == 0x20);

    SetBlocked(0);
    Require(signal_nid_postfix(15, EscapingHandler) != reinterpret_cast<Handler>(static_cast<std::uintptr_t>(-1)));
    for (int round = 1; round <= 2; ++round) {
        result = sigsetjmp_nid_postfix(&handlerJump, 1);
        if (result == 0) {
            raise_nid_postfix(15);
            std::abort();
        }
        Require(result == 15 && handled == round);
        Require(Blocked() == 0);
    }
    Require(signal_nid_postfix(15, nullptr) == EscapingHandler);
    return 0;
}
