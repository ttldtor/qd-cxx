// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/subscription.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace org::ttldtor::qdcxx::mux {
namespace detail {
struct Identity;
struct MuxStateAccess;
} // namespace detail

/** Synchronous local failures; rejection never publishes a partial client map or aggregate. */
enum class MuxError {
    INVALID_STATE,         ///< Moved-from owner or invalid immutable family.
    INVALID_CONFIGURATION, ///< Invalid schema or unrepresentable fixed backing reservation.
    INVALID_CLIENT,        ///< Default, foreign or retired client token.
    INVALID_CONTEXT,       ///< Record belongs to a different immutable schema.
    INVALID_SELECTOR,      ///< Null text or reserved leading asterisk.
    INVALID_HISTORY,       ///< Missing/unexpected raw bound or absent indexed layout.
    CONTRADICTORY_INPUT,   ///< Duplicate key has different bound attributes.
    RESOURCE_LIMIT,        ///< Fixed resident, client, entry, symbol or command capacity exceeded.
    WORK_LIMIT,            ///< Explicit turn credits exhausted; successful charges are not refunded.
    IDENTITY_EXHAUSTED,    ///< Revision or non-reused activation cannot advance.
    ALLOCATION_FAILURE,    ///< Factory allocation failed before publication.
    UNSUPPORTED_SELECTOR   ///< Recognized selector/profile is disabled for this family.
};

/** Explicit finite reservations; zero means zero, never unlimited.
 * Both complete banks are reserved at creation. Requested backing excludes allocator bookkeeping,
 * array cookies and shared_ptr control-block overhead, and is not measured process RSS.
 */
struct MuxLimits {
    std::size_t schemaRecords;    ///< Maximum captured record metadata slots.
    std::size_t clients;          ///< Fixed local client slots, including empty clients.
    std::size_t entries;          ///< Maximum total client-key slots in each bank.
    std::size_t aggregateEntries; ///< Maximum distinct union keys in each bank.
    std::size_t symbolUnits;      ///< Maximum UTF-16 units in one exact symbol.
    std::size_t totalSymbolUnits; ///< Packed client-symbol UTF-16 units reserved in each bank.
    std::size_t commandEntries;   ///< Maximum borrowed inputs, including duplicates, per command.
    std::size_t residentBytes;    ///< Both banks, local metadata and one conservative schema.storageBytes() reserve.
};

/** Caller-delimited turn credits; reuse across calls in one turn, with no refunds. */
struct MuxWork {
    std::size_t operations;  ///< Remaining public command/query/factory attempts.
    std::size_t visits;      ///< Conservative metadata, comparison, initialization and disposal visits.
    std::size_t symbolUnits; ///< Conservative examined/copied UTF-16 units, including duplicate comparisons.
    std::size_t bytes;       ///< Conservative fixed initialization, rewrite and retirement byte work.
};

class MuxState;

/** Owning opaque local client token; destruction does not remove desired interests.
 * Retains only a fixed owner identity, never entry arenas or schema declarations.
 */
class MuxClient {
    std::shared_ptr<const detail::Identity> identity_;
    std::size_t slot_ = 0;
    std::uint64_t activation_ = 0;
    friend class MuxState;

    public:
    /** Construct an invalid token, rejected by every client operation. */
    MuxClient() noexcept = default;
};

/** Owning client-key activation; does not retain key text or schema declarations. */
class MuxMembership {
    std::shared_ptr<const detail::Identity> identity_;
    std::uint64_t clientActivation_ = 0;
    std::uint64_t activation_ = 0;
    std::uint64_t alternateActivation_ = 0; ///< Second original matched activation, never current desire recomputed.
    friend class MuxState;

    public:
    /** Construct an invalid membership, which is never current. */
    MuxMembership() noexcept = default;
};

/** Borrowed desired entry; copy only membership to retain an owning eligibility token. */
struct MuxEntry {
    core::SubscriptionKey key;                ///< Exact provenance and symbol owned by the published bank.
    std::optional<std::int64_t> historyBound; ///< Signed raw lower bound only for HISTORY.
    MuxMembership membership;                 ///< Activation belonging to this client and this owner.
    core::DeliveryProjectionMask projectionMask = core::NATIVE_DELIVERY_PROJECTION; ///< Local selected outputs only.
    std::array<MuxMembership, core::MAX_DELIVERY_PROJECTIONS>
        projectionMemberships{}; ///< Independently stable original bit activations.
};

