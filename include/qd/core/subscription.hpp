// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/core/delivery_projection.hpp"
#include "qd/core/local_schema.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace org::ttldtor::qdcxx::core {

/** Immutable delivery contract selected for one desired-state owner. */
enum class SubscriptionFamily {
    STREAM, ///< Every eligible record; no collector is implemented here.
    TICKER, ///< Latest-value contract; no collector is implemented here.
    HISTORY ///< Explicit indexed records with signed64 raw lower bounds.
};

/** Explicit subscription identity, independent of literal DATA text. */
enum class SubscriptionSelectorKind {
    EXACT, ///< Exact units; reserved leading asterisk remains invalid.
    ALL    ///< Every admitted canonical STREAM symbol of one known record.
};

/** Immutable caller-selected local/controlled-peer profile, never inferred from descriptors. */
enum class SubscriptionSelectorPolicy {
    EXACT_ONLY,             ///< Existing exact keys and legacy FD output.
    CANONICAL_EXACT_V1,     ///< Canonical Exact symbol domain for all families; All remains disabled.
    CANONICAL_STREAM_ALL_V1 ///< Canonical Exact codec plus explicit STREAM-only All permission.
};

/** Synchronous local rejection; a rejection leaves published state unchanged. */
enum class SubscriptionError {
    INVALID_STATE,       ///< Moved-from owner or invalid family.
    INVALID_CONTEXT,     ///< Record does not belong to the captured immutable schema.
    INVALID_SELECTOR,    ///< Null exact string or reserved leading asterisk.
    INVALID_HISTORY,     ///< Missing/unexpected bound or missing explicit indexed layout.
    CONTRADICTORY_INPUT, ///< One command supplies different attributes for the same key.
    RESOURCE_LIMIT,      ///< Resident, input, peak or representable allocation size exceeded.
    WORK_LIMIT,          ///< Explicit caller-supplied turn credits exhausted; no refund.
    IDENTITY_EXHAUSTED,  ///< Revision or non-reused activation cannot advance.
    ALLOCATION_FAILURE,  ///< Allocation failed before publication.
    UNSUPPORTED_SELECTOR ///< Recognized selector/profile is disabled for this family.
};

/** Finite logical capacities; zero always means zero, never unlimited.
 * Array slots and UTF-16 units count requested backing capacity, excluding allocator overhead.
 * Fixed owner identity storage and schema identity retained from the caller are not symbol storage.
 */
struct SubscriptionLimits {
    std::size_t schemaRecords;    ///< Maximum captured record metadata slots, fixed at creation.
    std::size_t entries;          ///< Maximum distinct resident desired entries.
    std::size_t symbolUnits;      ///< Maximum UTF-16 units in one exact selector.
    std::size_t totalSymbolUnits; ///< Maximum aggregate resident UTF-16 backing units.
    std::size_t peakEntries;      ///< Maximum old plus candidate entry backing slots together.
    std::size_t peakSymbolUnits;  ///< Maximum old plus candidate UTF-16 backing units together.
    std::size_t commandEntries;   ///< Maximum borrowed inputs, including duplicates, in one command.
};

/** Remaining work credits for an explicitly delimited caller turn.
 * Reuse this object across commands in the same turn. Start another turn by supplying fresh credits.
 * Successfully charged work is never refunded, including after validation/allocation failure.
 */
struct SubscriptionWork {
    std::size_t commands;    ///< Remaining command attempts.
    std::size_t keyVisits;   ///< Remaining conservative input/record/entry/comparison and disposal visits.
    std::size_t symbolUnits; ///< Remaining conservative examined/copied UTF-16 units, including duplicates.
};

/** Borrowed explicit key; caller preserves Exact text for the complete command.
 * Trailing selector preserves ordinary two-field aggregate initialization, not binary ABI/sizeof.
 */
struct SubscriptionKey {
    RecordContext context;      ///< Owned provenance; equal-shaped independent schemas never match.
    std::u16string_view symbol; ///< Exact units; null is invalid, non-null empty is a valid key.
    SubscriptionSelectorKind selector = SubscriptionSelectorKind::EXACT; ///< All requires unused empty text.
};

/** Select all symbols of one known record without allocating or retaining text.
 * @param context Owning provenance, validated by the receiving owner.
 * @return Explicit All key with static empty text; no permission is granted.
 */
[[nodiscard]] SubscriptionKey allSymbols(RecordContext context) noexcept;

/** Borrowed command input; HISTORY time is an attribute rather than part of the key. */
struct SubscriptionRequest {
    SubscriptionKey key;                      ///< Record and exact symbol retained by copy on success.
    std::optional<std::int64_t> historyBound; ///< Raw indexed record time, never Unix milliseconds.
    DeliveryProjectionMask projectionMask = NATIVE_DELIVERY_PROJECTION; ///< Local output identities only.
};

