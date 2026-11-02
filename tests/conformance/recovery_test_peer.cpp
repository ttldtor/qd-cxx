// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_test_peer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <cerrno>
#    include <fcntl.h>
#    include <sys/select.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

namespace org::ttldtor::qdcxx::test {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto POLL_INTERVAL = std::chrono::milliseconds{10};
constexpr std::size_t MAXIMUM_SESSIONS = 3;
constexpr std::size_t MAXIMUM_BYTES = 4 * 1024 * 1024;

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket INVALID_SOCKET_VALUE = INVALID_SOCKET;

int socketError() {
    return WSAGetLastError();
}

void closeSocket(Socket value) {
    closesocket(value);
}

bool wouldBlock(int code) {
    return code == WSAEWOULDBLOCK || code == WSAEINTR;
}

bool setNonblocking(Socket value) {
    u_long enabled = 1;

    return ioctlsocket(value, FIONBIO, &enabled) == 0;
}
#else
using Socket = int;
constexpr Socket INVALID_SOCKET_VALUE = -1;

int socketError() {
    return errno;
}

void closeSocket(Socket value) {
    close(value);
}

bool wouldBlock(int code) {
    return code == EAGAIN || code == EWOULDBLOCK || code == EINTR;
}

bool setNonblocking(Socket value) {
    const auto flags = fcntl(value, F_GETFL, 0);

    return flags >= 0 && fcntl(value, F_SETFL, flags | O_NONBLOCK) == 0;
}
#endif

/** Sole socket owner; descriptors are never closed concurrently with worker I/O. */
struct SocketOwner {
    Socket value{INVALID_SOCKET_VALUE};

    ~SocketOwner() {
        if (value != INVALID_SOCKET_VALUE) {
            closeSocket(value);
        }
    }

    SocketOwner() = default;
    SocketOwner(const SocketOwner &) = delete;
    SocketOwner &operator=(const SocketOwner &) = delete;
};

bool validConfig(const TcpPeerConfig &config) {
    if (config.sessions.empty() || config.sessions.size() > MAXIMUM_SESSIONS || config.maximumBytesPerSession == 0 ||
        config.maximumBytesPerSession > MAXIMUM_BYTES || config.lifetime <= std::chrono::milliseconds::zero() ||
        config.lifetime > std::chrono::seconds{60}) {
        return false;
    }

    for (const auto &script : config.sessions) {
        if (!script.expectedClientBytes.empty() && script.clientBytesToRead != 0 &&
            script.clientBytesToRead != script.expectedClientBytes.size()) {
            return false;
        }

        auto remaining = config.maximumBytesPerSession;
        const auto readSize =
            script.expectedClientBytes.empty() ? script.clientBytesToRead : script.expectedClientBytes.size();

        for (const auto size :
             {script.initialBytes.size(), script.expectedClientBytes.size(), readSize, script.replyBytes.size()}) {
            if (size > remaining) {
                return false;
            }

            remaining -= size;
        }
    }

    return true;
}
} // namespace

struct OwnedTcpPeer::Impl {
    TcpPeerConfig config;
    mutable std::mutex mutex;
    TcpPeerSnapshot status;
    std::array<bool, MAXIMUM_SESSIONS> closeAllowed{};
    std::array<bool, MAXIMUM_SESSIONS> replyAllowed{};
    std::atomic<bool> stopping{};
    SocketOwner listener;
    std::thread worker;
    Clock::time_point deadline;
#ifdef _WIN32
    bool winsockStarted{};
#endif

    explicit Impl(TcpPeerConfig supplied) : config(std::move(supplied)) {
    }

    ~Impl() {
        stop();
#ifdef _WIN32

        if (winsockStarted) {
            WSACleanup();
        }

#endif
    }

    void fail(TcpPeerError error, int code = 0) {
        const std::lock_guard lock(mutex);

        if (status.error == TcpPeerError::NONE) {
            status.error = error;
            status.nativeCode = code;
        }
    }

    void closeListener() {
        if (listener.value != INVALID_SOCKET_VALUE) {
            closeSocket(listener.value);
            listener.value = INVALID_SOCKET_VALUE;
        }
    }

    void startupFailed(TcpPeerError error, int code = 0) {
        fail(error, code);
        closeListener();
        status.port = 0;
        status.finished = true;
    }

