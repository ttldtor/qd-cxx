// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"
#include "qd/transport/tcp_listener.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

namespace org::ttldtor::qdcxx::api {
namespace detail {
struct PublisherPort;
} // namespace detail

/** One numeric TCP publisher whose TICKER data is distributed to local clients. */
struct PublisherSourceOptions {
    std::string_view address = "127.0.0.1";    ///< Numeric IPv4 or IPv6, borrowed through create.
    std::uint16_t port = 0;                    ///< Remote port, required when a source is selected.
    std::chrono::milliseconds retryDelay{100}; ///< Delay before a fresh connection, between 1 ms and 60 s.
};

/** Detached observation of the current upstream generation. */
struct PublisherSourceStatus {
    std::uint64_t generation = 0; ///< Latest negotiated generation, zero before the first connection.
    bool connected = false;       ///< Bidirectional TICKER capabilities negotiated on a current connection.
    Diagnostic diagnostic;        ///< Last source failure until a fresh generation negotiates.
};

/** Finite local or remote publishing profile for composite Quote. */
struct PublisherOptions {
    transport::TcpListenerOptions listener{"127.0.0.1", 0, 16,
                                           4}; ///< Plain numeric bind; address is borrowed at create.
    std::size_t pendingQuotes = 64;            ///< Maximum queued publication requests before the owner takes them.
    std::optional<PublisherSourceOptions> source{}; ///< Remote upstream in place of local publication.
};

/** Detached producer port. Copies may be used from application threads and outlive the endpoint. */
class Publisher {
    std::shared_ptr<detail::PublisherPort> port_;
    friend class PublisherEndpoint;

    explicit Publisher(std::shared_ptr<detail::PublisherPort> port) noexcept;

    public:
    /** Submit an owned composite Quote to the finite source FIFO.
     * @param quote Borrowed value copied before return. Symbol, raw values and time must fit the selected profile.
     * @return Local admission or a detached refusal. Admission does not establish application receipt.
     */
    [[nodiscard]] std::expected<void, Diagnostic> publish(const events::Quote &quote) const;
};

/** One worker-owned plain QTP publishing endpoint. Destroy after application users have retired.
 * Operations on the endpoint facade are serialized by its creating application thread. The Publisher port
 * has an independent lifetime. Keep the original process bootstrap owner alive through endpoint destruction.
 */
class PublisherEndpoint final {
    struct Impl;
    std::unique_ptr<Impl> impl_;

    explicit PublisherEndpoint(std::unique_ptr<Impl> impl) noexcept;

    public:
    /** Bind and start the worker, acquiring an independent process bootstrap lease.
     * @param bootstrap Existing same-instance bootstrap owner.
     * @param options Finite plain listener and publication capacities; no option references survive this call.
     * @return Active endpoint with the actual port or an atomic detached failure.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<PublisherEndpoint>, Diagnostic>
    create(transport::RuntimeBootstrap &bootstrap, PublisherOptions options = {});

    ~PublisherEndpoint();
    PublisherEndpoint(const PublisherEndpoint &) = delete;
    PublisherEndpoint &operator=(const PublisherEndpoint &) = delete;

    /** @return Port reported by the listener at startup, including after close. */
    [[nodiscard]] std::uint16_t port() const noexcept;
    /** @return Independent thread-safe producer port. */
    [[nodiscard]] Publisher getPublisher() const noexcept;
    /** Wait for a requested aggregate exact Quote symbol state.
     * @param symbol Borrowed exact composite Quote symbol.
     * @param subscribed True waits for presence; false waits for absence.
     * @param deadline Finite monotonic wait deadline.
     * @return True if observed before closing or expiration; malformed input returns false.
     */
    [[nodiscard]] bool awaitDemand(std::u16string_view symbol, bool subscribed,
                                   std::chrono::steady_clock::time_point deadline) const;
    /** Observe a fresh negotiated remote source generation within a finite deadline.
     * @param after Previously observed generation, or zero for the first connection.
     * @param deadline Monotonic observation deadline.
     * @return True while a connected generation greater than after is observed. Local mode returns false.
     */
    [[nodiscard]] bool awaitSourceGeneration(std::uint64_t after, std::chrono::steady_clock::time_point deadline) const;
    /** @return Detached upstream evidence. Local mode reports generation zero and disconnected. */
    [[nodiscard]] PublisherSourceStatus getSourceStatus() const noexcept;
    /** Fence publication, stop socket and queue work, and join the worker. Safe to repeat. */
    void closeAndAwaitTermination() noexcept;
    /** @return First server terminal failure, or NONE. Individual peer failures remain isolated. */
    [[nodiscard]] Diagnostic getDiagnostic() const noexcept;
};

} // namespace org::ttldtor::qdcxx::api
