// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_dns_peer.hpp"

#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <fcntl.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include <array>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::test {
namespace {
#ifdef _WIN32
using NativeSocket = SOCKET;                       ///< Test-owned Windows UDP socket.
constexpr NativeSocket NO_SOCKET = INVALID_SOCKET; ///< No owned descriptor.
#else
using NativeSocket = int;              ///< Test-owned POSIX UDP socket.
constexpr NativeSocket NO_SOCKET = -1; ///< No owned descriptor.
#endif
constexpr std::size_t DNS_HEADER_BYTES = 12;              ///< Fixed ordinary DNS header.
constexpr std::size_t DNS_DATAGRAM_BYTES = 512;           ///< Deliberately finite traditional UDP fixture buffer.
constexpr std::string_view ALIAS = "alias.route.invalid"; ///< Stable synthetic route alias, unrelated to TLS identity.

/** Append an exact DNS network-order short.
 * @param bytes Owned finite response.
 * @param value Unsigned16 field.
 */
void shortValue(std::vector<std::uint8_t> &bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value));
}

/** Encode one fixed ordinary DNS name without compression.
 * @param name ASCII fixed test name, with labels shorter than64 bytes.
 * @return Wire name including its root terminator.
 */
std::vector<std::uint8_t> wireName(std::string_view name) {
    std::vector<std::uint8_t> result;

    for (std::size_t begin = 0; begin < name.size();) {
        const auto dot = name.find('.', begin);
        const auto end = dot == std::string_view::npos ? name.size() : dot;
        result.push_back(static_cast<std::uint8_t>(end - begin));
        result.insert(result.end(), name.begin() + static_cast<std::ptrdiff_t>(begin),
                      name.begin() + static_cast<std::ptrdiff_t>(end));
        begin = end + 1;
    }

    result.push_back(0);

    return result;
}

/** Append the constant class/TTL and bounded RDATA metadata.
 * @param bytes Owned finite response, after the RR owner name.
 * @param type Ordinary CNAME5 or A1.
 * @param length Exact RDATA byte count.
 */
void recordHeader(std::vector<std::uint8_t> &bytes, std::uint16_t type, std::uint16_t length) {
    shortValue(bytes, type);
    shortValue(bytes, 1);                    // Internet class.
    bytes.insert(bytes.end(), {0, 0, 0, 0}); // No cache lifetime is needed by this finite test.
    shortValue(bytes, length);
}
} // namespace

/** Socket ownership is initialized before fallible native setup in the public fixture constructor. */
struct TlsDnsPeer::State {
    NativeSocket socket = NO_SOCKET; ///< Sole descriptor; no operation retains a borrowed buffer.
    std::uint16_t port = 0;          ///< Actual bound numeric port.
    unsigned queries = 0;            ///< Syntactically eligible questions received.
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5); ///< Absolute finite fixture lifetime.

    /** Close only the owned socket, including constructor-unwind paths. */
    ~State() {
        if (socket != NO_SOCKET) {
#ifdef _WIN32
            closesocket(socket);
#else
            ::close(socket);
#endif
        }
    }
};

TlsDnsPeer::TlsDnsPeer() : state_(std::make_unique<State>()) {
    auto &state = *state_;
    state.socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (state.socket == NO_SOCKET) {
        throw std::runtime_error("test-dns-socket");
    }

#ifdef _WIN32
    u_long enabled = 1;

    if (ioctlsocket(state.socket, FIONBIO, &enabled) != 0) {
        throw std::runtime_error("test-dns-nonblocking");
    }

#else
    const int flags = fcntl(state.socket, F_GETFL, 0);

    if (flags < 0 || fcntl(state.socket, F_SETFL, flags | O_NONBLOCK) != 0) {
        throw std::runtime_error("test-dns-nonblocking");
    }

#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(state.socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
        throw std::runtime_error("test-dns-bind");
    }

#ifdef _WIN32
    int length = sizeof(address);
#else
    socklen_t length = sizeof(address);
