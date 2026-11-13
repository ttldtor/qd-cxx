// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "ticker_transaction.hpp"

#include "../core/delivery_statistics.hpp"
#include "qd/encoding/penta_symbol.hpp"

#include <algorithm>
#include <atomic>
#include <limits>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::collectors {

/** Fixed detached identity; producer-safe final release cannot reach protocol state. */
struct TickerIdentity {
    std::atomic<std::size_t> references{
        1};                           ///< Collector plus detached tokens; no reference is reused after retirement.
    bool replacementReserved = false; ///< Accessed only by the serialized collector and candidate.
};

TickerCollector::PreparedReplace::PreparedReplace(const TickerViewToken &view, CoreReplace &&desired,
                                                  std::size_t ordered) noexcept
    : view_(view), desired_(std::move(desired)), ordered_(ordered) {
    view_.origin_->replacementReserved = true;
}

TickerCollector::PreparedReplace::~PreparedReplace() {
    if (view_.origin_) {
        desired_.reset();
        view_.origin_->replacementReserved = false;
    }
}

namespace {
constexpr auto NONE =
    std::numeric_limits<std::size_t>::max(); ///< Absent queue/cache index and epoch-token discriminator.
constexpr std::size_t INITIAL = 0;           ///< Initial queue index.
constexpr std::size_t UPDATE = 1;            ///< Update queue index.
constexpr std::int32_t TICKER_DATA = 10;     ///< Selected whole-event source family.

/** Retain fixed identity without allocation; representability exhaustion cannot wrap.
 * @param identity Fixed marker, or null.
 */
void retain(TickerIdentity *identity) noexcept {
    if (!identity) {
        return;
    }

    auto count = identity->references.load(std::memory_order_relaxed);

    do {
        if (count == NONE) {
            std::terminate();
        }
    } while (!identity->references.compare_exchange_weak(count, count + 1, std::memory_order_relaxed));
}

/** Release only atomic scalar backing.
 * @param identity Fixed marker, or null.
 */
void release(TickerIdentity *identity) noexcept {
    if (identity && identity->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete identity;
    }
}

/** Checked work/backing arithmetic.
 * @param left First nonnegative quantity.
 * @param right Quantity to add.
 * @param error Typed failure on overflow.
 * @return Representable sum.
 */
std::size_t sum(std::size_t left, std::size_t right, TickerError error = TickerError::WORK_LIMIT) {
    if (right > NONE - left) {
        throw error;
    }

    return left + right;
}

/** Checked product, including zero factors.
 * @param left First nonnegative factor.
 * @param right Second nonnegative factor.
 * @param error Typed failure on overflow.
 * @return Representable product.
 */
std::size_t product(std::size_t left, std::size_t right, TickerError error = TickerError::WORK_LIMIT) {
    if (right != 0 && left > NONE / right) {
        throw error;
    }

    return left * right;
}

/** Debit one complete bounded phase before touching its state.
 * @param work Caller turn credits, never refunded.
 * @param operations Operation attempts.
 * @param visits Metadata visits.
 * @param text UTF-16 examination allowance.
 * @param bytes Payload/disposal allowance.
 */
void spend(TickerWork &work, std::size_t operations, std::size_t visits = 0, std::size_t text = 0,
           std::size_t bytes = 0) {
    if (operations > work.operations || visits > work.visits || text > work.textUnits || bytes > work.bytes) {
        throw TickerError::WORK_LIMIT;
    }

    work.operations -= operations;
    work.visits -= visits;
    work.textUnits -= text;
    work.bytes -= bytes;
}

/** Saturate diagnostics without overflowing accounting/identity counters.
 * @param value Diagnostic count to update.
 * @param increment Known local increment.
 * @param saturated Explicit saturation evidence.
 */
void count(std::size_t &value, std::size_t increment, bool &saturated) noexcept {
    if (increment > NONE - value) {
        value = NONE;
        saturated = true;
    } else {
        value += increment;
    }
}

/** Compare exact optional text, charging the complete possible examination first.
 * @param left First nullable text.
 * @param right Second nullable text.
 * @param work Caller examination credits.
 * @return Exact null/content equality.
 */
bool equalText(std::optional<std::u16string_view> left, std::optional<std::u16string_view> right, TickerWork &work) {
    spend(work, 0, 1, sum(left ? left->size() : 0, right ? right->size() : 0));

    return left == right;
}

/** Compare selected default-field data, never flags/cipher/binding/event-time metadata.
 * @param left Prior candidate value.
 * @param right Incoming admitted value.
 * @param work Complete field/text/opaque-byte credits.
 * @return Exact raw/default wire-value equality in the supported native catalog.
 */
bool equalData(const protocol::LiveRecordView &left, const protocol::LiveRecordView &right, TickerWork &work) {
    spend(work, 0, 1);

    if (left.values.size() != right.values.size() || left.typedValues.size() != right.typedValues.size()) {
        return false;
    }

    for (std::size_t i = 0; i < left.values.size(); ++i) {
        spend(work, 0, 1);

        if (left.values[i] != right.values[i]) {
            return false;
        }
    }

    for (std::size_t i = 0; i < left.typedValues.size(); ++i) {
        spend(work, 0, 1);
        const auto &a = left.typedValues[i];
        const auto &b = right.typedValues[i];

        if (a.type != b.type || a.raw != b.raw || !equalText(a.units, b.units, work)) {
            return false;
        }

        spend(work, 0, 1, 0, sum(a.bytes ? a.bytes->size() : 0, b.bytes ? b.bytes->size() : 0));

        if (a.bytes.has_value() != b.bytes.has_value()) {
            return false;
        }

        if (a.bytes && !std::ranges::equal(*a.bytes, *b.bytes)) {
            return false;
        }
    }

    return true;
}

/** Read the first Java physical int slot of an admitted scalar field.
 * @param field Complete canonical logical scalar, validated through the local schema.
 * @return Signed high32 for two-slot types, otherwise the signed raw int slot.
 */
std::int32_t physicalVersion(const protocol::LiveFieldValueView &field) noexcept {
    if (field.type == core::SerialFieldType::LONG || field.type == core::SerialFieldType::WIDE_DECIMAL ||
        field.type == core::SerialFieldType::TIME_MILLIS) {
        return static_cast<std::int32_t>(field.raw >> 32);
    }

    return static_cast<std::int32_t>(field.raw);
}

/** Admit only exact generic scalar identities with a Java int storage slot.
 * @param type Exact local full type, never inferred from serialization bits alone.
 * @return Whether a version comparison can use its first physical int slot.
 */
bool versionType(core::SerialFieldType type) noexcept {
    switch (type) {
    case core::SerialFieldType::VOID_FIELD:
    case core::SerialFieldType::BYTE:
    case core::SerialFieldType::UTF_CHAR:
    case core::SerialFieldType::SHORT:
    case core::SerialFieldType::INT:
    case core::SerialFieldType::COMPACT_INT:
    case core::SerialFieldType::DECIMAL:
    case core::SerialFieldType::SHORT_STRING:
    case core::SerialFieldType::TIME_SECONDS:
    case core::SerialFieldType::SEQUENCE:
    case core::SerialFieldType::DATE:
    case core::SerialFieldType::LONG:
    case core::SerialFieldType::WIDE_DECIMAL:
    case core::SerialFieldType::TIME_MILLIS:
        return true;
    default:
        return false;
    }
}
} // namespace

TickerToken::TickerToken(TickerIdentity *origin, std::size_t slot, std::uint64_t generation) noexcept
    : origin_(origin), slot_(slot), generation_(generation) {
    retain(origin_);
}

TickerToken::TickerToken(const TickerToken &other) noexcept
    : TickerToken(other.origin_, other.slot_, other.generation_) {
}

TickerToken::TickerToken(TickerToken &&other) noexcept
    : origin_(std::exchange(other.origin_, nullptr)), slot_(other.slot_), generation_(other.generation_) {
}

TickerToken &TickerToken::operator=(const TickerToken &other) noexcept {
    if (this != &other) {
        TickerToken candidate(other);
        std::swap(origin_, candidate.origin_);
        slot_ = other.slot_;
        generation_ = other.generation_;
    }

    return *this;
}

TickerToken &TickerToken::operator=(TickerToken &&other) noexcept {
    if (this != &other) {
        release(origin_);
        origin_ = std::exchange(other.origin_, nullptr);
        slot_ = other.slot_;
        generation_ = other.generation_;
    }

    return *this;
}

TickerToken::~TickerToken() {
    release(origin_);
}

bool TickerToken::matches(const TickerToken &other) const noexcept {
    return origin_ && origin_ == other.origin_ && slot_ == other.slot_ && generation_ == other.generation_;
}

bool TickerToken::valid() const noexcept {
    return origin_ != nullptr;
}

