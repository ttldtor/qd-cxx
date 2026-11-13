// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/transport/tcp_listener.hpp"

#include "bootstrap_lease.hpp"

#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <exception>
#include <limits>
#include <new>
#include <thread>
#include <utility>

namespace org::ttldtor::qdcxx::transport {
namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
using AddressSize = int;
constexpr SocketHandle INVALID_HANDLE = INVALID_SOCKET;
#else
using SocketHandle = int;
using AddressSize = socklen_t;
constexpr SocketHandle INVALID_HANDLE = -1;
#endif

/** Read the error immediately after a failed socket operation.
 * @return The calling thread's current native socket diagnostic number.
 */
int socketError() noexcept {
#ifdef _WIN32

    return ::WSAGetLastError();
#else

    return errno;
#endif
}

/** Yield on readiness gaps and interruption without an internal retry loop.
 * @param error Native error from this syscall.
 * @return True when a later owner turn can repeat the operation.
 */
bool retryLater(int error) noexcept {
#ifdef _WIN32

    return error == WSAEWOULDBLOCK || error == WSAEINTR;
#else

    return error == EAGAIN || error == EWOULDBLOCK || error == EINTR;
#endif
}

/** Own one native socket without asynchronous state or externally accessible options. */
class NativeSocket {
    public:
    SocketHandle handle = INVALID_HANDLE; ///< Owned native handle, or the invalid sentinel.

    NativeSocket() = default;
    NativeSocket(const NativeSocket &) = delete;
    NativeSocket &operator=(const NativeSocket &) = delete;

    /** Retire an outstanding socket before its enclosing bootstrap lease is released. */
    ~NativeSocket() {
        (void)close();

        if (handle != INVALID_HANDLE) {
            std::terminate();
        }
    }

    /** Attempt retirement, retaining a Windows handle if Winsock still owns it.
     * @return Zero on success, otherwise the native error.
     */
    int close() noexcept {
        if (handle == INVALID_HANDLE) {
            return 0;
        }

#ifdef _WIN32

        if (::closesocket(handle) != 0) {
            const auto error = socketError();

            if (error == WSAENOTSOCK) {
                handle = INVALID_HANDLE;
            }

            return error;
        }

        handle = INVALID_HANDLE;

        return 0;
#else

        // Linux releases the descriptor even on EINTR. Retrying could close an unrelated descriptor.
        return ::close(std::exchange(handle, INVALID_HANDLE)) == 0 ? 0 : socketError();
#endif
    }
};

/** Allocate a process-local identity without reuse after counter exhaustion.
 * @return Nonzero fresh identity, or zero if exhausted.
 */
std::uint64_t nextListenerId() noexcept {
    static std::atomic<std::uint64_t> last{0};
    auto observed = last.load(std::memory_order_relaxed);

    while (observed != std::numeric_limits<std::uint64_t>::max()) {
        if (last.compare_exchange_weak(observed, observed + 1, std::memory_order_relaxed)) {
            return observed + 1;
        }
    }

    return 0;
}

/** Create static/numeric failure evidence without retaining a listener or endpoint.
 * @param error Typed failure category.
 * @param stage Static library checkpoint.
 * @param nativeCode Native error number or zero when unavailable.
 * @return Unexpected diagnostic suitable for the checked operation result.
 */
std::unexpected<TcpListenerDiagnostic> fail(TcpListenerError error, std::string_view stage,
                                            int nativeCode = 0) noexcept {
    return std::unexpected(TcpListenerDiagnostic{error, stage, nativeCode});
}

constexpr auto MAX_IO_BYTES = static_cast<std::size_t>(std::numeric_limits<int>::max()); ///< Winsock I/O size bound.

} // namespace

/** Stable ownership domain shared by the listening socket and the fixed connection table. */
struct TcpListener::State {
    /** One admitted socket, including its EOF and retirement state. */
    struct Slot {
        NativeSocket socket;          ///< Exclusive, never exposed as a native handle.
        std::uint64_t generation = 0; ///< Last accepted incarnation; a saturated slot is never reused.
        bool readEnded = false;       ///< Sticky peer EOF for this incarnation.
        bool retiring = false;        ///< Admission fenced while a failed native close can still be retried.
    };

