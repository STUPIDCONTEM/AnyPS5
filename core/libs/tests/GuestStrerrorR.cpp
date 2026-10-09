#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

extern "C" {
int APS5_VABI strerror_r_nid_postfix(int, char*, std::size_t);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool condition, int line, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "GuestStrerrorR.cpp:%d: %s\n", line, message);
        std::abort();
    }
}

#define REQUIRE(condition, message) Require((condition), __LINE__, (message))

int main() {
    char buffer[128];
    constexpr char knownMessage[] = "Invalid argument";
    constexpr std::size_t knownLength = sizeof(knownMessage) - 1;

    *__error_nid_postfix() = 77;
    REQUIRE(strerror_r_nid_postfix(22, buffer, sizeof(buffer)) == 0, "known error succeeds with ample buffer");
    REQUIRE(std::strcmp(buffer, knownMessage) == 0, "known error text matches");
    REQUIRE(*__error_nid_postfix() == 77, "known error preserves guest errno");

    char exactKnown[sizeof(knownMessage)];
    *__error_nid_postfix() = 77;
    REQUIRE(strerror_r_nid_postfix(22, exactKnown, sizeof(exactKnown)) == 0, "known error succeeds with exact-size buffer");
    REQUIRE(std::strcmp(exactKnown, knownMessage) == 0, "exact-size buffer includes the terminator");
    REQUIRE(*__error_nid_postfix() == 77, "exact-size known error preserves guest errno");

    char shortKnown[sizeof(knownMessage) + 1];
    std::memset(shortKnown, 'x', sizeof(shortKnown));
    *__error_nid_postfix() = 77;
    REQUIRE(strerror_r_nid_postfix(22, shortKnown, knownLength) == 34, "one-byte-short known buffer returns ERANGE");
    REQUIRE(std::strncmp(shortKnown, knownMessage, knownLength - 1) == 0 && shortKnown[knownLength - 1] == '\0', "short known buffer is terminated after truncation");
    REQUIRE(shortKnown[knownLength] == 'x', "short known buffer does not overwrite following sentinel");
    REQUIRE(*__error_nid_postfix() == 77, "short known error preserves guest errno");

    char zeroKnown[] = {'x', 'y'};
    *__error_nid_postfix() = 77;
    REQUIRE(strerror_r_nid_postfix(22, zeroKnown, 0) == 34, "zero-size known buffer returns ERANGE");
    REQUIRE(zeroKnown[0] == 'x' && zeroKnown[1] == 'y', "zero-size known buffer remains untouched");
    REQUIRE(*__error_nid_postfix() == 77, "zero-size known error preserves guest errno");

    char lengthOneKnown[] = {'x', 's'};
    *__error_nid_postfix() = 77;
    REQUIRE(strerror_r_nid_postfix(22, lengthOneKnown, 1) == 34, "one-byte known buffer returns ERANGE");
    REQUIRE(lengthOneKnown[0] == '\0' && lengthOneKnown[1] == 's', "one-byte known buffer contains only NUL and preserves sentinel");
    REQUIRE(*__error_nid_postfix() == 77, "one-byte known error preserves guest errno");

    *__error_nid_postfix() = 77;
    REQUIRE(strerror_r_nid_postfix(-1, buffer, sizeof(buffer)) == 22, "negative unknown error returns EINVAL");
    REQUIRE(std::strcmp(buffer, "Unknown error: -1") == 0, "negative unknown error text includes number");
    REQUIRE(*__error_nid_postfix() == 77, "negative unknown error preserves guest errno");

    const int unknownErrors[] = {97, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
    for (int error : unknownErrors) {
        char expected[128];
        std::snprintf(expected, sizeof(expected), "Unknown error: %d", error);
        *__error_nid_postfix() = 77;
        REQUIRE(strerror_r_nid_postfix(error, buffer, sizeof(buffer)) == 22, "out-of-range error returns EINVAL");
        REQUIRE(std::strcmp(buffer, expected) == 0, "out-of-range error text formats full integer safely");
        REQUIRE(*__error_nid_postfix() == 77, "out-of-range error preserves guest errno");
    }

    char shortUnknown[] = {'x', 'y', 'z', 'w', 'v'};
    *__error_nid_postfix() = 77;
    REQUIRE(strerror_r_nid_postfix(-1, shortUnknown, 4) == 22, "short unknown buffer still returns EINVAL");
    REQUIRE(shortUnknown[0] == 'U' && shortUnknown[1] == 'n' && shortUnknown[2] == 'k' &&
        shortUnknown[3] == '\0' && shortUnknown[4] == 'v', "short unknown text truncates with NUL and preserves sentinel");
    REQUIRE(*__error_nid_postfix() == 77, "short unknown error preserves guest errno");

    char zeroUnknown[] = {'x', 'y'};
    *__error_nid_postfix() = 77;
    REQUIRE(strerror_r_nid_postfix(-1, zeroUnknown, 0) == 22, "zero-size unknown buffer returns EINVAL");
    REQUIRE(zeroUnknown[0] == 'x' && zeroUnknown[1] == 'y', "zero-size unknown buffer remains untouched");
    REQUIRE(*__error_nid_postfix() == 77, "zero-size unknown error preserves guest errno");

    char lengthOneUnknown[] = {'x', 's'};
    *__error_nid_postfix() = 77;
    REQUIRE(strerror_r_nid_postfix(97, lengthOneUnknown, 1) == 22, "one-byte unknown buffer returns EINVAL");
    REQUIRE(lengthOneUnknown[0] == '\0' && lengthOneUnknown[1] == 's', "one-byte unknown buffer contains only NUL and preserves sentinel");
    REQUIRE(*__error_nid_postfix() == 77, "one-byte unknown error preserves guest errno");

    return 0;
}