/** Fixed collector state; candidate metadata owns no borrowed source owner. */
struct TickerCollector::Impl {
    /** One latest key borrowing exactly one retained frame. */
    struct Entry {
        const protocol::LiveRecordView *record = nullptr; ///< Null for an unused slot.
        std::size_t frame = NONE;                         ///< Owning frame, or incoming candidate discriminator.
        std::uint64_t generation = 0;                     ///< Original source binding generation.
        bool defaultValue = false; ///< Fresh negative version materializes a canonical zero/null cursor.
    };

    /** Unique whole-event owner; views never own these independently. */
    struct Frame {
        std::optional<protocol::LiveEvent> event; ///< Complete immutable owner.
        std::size_t bytes = 0;                    ///< State plus full arena capacity.
    };

    /** One membership's two queue links and single pending value. */
    struct Pending {
        core::SubscriptionMembership membership; ///< Owning fixed core activation, not source storage.
        core::CapturedDeliveryProjections<core::SubscriptionMembership>
            projections{};                            ///< Known original bit activations, including acknowledged bits.
        core::DeliveryProjectionMask pendingMask = 0; ///< Remaining source-owned business outputs only.
        std::uint64_t revision = 0;                   ///< Desired revision at effective source/initial admission.
        std::size_t next[2]{NONE, NONE};              ///< Next initial/update links.
        bool queued[2]{false, false};                 ///< Each key occupies at most one node in either queue.
        bool first = true;                            ///< No value has yet been acknowledged for this activation.
        bool dirty = false;                           ///< Single logical pending value across both links.
    };

    /** Scalar queue/offer state copied into preallocated candidate scratch. */
    struct Queues {
        std::size_t admissions = 0;         ///< Transaction-local accepted occurrences, published only at commit.
        std::size_t conflations = 0;        ///< Transaction-local superseded pending occurrences.
        std::size_t head[2]{NONE, NONE};    ///< Initial and update heads.
        std::size_t tail[2]{NONE, NONE};    ///< Initial and update tails.
        std::size_t pending = 0;            ///< Distinct dirty keys, never more than cache cardinality.
        std::size_t pendingWeight = 0;      ///< Source-owned remaining business outputs; excludes transferred Mux FIFO.
        std::size_t credit = 0;             ///< Initial/update balance at the single-record retrieval boundary.
        std::size_t offered = NONE;         ///< Current prepared cache index.
        std::size_t offeredQueue = INITIAL; ///< Queue traversed by the prepared single-record retrieval.
        std::size_t nextCredit = 0;         ///< Balance published only by successful pop.
        bool initialOnlyOffer = false;      ///< Selected initial-only offer preserves ordinary fairness credit.
    };

    /** Independently bounded local desired owner and scalar queues. */
    struct View {
        std::shared_ptr<core::detail::DeliveryStatisticsState> statistics; ///< Optional pure delivery accounting.
        std::optional<core::SubscriptionState> desired;                    ///< No protocol owner or callable.
        std::uint64_t activation = 0;                                      ///< Non-reused selection within this origin.
        Queues queues;                                                     ///< Active per-view queue state.
    };

    core::LocalSchema schema;                     ///< Immutable exact source/local provenance and configured layout.
    TickerLimits limits;                          ///< Required immutable capacities.
    TickerOptions options;                        ///< Explicit retention and scheduling; no borrowed bindings.
    std::unique_ptr<std::size_t[]> versionFields; ///< Owned local-ID indexed optional scalar position; NONE disables.
    std::unique_ptr<protocol::LiveFieldValueView[]>
        defaultFields; ///< Canonical zero/null fields, immutable after create.
    std::unique_ptr<std::span<const protocol::LiveFieldValueView>[]> defaultShapes; ///< Per-record default field spans.
    std::unique_ptr<std::int32_t[]> defaultLegacy; ///< Shared zero compact-only fields, immutable after create.
    std::unique_ptr<protocol::LiveRecordView[]> defaultRecords; ///< Per-cache-slot stable normalized default views.
    TickerIdentity *origin = nullptr;            ///< One collector reference plus detached token references.
    std::unique_ptr<Entry[]> entries;            ///< Active fixed cache metadata.
    std::unique_ptr<Entry[]> candidate;          ///< Fixed whole-frame candidate metadata.
    std::unique_ptr<Frame[]> frames;             ///< Unique whole-event owners.
    std::unique_ptr<View[]> views;               ///< Fixed view slots.
    std::unique_ptr<Pending[]> pending;          ///< Reserved active metadata, viewSlots times cacheKeys.
    std::unique_ptr<Pending[]> planned;          ///< Complete candidate metadata, never source owners.
    std::unique_ptr<Queues[]> queues;            ///< Candidate view scalar state.
    std::unique_ptr<std::size_t[]> references;   ///< Candidate references per retained frame.
    std::unique_ptr<std::size_t[]> order;        ///< Input-order cache indices, never borrowed command text.
    std::optional<protocol::LiveIdentity> epoch; ///< Serialized source pin, retained through empty cache.
    std::size_t fixed = 0;                       ///< Impl/origin/array/schema requested backing.
    std::size_t keys = 0;                        ///< Resident cache cardinality.
    std::size_t frameCount = 0;                  ///< Unique retained event owners.
    std::size_t bytes = 0;                       ///< Unique retained whole-event backing.
    std::uint64_t nextActivation = 0;            ///< Last issued view activation.
    std::uint64_t generation = 1;                ///< Local epoch generation; source binds on first accepted frame.
    std::uint64_t resetSequence = 0;             ///< Exact monotonic owner barrier.
    TickerState state = TickerState::EMPTY;      ///< Shared freshness.
    TickerCounts counts;                         ///< Saturating observations.
    TickerDiagnostic diagnostic;                 ///< Reserved shared failure.
    std::optional<TickerResetNotice> notice;     ///< Reserved coalesced owner notice.

    /** Allocate only fixed backing; schema moves only after construction succeeds.
     * @param source Validated schema, transferred after all fixed allocations.
     * @param bounds Checked finite capacities.
     * @param settings Explicit profile.
     * @param backing Complete fixed-byte charge.
     */
    Impl(core::LocalSchema &&source, TickerLimits bounds, TickerOptions settings, std::size_t backing)
        : schema(std::move(source)), limits(bounds), options(settings), fixed(backing) {
    }

    /** Destroy source owners before dropping the fixed detached origin. */
    ~Impl() {
        if (views)
            for (std::size_t v = 0; v < limits.viewSlots; ++v) {
                core::detail::updateDelivery(views[v].statistics, [&](auto &s) {
                    s.pendingEvents -= views[v].queues.pendingWeight;
                    s.discardedEvents.add(views[v].queues.pendingWeight);
                });
            }

        release(origin);
    }

    /** Verify token origin, discriminator and live activation.
     * @param token Detached selection.
     * @return Slot or absent for default/foreign/closed/reused tokens.
     */
    std::optional<std::size_t> view(const TickerViewToken &token) const noexcept {
        if (token.origin_ != origin || token.slot_ >= limits.viewSlots) {
            return {};
        }

        const auto &selected = views[token.slot_];

        if (!selected.desired || selected.activation != token.generation_) {
            return {};
        }

        return token.slot_;
    }

    /** Address one fixed view's metadata.
     * @param storage Active or candidate whole storage.
     * @param slot Valid view index.
     * @return Its cacheKeys contiguous slots, or null at zero capacity.
     */
    Pending *rows(Pending *storage, std::size_t slot) const noexcept {
        return limits.cacheKeys ? storage + slot * limits.cacheKeys : nullptr;
    }

    /** Invalidate every borrowed current offer before mutation/release. */
    void invalidate() noexcept {
        for (std::size_t v = 0; v < limits.viewSlots; ++v) {
            views[v].queues.offered = NONE;
        }
    }

    /** Clear candidate references on every return path, including work rejection. */
    void clearPlan() noexcept {
        for (std::size_t i = 0; i < limits.cacheKeys; ++i) {
            candidate[i] = {};
        }

        for (std::size_t i = 0; i < limits.viewSlots * limits.cacheKeys; ++i) {
            planned[i] = {};
        }

        for (std::size_t v = 0; v < limits.viewSlots; ++v) {
            queues[v] = {};
        }
    }

    /** Find a current exact desired membership; optional work charges read-only planning.
     * @param slot Live view slot.
     * @param record Exact admitted cache key.
     * @param work Charged planning credits, or null after full maintenance preflight.
     * @return Borrowed current entry, or null.
     */
    const core::SubscriptionEntry *match(std::size_t slot, const protocol::LiveRecordView &record,
                                         TickerWork *work = nullptr) const {
        if (!views[slot].desired) {
            return nullptr;
        }

        for (const auto &entry : views[slot].desired->entries()) {
            if (work) {
                spend(*work, 0, 1);
            }

            if (!entry.key.context.matches(record.context)) {
                continue;
            }

            if (work) {
                spend(*work, 0, 0, sum(entry.key.symbol.size(), record.symbol.size()));
            }

            if (entry.key.symbol == record.symbol) {
                return &entry;
            }
        }

        return nullptr;
    }

