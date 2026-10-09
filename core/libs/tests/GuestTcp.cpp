#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
struct PollDescriptor { int descriptor; short events; short revents; };

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI poll_nid_postfix(PollDescriptor*, std::uint32_t, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI accept_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
int APS5_VABI getpeername_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}

static void Check(bool condition, int line) {
    if (!condition) { std::fprintf(stderr, "TCP check failed at line %d\n", line); std::abort(); }
}
#define Require(value) Check((value), __LINE__)

int main() {
    const int listener = socket_nid_postfix(2, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(bind_nid_postfix(listener, address.data(), address.size()) == -1 && *__error_nid_postfix() == 22);
    Require(listen_nid_postfix(listener, 4) == 0);
    std::uint32_t address_size = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &address_size) == 0);
    Require(address_size == 16 && (address[2] != 0 || address[3] != 0));

    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0);
    Require(accept_nid_postfix(client, nullptr, nullptr) == -1 && *__error_nid_postfix() == 22);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
    std::array<std::uint8_t, 16> peer{};
    address_size = peer.size();
    const int accepted = accept_nid_postfix(listener, peer.data(), &address_size);
    Require(accepted >= 0 && address_size == 16 && peer[1] == 2);
    Require(fcntl_nid_postfix(accepted, 3) == 2);

    std::array<std::uint8_t, 16> connected_peer{};
    address_size = connected_peer.size();
    Require(getpeername_nid_postfix(accepted, connected_peer.data(), &address_size) == 0);
    Require(address_size == 16 && connected_peer[1] == 2);

    const char request[] = "guest TCP loopback";
    char received[sizeof(request)]{};
    Require(send_nid_postfix(client, request, sizeof(request), 0x1) == -1 && *__error_nid_postfix() == 45);
    Require(send_nid_postfix(client, request, sizeof(request), 0x20000) == sizeof(request));
    Require(recv_nid_postfix(accepted, received, sizeof(received), 0) == sizeof(received));
    Require(std::strcmp(request, received) == 0);

    const int nonblocking_listener = socket_nid_postfix(2, 1, 6);
    Require(nonblocking_listener >= 0);
    std::array<std::uint8_t, 16> nonblocking_address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(nonblocking_listener, nonblocking_address.data(), nonblocking_address.size()) == 0);
    Require(listen_nid_postfix(nonblocking_listener, 4) == 0);
    address_size = nonblocking_address.size();
    Require(getsockname_nid_postfix(nonblocking_listener, nonblocking_address.data(), &address_size) == 0);
    Require(fcntl_nid_postfix(nonblocking_listener, 4, 6) == 0);
    Require(fcntl_nid_postfix(nonblocking_listener, 3) == 6);

    const int nonblocking_client = socket_nid_postfix(2, 1, 0);
    Require(nonblocking_client >= 0);
    Require(connect_nid_postfix(nonblocking_client, nonblocking_address.data(), nonblocking_address.size()) == 0);
    PollDescriptor pending{nonblocking_listener, 1, 0};
    Require(poll_nid_postfix(&pending, 1, 1000) == 1 && pending.revents == 1);
    const int nonblocking_accepted = accept_nid_postfix(nonblocking_listener, nullptr, nullptr);
    Require(nonblocking_accepted >= 0);
    Require(fcntl_nid_postfix(nonblocking_accepted, 3) == 6);
    char no_data = 0;
    Require(recv_nid_postfix(nonblocking_accepted, &no_data, 1, 0) == -1 && *__error_nid_postfix() == 35);
    Require(fcntl_nid_postfix(nonblocking_accepted, 4, 2) == 0);
    Require(fcntl_nid_postfix(nonblocking_accepted, 3) == 2);
    Require(fcntl_nid_postfix(nonblocking_listener, 3) == 6);
    Require(send_nid_postfix(nonblocking_client, request, sizeof(request), 0) == sizeof(request));
    Require(recv_nid_postfix(nonblocking_accepted, received, sizeof(received), 0) == sizeof(received));
    Require(std::strcmp(request, received) == 0);
    Require(fcntl_nid_postfix(nonblocking_accepted, 4, 6) == 0);
    Require(fcntl_nid_postfix(nonblocking_listener, 4, 2) == 0);
    Require(fcntl_nid_postfix(nonblocking_accepted, 3) == 6);
    Require(recv_nid_postfix(nonblocking_accepted, &no_data, 1, 0) == -1 && *__error_nid_postfix() == 35);
    Require(close_nid_postfix(nonblocking_accepted) == 0);
    Require(close_nid_postfix(nonblocking_client) == 0);
    Require(close_nid_postfix(nonblocking_listener) == 0);
    const int pending_listener = socket_nid_postfix(2, 1, 6);
    Require(pending_listener >= 0);
    std::array<std::uint8_t, 16> pending_address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(pending_listener, pending_address.data(), pending_address.size()) == 0);
    Require(listen_nid_postfix(pending_listener, 4) == 0);
    address_size = pending_address.size();
    Require(getsockname_nid_postfix(pending_listener, pending_address.data(), &address_size) == 0);
    const int pending_client = socket_nid_postfix(2, 1, 0);
    Require(pending_client >= 0 && fcntl_nid_postfix(pending_client, 4, 6) == 0);
    const int pending_connect = connect_nid_postfix(pending_client, pending_address.data(), pending_address.size());
    Require(pending_connect == 0 || (pending_connect == -1 && *__error_nid_postfix() == 36));
    PollDescriptor pending_client_ready{pending_client, 4, 0};
    Require(poll_nid_postfix(&pending_client_ready, 1, 1000) == 1);
    Require((pending_client_ready.revents & 4) != 0);
    PollDescriptor pending_listener_ready{pending_listener, 1, 0};
    Require(poll_nid_postfix(&pending_listener_ready, 1, 1000) == 1);
    Require((pending_listener_ready.revents & 1) != 0);
    const int pending_accepted = accept_nid_postfix(pending_listener, nullptr, nullptr);
    Require(pending_accepted >= 0);
    const char pending_message[] = "pending connect";
    char pending_received[sizeof(pending_message)]{};
    Require(send_nid_postfix(pending_client, pending_message, sizeof(pending_message), 0) == sizeof(pending_message));
    Require(recv_nid_postfix(pending_accepted, pending_received, sizeof(pending_received), 0) == sizeof(pending_received));
    Require(std::strcmp(pending_message, pending_received) == 0);
    Require(close_nid_postfix(pending_accepted) == 0);
    Require(close_nid_postfix(pending_client) == 0);
    Require(close_nid_postfix(pending_listener) == 0);


    Require(close_nid_postfix(accepted) == 0);
#ifndef _WIN32
    std::int64_t sent = 0;
    for (int i = 0; i < 100 && sent >= 0; ++i) sent = send_nid_postfix(client, request, sizeof(request), 0x20000);
    Require(sent == -1 && *__error_nid_postfix() == 32);
#endif
    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(listener) == 0);
    Require(close_nid_postfix(listener) == -1 && *__error_nid_postfix() == 9);
}