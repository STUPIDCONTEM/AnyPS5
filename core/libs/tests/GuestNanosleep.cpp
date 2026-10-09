#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <chrono>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

extern "C" {
int APS5_VABI nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int APS5_VABI _nanosleep_nid_postfix(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int APS5_VABI sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
int* APS5_VABI __error_nid_postfix();
}

using Nanosleep = int (APS5_VABI *)(const KernelTimespec*, KernelTimespec*);

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000E);
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);

static constexpr int GUEST_EACCES = 13;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_CLOCK_MONOTONIC = 4;

static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;
static constexpr std::int64_t NANOS_PER_MILLISECOND = 1000000LL;
static constexpr std::int64_t SLEEP_NANOS = 50 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t EARLY_WAKE_MARGIN_NANOS = 5 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t NO_SLEEP_LIMIT_NANOS = 2 * NANOS_PER_SECOND;

static void Require(bool value) { if (!value) std::abort(); }

static std::int64_t MonotonicNanos() {
    KernelTimespec time{-1, -1};
    Require(clock_gettime_nid_postfix(GUEST_CLOCK_MONOTONIC, &time) == SCE_OK);
    return time.tv_sec * NANOS_PER_SECOND + time.tv_nsec;
}

static void PosixRejects(Nanosleep sleep, const KernelTimespec* request, int error) {
    *__error_nid_postfix() = GUEST_EACCES;
    Require(sleep(request, nullptr) == -1);
    Require(*__error_nid_postfix() == error);
}

static void PosixRejectsInvalidRequests(Nanosleep sleep) {
    const KernelTimespec negativeNanos{0, -1};
    const KernelTimespec wholeSecondOfNanos{0, NANOS_PER_SECOND};
    const KernelTimespec negativeBoth{-1, -1};
    PosixRejects(sleep, &negativeNanos, GUEST_EINVAL);
    PosixRejects(sleep, &wholeSecondOfNanos, GUEST_EINVAL);
    PosixRejects(sleep, &negativeBoth, GUEST_EINVAL);
    PosixRejects(sleep, nullptr, GUEST_EFAULT);
}

static void SceRejectsInvalidRequests() {
    const KernelTimespec negativeNanos{0, -1};
    const KernelTimespec wholeSecondOfNanos{0, NANOS_PER_SECOND};
    const KernelTimespec negativeBoth{-1, -1};
    Require(sceKernelNanosleep(&negativeNanos, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&wholeSecondOfNanos, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&negativeBoth, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
}

static void ReturnsAtOnce(Nanosleep sleep, KernelTimespec request) {
    const std::int64_t start = MonotonicNanos();
    *__error_nid_postfix() = GUEST_EACCES;
    Require(sleep(&request, nullptr) == SCE_OK);
    Require(*__error_nid_postfix() == GUEST_EACCES);
    Require(MonotonicNanos() - start < NO_SLEEP_LIMIT_NANOS);
}

static void NegativeSecondsDoNotSleep(Nanosleep sleep) {
    ReturnsAtOnce(sleep, {-1, 0});
    ReturnsAtOnce(sleep, {-1, NANOS_PER_SECOND - 1});
    ReturnsAtOnce(sleep, {INT64_MIN, 0});
    ReturnsAtOnce(sleep, {0, 0});
}

static void SleepsForTheRequest(Nanosleep sleep) {
    const KernelTimespec request{0, SLEEP_NANOS};
    const std::int64_t start = MonotonicNanos();
    *__error_nid_postfix() = GUEST_EACCES;
    Require(sleep(&request, nullptr) == SCE_OK);
    Require(*__error_nid_postfix() == GUEST_EACCES);
    Require(MonotonicNanos() - start >= SLEEP_NANOS - EARLY_WAKE_MARGIN_NANOS);
}

static constexpr char Ready = 'R';

static int HugeNanosleepChild() {
#ifdef _WIN32
    DWORD written = 0;
    if (!::WriteFile(::GetStdHandle(STD_OUTPUT_HANDLE), &Ready, 1, &written, nullptr) || written != 1) return 90;
#else
    if (::write(STDOUT_FILENO, &Ready, 1) != 1) return 90;
#endif
    const KernelTimespec request{std::int64_t{1} << 55, 0};
    return nanosleep_nid_postfix(&request, nullptr) == 0 ? 0 : 91;
}

#ifdef _WIN32
static bool HugeRequestStaysActive(const char*) {
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!::CreatePipe(&readPipe, &writePipe, &security, 0)) return false;
    if (!::SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
        ::CloseHandle(readPipe);
        ::CloseHandle(writePipe);
        return false;
    }
    std::vector<wchar_t> executable(32768);
    const DWORD length = ::GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size()) {
        ::CloseHandle(readPipe);
        ::CloseHandle(writePipe);
        return false;
    }
    std::wstring command = L"\"" + std::wstring(executable.data(), length) + L"\" --huge-nanosleep-child";
    std::vector<wchar_t> commandLine(command.begin(), command.end());
    commandLine.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = writePipe;
    startup.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    const BOOL started = ::CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    ::CloseHandle(writePipe);
    if (!started) {
        ::CloseHandle(readPipe);
        return false;
    }
    bool ready = false;
    const ULONGLONG deadline = ::GetTickCount64() + 5000;
    while (::GetTickCount64() < deadline) {
        DWORD available = 0;
        if (!::PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr)) break;
        if (available != 0) {
            char byte = 0;
            DWORD received = 0;
            ready = ::ReadFile(readPipe, &byte, 1, &received, nullptr) && received == 1 && byte == Ready;
            break;
        }
        if (::WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) break;
        ::Sleep(10);
    }
    const bool active = ready && ::WaitForSingleObject(process.hProcess, 250) == WAIT_TIMEOUT;
    ::TerminateProcess(process.hProcess, 0);
    const bool reaped = ::WaitForSingleObject(process.hProcess, 5000) == WAIT_OBJECT_0;
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    ::CloseHandle(readPipe);
    return active && reaped;
}
#else
static int PollFor(pollfd& event, int milliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    for (;;) {
        const int result = ::poll(&event, 1, milliseconds);
        if (result >= 0 || errno != EINTR) return result;
        const auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero()) return 0;
        milliseconds = static_cast<int>(std::chrono::ceil<std::chrono::milliseconds>(remaining).count());
    }
}

