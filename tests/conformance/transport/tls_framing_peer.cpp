// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_framing_peer.hpp"

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
#include <stdexcept>

namespace org::ttldtor::qdcxx::test {
namespace {
#ifdef _WIN32
using NativeSocket = SOCKET;                       ///< Test-owned Windows TCP descriptor.
constexpr NativeSocket NO_SOCKET = INVALID_SOCKET; ///< No descriptor is owned.
#else
using NativeSocket = int;              ///< Test-owned POSIX TCP descriptor.
constexpr NativeSocket NO_SOCKET = -1; ///< No descriptor is owned.
#endif

/** Retire exactly one test-owned native descriptor.
 * @param socket Descriptor, or NO_SOCKET for an empty owner.
 */
void closeSocket(NativeSocket socket) noexcept {
    if (socket != NO_SOCKET) {
#ifdef _WIN32
        closesocket(socket);
#else
        ::close(socket);
#endif
    }
}

/** Set nonblocking behavior without changing process or foreign descriptor state.
 * @param socket Explicit owned listener/accepted descriptor.
 */
void nonblocking(NativeSocket socket) {
#ifdef _WIN32
    u_long enabled = 1;

    if (ioctlsocket(socket, FIONBIO, &enabled) != 0) {
        throw std::runtime_error("framing-peer-nonblocking");
    }

#else
    const auto flags = fcntl(socket, F_GETFL, 0);

    if (flags < 0 || fcntl(socket, F_SETFL, flags | O_NONBLOCK) != 0) {
        throw std::runtime_error("framing-peer-nonblocking");
    }

#endif
}
} // namespace

/** Stable finite native owner, allocated before fallible setup to protect constructor unwinding. */
struct TlsFramingPeer::State {
    NativeSocket listener = NO_SOCKET;    ///< Sole listener, never borrowed by a runtime.
    NativeSocket connection = NO_SOCKET;  ///< Sole accepted connection.
    std::uint16_t port = 0;               ///< Actual ephemeral listener port.
    std::array<std::uint8_t, 9> script{}; ///< One TLS record containing only a ServerHello handshake header.
    std::size_t sent = 0;                 ///< Native accepted prefix of script.
    std::size_t received = 0;             ///< Native ClientHello prefix count; no bytes are retained.
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5); ///< Absolute finite test activity bound.

    /** Close both exact descriptors, including setup/step exception paths. */
    ~State() {
        closeSocket(connection);
        closeSocket(listener);
    }
};

TlsFramingPeer::TlsFramingPeer(bool overLimit) : state_(std::make_unique<State>()) {
    auto &state = *state_;
    state.script = {22, 3, 3, 0, 4, 2, 1, 0, static_cast<std::uint8_t>(overLimit ? 0x48 : 0x47)};
    state.listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (state.listener == NO_SOCKET) {
        throw std::runtime_error("framing-peer-listener");
    }

    nonblocking(state.listener);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(state.listener, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0 ||
        listen(state.listener, 1) != 0) {
        throw std::runtime_error("framing-peer-bind-listen");
    }

#ifdef _WIN32
    int length = sizeof(address);
#else
    socklen_t length = sizeof(address);
#endif

    if (getsockname(state.listener, reinterpret_cast<sockaddr *>(&address), &length) != 0) {
        throw std::runtime_error("framing-peer-port");
    }

    state.port = ntohs(address.sin_port);
}

TlsFramingPeer::~TlsFramingPeer() = default;

std::uint16_t TlsFramingPeer::port() const noexcept {
    return state_->port;
}

std::size_t TlsFramingPeer::sentBytes() const noexcept {
    return state_->sent;
}

std::size_t TlsFramingPeer::receivedBytes() const noexcept {
    return state_->received;
}

void TlsFramingPeer::step() {
    auto &state = *state_;

    if (std::chrono::steady_clock::now() >= state.deadline) {
        return;
    }

    if (state.connection == NO_SOCKET) {
        state.connection = accept(state.listener, nullptr, nullptr);

        if (state.connection == NO_SOCKET) {
            return;
        }

        nonblocking(state.connection);

        return;
    }

    if (state.received == 0) {
        std::array<std::uint8_t, 4096> prefix{};
#ifdef _WIN32
        const auto count =
            recv(state.connection, reinterpret_cast<char *>(prefix.data()), static_cast<int>(prefix.size()), 0);
#else
        const auto count = recv(state.connection, prefix.data(), prefix.size(), 0);
#endif

        if (count > 0) {
            state.received = static_cast<std::size_t>(count);
        }

        return;
    }

    if (state.sent == state.script.size()) {
        return;
    }

#ifdef _WIN32
    const auto count = send(state.connection, reinterpret_cast<const char *>(state.script.data() + state.sent),
                            static_cast<int>(state.script.size() - state.sent), 0);
#else
    const auto count =
        send(state.connection, state.script.data() + state.sent, state.script.size() - state.sent, MSG_NOSIGNAL);
#endif

    if (count > 0) {
        state.sent += static_cast<std::size_t>(count);
    }
}

} // namespace org::ttldtor::qdcxx::test
