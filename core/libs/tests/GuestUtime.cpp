#include "prx/libc/include/General.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <source_location>
#include <stdexcept>
#include <string>
#ifndef _WIN32
#include <sys/stat.h>
#include <ctime>
#endif

struct GuestUtimbuf {
    std::int64_t actime;
    std::int64_t modtime;
};

extern "C" {
int APS5_VABI utime_nid_postfix(const char*, const GuestUtimbuf*);
int APS5_VABI chdir_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
}

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "utime check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

bool Fails(const char* path, const GuestUtimbuf* times, int error) {
    *__error_nid_postfix() = 0;
    return utime_nid_postfix(path, times) == -1 && *__error_nid_postfix() == error;
}
}

int main() {
    const auto host = std::filesystem::canonical(std::filesystem::current_path());
    const auto name = "anyps5-utime-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = host / name;
    Require(std::filesystem::create_directory(directory));
    { std::ofstream file(directory / "sample.txt"); file << "sample"; }
    const std::string guest = "/" + name + "/sample.txt";

    Require(Fails(nullptr, nullptr, 14));
    Require(Fails("", nullptr, 2));
    Require(Fails(("/" + name + "/missing").c_str(), nullptr, 2));

#ifdef _WIN32
    bool threw = false;
    try {
        utime_nid_postfix(guest.c_str(), nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
#else
    const GuestUtimbuf times{1000000000, 1100000000};
    const auto native = (directory / "sample.txt").string();
    *__error_nid_postfix() = 77;
    Require(utime_nid_postfix(guest.c_str(), &times) == 0 && *__error_nid_postfix() == 77);
    struct stat status{};
    Require(::stat(native.c_str(), &status) == 0);
    Require(status.st_atim.tv_sec == 1000000000 && status.st_atim.tv_nsec == 0);
    Require(status.st_mtim.tv_sec == 1100000000 && status.st_mtim.tv_nsec == 0);

    Require(chdir_nid_postfix(name.c_str()) == 0);
    const GuestUtimbuf relative{1200000000, 1300000000};
    Require(utime_nid_postfix("sample.txt", &relative) == 0);
    Require(::stat(native.c_str(), &status) == 0);
    Require(status.st_atim.tv_sec == 1200000000 && status.st_mtim.tv_sec == 1300000000);
    Require(chdir_nid_postfix("..") == 0);

    const auto before = std::time(nullptr);
    Require(utime_nid_postfix(guest.c_str(), nullptr) == 0);
    Require(::stat(native.c_str(), &status) == 0);
    Require(status.st_atim.tv_sec >= before - 2 && status.st_atim.tv_sec <= before + 5);
    Require(status.st_mtim.tv_sec >= before - 2 && status.st_mtim.tv_sec <= before + 5);
#endif
    std::filesystem::remove_all(directory);
    return 0;
}