/** Owning eligibility token; desired entries are singleton activations, delivery may capture two original choices.
 * Retaining it does not retain desired entries, symbols, callbacks or protocol owners.
 * Tokens share one fixed identity allocation per owner, which survives owner destruction until its
 * last token is released. These local limits do not enforce a runtime's aggregate retained-token quota.
 */
class SubscriptionMembership {
    struct Identity;
    std::shared_ptr<const Identity> identity_;
    std::uint64_t activation_ = 0;
    std::uint64_t alternateActivation_ = 0; ///< Second original activation in a captured eligibility choice.

    friend class SubscriptionState;

    public:
    /** Construct an invalid token, which is never current. */
    SubscriptionMembership() noexcept = default;
    /** Form a structural OR of singleton activations, not a liveness or record-match assertion.
     * Default, foreign or compound inputs return invalid before duplicate handling.
     * The actual collector first matches both desired entries to one concrete record/symbol.
     * Independent immutable copies may be combined/released concurrently; owners remain serialized.
     * @param first Original singleton; its owner need not still exist.
     * @param second Singleton from the same fixed origin.
     * @return Two original choices, or the same valid singleton for equal inputs; never allocates/throws.
     */
    [[nodiscard]] static SubscriptionMembership anyOf(const SubscriptionMembership &first,
                                                      const SubscriptionMembership &second) noexcept;
};

/** Borrowed published entry; text is invalidated by effective mutation, move or owner destruction.
 * Copying membership alone produces an owning token independent of the borrowed text lifetime.
 */
struct SubscriptionEntry {
    SubscriptionKey key;                      ///< Immutable borrowed exact key in the owner arena.
    std::optional<std::int64_t> historyBound; ///< Absent except for HISTORY.
    SubscriptionMembership membership;        ///< Physical-key activation belonging to this owner only.
    DeliveryProjectionMask projectionMask = NATIVE_DELIVERY_PROJECTION; ///< Current independently activated bits.
    std::array<SubscriptionMembership, MAX_DELIVERY_PROJECTIONS>
        projectionMemberships{}; ///< Per-bit singleton activations.
};

/** Capture only the original exact/all contributions already matched to one concrete record.
 * @param first Original matched desired entry.
 * @param second Optional original alternate entry for that same physical record/symbol.
 * @return Detached bounded per-bit choices; no later desired mask is consulted.
 */
[[nodiscard]] CapturedDeliveryProjections<SubscriptionMembership>
captureDeliveryProjections(const SubscriptionEntry &first, const SubscriptionEntry *second = nullptr) noexcept;

/** Application barrier for a fully validated synchronous command. */
struct SubscriptionChange {
    std::uint64_t revision; ///< Published revision after the command; starts at zero.
    bool changed;           ///< True exactly when the desired map changed.
};

/** Bounded, move-only desired state, independent of sender IDs, transport and collectors.
 * Callers serialize all access; concurrent or reentrant use is unsupported. No work occurs in
 * the background. Moves transfer memberships and invalidate borrowed entry views; the source
 * becomes empty and rejects commands with INVALID_STATE when command work is available. No-op
 * and failed commands preserve views. Command-credit exhaustion is checked before state validation.
 */
class SubscriptionState {
    struct Record;
    enum class Command { ADD, REMOVE, REPLACE, CLEAR };

    std::unique_ptr<Record[]> records_;
    std::size_t recordCount_ = 0;
    std::shared_ptr<const SubscriptionMembership::Identity> identity_;
    SubscriptionFamily family_ = SubscriptionFamily::STREAM;
    SubscriptionSelectorPolicy selectorPolicy_ = SubscriptionSelectorPolicy::EXACT_ONLY; ///< Immutable profile.
    SubscriptionLimits limits_{};
    std::unique_ptr<SubscriptionEntry[]> entries_;
    std::unique_ptr<char16_t[]> symbols_;
    std::size_t size_ = 0;
    std::size_t symbolUnits_ = 0;
    std::uint64_t revision_ = 0;
    std::uint64_t lastActivation_ = 0;

    SubscriptionState() noexcept;
    [[nodiscard]] std::expected<SubscriptionChange, SubscriptionError>
    apply(Command command, std::span<const SubscriptionRequest> requests, std::span<const SubscriptionKey> keys,
          SubscriptionWork &work);

    public:
    /** Release only this owner's storage; previously copied tokens become non-current. */
    ~SubscriptionState();
    /** Disable copying so mutable owners cannot accidentally share an eligibility identity.
     * @param other Owner that must instead be moved or configured independently.
     */
    SubscriptionState(const SubscriptionState &other) = delete;
    /** Disable copy assignment to preserve unique mutable ownership of an eligibility identity.
     * @param other Owner that must instead be moved or configured independently.
     * @return Unavailable because copying an owner is intentionally disabled.
     */
    SubscriptionState &operator=(const SubscriptionState &other) = delete;

