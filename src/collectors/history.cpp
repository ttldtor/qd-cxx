// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/collectors/history.hpp"
#include "qd/encoding/penta_symbol.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::collectors {

/** Fixed detached identity; producer-safe final release cannot reach protocol state. */
struct HistoryIdentity {
    std::atomic<std::size_t> references{
        1}; ///< Collector plus detached tokens; no reference is reused after retirement.
};

namespace {
constexpr auto NONE =
    std::numeric_limits<std::size_t>::max(); ///< Absent queue/cache index and epoch-token discriminator.
constexpr std::int32_t HISTORY_DATA = 20;    ///< Whole admitted source family.
constexpr std::int32_t TX = 1;               ///< Explicit pending transaction flag.
constexpr std::int32_t REMOVE = 2;           ///< Remove indexed point.
constexpr std::int32_t BEGIN = 4;            ///< Begin descending source snapshot.
constexpr std::int32_t END = 8;              ///< Projected completed requested boundary.
constexpr std::int32_t SNIP = 16;            ///< Truncated source snapshot boundary.
constexpr std::int32_t MODE = 64;            ///< Explicit source snapshot-mode declaration.
constexpr std::size_t TIME_WORD_BITS = 32;   ///< Width of each signed32 raw-index contributor.
constexpr auto HIGH = std::numeric_limits<std::int64_t>::max(); ///< Reserved initial/virtual cursor.
constexpr auto LOW = std::numeric_limits<std::int64_t>::min();  ///< Raw signed64 lower extreme.

/** Retain fixed identity without allocation; representability exhaustion cannot wrap.
 * @param identity Fixed marker, or null.
 */
void retain(HistoryIdentity *identity) noexcept {
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
void release(HistoryIdentity *identity) noexcept {
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
std::size_t sum(std::size_t left, std::size_t right, HistoryError error = HistoryError::WORK_LIMIT) {
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
std::size_t product(std::size_t left, std::size_t right, HistoryError error = HistoryError::WORK_LIMIT) {
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
void spend(HistoryWork &work, std::size_t operations, std::size_t visits = 0, std::size_t text = 0,
           std::size_t bytes = 0) {
    if (operations > work.operations || visits > work.visits || text > work.textUnits || bytes > work.bytes) {
        throw HistoryError::WORK_LIMIT;
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
bool equalText(std::optional<std::u16string_view> left, std::optional<std::u16string_view> right, HistoryWork &work) {
    spend(work, 0, 1, sum(left ? left->size() : 0, right ? right->size() : 0));

    return left == right;
}

/** Compare selected default-field data, never flags/cipher/binding/event-time metadata.
 * @param left Prior candidate value.
 * @param right Incoming admitted value.
 * @param work Complete field/text/opaque-byte credits.
 * @return Exact raw/default wire-value equality in the supported native catalog.
 */
bool equalData(const protocol::LiveRecordView &left, const protocol::LiveRecordView &right, HistoryWork &work) {
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
} // namespace

HistoryToken::HistoryToken(HistoryIdentity *origin, std::size_t slot, std::uint64_t generation) noexcept
    : origin_(origin), slot_(slot), generation_(generation) {
    retain(origin_);
}

HistoryToken::HistoryToken(const HistoryToken &other) noexcept
    : HistoryToken(other.origin_, other.slot_, other.generation_) {
}

HistoryToken::HistoryToken(HistoryToken &&other) noexcept
    : origin_(std::exchange(other.origin_, nullptr)), slot_(other.slot_), generation_(other.generation_) {
}

HistoryToken &HistoryToken::operator=(const HistoryToken &other) noexcept {
    if (this != &other) {
        HistoryToken candidate(other);
        std::swap(origin_, candidate.origin_);
        slot_ = other.slot_;
        generation_ = other.generation_;
    }

    return *this;
}

HistoryToken &HistoryToken::operator=(HistoryToken &&other) noexcept {
    if (this != &other) {
        release(origin_);
        origin_ = std::exchange(other.origin_, nullptr);
        slot_ = other.slot_;
        generation_ = other.generation_;
    }

    return *this;
}

HistoryToken::~HistoryToken() {
    release(origin_);
}

bool HistoryToken::matches(const HistoryToken &other) const noexcept {
    return origin_ && origin_ == other.origin_ && slot_ == other.slot_ && generation_ == other.generation_;
}

bool HistoryToken::valid() const noexcept {
    return origin_ != nullptr;
}

/** Fixed collector state; candidate metadata owns no borrowed source owner. */

/** Fixed banks borrow only frame-table roots or the caller's still-owned candidate input. */
struct HistoryCollector::Impl {
    /** Retained raw value or explicitly synthesized default projection. */
    struct Value {
        std::size_t frame = NONE;                     ///< Actual owner slot, INCOMING sentinel, or absent for defaults.
        std::optional<protocol::LiveRecordView> raw;  ///< Real admitted value only.
        std::optional<HistorySourceMetadata> trigger; ///< Real contributing scalar metadata.
        std::int64_t time = 0;                        ///< Produced full signed64 index.
        bool synthetic = false;                       ///< Ordinary fields must be exact typed defaults.
    };

    /** One exact record/symbol and generic snapshot protocol state. */
    struct Partition {
        bool used = false;                            ///< Slot is occupied, including empty retained gating state.
        std::int32_t record = 0;                      ///< Local record ID, validated against immutable schema.
        std::size_t symbolLength = 0;                 ///< Exact owned UTF16 length.
        std::uint64_t activation = 0;                 ///< Never reused across forget/reset/recreation.
        bool mode = false;                            ///< Source declared snapshot protocol.
        bool began = false;                           ///< Fresh source BEGIN is currently recognized.
        bool ended = false;                           ///< Source range reached total bound or SNIP.
        bool explicitTx = false;                      ///< Source TX_PENDING transaction.
        bool sweepTx = false;                         ///< Implicit snapshot update transaction.
        std::int64_t frontier = HIGH;                 ///< Current descending snapshot frontier.
        std::int64_t ever = HIGH;                     ///< Lowest retained known source frontier.
        std::int64_t snip = LOW;                      ///< Explicit snip frontier; LOW means absent.
        std::optional<HistorySourceMetadata> trigger; ///< Last real partition control provenance.
    };

    /** Shared value; each raw index occurs once per partition. */
    struct Point {
        bool used = false;            ///< Value slot is occupied.
        std::size_t partition = NONE; ///< Owning partition slot.
        Value value;                  ///< Immutable retained source value.
    };

    /** Local materialized update; FIFO ordering is independent of raw-time ordering. */
    struct Pending {
        std::size_t partition = NONE; ///< Exact cache partition.
        Value value;                  ///< Actual update or defaulted removal/boundary.
        std::int32_t flags = 0;       ///< Produced flags, independently mutable from raw source.
        HistoryDeliveryKind kind = HistoryDeliveryKind::UPDATE; ///< Local projection origin.
        core::SubscriptionMembership membership;                ///< Exact admission activation.
        std::uint64_t revision = 0;                             ///< Desired revision when admitted.
    };

    /** Per-membership initial cursor, without an independent frame/payload owner. */
    struct ViewPart {
        bool used = false;                       ///< Current desired activation is represented.
        std::size_t partition = NONE;            ///< Existing partition or NONE while reset is pending.
        core::SubscriptionMembership membership; ///< Current activation.
        std::int64_t bound = 0;                  ///< Inclusive requested raw lower bound.
        std::int64_t known = HIGH;               ///< Exclusive descending initial cursor.
        bool dirty = false;                      ///< Mixed initial/local transaction remains inconsistent.
        bool queued = false;                     ///< Initial projection has currently available data/boundary.
        std::uint64_t queueOrder = 0;            ///< Stable source/activation scheduling order.
        std::size_t snapshotRemaining = 0;       ///< Remaining per-partition rotation allowance.
        std::uint64_t revision = 0;              ///< Initial eligibility revision.
    };

    /** Candidate-local view lifecycle; failed state is not published early. */
    struct ViewMeta {
        HistoryViewState state = HistoryViewState::INVALID; ///< Local lifecycle.
        std::size_t size = 0;                               ///< Materialized pending FIFO occupancy.
        std::size_t balance = 0;                            ///< Snapshot/local fairness credit.
        std::uint64_t nextOrder = 0;               ///< Never-reused initial scheduling order within this activation.
        std::optional<HistoryViewFailure> failure; ///< Reserved detached failure evidence.
    };

    /** Complete metadata candidate; no vector grows after construction. */
    struct Bank {
        std::vector<Partition> partitions; ///< Fixed partition slots.
        std::vector<Point> points;         ///< Fixed indexed value slots.
        std::vector<char16_t> symbols;     ///< Fixed per-partition exact symbol storage.
        std::vector<ViewPart> parts;       ///< Fixed viewSlots*desiredEntries cursor slots.
        std::vector<Pending> pending;      ///< Fixed viewSlots*pendingItems FIFO slots.
        std::vector<ViewMeta> views;       ///< Fixed view lifecycle metadata.
        std::uint64_t nextPartition = 0;   ///< Next partition activation source, never wrapped.
    };

    /** Exactly one serialized event owner, regardless of all bank aliases. */
    struct Frame {
        std::optional<protocol::LiveEvent> owner; ///< Immutable actual admitted event.
        std::size_t bytes = 0;                    ///< Full State+arena requested capacity.
    };

    /** Planned single delivery, committed only by pop. */
    struct Selection {
        std::size_t part = NONE; ///< Per-view cursor slot.
        bool local = false;      ///< Select local FIFO head rather than shared initial state.
        Value value;             ///< Chosen actual value or synthetic boundary.
        std::int32_t flags = 0;  ///< Projected delivery flags.
        HistoryDeliveryKind kind = HistoryDeliveryKind::INITIAL; ///< Output origin.
        std::size_t balance = 0;                                 ///< Normalized pre-pop fairness credit.
        bool completeInitial = false; ///< This record completes currently available initial data.
        bool clearDirty = false;      ///< Completing this projection can clear mixed transaction state.
        std::uint64_t serial = 0;     ///< Exact publication to acknowledge.
    };

    /** Public handle slot; desired state remains independent of candidate cache banks. */
    struct View {
        std::optional<core::SubscriptionState> desired; ///< Pure serialized core owner.
        HistoryViewOptions options{};                   ///< Explicit projection mode.
        std::uint64_t activation = 0;                   ///< Non-reused public view selection.
        std::optional<Selection> offer;                 ///< Current prepared offer.
    };

    core::LocalSchema schema;                     ///< Immutable local declarations and provenance.
    HistoryLimits limits;                         ///< Finite requested capacities.
    HistoryOptions options;                       ///< Explicit retention/scheduling.
    HistoryIdentity *identity;                    ///< Fixed detached marker; no protocol owner reaches it.
    Bank banks[2];                                ///< Published and scratch fixed metadata.
    std::size_t published = 0;                    ///< Active bank.
    bool scratchActive = false;                   ///< Cleanup begins only after full candidate work reservation.
    std::vector<Frame> frames;                    ///< Fc+V*Fp+1 actual owner slots.
    std::vector<View> views;                      ///< Fixed public/core view slots.
    mutable std::vector<unsigned char> marks;     ///< Fixed unique-owner accounting scratch, including INCOMING.
    std::vector<HistoryFieldValueView> fields;    ///< Fixed per-view offer field scratch.
    std::size_t maxFields = 0;                    ///< Largest local record field count.
    std::size_t fixed = 0;                        ///< Known fixed/schema backing, excluding desired/session scopes.
    std::size_t bytes = 0;                        ///< Current unique actual event bytes.
    std::size_t owners = 0;                       ///< Current unique owner count.
    std::size_t cachedPartitions = 0;             ///< Published constant-time partition count.
    std::size_t cachedPoints = 0;                 ///< Published constant-time indexed value count.
    std::uint64_t epoch = 1;                      ///< Collector-local epoch generation.
    std::uint64_t serial = 0;                     ///< Non-reused publication serial.
    std::uint64_t viewGeneration = 0;             ///< Public view activation counter.
    std::uint64_t retentionGeneration = 0;        ///< Explicit forget report generation.
    protocol::LiveIdentity source;                ///< Serialized pinned source epoch, retained after drain.
    HistoryState freshness = HistoryState::EMPTY; ///< Shared source freshness.
    std::optional<HistoryResetNotice> notice;     ///< Reserved coalesced owner reset interval.

    /** Adopt already-allocated backing without another fallible operation.
     * @param sourceSchema Schema consumed only after all fixed allocation succeeds.
     * @param configured Immutable finite limits.
     * @param selected Explicit source profile.
     * @param marker Detached scalar origin.
     */
    Impl(core::LocalSchema &&sourceSchema, HistoryLimits configured, HistoryOptions selected,
         HistoryIdentity *marker) noexcept
        : schema(std::move(sourceSchema)), limits(configured), options(selected), identity(marker) {
    }

    /** Clear all borrowed references before serialized source-owner fallback release. */
    ~Impl() {
        invalidate();
        clear(banks[0]);
        clear(banks[1]);

        for (auto &frame : frames) {
            frame.owner.reset();
        }

        source = {};
        release(identity);
    }

    /** Inspect published bank.
     * @return Current metadata, never scratch.
     */
    Bank &current() noexcept {
        return banks[published];
    }

    /** Inspect published immutable metadata.
     * @return Current metadata.
     */
    const Bank &current() const noexcept {
        return banks[published];
    }

    /** Borrow exact owned symbol without null-pointer arithmetic for zero storage.
     * @param b Published or scratch bank.
     * @param k Occupied partition slot.
     * @return Exact units, including a non-null empty view.
     */
    std::u16string_view symbol(const Bank &b, std::size_t k) const noexcept {
        const auto n = b.partitions[k].symbolLength;

        return n ? std::u16string_view(b.symbols.data() + k * limits.symbolUnitsPerPartition, n)
                 : std::u16string_view(u"", 0);
    }

    /** Locate a public view without accepting foreign/stale selections.
     * @param token Detached view selection.
     * @param failed Permit query/close of failed evidence.
     * @return Valid physical slot or throw typed rejection.
     */
    std::size_t view(const HistoryViewToken &token, bool failed = false) const {
        if (token.origin_ != identity || token.slot_ >= views.size() ||
            token.generation_ != views[token.slot_].activation || !views[token.slot_].desired) {
            throw HistoryError::INVALID_VIEW;
        }

        if (!failed && current().views[token.slot_].state == HistoryViewState::FAILED) {
            throw HistoryError::FAILED_VIEW;
        }

        return token.slot_;
    }

    /** Require fresh owner admission/delivery.
     * @throws HistoryError STALE or RESET_PENDING.
     */
    void fresh() const {
        if (freshness == HistoryState::STALE) {
            throw HistoryError::STALE;
        }

        if (freshness == HistoryState::RESET_PENDING) {
            throw HistoryError::RESET_PENDING;
        }
    }

    /** Check publication identity before any destructive commit. */
    void nextPublication() const {
        if (serial == std::numeric_limits<std::uint64_t>::max()) {
            throw HistoryError::IDENTITY_EXHAUSTED;
        }
    }

    /** Invalidate borrowed current offers before mutation/release. */
    void invalidate() noexcept {
        for (auto &v : views) {
            v.offer.reset();
        }

        std::fill(fields.begin(), fields.end(), HistoryFieldValueView{});
    }

    /** Clear all scratch references before an input/current owner can die.
     * @param b Inactive bank; fixed vectors retain capacity.
     */
    void clear(Bank &b) noexcept {
        std::fill(b.partitions.begin(), b.partitions.end(), Partition{});
        std::fill(b.points.begin(), b.points.end(), Point{});
        std::fill(b.parts.begin(), b.parts.end(), ViewPart{});
        std::fill(b.pending.begin(), b.pending.end(), Pending{});
        std::fill(b.views.begin(), b.views.end(), ViewMeta{});

        if (&b == &banks[1 - published]) {
            scratchActive = false;
        }
    }

    /** Abandon only a candidate whose cleanup traversal was already precharged.
     * Preflight rejection before candidate creation must remain a constant-time failure path.
     */
    void abandon() noexcept {
        if (scratchActive) {
            clear(banks[1 - published]);
        }
    }

    /** Clone fixed metadata without allocating or acquiring another event owner.
     * @return Isolated scratch bank.
     */
    Bank &candidate() {
        auto &b = banks[1 - published];
        scratchActive = true;
        const auto &a = current();
        std::copy(a.partitions.begin(), a.partitions.end(), b.partitions.begin());
        std::copy(a.points.begin(), a.points.end(), b.points.begin());
        std::copy(a.symbols.begin(), a.symbols.end(), b.symbols.begin());
        std::copy(a.parts.begin(), a.parts.end(), b.parts.begin());
        std::copy(a.pending.begin(), a.pending.end(), b.pending.begin());
        std::copy(a.views.begin(), a.views.end(), b.views.begin());
        b.nextPartition = a.nextPartition;

        return b;
    }

    /** Bound every fixed clone/cleanup/accounting scan, including zero dimensions.
     * @return Conservative metadata visits per whole-owner maintenance phase.
     */
    std::size_t fixedVisits() const {
        const auto e = limits.viewSubscriptions.entries;
        const auto vp = product(limits.viewSlots, limits.pendingItems);
        const auto ve = product(limits.viewSlots, e);
        auto n = sum(8, product(12, sum(sum(limits.partitions, limits.points), sum(vp, ve))));
        n = sum(n, product(12, sum(frames.size(), limits.viewSlots)));
        n = sum(n, product(8, product(limits.partitions, product(limits.viewSlots, sum(e, 1)))));
        n = sum(n, product(sum(limits.viewSlots, 2), sum(sum(frames.size(), limits.points), sum(vp, 4))));
        n = sum(n, product(16, product(limits.partitions, limits.points)));
        // Empty partitions still examine other partitions retained in every local FIFO.
        n = sum(n, product(16, product(limits.partitions, vp)));
        n = sum(n, product(12, fields.size()));
        n = sum(n, product(16, product(schema.records().size(), sum(ve, 1))));

        return n;
    }

    /** Debit fixed scans/text and every potentially last source disposal before commit.
     * @param work Caller turn credits.
     * @param input Optional caller-held candidate source owner.
     */
    void maintenance(HistoryWork &work, const protocol::LiveEvent *input = nullptr) const {
        const auto texts =
            product(8, product(sum(limits.partitions, 1),
                               product(sum(limits.viewSlots, 1),
                                       sum(limits.viewSubscriptions.totalSymbolUnits,
                                           product(limits.partitions, limits.symbolUnitsPerPartition)))));
        spend(work, 0, fixedVisits(), texts);
        auto disposal = input ? input->releaseBytes() : 0;

        for (const auto &f : frames) {
            if (f.owner) {
                disposal = sum(disposal, f.owner->releaseBytes());
            }
        }

        spend(work, 0, 0, 0, disposal);
    }

    /** Locate one exact partition; context was validated against the local record first.
     * @param b Metadata bank.
     * @param record Local record ID.
     * @param text Exact symbol.
     * @return Slot or NONE.
     */
    std::size_t find(const Bank &b, std::int32_t record, std::u16string_view text) const noexcept {
        for (std::size_t k = 0; k < b.partitions.size(); ++k) {
            if (b.partitions[k].used && b.partitions[k].record == record && symbol(b, k) == text) {
                return k;
            }
        }

        return NONE;
    }

    /** Find immutable record identity from a validated exact key.
     * @param key Borrowed desired/query key.
     * @return Local ID or typed invalid context.
     */
    std::int32_t record(const core::SubscriptionKey &key) const {
        for (std::size_t r = 0; r < schema.records().size(); ++r) {
            if (schema.recordContext(static_cast<std::int32_t>(r)).matches(key.context)) {
                return static_cast<std::int32_t>(r);
            }
        }

        throw HistoryError::INVALID_CONTEXT;
    }

    /** Create exact bounded metadata without retaining caller symbol buffers.
     * @param b Candidate bank.
     * @param r Valid local record.
     * @param text Exact symbol.
     * @return Existing or newly initialized slot.
     */
    std::size_t partition(Bank &b, std::int32_t r, std::u16string_view text) {
        auto k = find(b, r, text);

        if (k != NONE) {
            return k;
        }

        if (text.size() > limits.symbolUnitsPerPartition) {
            throw HistoryError::RESOURCE_LIMIT;
        }

        if (b.nextPartition == std::numeric_limits<std::uint64_t>::max()) {
            throw HistoryError::IDENTITY_EXHAUSTED;
        }

        for (k = 0; k < b.partitions.size(); ++k) {
            if (!b.partitions[k].used) {
                auto &p = b.partitions[k];
                p = Partition{};
                p.used = true;
                p.record = r;
                p.symbolLength = text.size();
                p.activation = ++b.nextPartition;

                if (!text.empty()) {
                    std::copy(text.begin(), text.end(), b.symbols.data() + k * limits.symbolUnitsPerPartition);
                }

                return k;
            }
        }

        throw HistoryError::RESOURCE_LIMIT;
    }

    /** Locate one current membership cursor.
     * @param b Metadata bank.
     * @param v View slot.
     * @param k Exact partition slot.
     * @return Cursor slot or NONE.
     */
    std::size_t part(const Bank &b, std::size_t v, std::size_t k) const noexcept {
        const auto first = v * limits.viewSubscriptions.entries;

        for (std::size_t i = first; i < first + limits.viewSubscriptions.entries; ++i) {
            if (b.parts[i].used && b.parts[i].partition == k) {
                return i;
            }
        }

        return NONE;
    }

    /** Inspect active aggregate minimum solely for local cache retention.
     * @param b Metadata bank.
     * @param k Partition slot.
     * @return Inclusive minimum, HIGH when no active view; no wire-union claim.
     */
    std::int64_t minimum(const Bank &b, std::size_t k) const noexcept {
        auto result = HIGH;

        for (std::size_t v = 0; v < views.size(); ++v) {
            if (b.views[v].state != HistoryViewState::ACTIVE) {
                continue;
            }

            const auto i = part(b, v, k);

            if (i != NONE) {
                result = std::min(result, b.parts[i].bound);
            }
        }

        return result;
    }

    /** Inspect live interest separately from a valid requested bound of HIGH.
     * @param b Metadata bank.
     * @param k Partition slot.
     * @return True when an ACTIVE view owns this partition.
     */
    bool interested(const Bank &b, std::size_t k) const noexcept {
        for (std::size_t v = 0; v < views.size(); ++v) {
            if (b.views[v].state == HistoryViewState::ACTIVE && part(b, v, k) != NONE) {
                return true;
            }
        }

        return false;
    }

    /** Decode the declared logical raw index without business conversion.
     * @param r Actual admitted indexed record.
     * @return Full signed64 bits.
     */
    std::int64_t time(const protocol::LiveRecordView &r) const {
        const auto &layout = *schema.records()[static_cast<std::size_t>(r.localId)].indexedTimeLayout;
        const auto raw = [&](std::size_t i) {
            return r.typedValues.empty() ? static_cast<std::int64_t>(r.values[i]) : r.typedValues[i].raw;
        };

        if (const auto *pair = std::get_if<core::IndexedTimePairInt32>(&layout)) {
            const auto bits =
                (static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw(pair->firstField))) << TIME_WORD_BITS) |
                static_cast<std::uint32_t>(raw(pair->secondField));

            return std::bit_cast<std::int64_t>(bits);
        }

        return raw(std::get<core::IndexedTimeSingleInt64>(layout).field);
    }

    /** Inspect explicit/implicit source transaction.
     * @param p Partition state.
     * @return Any pending source transaction.
     */
    static bool tx(const Partition &p) noexcept {
        return p.explicitTx || p.sweepTx;
    }

    /** Find actual cached raw index.
     * @param b Metadata bank.
     * @param k Partition.
     * @param t Raw index.
     * @return Point slot or NONE.
     */
    std::size_t point(const Bank &b, std::size_t k, std::int64_t t) const noexcept {
        for (std::size_t i = 0; i < b.points.size(); ++i) {
            if (b.points[i].used && b.points[i].partition == k && b.points[i].value.time == t) {
                return i;
            }
        }

        return NONE;
    }

    /** Inspect whether an initial boundary can be generated.
     * @param b Metadata bank.
     * @param i Membership cursor.
     * @return Exact lower endpoint and projected END/SNIP flag.
     */
    std::pair<std::int64_t, std::int32_t> boundary(const Bank &b, std::size_t i) const noexcept {
        const auto &m = b.parts[i];
        const auto &p = b.partitions[m.partition];
        auto to = std::max(m.bound, p.frontier);
        auto flags = to == m.bound ? END : 0;

        if (p.snip != LOW && to == p.snip && p.snip >= m.bound) {
            flags = SNIP;
        }

        if (!p.mode) {
            flags &= ~END;
        }

        return {to, flags};
    }

    /** Determine if a cursor currently has cache data or a synthetic boundary.
     * @param b Metadata bank.
     * @param v View.
     * @param i Cursor slot.
     * @return An actual available projection, not a claim of complete history.
     */
    bool available(const Bank &b, std::size_t v, std::size_t i) const noexcept {
        const auto &m = b.parts[i];

        if (!m.used || m.partition == NONE || m.known <= m.bound) {
            return false;
        }

        const auto [to, flag] = boundary(b, i);

        if (m.known <= to) {
            return false;
        }

        for (const auto &p : b.points) {
            if (p.used && p.partition == m.partition && p.value.time < m.known && p.value.time >= to) {
                return true;
            }
        }

        return views[v].options.historySnapshot && flag != 0;
    }

    /** Reconcile initial queue readiness without creating historical payload copies.
     * @param b Candidate/current bank.
     * @param v View.
     * @param i Cursor slot.
     */
    void queue(Bank &b, std::size_t v, std::size_t i) {
        auto &m = b.parts[i];
        const bool ready = available(b, v, i);

        if (ready && !m.queued) {
            auto &vm = b.views[v];

            if (vm.nextOrder == std::numeric_limits<std::uint64_t>::max()) {
                throw HistoryError::IDENTITY_EXHAUSTED;
            }

            m.queueOrder = ++vm.nextOrder;
            m.snapshotRemaining = options.partitionSnapshotQuantum;
        }

        m.queued = ready;
    }

    /** Remove a partition's pending entries while preserving other FIFO order.
     * @param b Metadata bank.
     * @param v View.
     * @param k Partition to withdraw.
     */
    void dropPending(Bank &b, std::size_t v, std::size_t k) noexcept {
        auto &size = b.views[v].size;
        const auto base = v * limits.pendingItems;
        std::size_t kept = 0;

        for (std::size_t n = 0; n < size; ++n) {
            if (b.pending[base + n].partition != k) {
                if (kept != n) {
                    b.pending[base + kept] = std::move(b.pending[base + n]);
                }

                ++kept;
            }
        }

        for (auto n = kept; n < size; ++n) {
            b.pending[base + n] = Pending{};
        }

        size = kept;
    }

    /** Locate the latest still-pending local record for one exact partition.
     * @param b Metadata bank.
     * @param v View.
     * @param k Partition.
     * @return Physical pending slot or NONE.
     */
    std::size_t lastPending(const Bank &b, std::size_t v, std::size_t k) const noexcept {
        const auto base = v * limits.pendingItems;

        for (auto n = b.views[v].size; n != 0; --n) {
            if (b.pending[base + n - 1].partition == k) {
                return base + n - 1;
            }
        }

        return NONE;
    }

    /** Mark an inconsistent mixed projection without changing raw source flags.
     * @param b Candidate bank.
     * @param v View.
     * @param i Cursor slot.
     */
    void dirty(Bank &b, std::size_t v, std::size_t i) noexcept {
        b.parts[i].dirty = true;
        const auto base = v * limits.pendingItems;

        for (std::size_t n = 0; n < b.views[v].size; ++n) {
            if (b.pending[base + n].partition == b.parts[i].partition) {
                b.pending[base + n].flags |= TX;
            }
        }
    }

    /** Compute a unique frame/byte domain using fixed scratch, including INCOMING.
     * @param b Metadata bank.
     * @param v NONE for cache, otherwise local pending view.
     * @param incomingBytes Actual caller event State+arena capacity.
     * @return Unique domain frames/bytes.
     */
    std::pair<std::size_t, std::size_t> footprint(const Bank &b, std::size_t v, std::size_t incomingBytes) const {
        std::fill(marks.begin(), marks.end(), 0);

        if (v == NONE) {
            for (const auto &p : b.points) {
                if (p.used && p.value.frame != NONE) {
                    marks[p.value.frame] = 1;
                }
            }
        } else {
            const auto base = v * limits.pendingItems;

            for (std::size_t n = 0; n < b.views[v].size; ++n) {
                const auto f = b.pending[base + n].value.frame;

                if (f != NONE) {
                    marks[f] = 1;
                }
            }
        }

        std::size_t n = 0;
        std::size_t total = 0;

        for (std::size_t f = 0; f < marks.size(); ++f) {
            if (marks[f]) {
                ++n;
                total = sum(total, f == frames.size() ? incomingBytes : frames[f].bytes, HistoryError::RESOURCE_LIMIT);
            }
        }

        return {n, total};
    }

    /** Does a candidate domain retain the caller input?
     * @param b Candidate bank.
     * @param v Cache or local domain.
     * @param incomingBytes Actual caller input capacity.
     * @return True exactly when its referenced set includes INCOMING.
     */
    bool incoming(const Bank &b, std::size_t v, std::size_t incomingBytes) const {
        (void)footprint(b, v, incomingBytes);

        return marks[frames.size()] != 0;
    }

    /** Test old plus incoming peak before granting any retirement credit.
     * @param b Candidate domain.
     * @param v Cache or view.
     * @param incomingBytes New source capacity.
     * @return Fits independent peak owner/byte reservation.
     */
    bool peak(const Bank &b, std::size_t v, std::size_t incomingBytes) const {
        const auto [oldCount, oldBytes] = footprint(current(), v, incomingBytes);
        const bool added = incoming(b, v, incomingBytes);
        const auto n = sum(oldCount, added ? 1 : 0, HistoryError::RESOURCE_LIMIT);
        const auto total = sum(oldBytes, added ? incomingBytes : 0, HistoryError::RESOURCE_LIMIT);

        return n <= (v == NONE ? limits.cacheFrames : limits.pendingFrames) &&
               total <= (v == NONE ? limits.cacheBytes : limits.pendingBytes);
    }

    /** Record candidate-only failure; no published view changes yet.
     * @param b Candidate bank.
     * @param v View.
     * @param ordinal Failing source-record ordinal.
     */
    void failView(Bank &b, std::size_t v, std::size_t ordinal) noexcept {
        auto &m = b.views[v];

        if (!m.failure) {
            bool initial = false;
            const auto begin = v * limits.viewSubscriptions.entries;

            for (std::size_t i = begin; i < begin + limits.viewSubscriptions.entries; ++i) {
                initial |= b.parts[i].queued;
            }

            m.failure = HistoryViewFailure{HistoryError::RESOURCE_LIMIT, ordinal, m.size, initial, true};
        }
    }

    /** Append or last-same-index conflate one measured local update.
     * @param b Candidate bank.
     * @param v View.
     * @param i Cursor slot.
     * @param value Actual immutable or synthetic value.
     * @param flags Produced source-compatible flags.
     * @param kind Projection origin.
     * @param ordinal Source record causing this projection.
     * @param inputBytes Actual caller event capacity for peak accounting.
     * @param result Exact stage diagnostics.
     */
    void append(Bank &b, std::size_t v, std::size_t i, Value value, std::int32_t flags, HistoryDeliveryKind kind,
                std::size_t ordinal, std::size_t inputBytes, HistoryStage &result) {
        auto &vm = b.views[v];

        if (vm.failure || vm.state != HistoryViewState::ACTIVE) {
            return;
        }

        const auto &m = b.parts[i];
        const auto last = lastPending(b, v, m.partition);

        if (!views[v].options.historySnapshot) {
            flags = 0;
        }

        Pending item{m.partition, std::move(value), flags, kind, m.membership, views[v].desired->revision()};

        if (last != NONE && b.pending[last].value.time == item.value.time) {
            b.pending[last] = std::move(item);
            ++result.localConflations;
        } else {
            if (vm.size == limits.pendingItems) {
                failView(b, v, ordinal);

                return;
            }

            b.pending[v * limits.pendingItems + vm.size++] = std::move(item);
        }

        if (!peak(b, v, inputBytes)) {
            failView(b, v, ordinal);
        }
    }

    /** Route one source effect according to each view's acknowledged initial cursor.
     * @param b Candidate bank, shared state already updated for this source event.
     * @param k Partition.
     * @param value Actual value/defaulted removal.
     * @param flags Raw supported flags before view normalization.
     * @param changed Real stored-field/remove effect.
     * @param txEnd Source transaction ended even without a data change.
     * @param snipChanged New truncation boundary.
     * @param snipRemoval This tombstone is implicit below SNIP and only legacy needs it.
     * @param ordinal Source record ordinal.
     * @param inputBytes Actual input owner capacity.
     * @param result Stage counters.
     */
    void distribute(Bank &b, std::size_t k, Value value, std::int32_t flags, bool changed, bool txEnd, bool snipChanged,
                    bool snipRemoval, std::size_t ordinal, std::size_t inputBytes, HistoryStage &result) {
        for (std::size_t v = 0; v < views.size(); ++v) {
            if (b.views[v].state != HistoryViewState::ACTIVE || b.views[v].failure) {
                continue;
            }

            const auto i = part(b, v, k);

            if (i == NONE) {
                continue;
            }

            auto &m = b.parts[i];
            auto delivered = value;
            auto endTx = txEnd;
            const bool snapshot = views[v].options.historySnapshot;

            if (snipRemoval && snapshot) {
                continue;
            }

            auto produced = flags & (TX | REMOVE | SNIP);

            if (snapshot && b.partitions[k].sweepTx) {
                produced |= TX;
            }

            if (snapshot && (produced & SNIP) && value.time >= m.known) {
                m.known = m.bound;
            }

            if (snapshot && m.dirty && (produced & TX) == 0 && m.known == m.bound) {
                m.dirty = false;
                endTx = true;
            }

            auto processTime = delivered.time;

            if (endTx && m.known == m.bound && processTime < m.bound) {
                processTime = m.bound;
                delivered.time = HIGH;
                delivered.synthetic = true;
                delivered.frame = NONE;
                delivered.raw.reset();
                produced = REMOVE;
            }

            if (processTime < m.known) {
                if (((produced & REMOVE) == 0 && processTime >= m.bound) ||
                    (snapshot && !snipRemoval && m.bound < m.known)) {
                    queue(b, v, i);
                }

                continue;
            }

            const auto last = lastPending(b, v, k);

            if (!changed && !(snipChanged && snapshot)) {
                if (!endTx || m.dirty) {
                    continue;
                }

                if (last != NONE) {
                    b.pending[last].flags &= ~TX;
                    continue;
                }
            }

            if (snapshot && !m.dirty && m.known > m.bound && b.partitions[k].mode) {
                dirty(b, v, i);
            }

            if (snapshot && m.dirty) {
                produced |= TX;
            }

            const auto kind = delivered.time == HIGH
                                  ? HistoryDeliveryKind::BOUNDARY
                                  : ((produced & REMOVE) ? HistoryDeliveryKind::REMOVAL : HistoryDeliveryKind::UPDATE);
            append(b, v, i, delivered, produced, kind, ordinal, inputBytes, result);
        }
    }

    /** Trim cache after the stable final failed-view set or a desired mutation.
     * @param b Candidate bank.
     */
    void trim(Bank &b) noexcept {
        if (options.storeEverything) {
            return;
        }

        for (std::size_t k = 0; k < b.partitions.size(); ++k) {
            auto &p = b.partitions[k];

            if (!p.used) {
                continue;
            }

            const auto minimumTime = minimum(b, k);
            const bool active = interested(b, k);
            const auto &old = current();
            const bool same = old.partitions[k].used && old.partitions[k].activation == p.activation;

            if (same && minimumTime == minimum(old, k) && active == interested(old, k)) {
                continue;
            }

            for (auto &point : b.points) {
                if (point.used && point.partition == k && (!active || point.value.time < minimumTime)) {
                    point = Point{};
                }
            }

            if (!active) {
                p.frontier = HIGH;
                p.ever = HIGH;
                p.began = false;
                p.ended = false;
            } else {
                p.frontier = std::max(p.frontier, minimumTime);
                p.ever = std::max(p.ever, minimumTime);

                if (minimumTime < p.frontier && p.snip != p.frontier) {
                    p.began = false;
                    p.ended = false;
                }
            }
        }
    }

    /** Release no-longer-referenced roots after clearing retired metadata.
     * All traversal/disposal work was sampled and debited before the destructive commit.
     */
    void sweepFrames() noexcept {
        std::fill(marks.begin(), marks.end(), 0);
        const auto &b = current();
        cachedPartitions = 0;
        cachedPoints = 0;

        for (const auto &p : b.partitions) {
            cachedPartitions += p.used ? 1 : 0;
        }

        for (const auto &p : b.points) {
            cachedPoints += p.used ? 1 : 0;

            if (p.used && p.value.frame != NONE) {
                marks[p.value.frame] = 1;
            }
        }

        for (std::size_t v = 0; v < views.size(); ++v) {
            for (std::size_t n = 0; n < b.views[v].size; ++n) {
                const auto f = b.pending[v * limits.pendingItems + n].value.frame;

                if (f != NONE) {
                    marks[f] = 1;
                }
            }
        }

        bytes = 0;
        owners = 0;

        for (std::size_t f = 0; f < frames.size(); ++f) {
            if (!marks[f]) {
                frames[f].owner.reset();
                frames[f].bytes = 0;
            }

            if (frames[f].owner) {
                bytes += frames[f].bytes;
                ++owners;
            }
        }
    }

    /** Publish already-validated fixed metadata; optional input is still caller-owned until here.
     * @param input Actual new event, or null for a desired/pop/reset metadata transaction.
     */
    void commit(protocol::LiveEvent *input = nullptr) noexcept {
        invalidate();
        auto &b = banks[1 - published];
        const auto incomingIndex = frames.size();
        bool retainInput = false;

        for (const auto &p : b.points) {
            retainInput |= p.used && p.value.frame == incomingIndex;
        }

        for (std::size_t v = 0; v < views.size(); ++v) {
            for (std::size_t n = 0; n < b.views[v].size; ++n) {
                retainInput |= b.pending[v * limits.pendingItems + n].value.frame == incomingIndex;
            }
        }

        if (input && retainInput) {
            std::size_t slot = 0;

            while (slot < frames.size() && frames[slot].owner) {
                ++slot;
            }

            if (slot == frames.size()) {
                std::terminate();
            }

            frames[slot].bytes = input->residentBytes();
            frames[slot].owner.emplace(std::move(*input));

            for (auto &p : b.points) {
                if (p.used && p.value.frame == incomingIndex) {
                    p.value.frame = slot;
                }
            }

            for (auto &p : b.pending) {
                if (p.value.frame == incomingIndex) {
                    p.value.frame = slot;
                }
            }
        }

        published = 1 - published;
        ++serial;
        clear(banks[1 - published]);
        sweepFrames();

        if (input && !retainInput) {
            protocol::LiveEvent consumed(std::move(*input));
        }
    }

    /** Process one actual record in the isolated candidate in source order.
     * @param b Candidate bank.
     * @param r Actual admitted record.
     * @param generation Original immutable binding generation.
     * @param ordinal Zero-based record ordinal.
     * @param inputBytes Entire incoming owner capacity.
     * @param work Exact data equality credits in addition to precharged structural scans.
     * @param result Exact bounded source effects.
     */
    void process(Bank &b, const protocol::LiveRecordView &r, std::uint64_t generation, std::size_t ordinal,
                 std::size_t inputBytes, HistoryWork &work, HistoryStage &result) {
        const auto &declaration = schema.records()[static_cast<std::size_t>(r.localId)];

        if (!declaration.indexedTimeLayout) {
            ++result.ignoredRecords;

            return;
        }

        auto k = find(b, r.localId, r.symbol);

        if (!options.storeEverything && (k == NONE || !interested(b, k))) {
            ++result.ignoredRecords;

            return;
        }

        if (k == NONE) {
            k = partition(b, r.localId, r.symbol);
        }

        auto &p = b.partitions[k];
        const auto t = time(r);
        const auto flags = r.eventFlags;

        if (t == HIGH && (flags & REMOVE) == 0) {
            throw HistoryError::INVALID_INDEX;
        }

        const auto total = minimum(b, k);
        const HistorySourceMetadata metadata{r.senderId, r.cipher, flags, t, generation, r.fieldTime};
        p.trigger = metadata;
        bool sendSnapshot = false;

        if ((flags & (BEGIN | MODE)) != 0 && !p.mode) {
            p.mode = true;
            p.began = true;
            sendSnapshot = true;
        }

        bool txEnd = p.explicitTx && (flags & TX) == 0 && !p.sweepTx;
        p.explicitTx = (flags & TX) != 0;

        if (flags & BEGIN) {
            p.frontier = HIGH;
            p.began = true;
            p.ended = false;
        }

        if (sendSnapshot) {
            for (std::size_t v = 0; v < views.size(); ++v) {
                if (b.views[v].state != HistoryViewState::ACTIVE || !views[v].options.historySnapshot) {
                    continue;
                }

                const auto i = part(b, v, k);

                if (i == NONE) {
                    continue;
                }

                dropPending(b, v, k);
                auto &m = b.parts[i];
                m.known = HIGH;
                m.dirty = false;
                m.queued = false;
            }
        }

        const bool snipChanged = (flags & SNIP) != 0 && p.snip != t;

        if (snipChanged) {
            p.snip = t;
            p.frontier = std::max(p.frontier, t);
            p.ever = std::max(p.ever, t);

            while (true) {
                auto found = NONE;
                auto greatest = LOW;

                for (std::size_t n = 0; n < b.points.size(); ++n) {
                    if (b.points[n].used && b.points[n].partition == k && b.points[n].value.time < t &&
                        (found == NONE || b.points[n].value.time > greatest)) {
                        found = n;
                        greatest = b.points[n].value.time;
                    }
                }

                if (found == NONE) {
                    break;
                }

                b.points[found] = Point{};
                Value removal{NONE, std::nullopt, metadata, greatest, true};
                distribute(b, k, removal, REMOVE, true, false, false, true, ordinal, inputBytes, result);
                ++result.syntheticRemovals;
            }
        }

        const auto trimTo = p.mode && !p.began && !p.ended ? p.ever : (options.storeEverything ? LOW : total);
        const auto previous = p.frontier;
        const auto trimmed = std::max(t, trimTo);
        bool updatedEver = false;
        bool swept = false;

        if (trimmed < p.frontier) {
            const auto threshold = t < trimTo ? trimTo - 1 : t;

            while (true) {
                auto found = NONE;
                auto greatest = LOW;

                for (std::size_t n = 0; n < b.points.size(); ++n) {
                    if (b.points[n].used && b.points[n].partition == k && b.points[n].value.time < previous &&
                        b.points[n].value.time > threshold && (found == NONE || b.points[n].value.time > greatest)) {
                        found = n;
                        greatest = b.points[n].value.time;
                    }
                }

                if (found == NONE) {
                    break;
                }

                b.points[found] = Point{};
                p.sweepTx = true;
                swept = true;
                Value removal{NONE, std::nullopt, metadata, greatest, true};
                distribute(b, k, removal, REMOVE | TX, true, false, false, false, ordinal, inputBytes, result);
                ++result.syntheticRemovals;
            }

            p.frontier = trimmed;
            updatedEver = trimmed < p.ever;
            p.ever = std::min(p.ever, trimmed);

            if (trimmed < p.snip) {
                p.snip = LOW;
            }
        }

        bool changed = false;
        Value value{frames.size(), r, metadata, t, false};

        if (t >= trimTo && t != HIGH) {
            auto n = point(b, k, t);

            if (flags & REMOVE) {
                if (n != NONE) {
                    b.points[n] = Point{};
                    changed = true;
                }
            } else {
                if (n != NONE && b.points[n].value.raw && equalData(*b.points[n].value.raw, r, work)) {
                    changed = false;
                } else {
                    if (n == NONE) {
                        for (n = 0; n < b.points.size() && b.points[n].used; ++n) {
                        }

                        if (n == b.points.size()) {
                            throw HistoryError::RESOURCE_LIMIT;
                        }
                    }

                    b.points[n] = Point{true, k, value};
                    changed = true;
                }
            }
        }

        if (changed) {
            ++result.changedPoints;
        }

        if (swept || (changed && t < previous && t > total && !updatedEver)) {
            p.sweepTx = true;
        }

        const bool receivedEnd = (flags & SNIP) != 0 || t <= total;

        if (receivedEnd) {
            if (p.began) {
                p.ended = true;
            }

            if (p.sweepTx) {
                p.sweepTx = false;
                txEnd |= !p.explicitTx;
            }
        }

        if (t < trimTo && !txEnd && !snipChanged && !swept) {
            ++result.ignoredRecords;
        }

        distribute(b, k, value, flags, changed, txEnd, snipChanged, false, ordinal, inputBytes, result);

        for (std::size_t v = 0; v < views.size(); ++v) {
            const auto i = part(b, v, k);

            if (i != NONE && b.views[v].state == HistoryViewState::ACTIVE && !b.views[v].failure) {
                queue(b, v, i);
            }
        }

        if (!peak(b, NONE, inputBytes)) {
            throw HistoryError::RESOURCE_LIMIT;
        }
    }

    /** Produce a read-only exact next projection without changing queue/cursor/credit state.
     * @param b Published metadata.
     * @param v Active view.
     * @return Prepared selection or empty.
     */
    std::optional<Selection> select(const Bank &b, std::size_t v) const {
        auto first = NONE;
        const auto base = v * limits.viewSubscriptions.entries;

        for (std::size_t i = base; i < base + limits.viewSubscriptions.entries; ++i) {
            if (b.parts[i].used && b.parts[i].queued && available(b, v, i) &&
                (first == NONE || b.parts[i].queueOrder < b.parts[first].queueOrder)) {
                first = i;
            }
        }

        const bool local = b.views[v].size != 0;
        auto balance = b.views[v].balance;

        if (!local) {
            balance = 0;
        }

        if (first == NONE && !local) {
            return std::nullopt;
        }

        Selection s;
        s.balance = first == NONE ? options.retrievalQuantum : balance;
        s.serial = serial;

        if (local && (first == NONE || balance >= options.retrievalQuantum)) {
            const auto &p = b.pending[v * limits.pendingItems];
            s.local = true;
            s.part = part(b, v, p.partition);
            s.value = p.value;
            s.flags = p.flags;
            s.kind = p.kind;

            return s;
        }

        s.part = first;
        const auto &m = b.parts[first];
        const auto &p = b.partitions[m.partition];
        const auto [to, endFlag] = boundary(b, first);
        auto chosen = NONE;

        for (std::size_t n = 0; n < b.points.size(); ++n) {
            if (b.points[n].used && b.points[n].partition == m.partition && b.points[n].value.time < m.known &&
                b.points[n].value.time >= to &&
                (chosen == NONE || b.points[n].value.time > b.points[chosen].value.time)) {
                chosen = n;
            }
        }

        if (chosen != NONE) {
            s.value = b.points[chosen].value;
        } else {
            s.value = Value{NONE, std::nullopt, p.trigger, to, true};
            s.kind = HistoryDeliveryKind::BOUNDARY;
        }

        s.flags = ((m.dirty || tx(p)) ? TX : 0) | (m.known == HIGH ? BEGIN : 0);

        if (s.value.time == to) {
            s.flags |= endFlag;
        }

        if (s.value.synthetic) {
            s.flags |= REMOVE;
        }

        s.completeInitial = true;

        for (const auto &point : b.points) {
            if (point.used && point.partition == m.partition && point.value.time < s.value.time &&
                point.value.time >= to) {
                s.completeInitial = false;
            }
        }

        if (views[v].options.historySnapshot && endFlag != 0 && s.value.time > to) {
            s.completeInitial = false;
        }

        const auto last = lastPending(b, v, m.partition);
        s.clearDirty = m.dirty && endFlag != 0 && !tx(p) && s.completeInitial;

        if (s.clearDirty && last == NONE) {
            s.flags &= ~TX;
        }

        if (!p.mode) {
            s.flags &= ~BEGIN;
        }

        if (!views[v].options.historySnapshot) {
            s.flags = 0;
        }

        return s;
    }

    /** Materialize projected fields only after complete selection/default work preflight.
     * @param v View's independent fixed scratch.
     * @param r Local record.
     * @param value Actual retained raw or local synthetic value.
     * @return Borrowed exact fields in this view's offer scratch.
     */
    std::span<const HistoryFieldValueView> materialize(std::size_t v, std::int32_t r, const Value &value) noexcept {
        const auto &declaration = schema.records()[static_cast<std::size_t>(r)];
        auto *out = fields.data() + v * maxFields;

        for (std::size_t n = 0; n < declaration.fields.size(); ++n) {
            out[n] = HistoryFieldValueView{
                static_cast<core::SerialFieldType>(declaration.fields[n].serialTypeId), 0, {}, {}};

            if (!value.synthetic && value.raw) {
                if (!value.raw->typedValues.empty()) {
                    const auto &f = value.raw->typedValues[n];
                    out[n] = HistoryFieldValueView{f.type, f.raw, f.units, f.bytes};
                } else {
                    out[n].raw = value.raw->values[n];
                }
            }
        }

        const auto &layout = *declaration.indexedTimeLayout;

        if (const auto *pair = std::get_if<core::IndexedTimePairInt32>(&layout)) {
            const auto bits = std::bit_cast<std::uint64_t>(value.time);
            out[pair->firstField].raw = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> TIME_WORD_BITS));
            out[pair->secondField].raw = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits));
        } else {
            out[std::get<core::IndexedTimeSingleInt64>(layout).field].raw = value.time;
        }

        return {out, declaration.fields.size()};
    }

    /** Restore current core memberships after atomic desired publication.
     * Input symbols needed after core mutation were copied to candidate owned partitions beforehand.
     * @param b Candidate bank.
     * @param v Changed view.
     */
    void reconcile(Bank &b, std::size_t v) {
        const auto base = v * limits.viewSubscriptions.entries;

        for (std::size_t i = base; i < base + limits.viewSubscriptions.entries; ++i) {
            if (b.parts[i].used && !views[v].desired->isCurrent(b.parts[i].membership)) {
                dropPending(b, v, b.parts[i].partition);
                b.parts[i] = ViewPart{};
            }
        }

        for (const auto &entry : views[v].desired->entries()) {
            const auto k = partition(b, record(entry.key), entry.key.symbol);
            auto i = part(b, v, k);

            if (i == NONE) {
                for (i = base; i < base + limits.viewSubscriptions.entries && b.parts[i].used; ++i) {
                }

                if (i == base + limits.viewSubscriptions.entries) {
                    std::terminate();
                }

                b.parts[i] = ViewPart{};
                b.parts[i].used = true;
                b.parts[i].partition = k;
                b.parts[i].membership = entry.membership;
                b.parts[i].bound = *entry.historyBound;
                b.parts[i].revision = views[v].desired->revision();
            }

            queue(b, v, i);
        }
    }
    /** Desired operation; the underlying core remains the authoritative validator. */
    enum class Command {
        ADD,     ///< Upsert exact desired entries.
        REMOVE,  ///< Retire selected memberships.
        REPLACE, ///< Atomically publish a complete desired map.
        CLEAR    ///< Retire every desired membership.
    };

    /** Preflight collector maintenance before publishing authoritative core state.
     * @param token Active view.
     * @param command Exact desired operation.
     * @param requests Complete borrowed add/replace inputs.
     * @param keys Complete borrowed remove inputs.
     * @param coreWork Independent authoritative command credits.
     * @param work Collector scans/peak/disposal credits.
     * @return Exact core barrier or typed error; failed/no-op commands preserve offers.
     */
    std::expected<core::SubscriptionChange, HistoryFailure>
    change(const HistoryViewToken &token, Command command, std::span<const core::SubscriptionRequest> requests,
           std::span<const core::SubscriptionKey> keys, core::SubscriptionWork &coreWork, HistoryWork &work) {
        const auto v = view(token);
        maintenance(work);
        nextPublication();
        const auto inputs = sum(requests.size(), keys.size());
        spend(work, 0,
              product(8, product(inputs, sum(sum(schema.records().size(), limits.partitions), sum(inputs, 1)))));
        std::size_t text = 0;

        for (const auto &r : requests) {
            text = sum(text, r.key.symbol.size());
        }

        for (const auto &k : keys) {
            text = sum(text, k.symbol.size());
        }

        spend(work, 0, 0, product(8, product(sum(sum(inputs, limits.partitions), 1), text)));
        auto &b = candidate();
        const auto orderAllowance = product(2, sum(limits.viewSubscriptions.entries, 1));

        if (orderAllowance > std::numeric_limits<std::uint64_t>::max() - b.views[v].nextOrder) {
            throw HistoryError::IDENTITY_EXHAUSTED;
        }

        // Copy every valid prospective symbol before core can retire an aliased old arena.
        for (const auto &request : requests) {
            if (!request.key.symbol.data() || request.key.symbol.size() > limits.viewSubscriptions.symbolUnits ||
                !request.historyBound) {
                continue;
            }

            std::int32_t r = -1;

            for (std::size_t n = 0; n < schema.records().size(); ++n) {
                if (schema.recordContext(static_cast<std::int32_t>(n)).matches(request.key.context)) {
                    r = static_cast<std::int32_t>(n);
                    break;
                }
            }

            if (r < 0 || !schema.records()[static_cast<std::size_t>(r)].indexedTimeLayout) {
                continue;
            }

            (void)partition(b, r, request.key.symbol);
        }

        auto result = [&]() {
            switch (command) {
            case Command::ADD:
                return views[v].desired->add(requests, coreWork);
            case Command::REMOVE:
                return views[v].desired->remove(keys, coreWork);
            case Command::REPLACE:
                return views[v].desired->replace(requests, coreWork);
            case Command::CLEAR:
                return views[v].desired->clear(coreWork);
            }

            std::terminate();
        }();

        if (!result) {
            clear(b);

            return std::unexpected(HistoryFailure{HistoryError::CORE_REJECTED, result.error()});
        }

        if (!result->changed) {
            clear(b);

            return *result;
        }

        // All storage/identity/release requirements were validated before this core publication.
        reconcile(b, v);
        trim(b);
        commit();

        return *result;
    }
};

namespace {
/** Package a finite typed layer failure.
 * @param error Owner error.
 * @return Detached error without operation ownership.
 */
HistoryFailure failure(HistoryError error) noexcept {
    return {error, std::nullopt};
}
} // namespace

HistoryCollector::HistoryCollector(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

HistoryCollector::~HistoryCollector() = default;

HistoryCollector::HistoryCollector(HistoryCollector &&other) noexcept : impl_(std::move(other.impl_)) {
    if (impl_) {
        impl_->invalidate();
    }
}

HistoryCollector &HistoryCollector::operator=(HistoryCollector &&other) noexcept {
    if (this != &other) {
        if (impl_) {
            impl_->invalidate();
        }

        if (other.impl_) {
            other.impl_->invalidate();
        }

        impl_ = std::move(other.impl_);
    }

    return *this;
}

std::expected<HistoryCollector, HistoryFailure>
HistoryCollector::create(core::LocalSchema &&schema, HistoryLimits limits, HistoryOptions options, HistoryWork &work) {
    try {
        spend(work, 1, schema.records().size());

        if (options.selectorPolicy != core::SubscriptionSelectorPolicy::EXACT_ONLY &&
            options.selectorPolicy != core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1) {
            throw HistoryError::UNSUPPORTED_SELECTOR;
        }

        if (!schema.sharesIdentityWith(schema) || !options.retrievalQuantum || !options.partitionSnapshotQuantum) {
            throw HistoryError::INVALID_CONFIGURATION;
        }

        const auto error = HistoryError::INVALID_CONFIGURATION;
        const auto slots =
            sum(sum(limits.cacheFrames, product(limits.viewSlots, limits.pendingFrames, error), error), 1, error);
        const auto cursors = product(limits.viewSlots, limits.viewSubscriptions.entries, error);
        const auto pending = product(limits.viewSlots, limits.pendingItems, error);
        const auto symbols = product(limits.partitions, limits.symbolUnitsPerPartition, error);
        std::size_t maxFields = 0;

        for (const auto &r : schema.records()) {
            maxFields = std::max(maxFields, r.fields.size());
        }

        const auto scratch = product(limits.viewSlots, maxFields, error);
        const auto requested =
            sum(sum(sum(limits.partitions, limits.points, error), sum(cursors, pending, error), error),
                sum(sum(slots, symbols, error), sum(limits.viewSlots, scratch, error), error), error);
        spend(work, 0, sum(32, product(4, requested, error), error), product(2, symbols, error));
        const auto reserveBytes = sum(limits.cacheBytes, product(limits.viewSlots, limits.pendingBytes, error), error);

        if (reserveBytes > limits.residentBytes) {
            throw HistoryError::RESOURCE_LIMIT;
        }

        // Refuse known requested backing before any bank/table allocation. Actual capacities are checked again below.
        auto requestedFixed = sum(sizeof(Impl), sizeof(HistoryIdentity), error);
        requestedFixed = sum(requestedFixed, schema.storageBytes() - sizeof(core::LocalSchema), error);
        auto bankBytes = product(limits.partitions, sizeof(Impl::Partition), error);
        bankBytes = sum(bankBytes, product(limits.points, sizeof(Impl::Point), error), error);
        bankBytes = sum(bankBytes, product(symbols, sizeof(char16_t), error), error);
        bankBytes = sum(bankBytes, product(cursors, sizeof(Impl::ViewPart), error), error);
        bankBytes = sum(bankBytes, product(pending, sizeof(Impl::Pending), error), error);
        bankBytes = sum(bankBytes, product(limits.viewSlots, sizeof(Impl::ViewMeta), error), error);
        requestedFixed = sum(requestedFixed, product(2, bankBytes, error), error);
        requestedFixed = sum(requestedFixed, product(slots, sizeof(Impl::Frame), error), error);
        requestedFixed = sum(requestedFixed, product(limits.viewSlots, sizeof(Impl::View), error), error);
        requestedFixed = sum(requestedFixed, sum(slots, 1, error), error);
        requestedFixed = sum(requestedFixed, product(scratch, sizeof(HistoryFieldValueView), error), error);

        if (requestedFixed > limits.residentBytes || reserveBytes > limits.residentBytes - requestedFixed) {
            throw HistoryError::RESOURCE_LIMIT;
        }

        Impl::Bank allocated[2];

        for (auto &b : allocated) {
            b.partitions.resize(limits.partitions);
            b.points.resize(limits.points);
            b.symbols.resize(symbols);
            b.parts.resize(cursors);
            b.pending.resize(pending);
            b.views.resize(limits.viewSlots);
        }

        std::vector<Impl::Frame> frames(slots);
        std::vector<Impl::View> views(limits.viewSlots);
        std::vector<unsigned char> marks(sum(slots, 1, error));
        std::vector<HistoryFieldValueView> fields(scratch);
        std::size_t fixed = sum(sizeof(Impl), sizeof(HistoryIdentity), error);
        fixed = sum(fixed, schema.storageBytes() - sizeof(core::LocalSchema), error);

        for (const auto &b : allocated) {
            fixed = sum(fixed, product(b.partitions.capacity(), sizeof(Impl::Partition), error), error);
            fixed = sum(fixed, product(b.points.capacity(), sizeof(Impl::Point), error), error);
            fixed = sum(fixed, product(b.symbols.capacity(), sizeof(char16_t), error), error);
            fixed = sum(fixed, product(b.parts.capacity(), sizeof(Impl::ViewPart), error), error);
            fixed = sum(fixed, product(b.pending.capacity(), sizeof(Impl::Pending), error), error);
            fixed = sum(fixed, product(b.views.capacity(), sizeof(Impl::ViewMeta), error), error);
        }

        fixed = sum(fixed, product(frames.capacity(), sizeof(Impl::Frame), error), error);
        fixed = sum(fixed, product(views.capacity(), sizeof(Impl::View), error), error);
        fixed = sum(fixed, marks.capacity(), error);
        fixed = sum(fixed, product(fields.capacity(), sizeof(HistoryFieldValueView), error), error);

        if (fixed > limits.residentBytes || reserveBytes > limits.residentBytes - fixed) {
            throw HistoryError::RESOURCE_LIMIT;
        }

        std::unique_ptr<HistoryIdentity> marker(new HistoryIdentity);
        void *raw = ::operator new(sizeof(Impl));
        auto owned = std::unique_ptr<Impl>(new (raw) Impl(std::move(schema), limits, options, marker.release()));
        owned->banks[0] = std::move(allocated[0]);
        owned->banks[1] = std::move(allocated[1]);
        owned->frames = std::move(frames);
        owned->views = std::move(views);
        owned->marks = std::move(marks);
        owned->fields = std::move(fields);
        owned->maxFields = maxFields;
        owned->fixed = fixed;

        return HistoryCollector(std::move(owned));
    } catch (HistoryError e) {
        return std::unexpected(failure(e));
    } catch (const std::bad_alloc &) {
        return std::unexpected(failure(HistoryError::ALLOCATION_FAILURE));
    } catch (const std::length_error &) {
        return std::unexpected(failure(HistoryError::INVALID_CONFIGURATION));
    }
}

std::expected<HistoryViewToken, HistoryFailure> HistoryCollector::openView(HistoryViewOptions options,
                                                                           HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        auto &s = *impl_;
        s.maintenance(work);
        s.nextPublication();

        if (s.viewGeneration == std::numeric_limits<std::uint64_t>::max()) {
            throw HistoryError::IDENTITY_EXHAUSTED;
        }

        std::size_t v = 0;

        while (v < s.views.size() && s.views[v].desired) {
            ++v;
        }

        if (v == s.views.size()) {
            throw HistoryError::RESOURCE_LIMIT;
        }

        auto desired = core::SubscriptionState::create(s.schema, core::SubscriptionFamily::HISTORY,
                                                       s.limits.viewSubscriptions, s.options.selectorPolicy);

        if (!desired) {
            return std::unexpected(HistoryFailure{HistoryError::CORE_REJECTED, desired.error()});
        }

        auto &b = s.candidate();
        b.views[v] = Impl::ViewMeta{};
        b.views[v].state = HistoryViewState::ACTIVE;
        s.views[v].options = options;
        s.views[v].activation = ++s.viewGeneration;
        s.views[v].desired.emplace(std::move(*desired));
        s.commit();

        return HistoryViewToken(s.identity, v, s.views[v].activation);
    } catch (HistoryError e) {
        if (impl_) {
            impl_->abandon();
        }

        return std::unexpected(failure(e));
    } catch (const std::bad_alloc &) {
        return std::unexpected(failure(HistoryError::ALLOCATION_FAILURE));
    }
}

std::expected<void, HistoryFailure> HistoryCollector::closeView(const HistoryViewToken &token, HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        auto &s = *impl_;
        const auto v = s.view(token, true);
        s.maintenance(work);
        s.nextPublication();
        auto &b = s.candidate();
        b.views[v] = Impl::ViewMeta{};
        const auto base = v * s.limits.viewSubscriptions.entries;

        for (std::size_t i = base; i < base + s.limits.viewSubscriptions.entries; ++i) {
            b.parts[i] = Impl::ViewPart{};
        }

        const auto pending = v * s.limits.pendingItems;

        for (std::size_t n = 0; n < s.limits.pendingItems; ++n) {
            b.pending[pending + n] = Impl::Pending{};
        }

        s.trim(b);
        s.commit();
        s.views[v].desired.reset();

        return {};
    } catch (HistoryError e) {
        if (impl_) {
            impl_->abandon();
        }

        return std::unexpected(failure(e));
    }
}

