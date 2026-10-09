#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_POSIX_COMMON_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_POSIX_COMMON_HPP

#include <cstdint>
#include <limits>
#include "SceTypes.hpp"

extern "C" int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);

namespace PosixThread {

constexpr int GUEST_EINVAL = 22;
constexpr int GUEST_ETIMEDOUT = 60;

inline int ToErrno(int sceResult) {
    return sceResult == 0 ? 0 : static_cast<int>(static_cast<std::uint32_t>(sceResult) & 0xFFFFu);
}

// Converts an absolute deadline on the guest clock into a relative wait, clamped to what the sce layer accepts.
inline bool RelativeMicroseconds(int clockId, const KernelTimespec* abstime, KernelUseconds* usec) {
    if (!abstime || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) return false;
    KernelTimespec now{};
    clock_gettime_nid_postfix(clockId, &now);
    if (abstime->tv_sec < now.tv_sec ||
        (abstime->tv_sec == now.tv_sec && abstime->tv_nsec <= now.tv_nsec)) {
        *usec = 0;
        return true;
    }

    std::uint64_t seconds = static_cast<std::uint64_t>(abstime->tv_sec) - static_cast<std::uint64_t>(now.tv_sec);
    std::uint64_t nanoseconds = 0;
    if (abstime->tv_nsec >= now.tv_nsec) {
        nanoseconds = static_cast<std::uint64_t>(abstime->tv_nsec - now.tv_nsec);
    } else {
        --seconds;
        nanoseconds = 1000000000ULL - static_cast<std::uint64_t>(now.tv_nsec - abstime->tv_nsec);
    }

    constexpr std::uint64_t microsecondsPerSecond = 1000000ULL;
    constexpr std::uint64_t maxMicroseconds = std::numeric_limits<KernelUseconds>::max();
    constexpr std::uint64_t maxWholeSeconds = maxMicroseconds / microsecondsPerSecond;
    if (seconds > maxWholeSeconds) {
        *usec = static_cast<KernelUseconds>(maxMicroseconds);
        return true;
    }

    const std::uint64_t remaining = seconds * microsecondsPerSecond + nanoseconds / 1000ULL;
    *usec = static_cast<KernelUseconds>(remaining > maxMicroseconds ? maxMicroseconds : remaining);
    return true;
}

}

#endif
