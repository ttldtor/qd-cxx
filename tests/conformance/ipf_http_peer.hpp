// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace org::ttldtor::qdcxx::test {

/** Finite HTTP-only loopback fixture, independent of the production HTTP parser. */
class IpfHttpPeer {
    std::intptr_t listener_ = -1;          ///< Owned nonblocking loopback listener.
    std::uint16_t port_ = 0;               ///< Assigned numeric port.
    std::vector<std::string> replies_;     ///< At most32 finite raw response scripts.
    std::size_t chunk_;                    ///< Maximum fragment bytes, positive.
    bool hold_;                            ///< Hold accepted socket after script until stop/deadline.
    std::chrono::milliseconds delay_;      ///< Explicit finite interfragment pause.
    mutable std::mutex mutex_;             ///< Protect completed synthetic request captures.
    std::vector<std::string> requests_;    ///< Whole bounded captured requests.
    std::atomic<std::size_t> sent_{0};     ///< Cumulative successfully sent bytes.
    std::atomic<std::size_t> accepted_{0}; ///< Accepted connection count.
    std::atomic<bool> failed_{false};      ///< Fixture setup/I/O failure.
    std::jthread worker_;                  ///< Sole owned fixture worker, joined before socket retirement.
    /** Serve only this fixture's finite scripts.
     * @param stop Cooperative owned-thread cutoff.
     */
    void serve(std::stop_token stop) noexcept;

    public:
    /** Bind after application bootstrap and start one bounded test worker.
     * @param replies At most32 response scripts, each at most1MiB.
     * @param chunk Positive maximum fragment bytes.
     * @param hold True to retain a connection after sending until stop/five-second bound.
     * @param delay Nonnegative interfragment delay, at most100ms.
     * @param refuseConnections Bind without listening, reserving a deterministic refused TCP port.
     */
    explicit IpfHttpPeer(std::vector<std::string> replies, std::size_t chunk = 4096, bool hold = false,
                         std::chrono::milliseconds delay = std::chrono::milliseconds{0},
                         bool refuseConnections = false);
    /** Stop/join only the owned worker and close its listener. */
    ~IpfHttpPeer();
    IpfHttpPeer(const IpfHttpPeer &) = delete;
    IpfHttpPeer &operator=(const IpfHttpPeer &) = delete;
    /** Inspect numeric assigned port.
     * @return Owned listener port.
     */
    [[nodiscard]] std::uint16_t port() const noexcept;
    /** Inspect sent-byte barrier for deterministic cancellation requests.
     * @return Successfully submitted scripted bytes.
     */
    [[nodiscard]] std::size_t sentBytes() const noexcept;
    /** Inspect accepted connection barrier.
     * @return Number of accepted own sockets.
     */
    [[nodiscard]] std::size_t accepted() const noexcept;
    /** Copy bounded completed synthetic captures under fixture lock.
     * @return Owned request strings, never real credentials.
     */
    [[nodiscard]] std::vector<std::string> requests() const;
    /** Inspect fixture failure.
     * @return True for a setup/script I/O failure.
     */
    [[nodiscard]] bool failed() const noexcept;
};

/** Owned bounded UDP sink for explicit DNS failure/cancellation/admission tests; no SDK callback. */
class IpfDnsBlackhole {
    std::intptr_t socket_ = -1;           ///< Exact nonblocking UDP socket.
    std::uint16_t port_ = 0;              ///< Assigned loopback port.
    std::atomic<std::size_t> queries_{0}; ///< Actual received datagrams, shared only as a numeric barrier.
    std::jthread worker_;                 ///< Owned finite sink worker.
    /** Receive/drop at most64 datagrams over at mostfive seconds.
     * @param stop Owned worker cutoff.
     */
    void serve(std::stop_token stop) noexcept;

    public:
    /** Bind only numeric IPv4 loopback after bootstrap, then start the finite sink. */
    IpfDnsBlackhole();
    /** Stop/join only this worker before closing this socket. */
    ~IpfDnsBlackhole();
    /** Inspect the owned numeric UDP port.
     * @return Ephemeral loopback DNS port.
     */
    [[nodiscard]] std::uint16_t port() const noexcept;
    /** Observe actual datagram arrivals without accessing any client/facade/SDK.
     * @return Bounded received-datagram count.
     */
    [[nodiscard]] std::size_t queries() const noexcept;
};

} // namespace org::ttldtor::qdcxx::test
