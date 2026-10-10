#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" int APS5_VABI gethostname_nid_postfix(char* name, std::size_t namelen);

namespace {
constexpr int GuestNameTooLong = 63;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "gethostname check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

std::string HostName() {
    char host[256]{};
#ifdef _WIN32
    DWORD size = sizeof(host) - 1;
    Require(GetComputerNameExA(ComputerNameDnsHostname, host, &size) != 0);
#else
    Require(::gethostname(host, sizeof(host) - 1) == 0);
#endif
    return host;
}
}

int main() {
    const std::string host = HostName();
    Require(!host.empty());

    char buffer[256];
    std::memset(buffer, 0x7e, sizeof(buffer));
    errno = 0;
    Require(gethostname_nid_postfix(buffer, sizeof(buffer)) == 0 && errno == 0);
    Require(host == buffer);
    Require(buffer[host.size() + 1] == 0x7e);

    std::memset(buffer, 0x7e, sizeof(buffer));
    Require(gethostname_nid_postfix(buffer, host.size() + 1) == 0);
    Require(host == buffer);

    std::memset(buffer, 0x7e, sizeof(buffer));
    errno = 0;
    Require(gethostname_nid_postfix(buffer, host.size()) == -1 && errno == GuestNameTooLong);
    Require(std::memcmp(buffer, host.data(), host.size()) == 0);
    Require(buffer[host.size()] == 0x7e);

    std::memset(buffer, 0x7e, sizeof(buffer));
    errno = 0;
    Require(gethostname_nid_postfix(buffer, 0) == -1 && errno == GuestNameTooLong);
    Require(buffer[0] == 0x7e);

    errno = 0;
    Require(gethostname_nid_postfix(nullptr, 0) == 0 && errno == 0);
    return 0;
}
