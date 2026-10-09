#include "prx/libkernel/Socket/include/SocketError.hpp"
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) std::abort();
}

int main() {
#ifdef _WIN32
    Require(GuestSockets::PendingConnectError(WSAEWOULDBLOCK) == 36);
    Require(GuestSockets::PendingConnectError(WSAEALREADY) == 37);
    Require(GuestSockets::PendingConnectError(WSAECONNREFUSED) == -1);
#else
    Require(GuestSockets::PendingConnectError(EINPROGRESS) == 36);
    Require(GuestSockets::PendingConnectError(EALREADY) == 37);
    Require(GuestSockets::PendingConnectError(ECONNREFUSED) == -1);
#endif
}