    /** Find a candidate/active exact key.
     * @param storage Fixed metadata map.
     * @param record Admitted key.
     * @param work Lookup credits.
     * @return Cache slot or NONE.
     */
    std::size_t find(const Entry *storage, const protocol::LiveRecordView &record, TickerWork &work) const {
        for (std::size_t i = 0; i < limits.cacheKeys; ++i) {
            spend(work, 0, 1);

            if (!storage[i].record || !storage[i].record->context.matches(record.context)) {
                continue;
            }

            spend(work, 0, 0, sum(storage[i].record->symbol.size(), record.symbol.size()));

            if (storage[i].record->symbol == record.symbol) {
                return i;
            }
        }

        return NONE;
    }

    /** Build a temporary canonical default value without changing source/key metadata or allocating.
     * @param source Original immutable event record retained by the cache.
     * @return Value-copy metadata with all declared fields at their canonical zero/null defaults.
     */
    protocol::LiveRecordView defaultRecord(const protocol::LiveRecordView &source) const noexcept {
        auto result = source;
        result.typedValues = defaultShapes[static_cast<std::size_t>(source.localId)];
        result.values = {defaultLegacy.get(), source.values.size()};
        result.eventFlags = 0;
        result.removalTime.reset();

        if (result.fieldTime) {
            result.fieldTime->rawSeconds = 0;

            if (result.fieldTime->rawSequence) {
                result.fieldTime->rawSequence = 0;
            }
        }

        return result;
    }

    /** Borrow the selected cached value, including explicit fresh-version defaults.
     * @param index Valid occupied cache slot.
     * @return Stable view until the collector's documented next invalidation.
     */
    const protocol::LiveRecordView *valueRecord(std::size_t index) const noexcept {
        return entries[index].defaultValue ? &defaultRecords[index] : entries[index].record;
    }

    /** Append one fixed link if not already queued.
     * @param state Selected view queue state.
     * @param data Corresponding pending metadata.
     * @param index Resident cache slot.
     * @param kind INITIAL or UPDATE.
     */
    void push(Queues &state, Pending *data, std::size_t index, std::size_t kind) noexcept {
        if (data[index].queued[kind]) {
            return;
        }

        if (state.tail[kind] == NONE) {
            state.head[kind] = index;
        } else {
            data[state.tail[kind]].next[kind] = index;
        }

        state.tail[kind] = index;
        data[index].next[kind] = NONE;
        data[index].queued[kind] = true;
    }

    /** Unlink one bounded node, preserving other positions.
     * @param state Selected queue state.
     * @param data Corresponding metadata.
     * @param index Cache slot to unlink.
     * @param kind INITIAL or UPDATE.
     */
    void unlink(Queues &state, Pending *data, std::size_t index, std::size_t kind) noexcept {
        if (!data[index].queued[kind]) {
            return;
        }

        auto current = state.head[kind];
        auto previous = NONE;

        while (current != index && current != NONE) {
            previous = current;
            current = data[current].next[kind];
        }

        if (current == NONE) {
            std::terminate();
        }

        if (previous == NONE) {
            state.head[kind] = data[index].next[kind];
        } else {
            data[previous].next[kind] = data[index].next[kind];
        }

        if (state.tail[kind] == index) {
            state.tail[kind] = previous;
        }

        data[index].queued[kind] = false;
        data[index].next[kind] = NONE;
    }

    /** Traverse only clean heads of a queue actually reached by retrieval.
     * A clean nontraversed link survives because another update can reactivate its old position.
     * @param state Selected view queue state.
     * @param data Fixed per-key metadata, containing no payload owner.
     * @param kind Queue the retrieval algorithm actually visits.
     */
    void cleanHead(Queues &state, Pending *data, std::size_t kind) noexcept {
        while (state.head[kind] != NONE && !data[state.head[kind]].dirty) {
            unlink(state, data, state.head[kind], kind);
        }
    }

    /** Read-only prepared selection; no link or retrieval credit is acknowledged by front. */
    struct Selection {
        std::size_t index;      ///< Dirty cache head, or NONE.
        std::size_t kind;       ///< INITIAL or UPDATE.
        std::size_t nextCredit; ///< Balance committed only after pop.
    };

    /** Plan retrieval without cleaning any active link or advancing active credit.
     * @param state Active view scalar state, borrowed unchanged.
     * @param data Active fixed per-key rows, borrowed unchanged.
     * @return Prepared selection; source mutation may invalidate it before acknowledgment.
     */
    Selection prepareOffer(const Queues &state, const Pending *data) const noexcept {
        std::size_t head[2]{state.head[INITIAL], state.head[UPDATE]};
        auto credit = state.credit;

        while (head[INITIAL] != NONE || head[UPDATE] != NONE) {
            if (head[UPDATE] == NONE) {
                credit = 0;
            }

            if (credit < options.retrievalQuantum && head[INITIAL] != NONE) {
                while (head[INITIAL] != NONE && !data[head[INITIAL]].dirty) {
                    head[INITIAL] = data[head[INITIAL]].next[INITIAL];
                }

                if (head[INITIAL] != NONE) {
                    return {head[INITIAL], INITIAL, credit + 1};
                }
            }

            if (head[INITIAL] == NONE) {
                credit = options.retrievalQuantum;
            }

            if (credit && head[UPDATE] != NONE) {
                while (head[UPDATE] != NONE && !data[head[UPDATE]].dirty) {
                    head[UPDATE] = data[head[UPDATE]].next[UPDATE];
                }

                if (head[UPDATE] != NONE) {
                    return {head[UPDATE], UPDATE, credit - 1};
                }
            }
        }

        return {NONE, INITIAL, credit};
    }

    /** Select a dirty head at a one-record retrieval boundary, preserving untraversed links.
     * @param state Mutable preflighted queue metadata.
     * @param data Fixed per-key rows.
     * @return INITIAL, UPDATE, or NONE after both queues have been traversed empty.
     */
    std::size_t select(Queues &state, Pending *data) noexcept {
        while (state.head[INITIAL] != NONE || state.head[UPDATE] != NONE) {
            if (state.head[UPDATE] == NONE) {
                state.credit = 0;
            }

            if (state.credit < options.retrievalQuantum && state.head[INITIAL] != NONE) {
                cleanHead(state, data, INITIAL);

                if (state.head[INITIAL] != NONE) {
                    return INITIAL;
                }
            }

            if (state.head[INITIAL] == NONE) {
                state.credit = options.retrievalQuantum;
            }

            if (state.credit && state.head[UPDATE] != NONE) {
                cleanHead(state, data, UPDATE);

                if (state.head[UPDATE] != NONE) {
                    return UPDATE;
                }
            }
        }

        return NONE;
    }

    /** Complete queue traversal after the sink used its single record capacity.
     * Only a quantum boundary or queue exhaustion permits visiting the opposite queue.
     * @param state Selected view scalar state after acknowledgment.
     * @param data Fixed pending metadata.
     * @param selected Queue supplying the acknowledged record.
     */
    void finishRetrieval(Queues &state, Pending *data, std::size_t selected) noexcept {
        cleanHead(state, data, selected);

        if (selected == INITIAL) {
            if (state.head[INITIAL] == NONE || state.credit == options.retrievalQuantum) {
                if (state.head[INITIAL] == NONE) {
                    state.credit = options.retrievalQuantum;
                }

                cleanHead(state, data, UPDATE);
            }
        } else if (state.credit == 0 || state.head[UPDATE] == NONE) {
            if (state.head[UPDATE] == NONE) {
                state.credit = 0;
            }

            cleanHead(state, data, INITIAL);

            if (state.head[INITIAL] == NONE) {
                state.credit = options.retrievalQuantum;
                cleanHead(state, data, UPDATE);
            }
        }
    }

    /** Retire one logical pending membership without retaining historical payload.
     * @param slot View slot.
     * @param index Cache slot.
     */
    void withdraw(std::size_t slot, std::size_t index) noexcept {
        auto &state = views[slot].queues;
        auto *data = rows(pending.get(), slot);

        if (data[index].dirty) {
            --state.pending;
            const auto weight = static_cast<std::size_t>(std::popcount(data[index].pendingMask));
            state.pendingWeight -= weight;
            count(counts.withdrawn, 1, counts.saturated);
            core::detail::updateDelivery(views[slot].statistics, [&](auto &s) {
                s.pendingEvents -= weight;
                s.discardedEvents.add(weight);
            });
        }

        unlink(state, data, index, INITIAL);
        unlink(state, data, index, UPDATE);
        data[index] = {};
        state.offered = NONE;
    }