std::expected<core::SubscriptionChange, HistoryFailure>
HistoryCollector::add(const HistoryViewToken &view, std::span<const core::SubscriptionRequest> requests,
                      core::SubscriptionWork &coreWork, HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        return impl_->change(view, Impl::Command::ADD, requests, {}, coreWork, work);
    } catch (HistoryError e) {
        if (impl_) {
            impl_->abandon();
        }

        return std::unexpected(failure(e));
    }
}

std::expected<core::SubscriptionChange, HistoryFailure>
HistoryCollector::remove(const HistoryViewToken &view, std::span<const core::SubscriptionKey> keys,
                         core::SubscriptionWork &coreWork, HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        return impl_->change(view, Impl::Command::REMOVE, {}, keys, coreWork, work);
    } catch (HistoryError e) {
        if (impl_) {
            impl_->abandon();
        }

        return std::unexpected(failure(e));
    }
}

std::expected<core::SubscriptionChange, HistoryFailure>
HistoryCollector::replace(const HistoryViewToken &view, std::span<const core::SubscriptionRequest> requests,
                          core::SubscriptionWork &coreWork, HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        return impl_->change(view, Impl::Command::REPLACE, requests, {}, coreWork, work);
    } catch (HistoryError e) {
        if (impl_) {
            impl_->abandon();
        }

        return std::unexpected(failure(e));
    }
}

