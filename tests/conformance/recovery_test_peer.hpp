// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace org::ttldtor::qdcxx::test {

/** A single byte-only TCP exchange, closed before the next script is accepted. */
struct TcpPeerScript {
    std::vector<std::byte> initialBytes;        ///< Bytes written immediately after accept.
    std::vector<std::byte> expectedClientBytes; ///< Exact expected prefix; empty disables comparison.
    std::size_t clientBytesToRead{};   ///< Read count if no expected prefix was supplied; otherwise zero or its size.
    std::vector<std::byte> replyBytes; ///< Bytes written after the requested prefix was received.
    bool waitForControllerClose{};     ///< Hold the connection until allowClose(index), stop, or deadline.
    bool waitForControllerReply{};     ///< Gate reply bytes until allowReply(index), stop, or deadline.
};

/** Finite resource limits validated before a native listener is created. */
struct TcpPeerConfig {
    std::vector<TcpPeerScript> sessions;                 ///< One through three sequential connections sharing one port.
    std::size_t maximumBytesPerSession{4 * 1024 * 1024}; ///< Sum of supplied and captured bytes, at most 4 MiB.
    std::chrono::milliseconds lifetime{10000}; ///< Whole helper lifetime from startup, greater than zero, at most 60 s.
};

/** Terminal helper errors; nativeCode contains only the corresponding numeric socket error. */
enum class TcpPeerError { NONE, INVALID_CONFIG, STARTUP, SOCKET, TIMEOUT, PEER_CLOSED, MISMATCH, INTERNAL };

/** Thread-safe copied observations for one connection. No protocol parsing occurs in the helper. */
struct TcpPeerSessionSnapshot {
    std::vector<std::byte> received;   ///< Bounded received prefix retained for test-side comparisons.
    std::size_t initialBytesWritten{}; ///< Successfully sent initial byte count.
    std::size_t replyBytesWritten{};   ///< Successfully sent reply byte count.
    bool accepted{};                   ///< A connection was accepted for this script.
    bool matched{};                    ///< Exact expected prefix matched, or an unverified requested prefix was read.
    bool completed{};                  ///< The whole script completed and its connection closed.
};

/** Copied numeric status and bounded fixture bytes. */
struct TcpPeerSnapshot {
    std::uint16_t port{};                         ///< Bound IPv4 loopback port; zero on failed startup.
    std::vector<TcpPeerSessionSnapshot> sessions; ///< Exactly the configured number after validation.
    TcpPeerError error{TcpPeerError::NONE};       ///< First terminal error, unaffected by later stop.
    int nativeCode{};                             ///< Native socket code, or zero for non-socket failures.
    bool finished{};                              ///< Worker exited, successfully, with error, or after stop.
};

/** Test-only native loopback listener with one owned worker and finite polling.
 * Construction binds synchronously; scripts never call production code. Configuration is owned.
 * Allocation failures may throw. Destruction stops and joins the worker before releasing Winsock.
 * Controller methods may overlap the worker; stop/destruction require a single controller thread.
 */
class OwnedTcpPeer {
    public:
    /** Start one listener and worker, or retain a typed startup error in snapshot().
     * @param config Owned byte scripts and finite budgets.
     */
    explicit OwnedTcpPeer(TcpPeerConfig config);
    /** Stop and join only this helper's worker, closing only its sockets. */
    ~OwnedTcpPeer();
    OwnedTcpPeer(const OwnedTcpPeer &) = delete;
    OwnedTcpPeer &operator=(const OwnedTcpPeer &) = delete;

    /** Read the stable listener port.
     * @return Ephemeral loopback port, or zero on failed startup.
     */
    [[nodiscard]] std::uint16_t port() const;
    /** Copy synchronized progress and bounded captured fixture bytes.
     * @return Snapshot independent of subsequent worker activity.
     */
    [[nodiscard]] TcpPeerSnapshot snapshot() const;
    /** Permit reply transmission after the application's exact readiness condition.
     * @param index Zero-based session index; out-of-range indices have no effect.
     */
    void allowReply(std::size_t index);
    /** Permit the designated script to close after its writes and reads finish.
     * @param index Zero-based session index; out-of-range indices have no effect.
     */
    void allowClose(std::size_t index);
    /** Request cancellation and join; accept/read/write/gates use finite 10 ms polling intervals. */
    void stop();

    private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace org::ttldtor::qdcxx::test