    /** Prune only original pending bits; acknowledged bits carry no remaining source weight.
     * @param slot Actual source view.
     * @param index Stable cache slot.
     */
    void pruneProjections(std::size_t slot, std::size_t index) noexcept {
        auto &state = views[slot].queues;
        auto *data = rows(pending.get(), slot);
        auto &item = data[index];
        const auto current = views[slot].desired->currentProjectionMask(item.projections);
        const auto removed = item.pendingMask & ~current;
        const auto removedWeight = static_cast<std::size_t>(std::popcount(removed));

        if (removedWeight) {
            state.pendingWeight -= removedWeight;
            core::detail::updateDelivery(views[slot].statistics, [&](auto &s) {
                s.pendingEvents -= removedWeight;
                s.discardedEvents.add(removedWeight);
            });
        }

        item.projections.mask &= current;
        item.pendingMask &= current;

        if (item.dirty && item.pendingMask == 0) {
            --state.pending;
            item.dirty = false;
            count(counts.withdrawn, 1, counts.saturated);
            unlink(state, data, index, INITIAL);
            unlink(state, data, index, UPDATE);
        }

        if (!current && !views[slot].desired->isCurrent(item.membership)) {
            unlink(state, data, index, INITIAL);
            unlink(state, data, index, UPDATE);
            item = {};
        }

        state.offered = NONE;
    }

    /** Initialize or dirty a current membership in active/candidate fixed metadata.
     * @param state View queue state.
     * @param data Its fixed rows.
     * @param index Resident cache slot.
     * @param entry Current desired entry.
     * @param revision Current desired revision.
     */
    void dirty(Queues &state, Pending *data, std::size_t index, const core::SubscriptionEntry &entry,
               std::uint64_t revision) noexcept {
        auto &item = data[index];
        const auto next = core::captureDeliveryProjections(entry);
        const auto oldWeight = static_cast<std::size_t>(std::popcount(item.pendingMask));
        state.admissions += next.count();
        state.conflations += oldWeight;
        state.pendingWeight -= oldWeight;
        state.pendingWeight += next.count();

        if (!item.dirty) {
            ++state.pending;
            item.dirty = true;
        }

        item.membership = entry.membership;
        item.projections = next;
        item.pendingMask = next.mask;
        item.revision = revision;

        if (item.first) {
            push(state, data, index, INITIAL);
        }

        push(state, data, index, UPDATE);
        state.offered = NONE;
    }

    /** Preflight all maintenance and possible shared-owner disposal before core publication.
     * @param work Collector credits.
     * @param requests Input cardinality for stable order planning.
     */
    void maintenance(TickerWork &work, std::size_t requests = 0) const {
        // Unlink/retirement can revisit each queue; membership validation, ordered initial
        // lookup and last-interest eviction each scan desired entries independently.
        constexpr std::size_t QUEUE_PASSES = 8;
        constexpr std::size_t MEMBERSHIP_PASSES = 4 * (core::MAX_DELIVERY_PROJECTIONS + 1);
        auto visits = product(sum(limits.cacheKeys, 4), sum(limits.cacheKeys, 1));
        visits = product(product(visits, sum(limits.viewSlots, 1)), QUEUE_PASSES);
        visits =
            sum(visits, product(product(product(limits.cacheKeys, limits.viewSlots), limits.viewSubscriptions.entries),
                                MEMBERSHIP_PASSES));
        // Every request scans cache keys and may also scan the bounded deduplication list.
        visits = sum(visits, product(2, product(requests, sum(limits.cacheKeys, 1))));
        visits = sum(visits, product(4, limits.cachedFrames));
        spend(work, 0, visits);
        std::size_t text = 0;

        for (std::size_t i = 0; i < limits.cacheKeys; ++i) {
            if (entries[i].record) {
                text = sum(text,
                           product(sum(entries[i].record->symbol.size(), limits.viewSubscriptions.symbolUnits),
                                   sum(product(sum(limits.viewSlots, 1), limits.viewSubscriptions.entries), requests)));
            }
        }

        std::size_t releaseBytes = 0;

        for (std::size_t f = 0; f < limits.cachedFrames; ++f) {
            if (frames[f].event) {
                releaseBytes = sum(releaseBytes, frames[f].event->releaseBytes());
            }
        }

        spend(work, 0, 0, text, releaseBytes);
    }

    /** Collect unique cached indices in request order while request text is still valid.
     * @param requests Borrowed desired inputs, already covered by maintenance credits.
     * @return Number of written scratch indices.
     */
    std::size_t planInitialOrder(std::span<const core::SubscriptionRequest> requests) noexcept {
        std::size_t ordered = 0;

        for (const auto &request : requests) {
            if (!request.key.symbol.data() || request.key.symbol.size() > limits.viewSubscriptions.symbolUnits) {
                continue;
            }

            for (std::size_t i = 0; i < limits.cacheKeys; ++i) {
                if (!entries[i].record || !request.key.context.matches(entries[i].record->context)) {
                    continue;
                }

                if (request.key.symbol == entries[i].record->symbol &&
                    std::find(order.get(), order.get() + ordered, i) == order.get() + ordered) {
                    order[ordered++] = i;
                }
            }
        }

        return ordered;
    }

    /** Release event owners no active cache key references; full cost was preflighted. */
    void collectFrames() noexcept {
        for (std::size_t f = 0; f < limits.cachedFrames; ++f) {
            references[f] = 0;
        }

        for (std::size_t i = 0; i < limits.cacheKeys; ++i) {
            if (entries[i].record) {
                ++references[entries[i].frame];
            }
        }

        for (std::size_t f = 0; f < limits.cachedFrames; ++f) {
            if (frames[f].event && !references[f]) {
                bytes -= frames[f].bytes;
                --frameCount;
                frames[f].event.reset();
                frames[f].bytes = 0;
            }
        }
    }

    /** Evict only keys no live view retains in false mode; preflighted before publication. */
    void evict() noexcept {
        if (options.storeEverything) {
            return;
        }

        for (std::size_t i = 0; i < limits.cacheKeys; ++i) {
            if (!entries[i].record) {
                continue;
            }

            bool interested = false;

            for (std::size_t v = 0; v < limits.viewSlots; ++v) {
                if (match(v, *entries[i].record)) {
                    interested = true;
                    break;
                }
            }

            if (!interested) {
                for (std::size_t v = 0; v < limits.viewSlots; ++v) {
                    if (views[v].desired) {
                        withdraw(v, i);
                    }
                }

                entries[i] = {};
                --keys;
                count(counts.evicted, 1, counts.saturated);
            }
        }

        collectFrames();
    }

    /** Reconcile one effective core mutation using only stable cached-key indices.
     * @param slot Changed view.
     * @param ordered Count of preplanned input-order initial keys.
     */
    void reconcile(std::size_t slot, std::size_t ordered) noexcept {
        invalidate();
        auto *data = rows(pending.get(), slot);

        for (std::size_t i = 0; i < limits.cacheKeys; ++i) {
            pruneProjections(slot, i);
        }

        if (state != TickerState::STALE && state != TickerState::RESET_PENDING) {
            for (std::size_t n = 0; n < ordered; ++n) {
                const auto i = order[n];
                const auto *entry = match(slot, *entries[i].record);

                if (entry && (!views[slot].desired->isCurrent(data[i].membership) ||
                              (entry->projectionMask & ~data[i].projections.mask))) {
                    data[i].first = true;
                    dirty(views[slot].queues, data, i, *entry, views[slot].desired->revision());
                }
            }
        }

        publishAdmissions(slot, views[slot].queues);
        evict();
    }

    /** Publish one committed view mutation; scratch counters never escape rejected staging.
     * @param slot Actual admitted view.
     * @param queues Committed transaction-local admission/conflation amounts.
     */
    void publishAdmissions(std::size_t slot, Queues &queues) noexcept {
        core::detail::updateDelivery(views[slot].statistics, [&](auto &s) {
            s.admittedEvents.add(queues.admissions);
            s.conflatedEvents.add(queues.conflations);
            s.pendingEvents -= queues.conflations;
            s.pendingEvents += queues.admissions;
        });
        queues.admissions = queues.conflations = 0;
    }

    /** Charge exactly classified effective incoming view occurrences from a rejected staging transaction. */
    void rejectAdmissions() noexcept {
        for (std::size_t v = 0; v < limits.viewSlots; ++v) {
            core::detail::updateDelivery(views[v].statistics, [&](auto &s) {
                s.rejectedOverflowEvents.add(queues[v].admissions);
                s.droppedEvents.add(queues[v].admissions);
            });
        }
    }

    /** Mark shared freshness failure without consuming the rejected input.
     * @param incoming Complete rejected record count.
     */
    void stale(std::size_t incoming) noexcept {
        invalidate();
        state = TickerState::STALE;

        if (!diagnostic.error) {
            diagnostic = {TickerError::RESOURCE_LIMIT, incoming, keys};
        }
    }
};