std::expected<core::SubscriptionChange, HistoryFailure>
HistoryCollector::clear(const HistoryViewToken &view, core::SubscriptionWork &coreWork, HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        return impl_->change(view, Impl::Command::CLEAR, {}, {}, coreWork, work);
    } catch (HistoryError e) {
        if (impl_) {
            impl_->abandon();
        }

        return std::unexpected(failure(e));
    }
}

std::expected<HistoryStage, HistoryFailure> HistoryCollector::stage(protocol::LiveEvent &&event, HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        auto &s = *impl_;
        s.fresh();

        if (event.kind() != protocol::LiveEventKind::DATA || event.messageType() != HISTORY_DATA ||
            !event.residentBytes()) {
            throw HistoryError::INVALID_EVENT;
        }

        auto identity = event.snapshot().identity();

        if (!identity.matches(identity)) {
            throw HistoryError::INVALID_EVENT;
        }

        if (s.source.matches(s.source) && !s.source.sameEpoch(identity)) {
            throw HistoryError::INVALID_EPOCH;
        }

        if (s.options.selectorPolicy != core::SubscriptionSelectorPolicy::EXACT_ONLY) {
            // Guard arithmetic/work/domain failures return before the outer RESOURCE_LIMIT->STALE path.
            try {
                const auto records = event.records().size();
                spend(work, 0, sum(records, 1), product(records, 7));

                for (const auto &record : event.records()) {
                    if (!encoding::isCanonicalPentaSymbol(record.symbol, record.cipher)) {
                        return std::unexpected(HistoryFailure{HistoryError::UNSUPPORTED_SYMBOL_DOMAIN, {}});
                    }
                }
            } catch (HistoryError error) {
                return std::unexpected(HistoryFailure{error, {}});
            }
        }

        s.maintenance(work, &event);
        s.nextPublication();
        const auto &l = s.limits;
        const auto h = l.points;
        const auto rowCost = product(
            64, sum(sum(l.partitions, product(h, h)),
                    sum(product(sum(h, 1), product(l.viewSlots, sum(sum(l.viewSubscriptions.entries, l.pendingItems),
                                                                    sum(sum(s.frames.size(), h), 4)))),
                        sum(s.maxFields, 8))));
        spend(work, 0, product(event.records().size(), rowCost));
        std::size_t text = 0;

        for (const auto &r : event.records()) {
            text = sum(text, product(sum(l.partitions, 1), sum(r.symbol.size(), l.symbolUnitsPerPartition)));

            if (r.localId < 0 || !s.schema.recordContext(r.localId).matches(r.context)) {
                throw HistoryError::INVALID_CONTEXT;
            }
        }

        spend(work, 0, 0, product(8, text));
        auto &b = s.candidate();
        HistoryStage result{};
        result.admittedRecords = event.records().size();

        for (std::size_t n = 0; n < event.records().size(); ++n) {
            s.process(b, event.records()[n], identity.generation(), n, event.residentBytes(), work, result);
        }

        bool retainedInput = s.incoming(b, NONE, event.residentBytes());

        for (std::size_t v = 0; v < s.views.size(); ++v) {
            retainedInput |= s.incoming(b, v, event.residentBytes());
        }

        const auto peakBytes = sum(s.bytes, retainedInput ? event.residentBytes() : 0, HistoryError::RESOURCE_LIMIT);

        if (peakBytes > l.residentBytes - std::min(s.fixed, l.residentBytes)) {
            throw HistoryError::RESOURCE_LIMIT;
        }

        // Shared admission did not assume that a failed local view would release any old root.
        for (std::size_t v = 0; v < s.views.size(); ++v) {
            if (b.views[v].state == HistoryViewState::ACTIVE && b.views[v].failure) {
                b.views[v].state = HistoryViewState::FAILED;
                b.views[v].size = 0;
                const auto first = v * l.viewSubscriptions.entries;

                for (std::size_t i = first; i < first + l.viewSubscriptions.entries; ++i) {
                    b.parts[i] = Impl::ViewPart{};
                }

                const auto pending = v * l.pendingItems;

                for (std::size_t n = 0; n < l.pendingItems; ++n) {
                    b.pending[pending + n] = Impl::Pending{};
                }

                ++result.failedViews;
            }
        }

        if (result.failedViews) {
            s.trim(b);
        }

        if (!s.source.matches(s.source)) {
            s.source = std::move(identity);
        }

        s.freshness = HistoryState::CURRENT;
        s.commit(&event);

        return result;
    } catch (HistoryError e) {
        if (impl_) {
            impl_->abandon();

            if (e == HistoryError::RESOURCE_LIMIT || e == HistoryError::INVALID_INDEX) {
                impl_->invalidate();
                impl_->freshness = HistoryState::STALE;
                ++impl_->serial;
            }
        }

        return std::unexpected(failure(e));
    }
}