/** Local application barrier; neither wire emission nor peer acknowledgement is implied. */
struct MuxChange {
    std::uint64_t revision; ///< Global revision, including effective empty-client lifecycle changes.
    bool changed;           ///< Whether this call changed a client map or retired the client.
};

/** Atomic local desired maps and their exact upstream union, independent of transport.
 * Callers serialize every access; concurrent/reentrant use is unsupported. No callbacks or
 * background work occur. Commands do not allocate, including rejection/no-op paths. Every effective
 * mutation invalidates all borrowed entry/aggregate views; no-op and rejection preserve them.
 * HISTORY select tests ordinary raw-time eligibility, not collector snapshot/control semantics.
 */
class MuxState {
    struct State;
    enum class Command { ADD, REMOVE, REPLACE, CLEAR, REMOVE_CLIENT };

    class PreparedChange {
        friend class MuxState;
        friend struct detail::MuxStateAccess;
        std::shared_ptr<const detail::Identity> identity;
        std::uint64_t baseRevision;
        std::uint64_t preparation;
        std::uint64_t membership;
        std::optional<std::size_t> retiredSlot;
        bool changed;

        PreparedChange(std::shared_ptr<const detail::Identity> identity, std::uint64_t baseRevision,
                       std::uint64_t preparation, std::uint64_t membership, std::optional<std::size_t> retiredSlot,
                       bool changed) noexcept
            : identity(std::move(identity)), baseRevision(baseRevision), preparation(preparation),
              membership(membership), retiredSlot(retiredSlot), changed(changed) {
        }
    };
    friend struct detail::MuxStateAccess;
    std::unique_ptr<State> state_;

    /** Adopt fully reserved backing.
     * @param state Independently owned validated fixed state.
     */
    explicit MuxState(std::unique_ptr<State> state) noexcept;
    /** Apply one complete candidate before publication.
     * @param command Selected desired-state operation.
     * @param client Exact local client token.
     * @param requests Borrowed add/replace inputs, possibly referencing published text.
     * @param keys Borrowed remove keys, otherwise unused.
     * @param work Remaining explicit turn credits.
     * @return Published local barrier or rejection preserving all published state.
     */
    [[nodiscard]] std::expected<MuxChange, MuxError> apply(Command command, const MuxClient &client,
                                                           std::span<const core::SubscriptionRequest> requests,
                                                           std::span<const core::SubscriptionKey> keys, MuxWork &work);

    /** Prepare the inactive bank without publishing its membership or revision. */
    [[nodiscard]] std::expected<PreparedChange, MuxError> prepare(Command command, const MuxClient &client,
                                                                  std::span<const core::SubscriptionRequest> requests,
                                                                  std::span<const core::SubscriptionKey> keys,
                                                                  MuxWork &work);

    /** Publish the exact current candidate without allocation, or refuse a stale token. */
    [[nodiscard]] std::expected<MuxChange, MuxError> commit(const PreparedChange &candidate) noexcept;
    /** Check that a serialized candidate still names the current owner and inactive bank. */
    [[nodiscard]] bool current(const PreparedChange &candidate) const noexcept;
    /** Borrow candidate entries until the next mutation or preparation on this owner. */
    [[nodiscard]] std::expected<std::span<const MuxEntry>, MuxError>
    preparedEntries(const PreparedChange &candidate, const MuxClient &client) const noexcept;
    /** Borrow candidate aggregate interest until the next mutation or preparation. */
    [[nodiscard]] std::expected<std::span<const core::SubscriptionRequest>, MuxError>
    preparedAggregate(const PreparedChange &candidate) const noexcept;
    [[nodiscard]] std::expected<bool, MuxError>
    isCurrentInBank(const MuxClient &client, const MuxMembership &membership, MuxWork &work, bool inactive) const;
    [[nodiscard]] std::expected<core::DeliveryProjectionMask, MuxError>
    currentProjectionMaskInBank(const MuxClient &client,
                                const core::CapturedDeliveryProjections<MuxMembership> &captured, MuxWork &work,
                                bool inactive) const;

