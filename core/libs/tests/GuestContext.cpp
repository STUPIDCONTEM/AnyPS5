#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

struct GuestSignalSet {
    std::uint32_t bits[4];
};

struct GuestUcontext {
    GuestSignalSet sigmask;
    std::int32_t reserved[12];
    std::uint8_t mcontext[0x480];
    GuestUcontext* link;
    void* stackPointer;
    std::uint64_t stackSize;
    std::int32_t stackFlags;
    std::int32_t stackAlign;
    std::int32_t flags;
    std::int32_t spare[4];
    std::int32_t tail[3];
};
static_assert(offsetof(GuestUcontext, link) == 0x40 + 0x480);

extern "C" {
int APS5_VABI getcontext_nid_postfix(GuestUcontext*);
int APS5_VABI setcontext_nid_postfix(const GuestUcontext*);
void APS5_VABI makecontext_nid_postfix(GuestUcontext*, void (APS5_VABI *)(), int, ...);
int APS5_VABI sigprocmask_nid_postfix(int, const void*, void*);
int* APS5_VABI __error_nid_postfix();
}

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "context check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

std::uint32_t Blocked() {
    GuestSignalSet current{};
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    return current.bits[0];
}

void SetBlocked(std::uint32_t bits) {
    const GuestSignalSet set{{bits, 0, 0, 0}};
    Require(sigprocmask_nid_postfix(3, &set, nullptr) == 0);
}

alignas(16) GuestUcontext mainContext;
alignas(16) GuestUcontext childContext;
alignas(16) GuestUcontext thirdContext;
alignas(64) unsigned char childStack[64 * 1024];
alignas(64) unsigned char thirdStack[64 * 1024];
volatile int order = 0;
volatile std::uint64_t seen[9];
volatile bool aligned = false;
volatile int thirdRan = 0;

void APS5_VABI Child(std::uint64_t a, std::uint64_t b, std::uint64_t c, std::uint64_t d, std::uint64_t e, std::uint64_t f, std::uint64_t g, std::uint64_t h) {
    aligned = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)) % 16 == 0;
    seen[0] = a; seen[1] = b; seen[2] = c; seen[3] = d; seen[4] = e; seen[5] = f; seen[6] = g; seen[7] = h;
    alignas(16) volatile double wide[2] = {1.5, 2.5};
    seen[8] = static_cast<std::uint64_t>(wide[0] + wide[1]);
    order = order + 1;
}

void APS5_VABI Third() {
    thirdRan = thirdRan + 1;
}

void APS5_VABI Chained() {
    order = order * 10 + 2;
    setcontext_nid_postfix(&mainContext);
}
}

int main() {
    {
        volatile int passes = 0;
        SetBlocked(0x20);
        getcontext_nid_postfix(&mainContext);
        passes = passes + 1;
        if (passes < 3) {
            SetBlocked(0x400);
            setcontext_nid_postfix(&mainContext);
        }
        Require(passes == 3);
        Require(Blocked() == 0x20);
        SetBlocked(0);
    }

    std::memset(&childContext, 0, sizeof(childContext));
    Require(getcontext_nid_postfix(&childContext) == 0);
    childContext.stackPointer = childStack;
    childContext.stackSize = sizeof(childStack);
    childContext.link = &mainContext;
    makecontext_nid_postfix(&childContext, reinterpret_cast<void (APS5_VABI *)()>(Child), 8,
                            std::uint64_t{11}, std::uint64_t{22}, std::uint64_t{33}, std::uint64_t{44}, std::uint64_t{55},
                            std::uint64_t{66}, std::uint64_t{0x7777777777777777ull}, std::uint64_t{88});
    volatile bool resumed = false;
    Require(getcontext_nid_postfix(&mainContext) == 0);
    if (!resumed) {
        resumed = true;
        setcontext_nid_postfix(&childContext);
        std::abort();
    }
    Require(order == 1);
    Require(seen[0] == 11 && seen[1] == 22 && seen[2] == 33 && seen[3] == 44 && seen[4] == 55 && seen[5] == 66);
    Require(seen[6] == 0x7777777777777777ull && seen[7] == 88 && seen[8] == 4);
    Require(aligned);

    order = 0;
    std::memset(&childContext, 0, sizeof(childContext));
    std::memset(&thirdContext, 0, sizeof(thirdContext));
    Require(getcontext_nid_postfix(&thirdContext) == 0);
    thirdContext.stackPointer = thirdStack;
    thirdContext.stackSize = sizeof(thirdStack);
    Require(getcontext_nid_postfix(&childContext) == 0);
    childContext.stackPointer = childStack;
    childContext.stackSize = sizeof(childStack);
    childContext.link = &thirdContext;
    thirdContext.link = &mainContext;
    makecontext_nid_postfix(&thirdContext, Chained, 0);
    makecontext_nid_postfix(&childContext, reinterpret_cast<void (APS5_VABI *)()>(Third), 0);
    volatile bool second = false;
    Require(getcontext_nid_postfix(&mainContext) == 0);
    if (!second) {
        second = true;
        order = 1;
        setcontext_nid_postfix(&childContext);
        std::abort();
    }
    Require(thirdRan == 1 && order == 12);

    GuestUcontext invalid;
    std::memset(&invalid, 0, sizeof(invalid));
    *__error_nid_postfix() = 0;
    Require(setcontext_nid_postfix(&invalid) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(setcontext_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);

    GuestUcontext small;
    std::memset(&small, 0, sizeof(small));
    Require(getcontext_nid_postfix(&small) == 0);
    small.stackPointer = childStack;
    small.stackSize = 16;
    makecontext_nid_postfix(&small, Third, 0);
    *__error_nid_postfix() = 0;
    Require(setcontext_nid_postfix(&small) == -1 && *__error_nid_postfix() == 22);
    makecontext_nid_postfix(nullptr, Third, 0);

#ifndef _WIN32
    const pid_t child = ::fork();
    Require(child >= 0);
    if (child == 0) {
        std::memset(&childContext, 0, sizeof(childContext));
        getcontext_nid_postfix(&childContext);
        childContext.stackPointer = childStack;
        childContext.stackSize = sizeof(childStack);
        childContext.link = nullptr;
        makecontext_nid_postfix(&childContext, Third, 0);
        setcontext_nid_postfix(&childContext);
        _exit(99);
    }
    int status = 0;
    Require(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
#endif
    return 0;
}