TickerCollector::TickerCollector(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

TickerCollector::TickerCollector(TickerCollector &&other) noexcept : impl_(std::move(other.impl_)) {
    if (impl_) {
        impl_->invalidate();
    }
}

TickerCollector &TickerCollector::operator=(TickerCollector &&other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);

        if (impl_) {
            impl_->invalidate();
        }
    }

    return *this;
}

TickerCollector::~TickerCollector() = default;

bool TickerCollector::canMutate() const noexcept {
    return impl_ && !impl_->origin->replacementReserved;
}

std::expected<TickerCollector, TickerError> TickerCollector::create(core::LocalSchema &&schema, TickerLimits limits,
                                                                    TickerOptions options, TickerWork &work) {
    try {
        spend(work, 1);

        if (options.selectorPolicy != core::SubscriptionSelectorPolicy::EXACT_ONLY &&
            options.selectorPolicy != core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1) {
            return std::unexpected(TickerError::UNSUPPORTED_SELECTOR);
        }

        if (!options.retrievalQuantum || limits.viewSlots == NONE) {
            return std::unexpected(TickerError::INVALID_CONFIGURATION);
        }

        if (options.versionBindings.size() > schema.records().size()) {
            return std::unexpected(TickerError::INVALID_CONFIGURATION);
        }

        spend(work, 0,
              sum(options.versionBindings.size(),
                  product(options.versionBindings.size(), options.versionBindings.size())));

        for (std::size_t i = 0; i < options.versionBindings.size(); ++i) {
            const auto &binding = options.versionBindings[i];
            const auto id = binding.localRecordId;

            if (!schema.recordContext(id).matches(binding.context) ||
                binding.fieldIndex >= schema.records()[static_cast<std::size_t>(id)].fields.size() ||
                !versionType(static_cast<core::SerialFieldType>(
                    schema.records()[static_cast<std::size_t>(id)].fields[binding.fieldIndex].serialTypeId))) {
                return std::unexpected(TickerError::INVALID_CONFIGURATION);
            }

            for (std::size_t j = 0; j < i; ++j) {
                if (options.versionBindings[j].context.matches(binding.context)) {
                    return std::unexpected(TickerError::INVALID_CONFIGURATION);
                }
            }
        }

        const auto failure = TickerError::INVALID_CONFIGURATION;
        const auto cells = product(limits.viewSlots, limits.cacheKeys, failure);
        auto fixed = sum(sizeof(Impl), sizeof(TickerIdentity), failure);
        fixed = sum(fixed, product(limits.cacheKeys, 2 * sizeof(Impl::Entry) + sizeof(std::size_t), failure), failure);
        fixed = sum(fixed, product(limits.cachedFrames, sizeof(Impl::Frame) + sizeof(std::size_t), failure), failure);
        fixed = sum(fixed, product(limits.viewSlots, sizeof(Impl::View) + sizeof(Impl::Queues), failure), failure);
        fixed = sum(fixed, product(cells, 2 * sizeof(Impl::Pending), failure), failure);
        fixed = sum(fixed, schema.storageBytes() - sizeof(core::LocalSchema), failure);
        const auto versionSlots = options.versionBindings.empty() ? 0 : schema.records().size();
        fixed = sum(fixed, product(versionSlots, sizeof(std::size_t), failure), failure);
        std::size_t defaultFieldCount = 0;
        std::size_t defaultLegacyCount = 0;

        for (const auto &binding : options.versionBindings) {
            const auto fields = schema.records()[static_cast<std::size_t>(binding.localRecordId)].fields.size();
            defaultFieldCount = sum(defaultFieldCount, fields, failure);
            defaultLegacyCount = std::max(defaultLegacyCount, fields);
        }

        const auto defaultRecordCount = versionSlots ? limits.cacheKeys : 0;
        fixed = sum(fixed, product(defaultFieldCount, sizeof(protocol::LiveFieldValueView), failure), failure);
        fixed =
            sum(fixed, product(versionSlots, sizeof(std::span<const protocol::LiveFieldValueView>), failure), failure);
        fixed = sum(fixed, product(defaultLegacyCount, sizeof(std::int32_t), failure), failure);
        fixed = sum(fixed, product(defaultRecordCount, sizeof(protocol::LiveRecordView), failure), failure);

        if (fixed > limits.residentBytes) {
            return std::unexpected(TickerError::RESOURCE_LIMIT);
        }

        // Entry/candidate/order, frame/reference, view/queue, and both pending arrays
        // are separate initialized logical elements, including the zero-view frame axis.
        auto initialization = sum(2, product(3, limits.cacheKeys));
        initialization = sum(initialization, product(2, limits.cachedFrames));
        initialization = sum(initialization, product(2, limits.viewSlots));
        initialization = sum(initialization, product(2, cells));
        initialization = sum(initialization, versionSlots);
        initialization =
            sum(initialization, sum(defaultFieldCount, sum(versionSlots, sum(defaultLegacyCount, defaultRecordCount))));
        spend(work, 0, initialization, 0, fixed);
        // Allocate every fallible fixed owner before the schema's final move.
        auto entries = std::make_unique<Impl::Entry[]>(limits.cacheKeys);
        auto candidate = std::make_unique<Impl::Entry[]>(limits.cacheKeys);
        auto frames = std::make_unique<Impl::Frame[]>(limits.cachedFrames);
        auto views = std::make_unique<Impl::View[]>(limits.viewSlots);
        auto pending = std::make_unique<Impl::Pending[]>(cells);
        auto planned = std::make_unique<Impl::Pending[]>(cells);
        auto queues = std::make_unique<Impl::Queues[]>(limits.viewSlots);
        auto references = std::make_unique<std::size_t[]>(limits.cachedFrames);
        auto order = std::make_unique<std::size_t[]>(limits.cacheKeys);
        auto origin = std::make_unique<TickerIdentity>();
        std::unique_ptr<std::size_t[]> versionFields;
        std::unique_ptr<protocol::LiveFieldValueView[]> defaultFields;
        std::unique_ptr<std::span<const protocol::LiveFieldValueView>[]> defaultShapes;
        std::unique_ptr<std::int32_t[]> defaultLegacy;
        std::unique_ptr<protocol::LiveRecordView[]> defaultRecords;

        if (versionSlots) {
            versionFields = std::make_unique<std::size_t[]>(versionSlots);
            std::fill_n(versionFields.get(), versionSlots, NONE);
            defaultFields = std::make_unique<protocol::LiveFieldValueView[]>(defaultFieldCount);
            defaultShapes = std::make_unique<std::span<const protocol::LiveFieldValueView>[]>(versionSlots);
            defaultLegacy = std::make_unique<std::int32_t[]>(defaultLegacyCount);
            defaultRecords = std::make_unique<protocol::LiveRecordView[]>(defaultRecordCount);
        }

        std::size_t defaultOffset = 0;

        for (const auto &binding : options.versionBindings) {
            const auto id = static_cast<std::size_t>(binding.localRecordId);
            versionFields[id] = binding.fieldIndex;
            const auto &fields = schema.records()[id].fields;
            defaultShapes[id] = {defaultFields.get() + defaultOffset, fields.size()};

            for (const auto &field : fields) {
                defaultFields[defaultOffset++] = {static_cast<core::SerialFieldType>(field.serialTypeId), 0, {}, {}};
            }
        }

        options.versionBindings = {};
        // Impl allocation is also fallible: allocate raw storage before consuming schema.
        auto *storage = ::operator new(sizeof(Impl));
        std::unique_ptr<Impl> impl(new (storage) Impl(std::move(schema), limits, options, fixed));
        impl->entries = std::move(entries);
        impl->candidate = std::move(candidate);
        impl->frames = std::move(frames);
        impl->views = std::move(views);
        impl->pending = std::move(pending);
        impl->planned = std::move(planned);
        impl->queues = std::move(queues);
        impl->references = std::move(references);
        impl->order = std::move(order);
        impl->origin = origin.release();
        impl->versionFields = std::move(versionFields);
        impl->defaultFields = std::move(defaultFields);
        impl->defaultShapes = std::move(defaultShapes);
        impl->defaultLegacy = std::move(defaultLegacy);
        impl->defaultRecords = std::move(defaultRecords);

        return TickerCollector(std::move(impl));
    } catch (TickerError error) {
        return std::unexpected(error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TickerError::ALLOCATION_FAILURE);
    }
}

std::expected<TickerViewToken, TickerError> TickerCollector::openView(TickerWork &work) {
    return openObservedView(work, {});
}