std::expected<std::optional<HistoryDelivery>, HistoryFailure> HistoryCollector::front(const HistoryViewToken &token,
                                                                                      HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        auto &s = *impl_;
        s.fresh();
        const auto v = s.view(token);
        // All read-only planning and materialization work precedes the first prior-scratch write.
        spend(work, 0, sum(s.fixedVisits(), product(8, s.maxFields)));
        auto selected = s.select(s.current(), v);

        if (!selected) {
            s.views[v].offer.reset();

            return std::optional<HistoryDelivery>{};
        }

        const auto &m = s.current().parts[selected->part];
        const auto &p = s.current().partitions[m.partition];
        const auto values = s.materialize(v, p.record, selected->value);
        s.views[v].offer = *selected;
        auto membership = m.membership;
        auto revision = m.revision;

        if (selected->local) {
            const auto &queued = s.current().pending[v * s.limits.pendingItems];
            membership = queued.membership;
            revision = queued.revision;
        }

        HistoryDelivery delivery{{p.record, s.schema.recordContext(p.record), s.symbol(s.current(), m.partition),
                                  selected->value.time, selected->flags, values},
                                 selected->kind,
                                 selected->value.raw,
                                 selected->value.trigger,
                                 std::move(membership),
                                 revision,
                                 HistoryEpochToken(s.identity, NONE, s.epoch)};

        return std::optional<HistoryDelivery>(std::move(delivery));
    } catch (HistoryError e) {
        return std::unexpected(failure(e));
    }
}

