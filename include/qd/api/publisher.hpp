// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"
#include "qd/api/feed_filter.hpp"
#include "qd/transport/tcp_listener.hpp"

#include <array>
#include <chrono>
#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::api {
namespace detail {
struct PublisherPort;
struct PublisherPolicyCompletion;
struct PublisherPeerIdentityAccess;
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

/** Owned exact source demand for a physical record in one selected contract. */
struct PublisherDemand {
    std::u16string recordName;                ///< Exact local schema record name.
    std::u16string symbol;                    ///< Exact symbol, with all UTF-16 units preserved.
    std::optional<std::int64_t> historyBound; ///< Minimum raw indexed lower bound, present only for HISTORY.
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
    core::StatisticCounter failedFrames{0, core::StatisticAvailability::UNSUPPORTED};
    ///< Byte-mode STREAM frame composition failures that retire the affected peer.
    std::size_t maxSentFrameRecords = 0; ///< Largest completed DATA frame by record count.
    std::size_t maxSentFrameBytes = 0;   ///< Largest completed DATA frame including its length prefix.
};

/** Publisher-local lifetime counters and the most recent completed worker-turn state. */
struct PublisherStatistics {
    std::array<PublisherFamilyStatistics, 3> families{}; ///< TICKER, STREAM and HISTORY, in that order.
    core::StatisticCounter retiredPeers;                 ///< Downstream peer retirements observed by the worker.
    std::size_t activePeers = 0;                         ///< Peers not yet fenced by the worker.
    std::size_t pendingPublications =
        0; ///< Accepted local rows in the input FIFO or pending application in local catalog mode.
};

/** Borrowed binary record profile, copied before endpoint creation returns. */
struct PublisherCatalogOptions {
    const core::LocalSchema &schema;                             ///< Validated binary record catalog.
    std::span<const std::vector<std::u16string>> wireFieldNames; ///< Record/field order, exact QTP names.
};

/** One immutable connection policy. An omitted filter admits all exact keys for that contract. */
struct PublisherDemandPolicy {
    std::optional<FeedFilter> ticker;
    std::optional<FeedFilter> stream;
    std::optional<FeedFilter> history;
};

/** Use this policy in place of the default when the transport observes this exact remote IP address. */
struct PublisherPeerDemandPolicy {
    std::string address; ///< Unscoped numeric IPv4 or IPv6; IPv4-mapped IPv6 matches the same IPv4 address.
    PublisherDemandPolicy demand;
};

/** Detached connection identity. Reconnects and different endpoints have distinct identities. */
class PublisherPeerId {
    transport::TcpConnectionId connection_;
    friend struct detail::PublisherPeerIdentityAccess;

    public:
    /** Identity comparison includes the listener and exact accepted socket generation. */
    auto operator<=>(const PublisherPeerId &) const noexcept = default;
};

/** Latest observed active connection and the revision of its configured demand policy. */
struct PublisherPeer {
    PublisherPeerId id;
    transport::TcpPeerAddress address;
    std::uint64_t policyRevision = 0;
};

/** Terminal outcome for an accepted policy replacement ticket. */
enum class PublisherPolicyOutcome {
    APPLIED,    ///< Replacement and its aggregate demand are published.
    REJECTED,   ///< The previous policy remains in effect.
    PEER_CLOSED ///< The connection is no longer eligible for policy replacement.
};

/** Terminal result, independent of publisher and connection lifetime. */
struct PublisherPolicyResult {
    PublisherPolicyOutcome outcome;
    std::uint64_t revision = 0; ///< Nonzero revision only when the replacement was applied.
    Diagnostic diagnostic;
};

/** Detached handle for one bounded outstanding replacement. */
class PublisherPolicyChange {
    std::shared_ptr<detail::PublisherPolicyCompletion> completion_;
    friend class PublisherEndpoint;

    explicit PublisherPolicyChange(std::shared_ptr<detail::PublisherPolicyCompletion> completion) noexcept;

