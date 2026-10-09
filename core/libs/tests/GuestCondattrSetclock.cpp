#include "SceTypes.hpp"
#include <climits>
#include <cstdio>
#include <initializer_list>
#include <cstdlib>

extern "C" {
int APS5_VABI pthread_condattr_init_nid_postfix(PthreadCondattr* attr);
int APS5_VABI pthread_condattr_destroy_nid_postfix(PthreadCondattr* attr);
int APS5_VABI pthread_condattr_setclock_nid_postfix(PthreadCondattr* attr, KernelClockid clockId);
int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr);
int APS5_VABI pthread_cond_destroy_nid_postfix(PthreadCond* cond);
int APS5_VABI pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime);
int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr);
int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI clock_gettime_nid_postfix(KernelClockid clockId, KernelTimespec* tp);
int* APS5_VABI __error_nid_postfix();
}

static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ETIMEDOUT = 60;
static constexpr int GUEST_EINTR = 4;

static void RequireResult(const char* operation, int actual, int expected) {
    if (actual != expected) {
        std::fprintf(stderr, "%s: expected %d, got %d\n", operation, expected, actual);
        std::abort();
    }
}

static void RequireGuestErrno(const char* operation, int expected) {
    const int actual = *__error_nid_postfix();
    if (actual != expected) {
        std::fprintf(stderr, "%s changed guest errno: expected %d, got %d\n", operation, expected, actual);
        std::abort();
    }
}

int main() {
    PthreadCondattr attr = nullptr;
    RequireResult("pthread_condattr_setclock null pointer", pthread_condattr_setclock_nid_postfix(nullptr, 4), GUEST_EINVAL);
    RequireResult("pthread_condattr_setclock null attribute", pthread_condattr_setclock_nid_postfix(&attr, 4), GUEST_EINVAL);
    RequireResult("pthread_condattr_init", pthread_condattr_init_nid_postfix(&attr), 0);

    for (const KernelClockid clockId : {0, 1, 2, 4}) {
        *__error_nid_postfix() = GUEST_EINTR;
        RequireResult("pthread_condattr_setclock valid clock", pthread_condattr_setclock_nid_postfix(&attr, clockId), 0);
        RequireGuestErrno("pthread_condattr_setclock valid clock", GUEST_EINTR);
    }

    const KernelClockid invalidClockIds[] = {3, 5, -1, INT_MAX};
    for (const KernelClockid clockId : invalidClockIds) {
        *__error_nid_postfix() = GUEST_EINTR;
        RequireResult("pthread_condattr_setclock invalid clock", pthread_condattr_setclock_nid_postfix(&attr, clockId), GUEST_EINVAL);
        RequireGuestErrno("pthread_condattr_setclock invalid clock", GUEST_EINTR);
    }

    PthreadCond cond = nullptr;
    PthreadMutex mutex = nullptr;
    RequireResult("pthread_cond_init", pthread_cond_init_nid_postfix(&cond, &attr), 0);
    RequireResult("pthread_mutex_init", pthread_mutex_init_nid_postfix(&mutex, nullptr), 0);
    RequireResult("pthread_mutex_lock", pthread_mutex_lock_nid_postfix(&mutex), 0);

    KernelTimespec deadline{};
    RequireResult("clock_gettime monotonic", clock_gettime_nid_postfix(4, &deadline), 0);
    RequireResult("pthread_cond_timedwait expired monotonic deadline", pthread_cond_timedwait_nid_postfix(&cond, &mutex, &deadline), GUEST_ETIMEDOUT);

    RequireResult("pthread_mutex_unlock", pthread_mutex_unlock_nid_postfix(&mutex), 0);
    RequireResult("pthread_cond_destroy", pthread_cond_destroy_nid_postfix(&cond), 0);
    RequireResult("pthread_mutex_destroy", pthread_mutex_destroy_nid_postfix(&mutex), 0);
    RequireResult("pthread_condattr_destroy", pthread_condattr_destroy_nid_postfix(&attr), 0);
    RequireResult("pthread_condattr_setclock destroyed attribute", pthread_condattr_setclock_nid_postfix(&attr, 4), GUEST_EINVAL);
}