std::expected<void, HistoryFailure> HistoryCollector::pop(const HistoryViewToken &token, HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        auto &s = *impl_;
        s.fresh();
        const auto v = s.view(token);

        if (!s.views[v].offer || s.views[v].offer->serial != s.serial) {
            throw HistoryError::NO_OFFER;
        }

        s.maintenance(work);
        s.nextPublication();
        const auto selected = *s.views[v].offer;
        auto &b = s.candidate();
        auto &m = b.parts[selected.part];
        auto &vm = b.views[v];

        if (selected.local) {
            const auto base = v * s.limits.pendingItems;

            for (std::size_t n = 1; n < vm.size; ++n) {
                b.pending[base + n - 1] = std::move(b.pending[base + n]);
            }

            --vm.size;
            b.pending[base + vm.size] = Impl::Pending{};
            vm.balance = selected.balance ? selected.balance - 1 : 0;

            if (m.known == m.bound && !Impl::tx(b.partitions[m.partition]) &&
                s.lastPending(b, v, m.partition) == NONE) {
                m.dirty = false;
            }
        } else {
            m.known = selected.value.time;
            const auto [to, boundaryFlag] = s.boundary(b, selected.part);

            if ((selected.flags & SNIP) != 0 && selected.value.time == to) {
                m.known = m.bound;
            }

            vm.balance = selected.balance + 1;

            if (s.views[v].options.historySnapshot && Impl::tx(b.partitions[m.partition])) {
                s.dirty(b, v, selected.part);
            }

            if (selected.clearDirty) {
                m.dirty = false;
                const auto last = s.lastPending(b, v, m.partition);

                if (last != NONE) {
                    b.pending[last].flags &= ~TX;
                }
            }

            s.queue(b, v, selected.part);

            if (m.snapshotRemaining) {
                --m.snapshotRemaining;
            }

            if (m.queued && !m.snapshotRemaining) {
                if (vm.nextOrder == std::numeric_limits<std::uint64_t>::max()) {
                    throw HistoryError::IDENTITY_EXHAUSTED;
                }

                m.queueOrder = ++vm.nextOrder;
                m.snapshotRemaining = s.options.partitionSnapshotQuantum;
            }
        }

        s.commit();

        return {};
    } catch (HistoryError e) {
        if (impl_) {
            impl_->abandon();
        }

        return std::unexpected(failure(e));
    }
}