    /** Transfer state identity and storage without allocating.
     * @param other Owner to leave empty and invalid for commands.
     */
    SubscriptionState(SubscriptionState &&other) noexcept;
    /** Retire this owner's identity and transfer another owner's storage without allocating.
     * @param other Owner to leave empty and invalid, or this object for a no-op.
     * @return This owner after transfer.
     */
    SubscriptionState &operator=(SubscriptionState &&other) noexcept;

    /** Capture owned record provenance and indexed-layout facts without copying schema declarations.
     * @param schema Immutable schema borrowed for this call; it need not outlive the result.
     * @param family Fixed registration family; invalid enumeration values are rejected.
     * @param limits Required immutable finite capacities. Peak limits may intentionally prevent growth.
     * @param policy Immutable profile selected before activation; default preserves legacy Exact semantics.
     * @return Empty owner, or invalid-state/resource/allocation error. No work/I/O is started.
     */
    [[nodiscard]] static std::expected<SubscriptionState, SubscriptionError>
    create(const LocalSchema &schema, SubscriptionFamily family, SubscriptionLimits limits,
           SubscriptionSelectorPolicy policy = SubscriptionSelectorPolicy::EXACT_ONLY);
    /** Inspect the immutable selector profile without scanning or allocating.
     * @return Chosen profile; EXACT_ONLY for a moved-from owner.
     */
    [[nodiscard]] SubscriptionSelectorPolicy selectorPolicy() const noexcept;

    /** Upsert complete inputs atomically, collapsing identical duplicates.
     * @param requests Borrowed inputs; symbol views may refer to this owner's current entries.
     * @param work Remaining turn credits, charged even on failure.
     * @return One revision advance for an effective change, no-op, or typed rejection with rollback.
     */
    [[nodiscard]] std::expected<SubscriptionChange, SubscriptionError>
    add(std::span<const SubscriptionRequest> requests, SubscriptionWork &work);
    /** Remove exact keys atomically; HISTORY removal never requires an old time attribute.
     * @param keys Borrowed keys; identical duplicates and absent keys are no-ops.
     * @param work Remaining turn credits, charged even on failure.
     * @return Application barrier or typed rejection; no partial removal is published.
     */
    [[nodiscard]] std::expected<SubscriptionChange, SubscriptionError> remove(std::span<const SubscriptionKey> keys,
                                                                              SubscriptionWork &work);
    /** Publish exactly the validated input map; empty input clears it.
     * @param requests Borrowed inputs; contradictory duplicates are rejected.
     * @param work Remaining turn credits, charged even on failure.
     * @return Application barrier; unchanged memberships retain activation even if input is reordered.
     */
    [[nodiscard]] std::expected<SubscriptionChange, SubscriptionError>
    replace(std::span<const SubscriptionRequest> requests, SubscriptionWork &work);
    /** Clear all desired entries without allocating candidate storage on the successful path.
     * Failure follows the ordinary command rejection path.
     * @param work Remaining turn credits; one command is charged, including empty no-op.
     * @return Application barrier or work/state/revision error.
     */
    [[nodiscard]] std::expected<SubscriptionChange, SubscriptionError> clear(SubscriptionWork &work);
    /** Inspect immutable borrowed entries; empty for a moved-from owner.
     * @return View valid until effective mutation, move, assignment or destruction of this owner.
     */
    [[nodiscard]] std::span<const SubscriptionEntry> entries() const noexcept;
    /** Test current membership without comparing symbol buffers.
     * @param membership Owning token, possibly default, retired, or from another owner.
     * @return True only for a current activation in this exact owner; bounded by resident entries.
     */
    [[nodiscard]] bool isCurrent(const SubscriptionMembership &membership) const noexcept;
    /** Prune captured choices using only their original activations.
     * @param captured Original detached per-bit choices.
     * @return Surviving subset; removed and re-added bits are never acquired by an old capture.
     */
    [[nodiscard]] DeliveryProjectionMask
    currentProjectionMask(const CapturedDeliveryProjections<SubscriptionMembership> &captured) const noexcept;
    /** Inspect the published revision without consuming turn work.
     * @return Current revision; zero for a moved-from owner.
     */
    [[nodiscard]] std::uint64_t revision() const noexcept;
    /** Inspect the immutable family; a moved-from owner retains its former family value.
     * @return Selected contract family, not a claim of implemented collector support.
     */
    [[nodiscard]] SubscriptionFamily family() const noexcept;
};

} // namespace org::ttldtor::qdcxx::core
