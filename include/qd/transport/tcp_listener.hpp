// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace org::ttldtor::qdcxx::transport {

class RuntimeBootstrap;

/** Detached diagnostics for the listening socket and its accepted byte streams. */
enum class TcpListenerError {
    INVALID_CONFIGURATION, ///< Invalid bootstrap, bind address or requested limits.
    INVALID_STATE,         ///< Moved or stopped listener, or unavailable connection identity.
    INVALID_ARGUMENT,      ///< An I/O span was empty.
    WRONG_THREAD,          ///< A checked call was attempted outside the owner thread.
    RESOURCE_LIMIT,        ///< Connection slots or a required identity reservation are exhausted.
    ALLOCATION_FAILURE,    ///< Allocating the fixed connection table or listener state failed.
    SOCKET_FAILURE,        ///< Native listener allocation, configuration or inspection failed.
    BIND_FAILURE,          ///< The native listening address could not be bound.
    LISTEN_FAILURE,        ///< The native socket could not begin listening.
    ACCEPT_FAILURE,        ///< The native accept attempt failed.
    READ_FAILURE,          ///< The selected native socket read failed.
    WRITE_FAILURE,         ///< The selected native socket write failed.
    CLOSE_FAILURE          ///< The native socket reported an error during retirement.
};

/** Static/numeric evidence independent of listener or peer lifetime. */
struct TcpListenerDiagnostic {
    TcpListenerError error; ///< Static category.
    std::string_view stage; ///< Library-owned checkpoint; no endpoint or peer text.
    int nativeCode = 0;     ///< Native error number when available.
};

/** Finite limits and borrowed numeric bind configuration for one listener. */
struct TcpListenerOptions {
    static constexpr std::size_t MAX_CONNECTIONS = 4096; ///< Maximum accepted descriptor slots per listener.

    std::string_view address = "127.0.0.1"; ///< Numeric IPv4 or IPv6, consumed during creation.
    std::uint16_t port = 0;                 ///< Local port; zero selects an ephemeral port.
    int backlog = 16;                       ///< Positive requested kernel listen backlog.
    std::size_t maxConnections = 16;        ///< Accepted socket slots allocated at creation, in 1..MAX_CONNECTIONS.
    std::size_t maxReadBytes = 64 * 1024;   ///< Positive per-call receive cap, at most INT_MAX.
    std::size_t maxWriteBytes = 64 * 1024;  ///< Positive per-call send cap, at most INT_MAX.
};

/** Comparable, non-reusable identity scoped to one listener, valid until retirement. */
class TcpConnectionId {
    friend class TcpListener;

    std::uint64_t listener_ = 0;
    std::size_t slot_ = 0;
    std::uint64_t generation_ = 0;

    TcpConnectionId(std::uint64_t listener, std::size_t slot, std::uint64_t generation) noexcept
        : listener_(listener), slot_(slot), generation_(generation) {
    }

    public:
    /** Construct an invalid sentinel that never resolves to a socket. */
    TcpConnectionId() noexcept = default;

    /** Compare listener, slot and generation as an opaque identity.
     * @param other Peer identity to compare with this one.
     * @return Total ordering, equal only for the same identity.
     */
    auto operator<=>(const TcpConnectionId &other) const noexcept = default;
};

/** Result categories for one accepted-socket read or write. */
enum class TcpIoStatus {
    PROGRESS,    ///< Positive bytes received or accepted by the local socket.
    WOULD_BLOCK, ///< No bytes and no waiting; the caller can retry on a later turn.
    PEER_EOF     ///< The remote write side ended; local writes are still allowed.
};

/** Exact observation of one synchronous nonblocking byte operation. */
struct TcpIoResult {
    TcpIoStatus status; ///< Progress, temporary no-progress, or read-side end.
    std::size_t bytes;  ///< Exact prefix; zero for WOULD_BLOCK and PEER_EOF.
};