std::expected<HistoryRangeStatus, HistoryFailure> HistoryCollector::rangeStatus(const HistoryViewToken &token,
                                                                                const core::SubscriptionKey &key,
                                                                                HistoryWork &work) const {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        const auto &s = *impl_;
        const auto v = s.view(token, true);
        spend(work, 0, s.fixedVisits(),
              product(sum(s.limits.partitions, 1), sum(key.symbol.size(), s.limits.symbolUnitsPerPartition)));
        const core::SubscriptionEntry *entry = nullptr;

        for (const auto &e : s.views[v].desired->entries()) {
            if (e.key.context.matches(key.context) && e.key.symbol == key.symbol) {
                entry = &e;
                break;
            }
        }

        if (!entry) {
            throw HistoryError::INVALID_KEY;
        }

        const auto k = s.find(s.current(), s.record(key), key.symbol);
        const auto i = k == NONE ? NONE : s.part(s.current(), v, k);
        HistoryRangeStatus result{HistoryEpochToken(s.identity, NONE, s.epoch),
                                  entry->membership,
                                  s.views[v].desired->revision(),
                                  s.serial,
                                  0,
                                  *entry->historyBound,
                                  HIGH,
                                  HIGH,
                                  {},
                                  HistorySnapshotState::UNKNOWN,
                                  false,
                                  false,
                                  false,
                                  false};

        if (k != NONE) {
            const auto &p = s.current().partitions[k];
            result.partitionActivation = p.activation;
            result.sourceFrontier = p.frontier;
            result.knownFrontier = p.ever;

            if (p.snip != LOW) {
                result.snipFrontier = p.snip;
            }

            result.sourceTransactionPending = Impl::tx(p);

            if (p.mode) {
                result.snapshot = HistorySnapshotState::RECEIVING;

                if (p.began && p.ended && p.frontier <= result.requestedFrom) {
                    result.snapshot = HistorySnapshotState::COMPLETE;
                }

                if (p.began && p.ended && p.snip != LOW && p.snip > result.requestedFrom) {
                    result.snapshot = HistorySnapshotState::TRUNCATED;
                }
            }

            if (i != NONE) {
                result.initialPending = s.current().parts[i].queued;
                result.viewTransactionDirty = s.current().parts[i].dirty;
            }

            result.completeRequestedRange =
                s.freshness == HistoryState::CURRENT && s.current().views[v].state == HistoryViewState::ACTIVE &&
                p.mode && p.began && p.frontier <= result.requestedFrom &&
                (p.snip == LOW || p.snip <= result.requestedFrom) && !result.sourceTransactionPending &&
                !result.initialPending && !result.viewTransactionDirty && s.lastPending(s.current(), v, k) == NONE;
        }

        return result;
    } catch (HistoryError e) {
        return std::unexpected(failure(e));
    }
}

