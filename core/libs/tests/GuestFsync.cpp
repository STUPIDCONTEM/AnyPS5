#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, unsigned short);
int APS5_VABI sceKernelClose(int);
int APS5_VABI sceKernelFsync(int);
int APS5_VABI fsync_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI close_nid_postfix(int);
}

static void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "Fsync: %s (guest errno %d)\n", message, *__error_nid_postfix());
        std::abort();
    }
}

static void CheckFailure(int descriptor, int error) {
    *__error_nid_postfix() = 123;
    const int kernelError = static_cast<int>(0x80020000u | static_cast<unsigned>(error));
    Check(sceKernelFsync(descriptor) == kernelError, "kernel result encodes the guest error");
    Check(*__error_nid_postfix() == 123, "kernel error leaves guest errno unchanged");
    Check(fsync_nid_postfix(descriptor) == -1, "POSIX failure returns minus one");
    Check(*__error_nid_postfix() == error, "POSIX failure sets guest errno");
}

int main() {
    CheckFailure(-1, 9);
    CheckFailure(0x7fffffff, 9);
    const auto root = std::filesystem::path("anyps5-fsync-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(root), "create directory");
    const auto path = root / "data.bin";
    { std::ofstream stream(path, std::ios::binary); stream << "fsync data"; }
    const int file = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Check(file >= 0, "open regular file");
    *__error_nid_postfix() = 123;
    Check(sceKernelFsync(file) == 0, "kernel flush succeeds");
    Check(*__error_nid_postfix() == 123, "kernel success preserves guest errno");
    Check(fsync_nid_postfix(file) == 0, "POSIX flush succeeds");
    Check(*__error_nid_postfix() == 123, "POSIX success preserves guest errno");
    Check(sceKernelClose(file) == 0, "close regular file");
    CheckFailure(file, 9);
    const int socket = socket_nid_postfix(2, 1, 0);
    Check(socket >= 0, "create socket");
    CheckFailure(socket, 22);
    Check(close_nid_postfix(socket) == 0, "close socket");
    CheckFailure(socket, 9);
    std::filesystem::remove_all(root);
}
