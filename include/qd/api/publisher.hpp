// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"
#include "qd/transport/tcp_listener.hpp"

namespace org::ttldtor::qdcxx::api {
namespace detail {
struct PublisherPort;
} // namespace detail

/** Finite local publishing profile for composite Quote. */
struct PublisherOptions {
    transport::TcpListenerOptions listener{"127.0.0.1", 0, 16,
                                           4}; ///< Plain numeric bind; address is borrowed at create.
    std::size_t pendingQuotes = 64;            ///< Maximum queued publication requests before the owner takes them.
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
    /** Fence publication, stop socket and queue work, and join the worker. Safe to repeat. */
    void closeAndAwaitTermination() noexcept;
    /** @return First server terminal failure, or NONE. Individual peer failures remain isolated. */
    [[nodiscard]] Diagnostic getDiagnostic() const noexcept;
};

} // namespace org::ttldtor::qdcxx::api