std::expected<HistoryRetentionReport, HistoryFailure> HistoryCollector::forgetUnobserved(HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        auto &s = *impl_;
        s.fresh();
        s.maintenance(work);
        s.nextPublication();
        auto &b = s.candidate();
        std::size_t forgotten = 0;

        for (std::size_t k = 0; k < b.partitions.size(); ++k) {
            if (!b.partitions[k].used || s.interested(b, k)) {
                continue;
            }

            bool referenced = false;

            for (const auto &p : b.points) {
                referenced |= p.used && p.partition == k;
            }

            for (std::size_t v = 0; v < s.views.size(); ++v) {
                for (std::size_t n = 0; n < b.views[v].size; ++n) {
                    referenced |= b.pending[v * s.limits.pendingItems + n].partition == k;
                }
            }

            if (!referenced) {
                b.partitions[k] = Impl::Partition{};
                ++forgotten;
            }
        }

        if (!forgotten) {
            s.clear(b);

            return HistoryRetentionReport{s.retentionGeneration, 0, false};
        }

        if (s.retentionGeneration == std::numeric_limits<std::uint64_t>::max()) {
            throw HistoryError::IDENTITY_EXHAUSTED;
        }

        ++s.retentionGeneration;
        s.commit();

        return HistoryRetentionReport{s.retentionGeneration, forgotten, true};
    } catch (HistoryError e) {
        if (impl_) {
            impl_->abandon();
        }

        return std::unexpected(failure(e));
    }
}

