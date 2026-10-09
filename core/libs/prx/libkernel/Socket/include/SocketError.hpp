#pragma once
#ifdef _WIN32
#include <winsock2.h>
#else
#include <cerrno>
#endif
namespace GuestSockets {
inline int PendingConnectError(int nativeError) {
#ifdef _WIN32
    if (nativeError == WSAEWOULDBLOCK) return 36;
    if (nativeError == WSAEALREADY) return 37;
#else
    if (nativeError == EINPROGRESS) return 36;
    if (nativeError == EALREADY) return 37;
#endif
    return -1;
}
}