    public:
    /** Release fixed backing; copied opaque tokens retain only the fixed identity allocation.
     * Release work is finite by configured capacities and occurs synchronously outside turn credits.
     */
    ~MuxState();
    /** Transfer source tokens; invalidate borrowed views and leave the source invalid.
     * @param other Owner whose identity and backing transfer without allocation.
     */
    MuxState(MuxState &&other) noexcept;
    /** Retire destination tokens, then transfer source tokens; self-move is a no-op.
     * @param other Owner to transfer without allocation; cleanup is bounded by destination capacities.
     * @return This owner after transfer.
     */
    MuxState &operator=(MuxState &&other) noexcept;
    /** Disable mutable identity sharing.
     * @param other Owner that must instead be moved or configured independently.
     */
    MuxState(const MuxState &other) = delete;
    /** Disable mutable identity sharing.
     * @param other Owner that must instead be moved or configured independently.
     * @return Unavailable because copying is disabled.
     */
    MuxState &operator=(const MuxState &other) = delete;

    /** Capture record provenance/indexing and reserve both banks, without retaining declarations.
     * schema.storageBytes() is conservatively reserved once for retained schema identity, in addition
     * to all local backing. Initialization and cleanup are charged before allocations.
     * @param schema Valid immutable schema, borrowed only for the call; may die afterward.
     * @param family Fixed STREAM/TICKER/HISTORY contract.
     * @param policy Immutable selector profile; STREAM_ALL is unsupported for fixed TICKER/HISTORY owners.
     * @param limits Explicit finite immutable reservations.
     * @param work Caller-turn attempt, metadata, text and byte credits.
     * @return Empty owner or typed configuration/resource/work/allocation failure.
     */
    [[nodiscard]] static std::expected<MuxState, MuxError>
    create(const core::LocalSchema &schema, core::SubscriptionFamily family, MuxLimits limits, MuxWork &work,
           core::SubscriptionSelectorPolicy policy = core::SubscriptionSelectorPolicy::EXACT_ONLY);
    /** Inspect the immutable explicit selector profile.
     * @return Chosen profile; EXACT_ONLY when moved from. No work, allocation or mutation occurs.
     */
    /** Capture independently activated projections after matching original exact/all choices.
     * @param client Exact current logical client identity.
     * @param key Concrete physical record/symbol, never an All selector.
     * @param rawTime Actual indexed raw time for HISTORY only.
     * @param work Remaining attempt, matching and fixed metadata visits.
     * @return Original bounded per-bit capture, empty when no physical choice is eligible, or typed refusal.
     */
    [[nodiscard]] std::expected<core::CapturedDeliveryProjections<MuxMembership>, MuxError>
    selectProjections(const MuxClient &client, core::SubscriptionKey key, std::optional<std::int64_t> rawTime,
                      MuxWork &work) const;
    /** Prune only original captured bit activations, never looking up a new desired mask.
     * @param client Exact current client identity.
     * @param captured Detached original choices.
     * @param work Remaining finite membership inspections.
     * @return Surviving subset or explicit owner/work failure.
     */
    [[nodiscard]] std::expected<core::DeliveryProjectionMask, MuxError>
    currentProjectionMask(const MuxClient &client, const core::CapturedDeliveryProjections<MuxMembership> &captured,
                          MuxWork &work) const;
    [[nodiscard]] core::SubscriptionSelectorPolicy selectorPolicy() const noexcept;
    /** Activate one fixed slot; even an empty new client advances the global revision.
     * @param work Remaining turn credits, charged before scanning slots.
     * @return Owning client token, or rejection with no client/revision publication.
     */
    [[nodiscard]] std::expected<MuxClient, MuxError> createClient(MuxWork &work);
    /** Atomically remove only this client and its demand; token retirement is explicit.
     * @param client Exact current client token.
     * @param work Remaining turn credits.
     * @return Effective revision barrier or rejection.
     */
    [[nodiscard]] std::expected<MuxChange, MuxError> removeClient(const MuxClient &client, MuxWork &work);
    /** Upsert exact inputs atomically; identical duplicates collapse.
     * @param client Exact current client token.
     * @param requests Borrowed desired inputs; contradictory duplicates reject the entire call.
     * @param work Remaining turn credits.
     * @return Effective/no-op barrier or rejection preserving prior maps and views.
     */
    [[nodiscard]] std::expected<MuxChange, MuxError>
    add(const MuxClient &client, std::span<const core::SubscriptionRequest> requests, MuxWork &work);
    /** Remove exact keys without touching any other client's identical key.
     * @param client Exact current client token.
     * @param keys Borrowed keys; HISTORY still requires indexed record metadata.
     * @param work Remaining turn credits.
     * @return Effective/no-op barrier or rejection.
     */
    [[nodiscard]] std::expected<MuxChange, MuxError> remove(const MuxClient &client,
                                                            std::span<const core::SubscriptionKey> keys, MuxWork &work);
    /** Replace one client's whole desired map; input order alone is a no-op.
     * @param client Exact current client token.
     * @param requests Complete borrowed replacement, possibly empty.
     * @param work Remaining turn credits.
     * @return Effective/no-op barrier or rejection.
     */
    [[nodiscard]] std::expected<MuxChange, MuxError>
    replace(const MuxClient &client, std::span<const core::SubscriptionRequest> requests, MuxWork &work);
    /** Clear one client's desired map while keeping its client token active.
     * @param client Exact current client token.
     * @param work Remaining turn credits.
     * @return Effective/no-op barrier or rejection.
     */
    [[nodiscard]] std::expected<MuxChange, MuxError> clear(const MuxClient &client, MuxWork &work);
    /** Inspect one client's entries using constant-time token validation and stored ranges.
     * @param client Exact current client token.
     * @return Borrowed span, or INVALID_STATE/INVALID_CLIENT; effective owner mutation expires it.
     */
    [[nodiscard]] std::expected<std::span<const MuxEntry>, MuxError>
    clientEntries(const MuxClient &client) const noexcept;
    /** Inspect the complete exact union; HISTORY uses the signed minimum bound per key.
     * @return Borrowed writer-compatible snapshot, empty for moved-from state; any effective mutation expires it.
     */
    [[nodiscard]] std::span<const core::SubscriptionRequest> aggregateEntries() const noexcept;
    /** Select ordinary exact-key eligibility without publishing state or handling collector control flags.
     * @param client Exact current client token.
     * @param key Borrowed concrete DATA provenance/symbol with kind EXACT, never an All query.
     * Canonical STREAM-All permits concrete leading-asterisk DATA; desired Exact commands still refuse it.
     * Actual source admission validates the canonical cipher domain; this pure predicate is not a decoder.
     * @param rawTime Required only for HISTORY, compared inclusively in signed raw record units.
     * @param work Remaining query credits; failure is distinct from no match.
     * @return Current client membership or no selection, or typed work/input/client rejection.
     */
    [[nodiscard]] std::expected<std::optional<MuxMembership>, MuxError> select(const MuxClient &client,
                                                                               core::SubscriptionKey key,
                                                                               std::optional<std::int64_t> rawTime,
                                                                               MuxWork &work) const;
    /** Check membership in this exact active client, without comparing symbol buffers.
     * Charges one validation visit plus the complete client-entry count, even for a foreign token.
     * @param client Exact current client token.
     * @param membership Possibly default, foreign or retired token.
     * @param work Remaining query credits; no background or publication work occurs.
     * @return Currentness, or typed work/state/client rejection, never conflated with false.
     */
    [[nodiscard]] std::expected<bool, MuxError> isCurrent(const MuxClient &client, const MuxMembership &membership,
                                                          MuxWork &work) const;
    /** Inspect global local-desired revision without work or scans.
     * @return Revision starting at zero; zero for moved-from state.
     */
    [[nodiscard]] std::uint64_t revision() const noexcept;
    /** Inspect cached complete fixed reservation, including both banks and conservative provenance reserve.
     * @return Requested backing bytes, excluding implementation allocator bookkeeping/RSS; zero when moved from.
     */
    [[nodiscard]] std::size_t storageBytes() const noexcept;
};

} // namespace org::ttldtor::qdcxx::mux
