#include "SceTypes.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <future>
#include <initializer_list>
#include <limits>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadSetprio(Pthread thread, int prio);
int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize);
int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param);
int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask);
int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, std::size_t* stackSize);
int APS5_VABI scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state);
int APS5_VABI pthread_attr_setstacksize_nid_postfix(PthreadAttr* attr, std::size_t stackSize);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int EXPLICIT_SCHED = 0;
static constexpr int DETACH_JOINABLE = 0;
static constexpr int CREATION_PRIORITY = 256;
static constexpr int UPDATED_PRIORITY = 767;
static constexpr KernelCpumask CREATION_AFFINITY = 0x3;
static constexpr KernelCpumask UPDATED_AFFINITY = 0x1000;
static constexpr std::size_t STACK_SIZE = 2u << 20;
static constexpr std::size_t MIN_STACK_SIZE = 16384;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ENOTSUP = 45;

static void Require(bool value) { if (!value) std::abort(); }

struct ReportedAttributes {
    int priority = 0;
    KernelCpumask affinity = 0;
    std::size_t stackSize = 0;
    int detachState = -1;
};

extern "C" {
int APS5_VABI pthread_attr_setstack_nid_postfix(PthreadAttr* attr, void* addr, std::size_t size);
int APS5_VABI pthread_attr_getstack_nid_postfix(const PthreadAttr* attr, void** addr, std::size_t* size);
int APS5_VABI pthread_attr_setschedpolicy_nid_postfix(PthreadAttr* attr, int policy);
int APS5_VABI pthread_attr_getschedpolicy_nid_postfix(const PthreadAttr* attr, int* policy);
int* APS5_VABI __error_nid_postfix();
}

static ReportedAttributes Query(Pthread thread) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(thread, &attr) == SCE_OK);
    ReportedAttributes reported;
    KernelSchedParam param{};
    Require(scePthreadAttrGetschedparam(&attr, &param) == SCE_OK);
    reported.priority = param.sched_priority;
    Require(scePthreadAttrGetaffinity(&attr, &reported.affinity) == SCE_OK);
    Require(scePthreadAttrGetstacksize(&attr, &reported.stackSize) == SCE_OK);
    Require(scePthreadAttrGetdetachstate(&attr, &reported.detachState) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    return reported;
}

static void CheckSchedulingPolicyValidation() {
    PthreadAttr attr = nullptr;
    *__error_nid_postfix() = 123;
    Require(pthread_attr_setschedpolicy_nid_postfix(nullptr, 0) == GUEST_EINVAL);
    Require(pthread_attr_setschedpolicy_nid_postfix(&attr, 0) == GUEST_EINVAL);
    Require(*__error_nid_postfix() == 123);
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    int policy = -1;
    Require(pthread_attr_getschedpolicy_nid_postfix(&attr, &policy) == SCE_OK && policy == 1);
    for (const int supported : {1, 2, 3}) {
        Require(pthread_attr_setschedpolicy_nid_postfix(&attr, supported) == SCE_OK);
        policy = -1;
        Require(pthread_attr_getschedpolicy_nid_postfix(&attr, &policy) == SCE_OK && policy == supported);
    }
    const int rejected[] = {std::numeric_limits<int>::min(), -1, 0, 4, std::numeric_limits<int>::max()};
    for (const int unsupported : rejected) {
        const int previous = policy;
        *__error_nid_postfix() = 123;
        const int result = pthread_attr_setschedpolicy_nid_postfix(&attr, unsupported);
        if (result != GUEST_ENOTSUP) std::fprintf(stderr, "policy %d: expected ENOTSUP %d, got %d\n", unsupported, GUEST_ENOTSUP, result);
        Require(result == GUEST_ENOTSUP);
        Require(*__error_nid_postfix() == 123);
        policy = -1;
        Require(pthread_attr_getschedpolicy_nid_postfix(&attr, &policy) == SCE_OK && policy == previous);
    }
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    Require(pthread_attr_setschedpolicy_nid_postfix(&attr, 0) == GUEST_EINVAL);
    Require(*__error_nid_postfix() == 123);
}