/** Owner-driven TCP listener with exclusive accepted sockets and no queued application bytes.
 * Calls are serialized on the creating thread. Checked calls reject a wrong thread.
 * Queries, moves and destruction require the owner thread. The application retains its bootstrap
 * through listener destruction. No call retains caller buffers, registers callbacks, or starts a worker.
 */
class TcpListener {
    struct State;
    std::unique_ptr<State> state_;

    explicit TcpListener(std::unique_ptr<State> state) noexcept;
    [[nodiscard]] std::expected<void, TcpListenerDiagnostic> checkOwner() const noexcept;
    [[nodiscard]] std::expected<std::size_t, TcpListenerDiagnostic> find(TcpConnectionId connection) const noexcept;

    public:
    /** Transfer the listener and its connection identities; leave the source empty.
     * @param other Source on its owner thread.
     */
    TcpListener(TcpListener &&other) noexcept;

    /** Retire current descriptors and transfer the source; self-move preserves the listener.
     * @param other Source on its owner thread.
     * @return This listener.
     */
    TcpListener &operator=(TcpListener &&other) noexcept;

    /** Close owned native sockets before releasing the process bootstrap borrow.
     * If a Windows close fails and the handle remains live, destruction terminates.
     * Call shutdown explicitly to receive failure evidence and retry while the owner is live.
     */
    ~TcpListener();
    TcpListener(const TcpListener &) = delete;
    TcpListener &operator=(const TcpListener &) = delete;

    /** Allocate the fixed connection table, bind one numeric address and start listening.
     * @param bootstrap Application owner that outlives the listener.
     * @param options Numeric endpoint and finite limits consumed during creation.
     * @return A listening owner or detached failure; no socket survives failed creation.
     */
    [[nodiscard]] static std::expected<TcpListener, TcpListenerDiagnostic> create(RuntimeBootstrap &bootstrap,
                                                                                  TcpListenerOptions options = {});

    /** Inspect the active local port.
     * @return Actual bound port or zero after shutdown or move.
     */
    [[nodiscard]] std::uint16_t port() const noexcept;
    /** Inspect accepted socket occupancy.
     * @return Number still owned, including any awaiting a retry of failed retirement.
     */
    [[nodiscard]] std::size_t activeConnections() const noexcept;

    /** Accept at most one peer.
     * RESOURCE_LIMIT at capacity leaves the native backlog untouched.
     * @return Accepted identity, empty if no peer was ready, or detached failure.
     */
    [[nodiscard]] std::expected<std::optional<TcpConnectionId>, TcpListenerDiagnostic> accept() noexcept;

    /** Read at most one capped prefix. EOF is sticky for reads.
     * @param connection Active accepted identity of this listener.
     * @param destination Nonempty output span borrowed until the call returns.
     * @return Exact received count, would-block, peer EOF or detached failure.
     */
    [[nodiscard]] std::expected<TcpIoResult, TcpListenerDiagnostic> read(TcpConnectionId connection,
                                                                         std::span<std::byte> destination) noexcept;

    /** Submit at most one capped prefix. The caller retains any unsent suffix.
     * @param connection Active accepted identity of this listener.
     * @param source Nonempty input span borrowed until the call returns.
     * @return Exact local acceptance count, would-block or detached failure.
     */
    [[nodiscard]] std::expected<TcpIoResult, TcpListenerDiagnostic> write(TcpConnectionId connection,
                                                                          std::span<const std::byte> source) noexcept;

    /** Fence one connection and release its socket. Retired identities reject later operations.
     * @param connection Accepted identity of this listener.
     * @return Successful retirement or detached failure.
     */
    [[nodiscard]] std::expected<void, TcpListenerDiagnostic> retire(TcpConnectionId connection) noexcept;

    /** Fence acceptance and I/O and attempt to close every owned descriptor. Safe to repeat.
     * @return Success when all closes succeed, otherwise the first failure in this call.
     */
    [[nodiscard]] std::expected<void, TcpListenerDiagnostic> shutdown() noexcept;
};

} // namespace org::ttldtor::qdcxx::transport