static bool HugeRequestStaysActive(const char* executable) {
    int pipes[2]{};
    if (::pipe(pipes) != 0) return false;
    posix_spawn_file_actions_t actions{};
    if (::posix_spawn_file_actions_init(&actions) != 0) {
        ::close(pipes[0]);
        ::close(pipes[1]);
        return false;
    }
    const bool configured = ::posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO) == 0 &&
        ::posix_spawn_file_actions_addclose(&actions, pipes[0]) == 0 &&
        ::posix_spawn_file_actions_addclose(&actions, pipes[1]) == 0;
    char childMode[] = "--huge-nanosleep-child";
    char* arguments[] = {const_cast<char*>(executable), childMode, nullptr};
    pid_t child = -1;
    const int spawnError = configured ? ::posix_spawn(&child, executable, &actions, nullptr, arguments, environ) : EINVAL;
    ::posix_spawn_file_actions_destroy(&actions);
    ::close(pipes[1]);
    if (!configured || spawnError != 0) {
        ::close(pipes[0]);
        return false;
    }
    pollfd readyEvent{pipes[0], POLLIN | POLLHUP, 0};
    bool ready = false;
    if (PollFor(readyEvent, 5000) > 0 && (readyEvent.revents & POLLIN) != 0) {
        char byte = 0;
        ready = ::read(pipes[0], &byte, 1) == 1 && byte == Ready;
    }
    pollfd exitEvent{pipes[0], POLLIN | POLLHUP, 0};
    const bool active = ready && PollFor(exitEvent, 250) == 0;
    ::kill(child, SIGKILL);
    int status = 0;
    pid_t result = -1;
    do { result = ::waitpid(child, &status, 0); } while (result < 0 && errno == EINTR);
    const bool reaped = result == child;
    ::close(pipes[0]);
    return active && reaped;
}
#endif

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--huge-nanosleep-child") return HugeNanosleepChild();
#ifdef _WIN32
    Require(HugeRequestStaysActive(argv[0]));
#else
    const auto executable = std::filesystem::absolute(argv[0]).string();
    Require(HugeRequestStaysActive(executable.c_str()));
#endif
    PosixRejectsInvalidRequests(nanosleep_nid_postfix);
    PosixRejectsInvalidRequests(_nanosleep_nid_postfix);
    SceRejectsInvalidRequests();

    NegativeSecondsDoNotSleep(nanosleep_nid_postfix);
    NegativeSecondsDoNotSleep(_nanosleep_nid_postfix);
    NegativeSecondsDoNotSleep(sceKernelNanosleep);

    SleepsForTheRequest(nanosleep_nid_postfix);
    SleepsForTheRequest(_nanosleep_nid_postfix);
    SleepsForTheRequest(sceKernelNanosleep);
}