std::expected<TickerViewToken, TickerError>
TickerCollector::openObservedView(TickerWork &work, std::shared_ptr<core::detail::DeliveryStatisticsState> statistics) {
    try {
        spend(work, 1);

        if (!canMutate()) {
            return std::unexpected(TickerError::INVALID_STATE);
        }

        auto &s = *impl_;

        if (s.nextActivation == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(TickerError::IDENTITY_EXHAUSTED);
        }

        for (std::size_t v = 0; v < s.limits.viewSlots; ++v) {
            spend(work, 0, 1);

            if (s.views[v].desired) {
                continue;
            }

            spend(work, 0, sum(s.schema.records().size(), s.limits.cacheKeys));
            auto desired = core::SubscriptionState::create(s.schema, core::SubscriptionFamily::TICKER,
                                                           s.limits.viewSubscriptions, s.options.selectorPolicy);

            if (!desired) {
                return std::unexpected(desired.error() == core::SubscriptionError::ALLOCATION_FAILURE
                                           ? TickerError::ALLOCATION_FAILURE
                                           : TickerError::INVALID_CONFIGURATION);
            }

            s.views[v].desired.emplace(std::move(*desired));
            s.views[v].activation = ++s.nextActivation;
            s.views[v].queues = {};
            s.views[v].statistics = std::move(statistics);

            return TickerViewToken(s.origin, v, s.views[v].activation);
        }

        return std::unexpected(TickerError::RESOURCE_LIMIT);
    } catch (TickerError error) {
        return std::unexpected(error);
    }
}

std::expected<void, TickerError> TickerCollector::closeView(const TickerViewToken &view, TickerWork &work) {
    try {
        spend(work, 1);

        if (!canMutate()) {
            return std::unexpected(TickerError::INVALID_STATE);
        }

        auto &s = *impl_;
        const auto slot = s.view(view);

        if (!slot) {
            return std::unexpected(TickerError::INVALID_VIEW);
        }

        s.maintenance(work);
        spend(work, 0, sum(s.schema.records().size(), s.limits.viewSubscriptions.entries),
              s.limits.viewSubscriptions.totalSymbolUnits);
        s.invalidate();

        for (std::size_t i = 0; i < s.limits.cacheKeys; ++i) {
            s.withdraw(*slot, i);
        }

        s.views[*slot].desired.reset();
        s.views[*slot].queues = {};
        s.views[*slot].statistics.reset();
        s.evict();

        return {};
    } catch (TickerError error) {
        return std::unexpected(error);
    }
}

namespace {
/** Convert a collector precondition without inventing a pure-core cause.
 * @param error Collector failure.
 * @return Typed desired-command error.
 */
TickerCommandError commandError(TickerError error) {
    return {error, {}};
}
} // namespace

std::expected<core::SubscriptionChange, TickerCommandError>
TickerCollector::add(const TickerViewToken &view, std::span<const core::SubscriptionRequest> requests,
                     core::SubscriptionWork &subscriptionWork, TickerWork &work) {
    try {
        spend(work, 1);

        if (!canMutate()) {
            return std::unexpected(commandError(TickerError::INVALID_STATE));
        }

        auto &s = *impl_;
        const auto slot = s.view(view);

        if (!slot) {
            return std::unexpected(commandError(TickerError::INVALID_VIEW));
        }

        s.maintenance(work, requests.size());
        // Store indices before core publication: request text may borrow the old desired map.
        const auto ordered = s.planInitialOrder(requests);

        auto changed = s.views[*slot].desired->add(requests, subscriptionWork);

        if (!changed) {
            return std::unexpected(TickerCommandError{TickerError::INVALID_CONFIGURATION, changed.error()});
        }

        if (changed->changed) {
            s.reconcile(*slot, ordered);
        }

        return *changed;
    } catch (TickerError error) {
        return std::unexpected(commandError(error));
    }
}

std::expected<core::SubscriptionChange, TickerCommandError>
TickerCollector::replace(const TickerViewToken &view, std::span<const core::SubscriptionRequest> requests,
                         core::SubscriptionWork &subscriptionWork, TickerWork &work) {
    auto candidate = prepareReplace(view, requests, subscriptionWork, work);

    if (!candidate) {
        return std::unexpected(candidate.error());
    }

    return publishReplace(*candidate);
}

std::expected<TickerCollector::PreparedReplace, TickerCommandError>
TickerCollector::prepareReplace(const TickerViewToken &view, std::span<const core::SubscriptionRequest> requests,
                                core::SubscriptionWork &subscriptionWork, TickerWork &work) {
    try {
        spend(work, 1);

        if (!canMutate()) {
            return std::unexpected(commandError(TickerError::INVALID_STATE));
        }

        auto &s = *impl_;
        const auto slot = s.view(view);

        if (!slot) {
            return std::unexpected(commandError(TickerError::INVALID_VIEW));
        }

        s.maintenance(work, requests.size());
        const auto ordered = s.planInitialOrder(requests);
        auto desired =
            core::detail::SubscriptionStateAccess::prepareReplace(*s.views[*slot].desired, requests, subscriptionWork);

        if (!desired) {
            return std::unexpected(TickerCommandError{TickerError::INVALID_CONFIGURATION, desired.error()});
        }

        return PreparedReplace(view, std::move(*desired), ordered);
    } catch (TickerError error) {
        return std::unexpected(commandError(error));
    }
}

std::expected<core::SubscriptionChange, TickerCommandError>
TickerCollector::publishReplace(PreparedReplace &candidate) noexcept {
    if (!impl_ || impl_->origin != candidate.view_.origin_) {
        return std::unexpected(commandError(TickerError::INVALID_STATE));
    }

    auto &s = *impl_;
    const auto slot = s.view(candidate.view_);

    if (!slot) {
        return std::unexpected(commandError(TickerError::INVALID_VIEW));
    }

    auto changed =
        core::detail::SubscriptionStateAccess::commitReplace(*s.views[*slot].desired, std::move(*candidate.desired_));

    if (!changed) {
        return std::unexpected(TickerCommandError{TickerError::INVALID_CONFIGURATION, changed.error()});
    }

    if (changed->changed) {
        s.reconcile(*slot, candidate.ordered_);
    }

    return *changed;
}

std::expected<core::SubscriptionChange, TickerCommandError>
TickerCollector::remove(const TickerViewToken &view, std::span<const core::SubscriptionKey> keys,
                        core::SubscriptionWork &subscriptionWork, TickerWork &work) {
    try {
        spend(work, 1);

        if (!canMutate()) {
            return std::unexpected(commandError(TickerError::INVALID_STATE));
        }

        auto &s = *impl_;
        const auto slot = s.view(view);

        if (!slot) {
            return std::unexpected(commandError(TickerError::INVALID_VIEW));
        }

        s.maintenance(work);
        auto changed = s.views[*slot].desired->remove(keys, subscriptionWork);

        if (!changed) {
            return std::unexpected(TickerCommandError{TickerError::INVALID_CONFIGURATION, changed.error()});
        }

        if (changed->changed) {
            s.reconcile(*slot, 0);
        }

        return *changed;
    } catch (TickerError error) {
        return std::unexpected(commandError(error));
    }
}

std::expected<core::SubscriptionChange, TickerCommandError>
TickerCollector::clear(const TickerViewToken &view, core::SubscriptionWork &subscriptionWork, TickerWork &work) {
    try {
        spend(work, 1);

        if (!canMutate()) {
            return std::unexpected(commandError(TickerError::INVALID_STATE));
        }

        auto &s = *impl_;
        const auto slot = s.view(view);

        if (!slot) {
            return std::unexpected(commandError(TickerError::INVALID_VIEW));
        }

        s.maintenance(work);
        auto changed = s.views[*slot].desired->clear(subscriptionWork);

        if (!changed) {
            return std::unexpected(TickerCommandError{TickerError::INVALID_CONFIGURATION, changed.error()});
        }

        if (changed->changed) {
            s.reconcile(*slot, 0);
        }

        return *changed;
    } catch (TickerError error) {
        return std::unexpected(commandError(error));
    }
}