    detail::BootstrapLease bootstrap; ///< Keep Winsock available through descriptor destruction.
    std::unique_ptr<Slot[]> slots;
    NativeSocket listener;
    const std::thread::id owner = std::this_thread::get_id();
    const std::size_t capacity;
    const std::size_t readLimit;
    const std::size_t writeLimit;
    const std::uint64_t identity;
    std::size_t active = 0;
    std::uint16_t boundPort = 0;
    bool stopped = false;

    State(detail::BootstrapLease lease, TcpListenerOptions options, std::uint64_t id)
        : bootstrap(std::move(lease)), slots(std::make_unique<Slot[]>(options.maxConnections)),
          capacity(options.maxConnections), readLimit(options.maxReadBytes), writeLimit(options.maxWriteBytes),
          identity(id) {
    }

    ~State() {
        requireOwner();
    }

    /** Enforce the owner-only contract of queries, movement and destruction. */
    void requireOwner() const noexcept {
        if (owner != std::this_thread::get_id()) {
            std::terminate();
        }
    }

    /** Fence an owned socket and release occupancy only when its descriptor is gone.
     * @param slot Slot with a live descriptor, including a retry after native failure.
     * @return Success or first failure of this close attempt.
     */
    std::expected<void, TcpListenerDiagnostic> retire(Slot &slot) noexcept {
        slot.retiring = true;
        const auto error = slot.socket.close();

        if (slot.socket.handle == INVALID_HANDLE) {
            --active;
        }

        if (error != 0) {
            return fail(TcpListenerError::CLOSE_FAILURE, "accepted-close", error);
        }

        return {};
    }
};

TcpListener::TcpListener(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {
}

TcpListener::TcpListener(TcpListener &&other) noexcept {
    if (other.state_) {
        other.state_->requireOwner();
    }

    state_ = std::move(other.state_);
}

TcpListener &TcpListener::operator=(TcpListener &&other) noexcept {
    if (other.state_) {
        other.state_->requireOwner();
    }

    if (this != &other) {
        state_ = std::move(other.state_);
    }

    return *this;
}

TcpListener::~TcpListener() = default;

std::expected<TcpListener, TcpListenerDiagnostic> TcpListener::create(RuntimeBootstrap &bootstrap,
                                                                      TcpListenerOptions options) {
    if (options.address.empty() || options.address.size() >= INET6_ADDRSTRLEN ||
        options.address.find('\0') != std::string_view::npos || options.backlog <= 0 || options.maxConnections == 0 ||
        options.maxConnections > TcpListenerOptions::MAX_CONNECTIONS || options.maxReadBytes == 0 ||
        options.maxReadBytes > MAX_IO_BYTES || options.maxWriteBytes == 0 || options.maxWriteBytes > MAX_IO_BYTES) {
        return fail(TcpListenerError::INVALID_CONFIGURATION, "listener-options");
    }

    auto lease = detail::BootstrapLease::acquire(bootstrap);

    if (!lease) {
        const auto error = lease.error().error == TcpError::RESOURCE_LIMIT ? TcpListenerError::RESOURCE_LIMIT
                                                                           : TcpListenerError::INVALID_CONFIGURATION;

        return fail(error, lease.error().stage, lease.error().nativeCode);
    }

    std::array<char, INET6_ADDRSTRLEN> address{};
    std::copy(options.address.begin(), options.address.end(), address.begin());
    sockaddr_in ipv4{};
    sockaddr_in6 ipv6{};
    sockaddr *endpoint;
    AddressSize size;
    int family;

    if (::inet_pton(AF_INET, address.data(), &ipv4.sin_addr) == 1) {
        ipv4.sin_family = AF_INET;
        ipv4.sin_port = ::htons(options.port);
        endpoint = reinterpret_cast<sockaddr *>(&ipv4);
        size = sizeof(ipv4);
        family = AF_INET;
    } else if (::inet_pton(AF_INET6, address.data(), &ipv6.sin6_addr) == 1) {
        ipv6.sin6_family = AF_INET6;
        ipv6.sin6_port = ::htons(options.port);
        endpoint = reinterpret_cast<sockaddr *>(&ipv6);
        size = sizeof(ipv6);
        family = AF_INET6;
    } else {
        return fail(TcpListenerError::INVALID_CONFIGURATION, "listener-address");
    }

    const auto identity = nextListenerId();

    if (identity == 0) {
        return fail(TcpListenerError::RESOURCE_LIMIT, "listener-identity");
    }

    std::unique_ptr<State> state;
    try {
        state = std::make_unique<State>(std::move(*lease), options, identity);
    } catch (const std::bad_alloc &) {
        return fail(TcpListenerError::ALLOCATION_FAILURE, "listener-storage");
    }

    auto &socket = state->listener.handle;
#ifdef _WIN32
    socket = ::WSASocketW(family, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT);
#else
    socket = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
#endif

    if (socket == INVALID_HANDLE) {
        return fail(TcpListenerError::SOCKET_FAILURE, "listener-open", socketError());
    }

#ifdef _WIN32
    u_long nonblocking = 1;

    if (::ioctlsocket(socket, FIONBIO, &nonblocking) != 0) {
        return fail(TcpListenerError::SOCKET_FAILURE, "listener-nonblocking", socketError());
    }

    const int exclusive = 1;

    if (::setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&exclusive),
                     sizeof(exclusive)) != 0) {
        return fail(TcpListenerError::SOCKET_FAILURE, "listener-exclusive-address", socketError());
    }

