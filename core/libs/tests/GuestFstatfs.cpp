#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/vfs.h>
#include <unistd.h>
#endif

struct GuestStatfs {
    std::uint32_t f_version;
    std::uint32_t f_type;
    std::uint64_t f_flags;
    std::uint64_t f_bsize;
    std::uint64_t f_iosize;
    std::uint64_t f_blocks;
    std::uint64_t f_bfree;
    std::int64_t f_bavail;
    std::uint64_t f_files;
    std::int64_t f_ffree;
    std::uint64_t f_syncwrites;
    std::uint64_t f_asyncwrites;
    std::uint64_t f_syncreads;
    std::uint64_t f_asyncreads;
    std::uint64_t f_spare[10];
    std::uint32_t f_namemax;
    std::uint32_t f_owner;
    std::int32_t f_fsid[2];
    char f_charspare[80];
    char f_fstypename[16];
    char f_mntfromname[88];
    char f_mntonname[88];
};
static_assert(sizeof(GuestStatfs) == 472);
static_assert(offsetof(GuestStatfs, f_bsize) == 16 && offsetof(GuestStatfs, f_bavail) == 48 && offsetof(GuestStatfs, f_ffree) == 64);
static_assert(offsetof(GuestStatfs, f_spare) == 104 && offsetof(GuestStatfs, f_namemax) == 184 && offsetof(GuestStatfs, f_owner) == 188);
static_assert(offsetof(GuestStatfs, f_fsid) == 192 && offsetof(GuestStatfs, f_charspare) == 200);
static_assert(offsetof(GuestStatfs, f_fstypename) == 280 && offsetof(GuestStatfs, f_mntfromname) == 296 && offsetof(GuestStatfs, f_mntonname) == 384);

extern "C" {
int APS5_VABI _fstatfs_nid_postfix(int, GuestStatfs*);
int* APS5_VABI __error_nid_postfix();
}

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "fstatfs check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}
}

int main() {
    GuestStatfs result;
#ifdef _WIN32
    bool threw = false;
    try {
        _fstatfs_nid_postfix(0, &result);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
#else
    const int fd = ::open(".", O_RDONLY | O_DIRECTORY);
    Require(fd >= 0);
    struct statfs host{};
    Require(::fstatfs(fd, &host) == 0);

    std::memset(&result, 0x7e, sizeof(result));
    *__error_nid_postfix() = 77;
    Require(_fstatfs_nid_postfix(fd, &result) == 0 && *__error_nid_postfix() == 77);
    Require(result.f_version == 0x20030518);
    Require(result.f_type == 0 && result.f_syncwrites == 0 && result.f_asyncreads == 0 && result.f_owner == 0);
    Require(result.f_bsize == static_cast<std::uint64_t>(host.f_bsize) && result.f_iosize == result.f_bsize);
    Require(result.f_blocks == static_cast<std::uint64_t>(host.f_blocks));
    Require(result.f_files == static_cast<std::uint64_t>(host.f_files));
    Require(result.f_namemax == static_cast<std::uint32_t>(host.f_namelen));
    Require(std::memcmp(result.f_fsid, &host.f_fsid, sizeof(result.f_fsid)) == 0);
    Require(result.f_bfree <= result.f_blocks && result.f_bavail <= static_cast<std::int64_t>(result.f_bfree));
    for (const char byte : result.f_charspare) Require(byte == 0);
    for (const char byte : result.f_fstypename) Require(byte == 0);
    for (const char byte : result.f_mntfromname) Require(byte == 0);
    for (const char byte : result.f_mntonname) Require(byte == 0);
    for (const auto word : result.f_spare) Require(word == 0);

    *__error_nid_postfix() = 0;
    Require(_fstatfs_nid_postfix(fd, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(_fstatfs_nid_postfix(-1, &result) == -1 && *__error_nid_postfix() == 9);
    Require(::close(fd) == 0);
    *__error_nid_postfix() = 0;
    Require(_fstatfs_nid_postfix(fd, &result) == -1 && *__error_nid_postfix() == 9);
#endif
    return 0;
}