    public:
    /** Wait for a terminal result. Timeout returns empty while the same ticket remains observable. */
    [[nodiscard]] std::optional<PublisherPolicyResult> waitUntil(std::chrono::steady_clock::time_point deadline) const;
};

/** Logical row limits and complete admitted-publication capacity; excludes allocator bookkeeping. */
struct PublisherRecordLimits {
    static constexpr std::size_t MAX_FIELDS = 128;                    ///< Supported logical fields per submitted row.
    static constexpr std::size_t MAX_ROW_BYTES = 16 * 1024;           ///< Largest configurable row reservation.
    static constexpr std::size_t MAX_PENDING_BYTES = 8 * 1024 * 1024; ///< Largest configurable aggregate reservation.

    std::size_t rowBytes = MAX_ROW_BYTES; ///< Maximum one row's field views, UTF-16 units and opaque bytes.
    std::size_t pendingBytes =
        256 * 1024; ///< Binding plus complete publication storage until entire collector consumption/discard.
};

/** Independent delivery and wire budgets for local STREAM. */
struct PublisherStreamByteLimits {
    static constexpr std::size_t MAX_DELIVERY_WINDOW_BYTES = 8 * 1024 * 1024; ///< Maximum configured window.

    std::size_t targetPayloadBytes = 8000;        ///< Soft encoded payload threshold before taking another record.
    std::size_t maxFrameBytes = 64 * 1024;        ///< Hard complete-frame limit including the length prefix.
    std::size_t deliveryWindowBytes = 256 * 1024; ///< Detached record bytes selected per peer; one large row may pass.
};

/** Finite local or remote publishing profile. */
struct PublisherOptions {
    static constexpr std::size_t MAX_PEER_DEMAND_POLICIES = 32; ///< Maximum address overrides per endpoint.
    static constexpr std::size_t MAX_STREAM_BATCH_RECORDS = 8;  ///< Maximum local STREAM rows in one batch.

    transport::TcpListenerOptions listener{"127.0.0.1", 0, 16,
                                           4}; ///< Plain numeric bind; address is borrowed at create.
    std::size_t pendingQuotes =
        64; ///< Maximum unconsumed source rows across enabled contracts, including detached input.
    std::optional<PublisherSourceOptions> source{}; ///< Remote upstream in place of local publication.
    bool mixed = false; ///< Enable Quote/TICKER, TimeAndSale/STREAM and default Order/HISTORY on each connection.
    std::optional<PublisherCatalogOptions> catalog{}; ///< Local record publisher or full relay. Requires mixed=false.
    PublisherDemandPolicy demand; ///< Default policy. Filters must be static and built before create.
    std::vector<PublisherPeerDemandPolicy> peerDemand; ///< Owned overrides, matched by numeric IP on connection.
    PublisherRecordLimits recordLimits;                ///< Pending storage limits for local record publication.
    std::size_t streamBatchRecords = 1; ///< Enable batching in local mixed or catalog mode by setting 2 through 8.
    std::optional<PublisherStreamByteLimits> streamBytes{}; ///< Enable byte-bounded local STREAM publication.
};

/** Raw controls for one indexed HISTORY publication. */
struct PublisherHistoryMetadata {
    std::int32_t eventFlags = 0;                   ///< Snapshot and transaction flags for this occurrence.
    std::optional<std::int64_t> removalIndex = {}; ///< Exact signed64 index, present only with REMOVE_EVENT.
};

/** Borrowed catalog row for an atomic local STREAM admission. */
struct PublisherStreamRecordView {
    core::RecordContext context;                  ///< Record identity from the configured local catalog.
    std::u16string_view symbol;                   ///< Exact nonempty symbol.
    std::span<const core::FieldValueView> fields; ///< Logical values in catalog order, without indexed controls.
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
    /** Submit an ordered batch of TimeAndSale occurrences in local mixed mode.
     * @param sales Nonempty borrowed batch within the configured STREAM batch limit.
     * @return Admission of every copied occurrence, or a refusal with no occurrence admitted.
     */
    [[nodiscard]] std::expected<void, Diagnostic> publish(std::span<const events::TimeAndSale> sales) const;
    /** Submit one ordinary default-source Order event in mixed mode. */
    [[nodiscard]] std::expected<void, Diagnostic> publish(const events::Order &order) const;