#endif

    // Select a single address family consistently on operating systems with different defaults.
    if (family == AF_INET6) {
        const int ipv6Only = 1;

        if (::setsockopt(socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char *>(&ipv6Only),
                         sizeof(ipv6Only)) != 0) {
            return fail(TcpListenerError::SOCKET_FAILURE, "listener-ipv6-only", socketError());
        }
    }

    if (::bind(socket, endpoint, size) != 0) {
        return fail(TcpListenerError::BIND_FAILURE, "listener-bind", socketError());
    }

    if (::listen(socket, options.backlog) != 0) {
        return fail(TcpListenerError::LISTEN_FAILURE, "listener-listen", socketError());
    }

    if (::getsockname(socket, endpoint, &size) != 0) {
        return fail(TcpListenerError::SOCKET_FAILURE, "listener-local-address", socketError());
    }

    state->boundPort = ::ntohs(family == AF_INET ? ipv4.sin_port : ipv6.sin6_port);

    return TcpListener(std::move(state));
}

std::uint16_t TcpListener::port() const noexcept {
    if (!state_) {
        return 0;
    }

    state_->requireOwner();

    return state_->stopped ? 0 : state_->boundPort;
}

std::size_t TcpListener::activeConnections() const noexcept {
    if (!state_) {
        return 0;
    }

    state_->requireOwner();

    return state_->active;
}