#endif

    if (getsockname(state.socket, reinterpret_cast<sockaddr *>(&address), &length) != 0) {
        throw std::runtime_error("test-dns-port");
    }

    state.port = ntohs(address.sin_port);
}

TlsDnsPeer::~TlsDnsPeer() = default;

std::uint16_t TlsDnsPeer::port() const noexcept {
    return state_->port;
}

unsigned TlsDnsPeer::queries() const noexcept {
    return state_->queries;
}

void TlsDnsPeer::step() {
    auto &state = *state_;

    if (std::chrono::steady_clock::now() >= state.deadline || state.queries >= 16) {
        return;
    }

    std::array<std::uint8_t, DNS_DATAGRAM_BYTES> input{};
    sockaddr_in sender{};
#ifdef _WIN32
    int senderBytes = sizeof(sender);
    const int received = recvfrom(state.socket, reinterpret_cast<char *>(input.data()), static_cast<int>(input.size()),
                                  0, reinterpret_cast<sockaddr *>(&sender), &senderBytes);
#else
    socklen_t senderBytes = sizeof(sender);
    const auto received =
        recvfrom(state.socket, input.data(), input.size(), 0, reinterpret_cast<sockaddr *>(&sender), &senderBytes);
#endif

    if (received < static_cast<decltype(received)>(DNS_HEADER_BYTES + 5)) {
        return;
    }

    const auto size = static_cast<std::size_t>(received);

    if (input[4] != 0 || input[5] != 1) {
        return;
    }

    std::size_t cursor = DNS_HEADER_BYTES;
    std::string name;

    while (cursor < size && input[cursor] != 0) {
        const auto label = input[cursor++];

        if (label == 0 || label > 63 || label > size - cursor || name.size() + label + 1 > 254) {
            return;
        }

        if (!name.empty()) {
            name.push_back('.');
        }

        for (std::size_t index = 0; index < label; ++index) {
            const auto byte = input[cursor + index];
            name.push_back(static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte));
        }

        cursor += label;
    }

    if (cursor + 5 > size) {
        return;
    }

    ++cursor;
    const auto type = static_cast<std::uint16_t>((input[cursor] << 8) | input[cursor + 1]);

    if ((type != 1 && type != 28) || input[cursor + 2] != 0 || input[cursor + 3] != 1) {
        return;
    }

    cursor += 4;
    ++state.queries;
    const bool alias = name == ALIAS;
    const std::uint16_t answers = static_cast<std::uint16_t>((alias ? 0 : 1) + (type == 1 ? 1 : 0));
    std::vector<std::uint8_t> response{input[0], input[1], 0x81, 0x80, 0, 1, 0, 0, 0, 0, 0, 0};
    response[7] = static_cast<std::uint8_t>(answers);
    response.insert(response.end(), input.begin() + DNS_HEADER_BYTES,
                    input.begin() + static_cast<std::ptrdiff_t>(cursor));
    const auto target = wireName(ALIAS);

    if (!alias) {
        response.insert(response.end(), {0xc0, 0x0c}); // Original question name.
        recordHeader(response, 5, static_cast<std::uint16_t>(target.size()));
        response.insert(response.end(), target.begin(), target.end());
    }

    if (type == 1) {
        response.insert(response.end(), target.begin(), target.end());
        recordHeader(response, 1, 4);
        response.insert(response.end(), {127, 0, 0, 1});
    }

    if (response.size() > DNS_DATAGRAM_BYTES) {
        throw std::runtime_error("test-dns-response-bound");
    }

#ifdef _WIN32
    (void)sendto(state.socket, reinterpret_cast<const char *>(response.data()), static_cast<int>(response.size()), 0,
                 reinterpret_cast<const sockaddr *>(&sender), senderBytes);
#else
    (void)sendto(state.socket, response.data(), response.size(), 0, reinterpret_cast<const sockaddr *>(&sender),
                 senderBytes);
#endif
}

} // namespace org::ttldtor::qdcxx::test