std::expected<HistoryResetNotice, HistoryFailure> HistoryCollector::reset(HistoryWork &work) {
    try {
        spend(work, 1);

        if (!impl_) {
            throw HistoryError::INVALID_STATE;
        }

        auto &s = *impl_;
        s.maintenance(work);
        s.nextPublication();

        if (s.epoch == std::numeric_limits<std::uint64_t>::max()) {
            throw HistoryError::IDENTITY_EXHAUSTED;
        }

        auto &b = s.candidate();
        std::size_t points = 0;
        std::size_t pending = 0;

        for (const auto &p : b.points) {
            points += p.used ? 1 : 0;
        }

        for (const auto &v : b.views) {
            pending = sum(pending, v.size);
        }

        const auto partitionGeneration = b.nextPartition;
        // Preserve view lifecycle/core desired ownership while retiring every source/payload cursor.
        std::fill(b.partitions.begin(), b.partitions.end(), Impl::Partition{});
        std::fill(b.points.begin(), b.points.end(), Impl::Point{});
        std::fill(b.parts.begin(), b.parts.end(), Impl::ViewPart{});
        std::fill(b.pending.begin(), b.pending.end(), Impl::Pending{});
        b.nextPartition = partitionGeneration;

        for (std::size_t v = 0; v < s.views.size(); ++v) {
            b.views[v].size = 0;
            b.views[v].balance = 0;
            b.views[v].nextOrder = 0;

            if (b.views[v].state == HistoryViewState::ACTIVE) {
                s.reconcile(b, v);
            }
        }

        HistoryResetNotice notice{s.epoch + 1,
                                  HistoryEpochToken(s.identity, NONE, s.epoch),
                                  HistoryEpochToken(s.identity, NONE, s.epoch + 1),
                                  points,
                                  pending,
                                  0,
                                  true,
                                  false};

        if (s.notice) {
            notice.from = s.notice->from;
            notice.countsSaturated = s.notice->countsSaturated;
            notice.discardedPoints = s.notice->discardedPoints;
            notice.discardedPending = s.notice->discardedPending;
            notice.skippedResets = s.notice->skippedResets;
            count(notice.discardedPoints, points, notice.countsSaturated);
            count(notice.discardedPending, pending, notice.countsSaturated);
            count(notice.skippedResets, 1, notice.countsSaturated);
        }

        ++s.epoch;
        s.source = {};
        s.freshness = HistoryState::RESET_PENDING;
        s.notice = notice;
        s.commit();

        return notice;
    } catch (HistoryError e) {
        if (impl_) {
            impl_->abandon();
        }

        return std::unexpected(failure(e));
    }
}

std::expected<void, HistoryFailure> HistoryCollector::acknowledgeReset(std::uint64_t sequence) noexcept {
    if (!impl_) {
        return std::unexpected(failure(HistoryError::INVALID_STATE));
    }

    if (!impl_->notice || impl_->notice->sequence != sequence || impl_->freshness != HistoryState::RESET_PENDING) {
        return std::unexpected(failure(HistoryError::INVALID_STATE));
    }

    if (impl_->serial == std::numeric_limits<std::uint64_t>::max()) {
        return std::unexpected(failure(HistoryError::IDENTITY_EXHAUSTED));
    }

    // Reset already invalidated every offer; this allocation-free barrier changes only fixed scalars/tokens.
    impl_->notice.reset();
    impl_->freshness = HistoryState::EMPTY;
    ++impl_->serial;

    return {};
}

const core::LocalSchema *HistoryCollector::schema() const noexcept {
    return impl_ ? &impl_->schema : nullptr;
}

const core::SubscriptionState *HistoryCollector::desired(const HistoryViewToken &token) const noexcept {
    if (!impl_) {
        return nullptr;
    }

    try {
        return &*impl_->views[impl_->view(token, true)].desired;
    } catch (HistoryError) {
        return nullptr;
    }
}

HistoryState HistoryCollector::state() const noexcept {
    return impl_ ? impl_->freshness : HistoryState::EMPTY;
}

HistoryViewState HistoryCollector::viewState(const HistoryViewToken &token) const noexcept {
    if (!impl_) {
        return HistoryViewState::INVALID;
    }

    try {
        return impl_->current().views[impl_->view(token, true)].state;
    } catch (HistoryError) {
        return HistoryViewState::INVALID;
    }
}

std::optional<HistoryViewFailure> HistoryCollector::viewFailure(const HistoryViewToken &token) const noexcept {
    if (!impl_) {
        return {};
    }

    try {
        return impl_->current().views[impl_->view(token, true)].failure;
    } catch (HistoryError) {
        return {};
    }
}

std::size_t HistoryCollector::partitionCount() const noexcept {
    return impl_ ? impl_->cachedPartitions : 0;
}

std::size_t HistoryCollector::pointCount() const noexcept {
    return impl_ ? impl_->cachedPoints : 0;
}

std::size_t HistoryCollector::pendingCount(const HistoryViewToken &token) const noexcept {
    if (!impl_) {
        return 0;
    }

    try {
        return impl_->current().views[impl_->view(token, true)].size;
    } catch (HistoryError) {
        return 0;
    }
}

std::size_t HistoryCollector::frameCount() const noexcept {
    return impl_ ? impl_->owners : 0;
}

std::size_t HistoryCollector::eventBytes() const noexcept {
    return impl_ ? impl_->bytes : 0;
}

std::size_t HistoryCollector::residentBytes() const noexcept {
    return impl_ ? impl_->fixed + impl_->bytes : 0;
}

std::optional<HistoryResetNotice> HistoryCollector::resetNotice() const noexcept {
    return impl_ ? impl_->notice : std::nullopt;
}

} // namespace org::ttldtor::qdcxx::collectors