static void CheckStackSizeLimit() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    std::size_t stackSize = 0;
    Require(scePthreadAttrSetstacksize(&attr, MIN_STACK_SIZE) == SCE_OK);
    Require(scePthreadAttrGetstacksize(&attr, &stackSize) == SCE_OK && stackSize == MIN_STACK_SIZE);
    Require(scePthreadAttrSetstacksize(&attr, MIN_STACK_SIZE - 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetstacksize(&attr, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_attr_setstacksize_nid_postfix(&attr, MIN_STACK_SIZE - 1) == GUEST_EINVAL);
    Require(scePthreadAttrGetstacksize(&attr, &stackSize) == SCE_OK && stackSize == MIN_STACK_SIZE);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
}

static void* APS5_VABI Worker(void* arg) {
    static_cast<std::future<void>*>(arg)->get();
    return nullptr;
}

int main() {
    CheckSchedulingPolicyValidation();
    CheckStackSizeLimit();
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrSetinheritsched(&attr, EXPLICIT_SCHED) == SCE_OK);
    const KernelSchedParam requested{CREATION_PRIORITY};
    Require(scePthreadAttrSetschedparam(&attr, &requested) == SCE_OK);
    Require(scePthreadAttrSetaffinity(&attr, CREATION_AFFINITY) == SCE_OK);
    Require(scePthreadAttrSetstacksize(&attr, STACK_SIZE) == SCE_OK);

    std::promise<void> release;
    auto released = release.get_future();
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, Worker, &released, nullptr) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);

    auto reported = Query(thread);
    Require(reported.priority == CREATION_PRIORITY);
    Require(reported.affinity == CREATION_AFFINITY);
    Require(reported.stackSize == STACK_SIZE);
    Require(reported.detachState == DETACH_JOINABLE);

    Require(scePthreadSetprio(thread, UPDATED_PRIORITY) == SCE_OK);
    Require(scePthreadSetaffinity(thread, UPDATED_AFFINITY) == SCE_OK);
    reported = Query(thread);
    Require(reported.priority == UPDATED_PRIORITY);
    Require(reported.affinity == UPDATED_AFFINITY);
    Require(reported.stackSize == STACK_SIZE);
    Require(reported.detachState == DETACH_JOINABLE);

    PthreadAttr destroyed = nullptr;
    Require(scePthreadAttrGet(thread, &destroyed) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGet(thread, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(nullptr, &attr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);

    release.set_value();
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    PthreadAttr stackAttr = nullptr;
    char stackMarker;
    Require(pthread_attr_setstack_nid_postfix(nullptr, &stackMarker, STACK_SIZE) == 22);
    Require(pthread_attr_setstack_nid_postfix(&stackAttr, &stackMarker, STACK_SIZE) == 22);
    Require(scePthreadAttrInit(&stackAttr) == SCE_OK);
    Require(pthread_attr_setstack_nid_postfix(&stackAttr, &stackMarker, STACK_SIZE) == 0);
    Require(pthread_attr_setstack_nid_postfix(&stackAttr, nullptr, STACK_SIZE) == 22);
    Require(pthread_attr_setstack_nid_postfix(&stackAttr, &stackMarker, 16383) == 22);
    void* reportedAddress = nullptr;
    std::size_t reportedSize = 0;
    Require(pthread_attr_getstack_nid_postfix(&stackAttr, &reportedAddress, &reportedSize) == 0);
    Require(reportedAddress == &stackMarker && reportedSize == STACK_SIZE);
    Require(scePthreadAttrDestroy(&stackAttr) == SCE_OK);
    Require(pthread_attr_setstack_nid_postfix(&stackAttr, &stackMarker, STACK_SIZE) == 22);
}
