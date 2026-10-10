#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

extern "C" {
void* APS5_VABI mmap_nid_postfix(void*, std::size_t, int, int, int, std::int64_t) noexcept;
int APS5_VABI munmap_nid_postfix(void*, std::size_t) noexcept;
int APS5_VABI madvise_nid_postfix(void*, std::size_t, int);
int* APS5_VABI __error_nid_postfix();
}

namespace {
constexpr std::size_t Page = 0x4000;
constexpr int ReadWrite = 3;
constexpr int PrivateAnonymous = 0x1002;
constexpr int Invalid = 22;
constexpr int Normal = 0;
constexpr int Random = 1;
constexpr int Sequential = 2;
constexpr int WillNeed = 3;
constexpr int DontNeed = 4;
constexpr int Free = 5;
constexpr int Core = 9;
constexpr int Protect = 10;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "madvise check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

bool Succeeds(void* address, std::size_t length, int advice) {
    *__error_nid_postfix() = 77;
    return madvise_nid_postfix(address, length, advice) == 0 && *__error_nid_postfix() == 77;
}

bool Fails(void* address, std::size_t length, int advice, int error) {
    *__error_nid_postfix() = 0;
    return madvise_nid_postfix(address, length, advice) == -1 && *__error_nid_postfix() == error;
}

bool Throws(void* address, std::size_t length, int advice) {
    try {
        madvise_nid_postfix(address, length, advice);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

char* Map(std::size_t pages) {
    void* mapped = mmap_nid_postfix(nullptr, pages * Page, ReadWrite, PrivateAnonymous, -1, 0);
    Require(mapped != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    return static_cast<char*>(mapped);
}
}

int main() {
    char* mapped = Map(3);
    for (int advice : {Normal, Random, Sequential, WillNeed, DontNeed, Free, 6, 7, 8, Core})
        Require(Succeeds(mapped, Page, advice));

    Require(Succeeds(mapped + 1, Page, Normal));
    Require(Succeeds(mapped, 0, WillNeed));
    Require(Succeeds(mapped + 3, 1, DontNeed));
    Require(Succeeds(mapped, Page * 3, Random));

    Require(Fails(mapped, Page, -1, Invalid));
    Require(Fails(mapped, Page, Protect + 1, Invalid));
    Require(Fails(mapped, Page, 0x7fffffff, Invalid));

    Require(Fails(nullptr, Page, Normal, Invalid));
    Require(Fails(mapped, std::numeric_limits<std::size_t>::max(), Normal, Invalid));
    Require(Fails(reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - 1), 16, Normal, Invalid));
    Require(Throws(mapped, Page, Protect));

    std::memset(mapped, 0x5a, Page);
    Require(Succeeds(mapped, Page, DontNeed));
    for (std::size_t i = 0; i < Page; ++i) Require(static_cast<unsigned char>(mapped[i]) == 0x5a);

    Require(munmap_nid_postfix(mapped, 3 * Page) == 0);
    Require(Fails(mapped, Page, Normal, Invalid));
    return 0;
}
