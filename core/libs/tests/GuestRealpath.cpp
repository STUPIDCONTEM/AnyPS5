#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <source_location>
#include <string>

extern "C" {
int APS5_VABI chdir_nid_postfix(const char*);
char* APS5_VABI realpath_nid_postfix(const char*, char*);
int* APS5_VABI __error_nid_postfix();
}

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "realpath check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

bool Fails(const char* path, int error) {
    char buffer[1024];
    std::memset(buffer, 0x7e, sizeof(buffer));
    *__error_nid_postfix() = 0;
    return realpath_nid_postfix(path, buffer) == nullptr && *__error_nid_postfix() == error && buffer[0] == 0x7e;
}

std::string Resolve(const std::string& path) {
    char buffer[1024];
    char* result = realpath_nid_postfix(path.c_str(), buffer);
    Require(result == buffer);
    return result;
}
}

int main() {
    const auto host = std::filesystem::canonical(std::filesystem::current_path());
    const auto name = "anyps5-realpath-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = host / name;
    Require(std::filesystem::create_directory(directory));
    Require(std::filesystem::create_directory(directory / "sub"));
    { std::ofstream file(directory / "sample.txt"); file << "sample"; }

    const std::string guest = "/" + name;
    Require(Resolve(name + "/sample.txt") == guest + "/sample.txt");
    Require(Resolve(guest + "/sub/../sample.txt") == guest + "/sample.txt");
    Require(Resolve(guest + "//sub/./") == guest + "/sub");
    Require(Resolve("/") == "/");
    Require(Resolve(".") == "/");
    Require(Resolve(guest + "\\sample.txt") == guest + "/sample.txt");

    Require(chdir_nid_postfix(name.c_str()) == 0);
    Require(Resolve("sample.txt") == guest + "/sample.txt");
    Require(Resolve("sub/..") == guest);
    Require(Resolve(".") == guest);
    Require(chdir_nid_postfix("..") == 0);

#ifndef _WIN32
    std::filesystem::create_symlink("sample.txt", directory / "link.txt");
    std::filesystem::create_directory_symlink("sub", directory / "linkdir");
    std::filesystem::create_symlink("loop-b", directory / "loop-a");
    std::filesystem::create_symlink("loop-a", directory / "loop-b");
    Require(Resolve(guest + "/link.txt") == guest + "/sample.txt");
    Require(Resolve(guest + "/linkdir") == guest + "/sub");
    Require(Fails((guest + "/loop-a").c_str(), 62));
#endif

    char* allocated = realpath_nid_postfix((guest + "/sample.txt").c_str(), nullptr);
    Require(allocated && guest + "/sample.txt" == allocated);
    GuestHeap::GuestHeapFree_nid_postfix(allocated);

    Require(Fails((guest + "/missing").c_str(), 2));
    Require(Fails((guest + "/sample.txt/below").c_str(), 20));
    Require(Fails("", 2));
    Require(Fails(nullptr, 22));
#ifndef _WIN32
    const std::string longName(1100, 'x');
    Require(Fails((guest + "/" + longName).c_str(), 63));
#endif

    std::filesystem::remove_all(directory);
    return 0;
}