    /** Submit a local catalog row with no indexed controls.
     * @param family Collector whose delivery policy applies to the record.
     * @param context Owning record identity resolved against the configured catalog's immutable provenance.
     * @param symbol Exact nonempty symbol. Its storage is borrowed only through this call.
     * @param fields Exact tagged values in logical catalog order. Nullable field storage is copied before admission.
     * @return Local admission or detached refusal. A successful call does not establish application receipt.
     */
    [[nodiscard]] std::expected<void, Diagnostic> publishRecord(core::SubscriptionFamily family,
                                                                const core::RecordContext &context,
                                                                std::u16string_view symbol,
                                                                std::span<const core::FieldValueView> fields) const;

    /** Submit a local catalog row with explicit HISTORY controls.
     * @param family Collector family. Nondefault metadata requires HISTORY.
     * @param context Owning provenance from the configured immutable catalog.
     * @param symbol Exact nonempty symbol, borrowed through this call.
     * @param fields Complete logical values for data, or empty with REMOVE_EVENT. Borrowed through this call.
     * @param history Raw supported control flags and exact signed removal index when indicated.
     * @return Local admission or detached refusal. Coordinate writes forming one transaction on the same key.
     */
    [[nodiscard]] std::expected<void, Diagnostic>
    publishRecord(core::SubscriptionFamily family, const core::RecordContext &context, std::u16string_view symbol,
                  std::span<const core::FieldValueView> fields, PublisherHistoryMetadata history) const;

    /** Submit an ordered batch of physical records in local catalog mode.
     * @param records Nonempty borrowed batch within the configured STREAM batch limit. All backing is copied.
     * @return Admission of every copied row, or a refusal with no row admitted.
     */
    [[nodiscard]] std::expected<void, Diagnostic>
    publishStreamRecords(std::span<const PublisherStreamRecordView> records) const;
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
    /** Wait for presence in any record, or absence from all records, in the selected family. */
    [[nodiscard]] bool awaitDemand(core::SubscriptionFamily family, std::u16string_view symbol, bool subscribed,
                                   std::chrono::steady_clock::time_point deadline) const;
    /** Wait for an exact record and symbol in the selected family's last published desired union.
     * @param family Selected contract.
     * @param recordName Nonempty exact physical record name. Names outside this contract's union count as absent.
     * @param symbol Borrowed exact symbol, including a non-null empty symbol.
     * @param subscribed True waits for presence; false waits for absence.
     * @param deadline Finite monotonic wait deadline.
     * @return True if observed before shutdown or expiration. Invalid selector text or disabled families return false.
     */
    [[nodiscard]] bool awaitDemand(core::SubscriptionFamily family, std::u16string_view recordName,
                                   std::u16string_view symbol, bool subscribed,
                                   std::chrono::steady_clock::time_point deadline) const;
    /** Copy one family's latest published desired union. Order is unspecified.
     * @param family Selected contract.
     * @return Detached snapshot, empty for a disabled or invalid family or a closed endpoint. The number of
     * entries is bounded by downstream capacity. Allocation failure may throw. Demand does not imply an
     * upstream acknowledgment, and persists across an upstream reconnect while downstream peers remain active.
     */
    [[nodiscard]] std::vector<PublisherDemand> getDemand(core::SubscriptionFamily family) const;
    /** Copy the latest active peer list. The result owns its address and identity values. */
    [[nodiscard]] std::vector<PublisherPeer> getPeers() const;
    /** Submit a replacement for one peer's complete policy. Immutable dynamic snapshots are accepted by value.
     * A busy slot returns a diagnostic without accepting the new request. Other peers keep their own policy.
     * If revocation would interrupt a partially sent DATA frame or an open HISTORY sequence, the peer is closed.
     */
    [[nodiscard]] std::expected<PublisherPolicyChange, Diagnostic> replacePeerDemand(PublisherPeerId id,
                                                                                     PublisherDemandPolicy policy);
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
