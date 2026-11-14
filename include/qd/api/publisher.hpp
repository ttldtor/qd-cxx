// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"
#include "qd/transport/tcp_listener.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

namespace org::ttldtor::qdcxx::api {
namespace detail {
struct PublisherPort;
} // namespace detail

/** One numeric TCP publisher whose data is distributed to downstream clients. */
struct PublisherSourceOptions {
    std::string_view address = "127.0.0.1";    ///< Numeric IPv4 or IPv6, borrowed through create.
    std::uint16_t port = 0;                    ///< Remote port, required when a source is selected.
    std::chrono::milliseconds retryDelay{100}; ///< Delay before a fresh connection, between 1 ms and 60 s.
};

/** Detached observation of the current upstream generation. */
struct PublisherSourceStatus {
    std::uint64_t generation = 0; ///< Latest negotiated generation, zero before the first connection.
    bool connected = false;       ///< Bidirectional configured capabilities negotiated on a current connection.
    Diagnostic diagnostic;        ///< Last source failure until a fresh generation negotiates.
};

/** Detached observations for one local collector family. */
struct PublisherFamilyStatistics {
    core::StatisticCounter ingressFrames;  ///< Complete source events admitted to this family's queue.
    core::StatisticCounter ingressRecords; ///< Records in those admitted source events.
    core::StatisticCounter sentFrames;     ///< Complete DATA frames accepted by downstream sockets.
    std::size_t desiredKeys = 0;           ///< Distinct keys in the currently applied aggregate union.
    std::size_t pendingSourceRecords = 0;  ///< Accepted source records still awaiting queue processing.
    std::size_t queuedDeliveryRecords = 0; ///< Records in active or retiring per-peer delivery FIFOs.
    std::size_t retainedOutputRecords = 0; ///< Records held by peer writers outside those FIFOs.
    std::size_t deliveryRecordOwners = 0;  ///< Detached records owned by the queue or retained by a consumer.
    core::StatisticCounter knownDiscardedRecords{0, core::StatisticAvailability::UNSUPPORTED};
    ///< Live discarded-record observations are not currently exposed by the queue.
};

/** Publisher-local lifetime counters and the most recent completed worker-turn state. */
struct PublisherStatistics {
    std::array<PublisherFamilyStatistics, 3> families{}; ///< TICKER, STREAM and HISTORY, in that order.
    core::StatisticCounter retiredPeers;                 ///< Downstream peer retirements observed by the worker.
    std::size_t activePeers = 0;                         ///< Peers not yet fenced by the worker.
    std::size_t pendingPublications = 0;                 ///< Accepted local publication rows still in the input FIFO.
};

/** Finite local or remote publishing profile. */
struct PublisherOptions {
    transport::TcpListenerOptions listener{"127.0.0.1", 0, 16,
                                           4};      ///< Plain numeric bind; address is borrowed at create.
    std::size_t pendingQuotes = 64;                 ///< Maximum queued publication requests across enabled contracts.
    std::optional<PublisherSourceOptions> source{}; ///< Remote upstream in place of local publication.
    bool mixed = false; ///< Enable Quote/TICKER, TimeAndSale/STREAM and default Order/HISTORY on each connection.
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
    /** Submit one composite TimeAndSale occurrence in mixed mode. */
    [[nodiscard]] std::expected<void, Diagnostic> publish(const events::TimeAndSale &sale) const;
    /** Submit one ordinary default-source Order event in mixed mode. */
    [[nodiscard]] std::expected<void, Diagnostic> publish(const events::Order &order) const;
};

/** One worker-owned plain QTP publishing endpoint. Destroy after application users have retired.
 * Read-only endpoint queries and waits may run concurrently while the caller keeps the facade alive.
 * The creating application thread owns shutdown. The Publisher port has an independent lifetime.
 * Keep the original process bootstrap owner alive through endpoint destruction.
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
    /** Wait for presence or absence of an exact symbol in the selected family's desired union. */
    [[nodiscard]] bool awaitDemand(core::SubscriptionFamily family, std::u16string_view symbol, bool subscribed,
                                   std::chrono::steady_clock::time_point deadline) const;
    /** Observe a fresh negotiated remote source generation within a finite deadline.
     * @param after Previously observed generation, or zero for the first connection.
     * @param deadline Monotonic observation deadline.
     * @return True while a connected generation greater than after is observed. Local mode returns false.
     */
    [[nodiscard]] bool awaitSourceGeneration(std::uint64_t after, std::chrono::steady_clock::time_point deadline) const;
    /** @return Detached upstream evidence. Local mode reports generation zero and disconnected. */
    [[nodiscard]] PublisherSourceStatus getSourceStatus() const noexcept;
    /** Copy local counters and the latest worker-turn state without allocating.
     * Complete socket acceptance does not establish peer receipt. Upstream loss is unknown.
     * @return Independent snapshot; counters retain their saturation state.
     */
    [[nodiscard]] PublisherStatistics getStatistics() const noexcept;
    /** Fence publication, stop socket and queue work, and join the worker. Safe to repeat. */
    void closeAndAwaitTermination() noexcept;
    /** @return First server terminal failure, or NONE. Individual peer failures remain isolated. */
    [[nodiscard]] Diagnostic getDiagnostic() const noexcept;
};

} // namespace org::ttldtor::qdcxx::api