std::expected<std::optional<TcpConnectionId>, TcpListenerDiagnostic> TcpListener::accept() noexcept {
    if (const auto checked = checkOwner(); !checked) {
        return std::unexpected(checked.error());
    }

    if (state_->stopped) {
        return fail(TcpListenerError::INVALID_STATE, "listener-stopped");
    }

    if (state_->active == state_->capacity) {
        return fail(TcpListenerError::RESOURCE_LIMIT, "listener-capacity");
    }

    for (std::size_t index = 0; index < state_->capacity; ++index) {
        auto &slot = state_->slots[index];

        if (slot.socket.handle != INVALID_HANDLE || slot.generation == std::numeric_limits<std::uint64_t>::max()) {
            continue;
        }

#ifdef _WIN32
        // Winsock accept inherits the listening socket's nonblocking mode.
        const auto accepted = ::accept(state_->listener.handle, nullptr, nullptr);
#else
        const auto accepted = ::accept4(state_->listener.handle, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif

        if (accepted == INVALID_HANDLE) {
            const auto error = socketError();

            if (retryLater(error)) {
                return std::nullopt;
            }

            return fail(TcpListenerError::ACCEPT_FAILURE, "socket-accept", error);
        }

        slot.socket.handle = accepted;
        slot.readEnded = false;
        slot.retiring = false;
        ++state_->active;

        return TcpConnectionId(state_->identity, index, ++slot.generation);
    }

    return fail(TcpListenerError::RESOURCE_LIMIT, "listener-capacity");
}

std::expected<TcpIoResult, TcpListenerDiagnostic> TcpListener::read(TcpConnectionId connection,
                                                                    std::span<std::byte> destination) noexcept {
    auto checked = find(connection);

    if (!checked) {
        return std::unexpected(checked.error());
    }

    auto &slot = state_->slots[*checked];

    if (slot.retiring || state_->stopped) {
        return fail(TcpListenerError::INVALID_STATE, "connection-stopped");
    }

    if (destination.empty()) {
        return fail(TcpListenerError::INVALID_ARGUMENT, "empty-destination");
    }

    if (slot.readEnded) {
        return TcpIoResult{TcpIoStatus::PEER_EOF, 0};
    }

    const auto size = static_cast<int>(std::min(destination.size(), state_->readLimit));
    const auto count = ::recv(slot.socket.handle, reinterpret_cast<char *>(destination.data()), size, 0);

    if (count > 0) {
        return TcpIoResult{TcpIoStatus::PROGRESS, static_cast<std::size_t>(count)};
    }

    if (count == 0) {
        slot.readEnded = true;

        return TcpIoResult{TcpIoStatus::PEER_EOF, 0};
    }

    const auto error = socketError();

    if (retryLater(error)) {
        return TcpIoResult{TcpIoStatus::WOULD_BLOCK, 0};
    }

    return fail(TcpListenerError::READ_FAILURE, "socket-read", error);
}

std::expected<TcpIoResult, TcpListenerDiagnostic> TcpListener::write(TcpConnectionId connection,
                                                                     std::span<const std::byte> source) noexcept {
    auto checked = find(connection);

    if (!checked) {
        return std::unexpected(checked.error());
    }

    auto &slot = state_->slots[*checked];

    if (slot.retiring || state_->stopped) {
        return fail(TcpListenerError::INVALID_STATE, "connection-stopped");
    }

    if (source.empty()) {
        return fail(TcpListenerError::INVALID_ARGUMENT, "empty-source");
    }

    const auto size = static_cast<int>(std::min(source.size(), state_->writeLimit));
#ifdef _WIN32
    constexpr int flags = 0;
#else
    constexpr int flags = MSG_NOSIGNAL;
#endif
    const auto count = ::send(slot.socket.handle, reinterpret_cast<const char *>(source.data()), size, flags);

    if (count > 0) {
        return TcpIoResult{TcpIoStatus::PROGRESS, static_cast<std::size_t>(count)};
    }

    if (count == 0) {
        return TcpIoResult{TcpIoStatus::WOULD_BLOCK, 0};
    }

    const auto error = socketError();

    if (retryLater(error)) {
        return TcpIoResult{TcpIoStatus::WOULD_BLOCK, 0};
    }

    return fail(TcpListenerError::WRITE_FAILURE, "socket-write", error);
}

std::expected<void, TcpListenerDiagnostic> TcpListener::checkOwner() const noexcept {
    if (!state_) {
        return fail(TcpListenerError::INVALID_STATE, "moved-listener");
    }

    if (state_->owner != std::this_thread::get_id()) {
        return fail(TcpListenerError::WRONG_THREAD, "listener-owner");
    }

    return {};
}

std::expected<std::size_t, TcpListenerDiagnostic> TcpListener::find(TcpConnectionId connection) const noexcept {
    if (const auto checked = checkOwner(); !checked) {
        return std::unexpected(checked.error());
    }

    if (connection.listener_ != state_->identity || connection.slot_ >= state_->capacity) {
        return fail(TcpListenerError::INVALID_STATE, "connection-identity");
    }

    const auto &slot = state_->slots[connection.slot_];

    if (slot.socket.handle == INVALID_HANDLE || slot.generation != connection.generation_) {
        return fail(TcpListenerError::INVALID_STATE, "connection-identity");
    }

    return connection.slot_;
}

std::expected<void, TcpListenerDiagnostic> TcpListener::retire(TcpConnectionId connection) noexcept {
    auto checked = find(connection);

    if (!checked) {
        return std::unexpected(checked.error());
    }

    return state_->retire(state_->slots[*checked]);
}

std::expected<void, TcpListenerDiagnostic> TcpListener::shutdown() noexcept {
    if (const auto checked = checkOwner(); !checked) {
        return std::unexpected(checked.error());
    }

    state_->stopped = true;
    std::optional<TcpListenerDiagnostic> firstFailure;

    if (const auto error = state_->listener.close(); error != 0) {
        firstFailure = TcpListenerDiagnostic{TcpListenerError::CLOSE_FAILURE, "listener-close", error};
    }

    for (std::size_t index = 0; index < state_->capacity && state_->active != 0; ++index) {
        auto &slot = state_->slots[index];

        if (slot.socket.handle == INVALID_HANDLE) {
            continue;
        }

        const auto result = state_->retire(slot);

        if (!result && !firstFailure) {
            firstFailure = result.error();
        }
    }

    if (firstFailure) {
        return std::unexpected(*firstFailure);
    }

    return {};
}

} // namespace org::ttldtor::qdcxx::transport
