// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "ipf_http_peer.hpp"

#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <cerrno>
#    include <fcntl.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::test {
namespace {
#ifdef _WIN32
using NativeSocket = SOCKET;
inline constexpr NativeSocket INVALID = INVALID_SOCKET;
#else
using NativeSocket = int;
inline constexpr NativeSocket INVALID = -1;
#endif
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

/** Close one exact fixture socket.
 * @param socket Owned descriptor.
 */
void closeSocket(NativeSocket socket) noexcept {
    if (socket == INVALID) {
        return;
    }

#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

/** Select nonblocking mode for an owned fixture socket.
 * @param socket Owned descriptor.
 * @return True after successful configuration.
 */
bool nonblocking(NativeSocket socket) noexcept {
#ifdef _WIN32
    u_long enabled = 1;

    return ioctlsocket(socket, FIONBIO, &enabled) == 0;
#else
    const auto flags = fcntl(socket, F_GETFL, 0);

    return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

/** Check the last fixture I/O status.
 * @return True for a transient nonblocking result.
 */
bool wouldBlock() noexcept {
#ifdef _WIN32

    return WSAGetLastError() == WSAEWOULDBLOCK;
#else

    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

/** Scoped exact fixture connection owner. */
struct Connection {
    NativeSocket socket;

    /** Retire this socket only. */
    ~Connection() {
        closeSocket(socket);
    }
};

} // namespace

IpfHttpPeer::IpfHttpPeer(std::vector<std::string> replies, std::size_t chunk, bool hold,
                         std::chrono::milliseconds delay, bool refuseConnections)
    : replies_(std::move(replies)), chunk_(chunk), hold_(hold), delay_(delay) {
    if (replies_.empty() || replies_.size() > 32 || chunk == 0 || delay.count() < 0 || delay > 100ms ||
        std::ranges::any_of(replies_, [](const auto &reply) {
            return reply.size() > 1024 * 1024;
        })) {
        throw std::invalid_argument("finite HTTP fixture options");
    }

    const auto listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    listener_ = static_cast<std::intptr_t>(listener);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (listener == INVALID || !nonblocking(listener) ||
        bind(listener, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0 ||
        (!refuseConnections && listen(listener, 8) != 0)) {
        closeSocket(listener);
        listener_ = -1;

        throw std::runtime_error("HTTP fixture bind");
    }

#ifdef _WIN32
    int size = sizeof(address);
#else
    socklen_t size = sizeof(address);
#endif

    if (getsockname(listener, reinterpret_cast<sockaddr *>(&address), &size) != 0) {
        closeSocket(listener);
        listener_ = -1;

        throw std::runtime_error("HTTP fixture address");
    }

    port_ = ntohs(address.sin_port);

    if (!refuseConnections) {
        try {
            worker_ = std::jthread([this](std::stop_token stop) {
                serve(stop);
            });
        } catch (...) {
            closeSocket(listener);
            listener_ = -1;

            throw;
        }
    }
}

IpfHttpPeer::~IpfHttpPeer() {
    worker_.request_stop();

    if (worker_.joinable()) {
        worker_.join();
    }

    closeSocket(static_cast<NativeSocket>(listener_));
}

void IpfHttpPeer::serve(std::stop_token stop) noexcept {
    try {
        const auto deadline = Clock::now() + 5s;

        for (const auto &reply : replies_) {
            NativeSocket client = INVALID;

            while (!stop.stop_requested() && Clock::now() < deadline) {
                client = accept(static_cast<NativeSocket>(listener_), nullptr, nullptr);

                if (client != INVALID) {
                    break;
                }

                if (!wouldBlock()) {
                    failed_ = true;

                    return;
                }

                std::this_thread::sleep_for(1ms);
            }

            if (client == INVALID) {
                return;
            }

            Connection connection{client};
            ++accepted_;

            if (!nonblocking(client)) {
                failed_ = true;

                return;
            }

            std::string request;
            request.reserve(32768);

            while (!stop.stop_requested() && Clock::now() < deadline && request.find("\r\n\r\n") == std::string::npos) {
                char buffer[1024];
                const auto count = recv(client, buffer, sizeof(buffer), 0);

                if (count > 0) {
                    if (static_cast<std::size_t>(count) > 32768 - request.size()) {
                        failed_ = true;

                        return;
                    }

                    request.append(buffer, static_cast<std::size_t>(count));
                } else if (count == 0) {
                    return;
                } else if (!wouldBlock()) {
                    return;
                } else {
                    std::this_thread::sleep_for(1ms);
                }
            }

            {
                std::lock_guard lock(mutex_);
                requests_.push_back(std::move(request));
            }

            std::size_t sent = 0;

            while (sent < reply.size() && !stop.stop_requested() && Clock::now() < deadline) {
                const auto count = (std::min)(chunk_, reply.size() - sent);
#ifdef _WIN32
                const auto result = send(client, reply.data() + sent, static_cast<int>(count), 0);
#else
                const auto result = send(client, reply.data() + sent, count, MSG_NOSIGNAL);
#endif

                if (result > 0) {
                    sent += static_cast<std::size_t>(result);
                    sent_ += static_cast<std::size_t>(result);

                    if (delay_ != 0ms) {
                        std::this_thread::sleep_for(delay_);
                    }
                } else if (!wouldBlock()) {
                    return;
                } else {
                    std::this_thread::sleep_for(1ms);
                }
            }

            while (hold_ && !stop.stop_requested() && Clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
            }
        }
    } catch (...) {
        failed_ = true;
    }
}

std::uint16_t IpfHttpPeer::port() const noexcept {
    return port_;
}

std::size_t IpfHttpPeer::sentBytes() const noexcept {
    return sent_.load();
}

std::size_t IpfHttpPeer::accepted() const noexcept {
    return accepted_.load();
}

std::vector<std::string> IpfHttpPeer::requests() const {
    std::lock_guard lock(mutex_);

    return requests_;
}

bool IpfHttpPeer::failed() const noexcept {
    return failed_.load();
}

IpfDnsBlackhole::IpfDnsBlackhole() {
    const auto descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    socket_ = static_cast<std::intptr_t>(descriptor);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (descriptor == INVALID || !nonblocking(descriptor) ||
        bind(descriptor, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
        closeSocket(descriptor);
        socket_ = -1;

        throw std::runtime_error("DNS sink bind");
    }

#ifdef _WIN32
    int size = sizeof(address);
#else
    socklen_t size = sizeof(address);
#endif

    if (getsockname(descriptor, reinterpret_cast<sockaddr *>(&address), &size) != 0) {
        closeSocket(descriptor);
        socket_ = -1;

        throw std::runtime_error("DNS sink address");
    }

    port_ = ntohs(address.sin_port);
    try {
        worker_ = std::jthread([this](std::stop_token stop) {
            serve(stop);
        });
    } catch (...) {
        closeSocket(descriptor);
        socket_ = -1;

        throw;
    }
}

IpfDnsBlackhole::~IpfDnsBlackhole() {
    worker_.request_stop();

    if (worker_.joinable()) {
        worker_.join();
    }

    closeSocket(static_cast<NativeSocket>(socket_));
}

void IpfDnsBlackhole::serve(std::stop_token stop) noexcept {
    const auto deadline = Clock::now() + 5s;

    while (!stop.stop_requested() && Clock::now() < deadline && queries_.load() < 64) {
        std::array<char, 1024> datagram{};
        const auto bytes = recvfrom(static_cast<NativeSocket>(socket_), datagram.data(),
                                    static_cast<int>(datagram.size()), 0, nullptr, nullptr);

        if (bytes > 0) {
            ++queries_;
        } else {
            std::this_thread::sleep_for(1ms);
        }
    }
}

std::uint16_t IpfDnsBlackhole::port() const noexcept {
    return port_;
}

std::size_t IpfDnsBlackhole::queries() const noexcept {
    return queries_.load();
}

} // namespace org::ttldtor::qdcxx::test