    bool active() {
        if (stopping.load()) {
            return false;
        }

        if (Clock::now() >= deadline) {
            fail(TcpPeerError::TIMEOUT);

            return false;
        }

        return true;
    }

    bool waitReady(Socket socket, bool writing) {
#ifndef _WIN32

        if (socket >= FD_SETSIZE) {
            fail(TcpPeerError::SOCKET, EINVAL);

            return false;
        }

#endif

        while (active()) {
            fd_set selected;
            FD_ZERO(&selected);
            FD_SET(socket, &selected);
            timeval timeout{};
            timeout.tv_usec =
                static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(POLL_INTERVAL).count());
            const auto result = select(
#ifdef _WIN32
                0,
#else
                socket + 1,
#endif
                writing ? nullptr : &selected, writing ? &selected : nullptr, nullptr, &timeout);

            if (result > 0) {
                return active();
            }

            if (result < 0) {
                const auto code = socketError();

                if (!wouldBlock(code)) {
                    fail(TcpPeerError::SOCKET, code);

                    return false;
                }
            }
        }

        return false;
    }

    bool sendBytes(Socket socket, const std::vector<std::byte> &bytes, std::size_t index, bool reply) {
        std::size_t sent = 0;

        while (sent < bytes.size() && waitReady(socket, true)) {
            const auto count = static_cast<int>(std::min(bytes.size() - sent, std::size_t{16384}));
            const auto result = send(socket, reinterpret_cast<const char *>(bytes.data() + sent), count,
#ifdef _WIN32
                                     0
#else
                                     MSG_NOSIGNAL
#endif
            );

            if (result > 0) {
                sent += static_cast<std::size_t>(result);
                const std::lock_guard lock(mutex);
                auto &session = status.sessions[index];
                (reply ? session.replyBytesWritten : session.initialBytesWritten) = sent;
            } else if (result == 0) {
                fail(TcpPeerError::PEER_CLOSED);

                return false;
            } else {
                const auto code = socketError();

                if (!wouldBlock(code)) {
                    fail(TcpPeerError::SOCKET, code);

                    return false;
                }
            }
        }

        return sent == bytes.size() && active();
    }

    bool receiveBytes(Socket socket, const TcpPeerScript &script, std::size_t index) {
        const auto required =
            script.expectedClientBytes.empty() ? script.clientBytesToRead : script.expectedClientBytes.size();
        std::array<std::byte, 16384> buffer{};
        std::size_t received = 0;

        while (received < required && waitReady(socket, false)) {
            const auto capacity = static_cast<int>(std::min(required - received, buffer.size()));
            const auto result = recv(socket, reinterpret_cast<char *>(buffer.data()), capacity, 0);

            if (result > 0) {
                const auto count = static_cast<std::size_t>(result);
                const std::lock_guard lock(mutex);
                auto &capture = status.sessions[index].received;
                capture.insert(capture.end(), buffer.begin(), buffer.begin() + result);
                received += count;
            } else if (result == 0) {
                fail(TcpPeerError::PEER_CLOSED);

                return false;
            } else {
                const auto code = socketError();

                if (!wouldBlock(code)) {
                    fail(TcpPeerError::SOCKET, code);

                    return false;
                }
            }
        }

        if (received != required || !active()) {
            return false;
        }

        bool matched;
        {
            const std::lock_guard lock(mutex);
            auto &session = status.sessions[index];
            matched = script.expectedClientBytes.empty() || session.received == script.expectedClientBytes;
            session.matched = matched;
        }

        if (!matched) {
            fail(TcpPeerError::MISMATCH);
        }

        return matched;
    }

    bool runSession(std::size_t index) {
        SocketOwner connection;

        while (waitReady(listener.value, false)) {
            connection.value = accept(listener.value, nullptr, nullptr);

            if (connection.value != INVALID_SOCKET_VALUE) {
                break;
            }

            const auto code = socketError();

            if (!wouldBlock(code)) {
                fail(TcpPeerError::SOCKET, code);

                return false;
            }
        }

        if (connection.value == INVALID_SOCKET_VALUE) {
            return false;
        }

        if (!setNonblocking(connection.value)) {
            fail(TcpPeerError::SOCKET, socketError());

            return false;
        }

        {
            const std::lock_guard lock(mutex);
            status.sessions[index].accepted = true;
        }
        const auto &script = config.sessions[index];

        if (!sendBytes(connection.value, script.initialBytes, index, false) ||
            !receiveBytes(connection.value, script, index)) {
            return false;
        }

        while (script.waitForControllerReply && active()) {
            {
                const std::lock_guard lock(mutex);

                if (replyAllowed[index])
                    break;
            }

            std::this_thread::sleep_for(POLL_INTERVAL);
        }

        if (!active() || !sendBytes(connection.value, script.replyBytes, index, true))
            return false;

        while (script.waitForControllerClose && active()) {
            {
                const std::lock_guard lock(mutex);

                if (closeAllowed[index]) {
                    break;
                }
            }
            std::this_thread::sleep_for(POLL_INTERVAL);
        }

        if (!active()) {
            return false;
        }

        closeSocket(connection.value);
        connection.value = INVALID_SOCKET_VALUE;
        const std::lock_guard lock(mutex);
        status.sessions[index].completed = true;

        return true;
    }

    void run() noexcept {
        try {
            for (std::size_t index = 0; index < config.sessions.size(); ++index) {
                if (!runSession(index)) {
                    break;
                }
            }
        } catch (...) {
            fail(TcpPeerError::INTERNAL);
        }
        closeListener();
        const std::lock_guard lock(mutex);
        status.finished = true;
    }

    void start() {
        if (!validConfig(config)) {
            startupFailed(TcpPeerError::INVALID_CONFIG);

            return;
        }

        deadline = Clock::now() + config.lifetime;
        status.sessions.resize(config.sessions.size());

        for (std::size_t index = 0; index < config.sessions.size(); ++index) {
            const auto &script = config.sessions[index];
            status.sessions[index].received.reserve(
                script.expectedClientBytes.empty() ? script.clientBytesToRead : script.expectedClientBytes.size());
        }

#ifdef _WIN32
        WSADATA data{};
        const auto code = WSAStartup(MAKEWORD(2, 2), &data);

        if (code != 0) {
            startupFailed(TcpPeerError::STARTUP, code);

            return;
        }

        winsockStarted = true;
#endif
        listener.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (listener.value == INVALID_SOCKET_VALUE) {
            startupFailed(TcpPeerError::SOCKET, socketError());

            return;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;

        if (bind(listener.value, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0 ||
            listen(listener.value, static_cast<int>(MAXIMUM_SESSIONS)) != 0 || !setNonblocking(listener.value)) {
            startupFailed(TcpPeerError::SOCKET, socketError());

            return;
        }

#ifdef _WIN32
        int size = sizeof(address);
#else
        socklen_t size = sizeof(address);
#endif

        if (getsockname(listener.value, reinterpret_cast<sockaddr *>(&address), &size) != 0) {
            startupFailed(TcpPeerError::SOCKET, socketError());

            return;
        }

        status.port = ntohs(address.sin_port);
        try {
            worker = std::thread([this] {
                run();
            });
        } catch (...) {
            startupFailed(TcpPeerError::STARTUP);
        }
    }

    void stop() {
        stopping.store(true);

        if (worker.joinable()) {
            worker.join();
        }

        closeListener();
    }
};

OwnedTcpPeer::OwnedTcpPeer(TcpPeerConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {
    impl_->start();
}

OwnedTcpPeer::~OwnedTcpPeer() = default;

std::uint16_t OwnedTcpPeer::port() const {
    const std::lock_guard lock(impl_->mutex);

    return impl_->status.port;
}

TcpPeerSnapshot OwnedTcpPeer::snapshot() const {
    const std::lock_guard lock(impl_->mutex);

    return impl_->status;
}

void OwnedTcpPeer::allowClose(std::size_t index) {
    const std::lock_guard lock(impl_->mutex);

    if (index < impl_->config.sessions.size() && index < MAXIMUM_SESSIONS) {
        impl_->closeAllowed[index] = true;
    }
}

void OwnedTcpPeer::allowReply(std::size_t index) {
    const std::lock_guard lock(impl_->mutex);

    if (index < impl_->config.sessions.size() && index < MAXIMUM_SESSIONS)
        impl_->replyAllowed[index] = true;
}

void OwnedTcpPeer::stop() {
    impl_->stop();
}
} // namespace org::ttldtor::qdcxx::test
