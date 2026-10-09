#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, unsigned short);
int APS5_VABI sceKernelClose(int);
int APS5_VABI sceKernelFtruncate(int, std::int64_t);
int APS5_VABI ftruncate_nid_postfix(int, std::int64_t);
std::int64_t APS5_VABI sceKernelLseek(int, std::int64_t, int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI pipe_nid_postfix(int*);
}

static constexpr int ErrorBadDescriptor = static_cast<int>(0x80020009u);
static constexpr int ErrorInvalid = static_cast<int>(0x80020016u);

static void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "File resize: %s (guest errno %d)\n", message, *__error_nid_postfix());
        std::abort();
    }
}

static std::string Contents(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    Check(stream.is_open(), "native file opens");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

static void Failure(int file, std::int64_t length, int expected) {
    *__error_nid_postfix() = 123;
    Check(sceKernelFtruncate(file, length) == expected, "kernel failure returns encoded error");
    Check(*__error_nid_postfix() == 123, "kernel failure preserves errno");
    Check(ftruncate_nid_postfix(file, length) == -1, "POSIX failure returns minus one");
    Check(*__error_nid_postfix() == (expected & 0xffff), "POSIX failure sets guest errno");
}

int main() {
    Failure(-1, -1, ErrorInvalid);
    Failure(-1, 0, ErrorBadDescriptor);
    Failure(0x7fffffff, 0, ErrorBadDescriptor);
    const auto root = std::filesystem::path("anyps5-file-resize-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(root), "create directory");
    const auto path = root / "data.bin";
    { std::ofstream stream(path, std::ios::binary); stream << "0123456789"; }
    const int file = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Check(file >= 0, "open read-write file");
    Check(sceKernelLseek(file, 8, 0) == 8, "set file position");
    Failure(file, -1, ErrorInvalid);
    Check(Contents(path) == "0123456789", "negative length leaves file untouched");
    *__error_nid_postfix() = 123;
    Check(sceKernelFtruncate(file, 3) == 0, "kernel shrink succeeds");
    Check(*__error_nid_postfix() == 123, "kernel success preserves errno");
    Check(sceKernelLseek(file, 0, 1) == 8, "shrink preserves position beyond EOF");
    Check(Contents(path) == "012", "shrink keeps the prefix");
    *__error_nid_postfix() = 123;
    Check(ftruncate_nid_postfix(file, 16) == 0, "POSIX growth succeeds");
    Check(*__error_nid_postfix() == 123, "POSIX success preserves errno");
    Check(sceKernelLseek(file, 0, 1) == 8, "growth preserves position");
    const std::string expected = std::string("012") + std::string(13, '\0');
    Check(Contents(path) == expected, "growth zero fills new bytes");
    Check(sceKernelClose(file) == 0, "close read-write file");
    Failure(file, 0, ErrorBadDescriptor);
    Check(Contents(path) == expected, "closed descriptor cannot resize file");

    const int readOnly = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(readOnly >= 0, "open read-only file");
    Failure(readOnly, 0, ErrorInvalid);
    Failure(readOnly, 16, ErrorInvalid);
    Check(Contents(path) == expected, "read-only descriptor cannot resize file");
    Check(sceKernelClose(readOnly) == 0, "close read-only file");

    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(directory >= 0, "open directory");
    Failure(directory, 0, ErrorInvalid);
    Check(sceKernelClose(directory) == 0, "close directory");
    int pipes[2]{};
    Check(pipe_nid_postfix(pipes) == 0, "create pipe");
    Failure(pipes[0], 0, ErrorInvalid);
    Failure(pipes[1], 0, ErrorInvalid);
    Check(close_nid_postfix(pipes[0]) == 0 && close_nid_postfix(pipes[1]) == 0, "close pipe");
    const int socket = socket_nid_postfix(2, 1, 0);
    Check(socket >= 0, "create socket");
    Failure(socket, 0, ErrorInvalid);
    Check(close_nid_postfix(socket) == 0, "close socket");
    Failure(socket, 0, ErrorBadDescriptor);
    Check(Contents(path) == expected, "all rejected transfers leave file intact");
    const int writeOnly = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_WRONLY, 0);
    Check(writeOnly >= 0, "open write-only file");
    Check(sceKernelLseek(writeOnly, 5, 0) == 5, "set write-only position");
    *__error_nid_postfix() = 123;
    Check(sceKernelFtruncate(writeOnly, 16) == 0, "write-only same-size resize succeeds");
    Check(*__error_nid_postfix() == 123, "same-size success preserves errno");
    Check(sceKernelLseek(writeOnly, 0, 1) == 5, "same-size resize preserves position");
    Check(Contents(path) == expected, "same-size resize preserves contents");
    *__error_nid_postfix() = 123;
    Check(ftruncate_nid_postfix(writeOnly, 0) == 0, "write-only truncation to zero succeeds");
    Check(*__error_nid_postfix() == 123, "zero-size success preserves errno");
    Check(sceKernelLseek(writeOnly, 0, 1) == 5, "zero-size resize preserves position");
    Check(Contents(path).empty(), "zero-size resize removes all bytes");
    Check(sceKernelClose(writeOnly) == 0, "close write-only file");

    const int append = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_APPEND, 0);
    Check(append >= 0, "open append file");
    Check(sceKernelLseek(append, 5, 0) == 5, "set append position");
    *__error_nid_postfix() = 123;
    Check(sceKernelFtruncate(append, 4) == 0, "append descriptor growth succeeds");
    Check(*__error_nid_postfix() == 123, "append growth preserves errno");
    Check(sceKernelLseek(append, 0, 1) == 5, "append growth preserves position");
    Check(Contents(path) == std::string(4, '\0'), "append growth zero fills new bytes");
    *__error_nid_postfix() = 123;
    Check(ftruncate_nid_postfix(append, 2) == 0, "append descriptor shrink succeeds");
    Check(*__error_nid_postfix() == 123, "append shrink preserves errno");
    Check(sceKernelLseek(append, 0, 1) == 5, "append shrink preserves position");
    Check(Contents(path) == std::string(2, '\0'), "append shrink keeps requested size");
    Check(sceKernelClose(append) == 0, "close append file");
    std::filesystem::remove_all(root);
}