std::expected<TickerStage, TickerError> TickerCollector::stage(protocol::LiveEvent &&event, TickerWork &work) {
    try {
        spend(work, 1);

        if (!canMutate()) {
            return std::unexpected(TickerError::INVALID_STATE);
        }

        auto &s = *impl_;

        if (s.state == TickerState::STALE) {
            return std::unexpected(TickerError::STALE);
        }

        if (s.notice) {
            return std::unexpected(TickerError::RESET_PENDING);
        }

        if (event.kind() != protocol::LiveEventKind::DATA || event.messageType() != TICKER_DATA ||
            !event.residentBytes()) {
            return std::unexpected(TickerError::INVALID_EVENT);
        }

        auto identity = event.snapshot().identity();

        if (s.epoch && !s.epoch->sameEpoch(identity)) {
            return std::unexpected(TickerError::INVALID_EPOCH);
        }

        if (s.options.selectorPolicy != core::SubscriptionSelectorPolicy::EXACT_ONLY) {
            // Prepay the metadata traversal and maximum seven encoder units per actual record.
            const auto records = event.records().size();
            spend(work, 0, sum(records, 1), product(records, 7));

            for (const auto &record : event.records()) {
                if (!encoding::isCanonicalPentaSymbol(record.symbol, record.cipher)) {
                    return std::unexpected(TickerError::UNSUPPORTED_SYMBOL_DOMAIN);
                }
            }
        }

        // This guard clears every borrowed candidate pointer even if a later read-only pass fails.
        struct Guard {
            Impl &state;

            ~Guard() {
                state.clearPlan();
            }
        };

        const auto cells = product(s.limits.viewSlots, s.limits.cacheKeys);
        spend(work, 0,
              sum(product(4, cells), sum(sum(product(8, s.limits.cacheKeys), product(6, s.limits.cachedFrames)),
                                         product(6, s.limits.viewSlots))));

        if (s.versionFields) {
            spend(work, 0, s.limits.cacheKeys);
        }

        Guard guard{s};

        for (std::size_t i = 0; i < s.limits.cacheKeys; ++i) {
            s.candidate[i] = s.entries[i];
        }

        for (std::size_t i = 0; i < cells; ++i) {
            s.planned[i] = s.pending[i];
        }

        for (std::size_t v = 0; v < s.limits.viewSlots; ++v) {
            s.queues[v] = s.views[v].queues;
            s.queues[v].admissions = s.queues[v].conflations = 0;
        }

        TickerStage result{0, 0, 0, false};
        auto keyCount = s.keys;
        bool overflow = false;

        for (const auto &record : event.records()) {
            spend(work, 0, sum(1, product(6, s.limits.viewSlots)));

            if (record.localId < 0 || !s.schema.recordContext(record.localId).matches(record.context)) {
                return std::unexpected(TickerError::INVALID_CONTEXT);
            }

            bool interested = s.options.storeEverything;

            for (std::size_t v = 0; v < s.limits.viewSlots && !interested; ++v) {
                interested = s.match(v, record, &work) != nullptr;
            }

            if (!interested) {
                ++result.ignored;
                continue;
            }

            auto index = s.find(s.candidate.get(), record, work);
            bool materializeDefault = false;

            if (s.versionFields) {
                const auto field = s.versionFields[static_cast<std::size_t>(record.localId)];

                if (field != NONE) {
                    spend(work, 0, 2);
                    const auto prior = index == NONE || s.candidate[index].defaultValue
                                           ? 0
                                           : physicalVersion(s.candidate[index].record->typedValues[field]);

                    if (physicalVersion(record.typedValues[field]) < prior && index != NONE) {
                        ++result.ignored;
                        continue;
                    }

                    materializeDefault = index == NONE && physicalVersion(record.typedValues[field]) < 0;
                }
            }

            if (index != NONE && equalData(s.candidate[index].defaultValue ? s.defaultRecord(*s.candidate[index].record)
                                                                           : *s.candidate[index].record,
                                           record, work)) {
                ++result.equal;
                continue;
            }

            if (index == NONE) {
                for (std::size_t i = 0; i < s.limits.cacheKeys; ++i) {
                    spend(work, 0, 1);

                    if (!s.candidate[i].record) {
                        index = i;
                        break;
                    }
                }

                if (index == NONE) {
                    overflow = true;

                    for (std::size_t v = 0; v < s.limits.viewSlots; ++v) {
                        if (s.match(v, record, &work))
                            ++s.queues[v].admissions;
                    }

                    continue;
                }

                ++keyCount;
            }

            s.candidate[index] = {&record, NONE, identity.generation(), materializeDefault};
            ++result.changed;

            for (std::size_t v = 0; v < s.limits.viewSlots; ++v) {
                const auto *entry = s.match(v, record, &work);

                if (entry) {
                    s.dirty(s.queues[v], s.rows(s.planned.get(), v), index, *entry, s.views[v].desired->revision());
                }
            }
        }

        if (overflow) {
            s.rejectAdmissions();
            s.stale(event.records().size());

            return std::unexpected(TickerError::RESOURCE_LIMIT);
        }

        bool incoming = false;

        for (std::size_t f = 0; f < s.limits.cachedFrames; ++f) {
            s.references[f] = 0;
        }

        for (std::size_t i = 0; i < s.limits.cacheKeys; ++i) {
            if (!s.candidate[i].record) {
                continue;
            }

            if (s.candidate[i].frame == NONE) {
                incoming = true;
            } else {
                ++s.references[s.candidate[i].frame];
            }
        }

        const auto incomingBytes = incoming ? event.residentBytes() : 0;

        if (incoming &&
            (s.frameCount == s.limits.cachedFrames ||
             incomingBytes > s.limits.cacheBytes - std::min(s.bytes, s.limits.cacheBytes) ||
             incomingBytes > s.limits.residentBytes - std::min(sum(s.fixed, s.bytes), s.limits.residentBytes))) {
            s.rejectAdmissions();
            s.stale(event.records().size());

            return std::unexpected(TickerError::RESOURCE_LIMIT);
        }

        std::size_t releaseBytes = 0;

        for (std::size_t f = 0; f < s.limits.cachedFrames; ++f) {
            if (s.frames[f].event && !s.references[f]) {
                releaseBytes = sum(releaseBytes, s.frames[f].event->releaseBytes());
            }
        }

        spend(work, 0, 0, 0, releaseBytes);

        if (result.changed) {
            s.invalidate();
        }

        if (!s.epoch) {
            s.epoch.emplace(std::move(identity));
        }

        s.state = TickerState::CURRENT;

        if (incoming) {
            std::size_t free = 0;

            while (s.frames[free].event) {
                ++free;
            }

            s.frames[free].event.emplace(std::move(event));
            s.frames[free].bytes = incomingBytes;
            ++s.frameCount;
            s.bytes += incomingBytes;
            result.retained = true;

            for (std::size_t i = 0; i < s.limits.cacheKeys; ++i) {
                if (s.candidate[i].record && s.candidate[i].frame == NONE) {
                    s.candidate[i].frame = free;
                }
            }
        }

        if (result.changed) {
            for (std::size_t i = 0; i < s.limits.cacheKeys; ++i) {
                s.entries[i] = s.candidate[i];

                if (s.entries[i].record && s.entries[i].defaultValue) {
                    s.defaultRecords[i] = s.defaultRecord(*s.entries[i].record);
                }
            }

            for (std::size_t i = 0; i < cells; ++i) {
                s.pending[i] = s.planned[i];
            }

            for (std::size_t v = 0; v < s.limits.viewSlots; ++v) {
                s.views[v].queues = s.queues[v];
                s.views[v].queues.offered = NONE;
                s.publishAdmissions(v, s.views[v].queues);
            }

            s.keys = keyCount;
            s.collectFrames();
        }

        count(s.counts.changed, result.changed, s.counts.saturated);
        count(s.counts.equal, result.equal, s.counts.saturated);
        count(s.counts.ignored, result.ignored, s.counts.saturated);

        return result;
    } catch (TickerError error) {
        return std::unexpected(error);
    }
}

std::expected<std::optional<TickerDelivery>, TickerError> TickerCollector::front(const TickerViewToken &view,
                                                                                 TickerWork &work) {
    return frontWithPolicy(view, work, TickerOfferPolicy::NORMAL);
}

std::expected<std::optional<TickerDelivery>, TickerError>
TickerCollector::frontWithPolicy(const TickerViewToken &view, TickerWork &work, TickerOfferPolicy policy) {
    try {
        spend(work, 1, 1);

        if (!canMutate()) {
            return std::unexpected(TickerError::INVALID_STATE);
        }

        auto &s = *impl_;
        const auto slot = s.view(view);

        if (!slot) {
            return std::unexpected(TickerError::INVALID_VIEW);
        }

        if (s.state == TickerState::STALE) {
            return std::unexpected(TickerError::STALE);
        }

        if (s.notice) {
            return std::unexpected(TickerError::RESET_PENDING);
        }

        if (policy != TickerOfferPolicy::NORMAL && policy != TickerOfferPolicy::INITIAL_ONLY)
            return std::unexpected(TickerError::INVALID_CONFIGURATION);

        auto &q = s.views[*slot].queues;

        if (!q.pending) {
            q.offered = NONE;

            return std::optional<TickerDelivery>{};
        }

        spend(work, 0, sum(product(2, s.limits.cacheKeys), 2));
        const auto *data = s.rows(s.pending.get(), *slot);
        auto selected =
            policy == TickerOfferPolicy::NORMAL ? s.prepareOffer(q, data) : Impl::Selection{NONE, INITIAL, q.credit};

        if (policy == TickerOfferPolicy::INITIAL_ONLY) {
            auto index = q.head[INITIAL];

            while (index != NONE && (!data[index].dirty || !data[index].first))
                index = data[index].next[INITIAL];

            selected = {index, INITIAL, q.credit};

            if (index == NONE) {
                q.offered = NONE;

                return std::optional<TickerDelivery>{};
            }
        }

        if (selected.index == NONE) {
            std::terminate();
        }

        q.offeredQueue = selected.kind;
        q.offered = selected.index;
        q.nextCredit = selected.nextCredit;
        q.initialOnlyOffer = policy == TickerOfferPolicy::INITIAL_ONLY;
        const auto &item = s.rows(s.pending.get(), *slot)[q.offered];
        const auto &entry = s.entries[q.offered];
        auto projections = item.projections;
        projections.mask = item.pendingMask;

        return std::optional<TickerDelivery>{{{s.valueRecord(q.offered), TickerEpochToken(s.origin, NONE, s.generation),
                                               entry.generation, true, entry.record, entry.defaultValue},
                                              item.membership,
                                              item.revision,
                                              item.first ? TickerDeliveryKind::INITIAL : TickerDeliveryKind::LIVE,
                                              0,
                                              std::move(projections)}};
    } catch (TickerError error) {
        return std::unexpected(error);
    }
}

std::expected<void, TickerError> TickerCollector::pop(const TickerViewToken &view, TickerWork &work) {
    try {
        spend(work, 1);

        if (!canMutate()) {
            return std::unexpected(TickerError::INVALID_STATE);
        }

        auto &s = *impl_;
        const auto slot = s.view(view);

        if (!slot) {
            return std::unexpected(TickerError::INVALID_VIEW);
        }

        auto &q = s.views[*slot].queues;

        if (q.offered == NONE) {
            return std::unexpected(TickerError::NO_OFFER);
        }

        spend(work, 0, sum(product(4, s.limits.cacheKeys), sum(4, s.limits.viewSlots)));
        auto *data = s.rows(s.pending.get(), *slot);
        const auto index = q.offered;
        auto selected = INITIAL;

        if (q.initialOnlyOffer) {
            while (q.head[INITIAL] != NONE && (!data[q.head[INITIAL]].dirty || !data[q.head[INITIAL]].first))
                s.unlink(q, data, q.head[INITIAL], INITIAL);
        } else {
            selected = s.select(q, data);
        }

        if (selected != q.offeredQueue || q.head[selected] != index) {
            std::terminate();
        }

        s.unlink(q, data, index, q.offeredQueue);
        q.pendingWeight -= static_cast<std::size_t>(std::popcount(data[index].pendingMask));
        data[index].pendingMask = 0;
        data[index].dirty = false;
        data[index].first = false;
        --q.pending;
        q.credit = q.nextCredit;

        if (!q.initialOnlyOffer)
            s.finishRetrieval(q, data, q.offeredQueue);

        s.invalidate();
        count(s.counts.delivered, 1, s.counts.saturated);

        return {};
    } catch (TickerError error) {
        return std::unexpected(error);
    }
}

std::expected<std::optional<TickerValue>, TickerError> TickerCollector::latest(core::SubscriptionKey key,
                                                                               TickerWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            return std::unexpected(TickerError::INVALID_STATE);
        }

        auto &s = *impl_;
        bool valid = false;

        for (std::size_t r = 0; r < s.schema.records().size(); ++r) {
            spend(work, 0, 1);

            if (s.schema.recordContext(static_cast<std::int32_t>(r)).matches(key.context)) {
                valid = true;
                break;
            }
        }

        if (!valid) {
            return std::unexpected(TickerError::INVALID_CONTEXT);
        }

        if (!key.symbol.data()) {
            return std::unexpected(TickerError::INVALID_CONFIGURATION);
        }

        for (std::size_t i = 0; i < s.limits.cacheKeys; ++i) {
            spend(work, 0, 1);
            const auto &entry = s.entries[i];

            if (!entry.record || !entry.record->context.matches(key.context)) {
                continue;
            }

            spend(work, 0, 0, sum(entry.record->symbol.size(), key.symbol.size()));

            if (entry.record->symbol == key.symbol) {
                return std::optional<TickerValue>{{s.valueRecord(i), TickerEpochToken(s.origin, NONE, s.generation),
                                                   entry.generation, s.state == TickerState::CURRENT, entry.record,
                                                   entry.defaultValue}};
            }
        }

        return std::optional<TickerValue>{};
    } catch (TickerError error) {
        return std::unexpected(error);
    }
}

std::expected<TickerResetNotice, TickerError> TickerCollector::reset(TickerWork &work) {
    try {
        spend(work, 1);

        if (!canMutate()) {
            return std::unexpected(TickerError::INVALID_STATE);
        }

        auto &s = *impl_;

        if (s.generation == std::numeric_limits<std::uint64_t>::max() ||
            s.resetSequence == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(TickerError::IDENTITY_EXHAUSTED);
        }

        s.maintenance(work);
        std::size_t pending = 0;

        for (std::size_t v = 0; v < s.limits.viewSlots; ++v) {
            pending = sum(pending, s.views[v].queues.pending);
        }

        auto notice = s.notice.value_or(TickerResetNotice{
            0, TickerEpochToken(s.origin, NONE, s.generation), {}, 0, 0, 0, s.diagnostic.error, true, false});

        if (s.notice) {
            count(notice.skipped, 1, notice.saturated);
        }

        count(notice.discarded, pending, notice.saturated);
        count(notice.evicted, s.keys, notice.saturated);
        notice.sequence = ++s.resetSequence;
        notice.nextEpoch = TickerEpochToken(s.origin, NONE, ++s.generation);

        if (!notice.previousCause) {
            notice.previousCause = s.diagnostic.error;
        }

        s.invalidate();

        for (std::size_t i = 0; i < s.limits.cacheKeys; ++i) {
            s.entries[i] = {};
        }

        for (std::size_t i = 0; i < s.limits.viewSlots * s.limits.cacheKeys; ++i) {
            s.pending[i] = {};
        }

        for (std::size_t v = 0; v < s.limits.viewSlots; ++v) {
            core::detail::updateDelivery(s.views[v].statistics, [&](auto &statistics) {
                statistics.pendingEvents -= s.views[v].queues.pendingWeight;
                statistics.discardedEvents.add(s.views[v].queues.pendingWeight);
            });
            s.views[v].queues = {};
        }

        count(s.counts.discarded, pending, s.counts.saturated);
        count(s.counts.evicted, s.keys, s.counts.saturated);
        count(s.counts.resets, 1, s.counts.saturated);
        s.keys = 0;
        s.collectFrames();
        s.epoch.reset();
        s.diagnostic = {};
        s.notice = notice;
        s.state = TickerState::RESET_PENDING;

        return notice;
    } catch (TickerError error) {
        return std::unexpected(error);
    }
}

std::expected<void, TickerError> TickerCollector::acknowledgeReset(std::uint64_t sequence, TickerWork &work) {
    try {
        spend(work, 1, 1);

        if (!canMutate()) {
            return std::unexpected(TickerError::INVALID_STATE);
        }

        if (!impl_->notice || impl_->notice->sequence != sequence) {
            return std::unexpected(TickerError::RESET_PENDING);
        }

        impl_->notice.reset();
        impl_->state = TickerState::EMPTY;

        return {};
    } catch (TickerError error) {
        return std::unexpected(error);
    }
}

const core::LocalSchema *TickerCollector::schema() const noexcept {
    return impl_ ? &impl_->schema : nullptr;
}

const core::SubscriptionState *TickerCollector::desired(const TickerViewToken &view) const noexcept {
    if (!impl_) {
        return nullptr;
    }

    const auto slot = impl_->view(view);

    return slot ? &*impl_->views[*slot].desired : nullptr;
}

std::optional<std::size_t> TickerCollector::pendingKeys(const TickerViewToken &view) const noexcept {
    if (!impl_) {
        return {};
    }

    const auto slot = impl_->view(view);

    return slot ? std::optional(impl_->views[*slot].queues.pending) : std::nullopt;
}

std::size_t TickerCollector::cachedKeys() const noexcept {
    return impl_ ? impl_->keys : 0;
}

std::size_t TickerCollector::cachedFrames() const noexcept {
    return impl_ ? impl_->frameCount : 0;
}

std::size_t TickerCollector::cacheBytes() const noexcept {
    return impl_ ? impl_->bytes : 0;
}

std::size_t TickerCollector::residentBytes() const noexcept {
    return impl_ ? impl_->fixed + impl_->bytes : 0;
}

TickerState TickerCollector::state() const noexcept {
    return impl_ ? impl_->state : TickerState::EMPTY;
}

TickerCounts TickerCollector::counts() const noexcept {
    return impl_ ? impl_->counts : TickerCounts{};
}

TickerDiagnostic TickerCollector::diagnostic() const noexcept {
    return impl_ ? impl_->diagnostic : TickerDiagnostic{};
}

std::optional<TickerResetNotice> TickerCollector::resetNotice() const noexcept {
    return impl_ ? impl_->notice : std::nullopt;
}

} // namespace org::ttldtor::qdcxx::collectors
