// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/mux/queue.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::mux {
namespace {
constexpr std::uint64_t INITIAL_QUEUE_EPOCH = 1; ///< Initial unbound local source generation.

/** Check a requested sum before allocation or counter publication.
 * @param a First nonnegative quantity.
 * @param b Second quantity.
 * @return Exact representable sum or throw a typed local configuration error.
 */
std::size_t sum(std::size_t a, std::size_t b) {
    if (b > std::numeric_limits<std::size_t>::max() - a) {
        throw QueueError::INVALID_CONFIGURATION;
    }

    return a + b;
}

/** Check requested multiplication.
 * @param a First quantity.
 * @param b Second quantity.
 * @return Product or typed configuration failure.
 */
std::size_t product(std::size_t a, std::size_t b) {
    if (a && b > std::numeric_limits<std::size_t>::max() / a) {
        throw QueueError::INVALID_CONFIGURATION;
    }

    return a * b;
}

/** Debit one independent work axis.
 * @param credit Remaining caller credits.
 * @param amount Conservative requested work.
 */
void debit(std::size_t &credit, std::size_t amount) {
    if (credit < amount) {
        throw QueueError::WORK_LIMIT;
    }

    credit -= amount;
}

/** Preflight all local axes before a bounded action.
 * @param w Remaining owner turn.
 * @param visits Metadata visits.
 * @param fields Field visits.
 * @param text Exact UTF16 examination/copy allowance.
 * @param bytes Backing/rewrite/disposal allowance.
 */
void spend(QueueWork &w, std::size_t visits = 0, std::size_t fields = 0, std::size_t text = 0, std::size_t bytes = 0) {
    debit(w.visits, visits);
    debit(w.fields, fields);
    debit(w.textUnits, text);
    debit(w.bytes, bytes);
}

/** Preserve a typed source failure.
 * @param error Actual component error.
 * @return WORK yield or local source failure with the exact nested category.
 */
template <class Error> QueueFailure sourceFailure(Error error) {
    return {error == Error::WORK_LIMIT ? QueueError::WORK_LIMIT : QueueError::SOURCE_FAILURE, QueueSourceError{error}};
}

/** Increment diagnostics without wrapping resource counters.
 * @param value Diagnostic target.
 * @param amount Increment.
 * @param saturated Saturation evidence.
 */
void count(std::size_t &value, std::size_t amount, bool &saturated) noexcept {
    if (amount > std::numeric_limits<std::size_t>::max() - value) {
        value = std::numeric_limits<std::size_t>::max();
        saturated = true;
    } else {
        value += amount;
    }
}

/** Atomically reserve an independently bounded quantity.
 * @param value Current charge.
 * @param amount New charge.
 * @param maximum Immutable maximum.
 * @return Whether the complete charge was reserved.
 */
bool reserve(std::atomic<std::size_t> &value, std::size_t amount, std::size_t maximum) noexcept {
    auto current = value.load(std::memory_order_relaxed);

    for (;;) {
        if (current > maximum || amount > maximum - current) {
            return false;
        }

        if (value.compare_exchange_weak(current, current + amount, std::memory_order_acq_rel)) {
            return true;
        }
    }
}
} // namespace

namespace detail {
/** Shared allocation shape, measured through the actual rebound allocator. */
enum class ControlKind : std::size_t {
    ORIGIN,     ///< Opaque origin control/object request.
    PORT,       ///< Finite producer port control/object request.
    COMPLETION, ///< Pure result control/object request.
    RECORD,     ///< Immutable record control/object request.
    BATCH,      ///< Immutable composite control/object request.
    COUNT,      ///< Finite numeric layout array extent.
};
/** Exact allocation resource domain, never an execution-owner pointer. */
enum class AllocationTag {
    OWNER,       ///< Queue owner fixed metadata/object/array request.
    CLIENT,      ///< Generation fixed metadata/object/array request.
    ORIGIN,      ///< Opaque origin shared request.
    PORT,        ///< Producer port shared request.
    COMPLETION,  ///< Detached completion shared request.
    RECORD,      ///< One record owner plus its control/object bytes.
    ARENA,       ///< Record arena bytes without a second owner count.
    BATCH,       ///< One composite owner plus its control/object bytes.
    BATCH_ARRAY, ///< Composite root/view bytes without a second owner count.
};

/** Fixed numeric request transport; no backing/object construction has occurred. */
struct Measurement {
    std::size_t bytes; ///< Exact rebound count times element size.
};

/** Scalar-only aggregate ledger; no callbacks, payloads or source owners. */
struct ContextLedger {
    std::atomic<std::size_t> references{1};                ///< Intrusive pure metadata leases.
    QueueLimits limits{};                                  ///< Immutable aggregate maxima.
    std::size_t fixedCapacity = 0;                         ///< Complete preflighted owner request sum.
    std::atomic<unsigned> stop{0};                         ///< Reserved aggregate admission cutoff.
    std::atomic<std::size_t> fixed{sizeof(ContextLedger)}; ///< Actual requested owner blocks, including this object.
    std::atomic<std::size_t> reservedBytes{sizeof(ContextLedger)}; ///< Owner blocks plus complete generation maxima.
    std::atomic<std::size_t> generations{0};         ///< Active or caller-retained retired generation reservations.
    std::atomic<std::size_t> reservedItems{0};       ///< Sum of immutable per-generation record slot maxima.
    std::atomic<std::size_t> reservedRecordBytes{0}; ///< Sum of immutable per-generation record byte maxima.
    std::atomic<std::size_t> reservedBatchOwners{0}; ///< Sum of immutable per-generation composite owner maxima.
    std::atomic<std::size_t> reservedBatchBytes{0};  ///< Sum of immutable per-generation composite byte maxima.
    std::atomic<std::size_t> reservedCommands{0};    ///< Finite port capacities.
    std::atomic<std::size_t> reservedResults{0};     ///< Finite port capacities.
    std::atomic<std::size_t> reservedControls{0};    ///< Finite port capacities.
    std::atomic<std::size_t> records{0};             ///< Actual requested live blocks.
    std::atomic<std::size_t> recordBytes{0};         ///< Complete charged immutable record object/arena bytes.
    std::atomic<std::size_t> batches{0};             ///< Actual requested live blocks.
    std::atomic<std::size_t> batchBytes{0};  ///< Complete charged composite object/reference/view capacity bytes.
    std::atomic<std::size_t> clientFixed{0}; ///< Actual requested live blocks.
    std::array<std::size_t, static_cast<std::size_t>(ControlKind::COUNT)>
        controlBytes{};                      ///< Qualified actual request shapes.
    std::optional<ControlKind> constructing; ///< Owner-serialized numeric single-request permit; no operation object.
    bool requestUsed = false;                ///< Reject a second request before its allocation.
};

/** Retain a finite internal ledger reference without wrapping.
 * @param value Existing positive reference count.
 */
void retainLedger(std::atomic<std::size_t> &value) noexcept {
    auto n = value.load();

    for (;;) {
        if (!n || n == std::numeric_limits<std::size_t>::max())
            std::terminate();

        if (value.compare_exchange_weak(n, n + 1))
            return;
    }
}

/** Pure counter lease, safe to destroy on a producer thread. */
class ContextLease {
    ContextLedger *value_ = nullptr; ///< Counter metadata only.
    public:
    /** Construct an empty lease. */
    ContextLease() noexcept = default;

    /** Adopt one newly allocated ledger reference.
     * @param value New ledger, initially referenced once.
     * @return Owning lease without allocation.
     */
    static ContextLease adopt(ContextLedger *value) noexcept {
        ContextLease r;
        r.value_ = value;

        return r;
    }

    /** Copy an independent metadata reference.
     * @param other Existing protected wrapper.
     */
    ContextLease(const ContextLease &other) noexcept : value_(other.value_) {
        if (value_)
            retainLedger(value_->references);
    }

    /** Transfer a metadata reference.
     * @param other Source made empty.
     */
    ContextLease(ContextLease &&other) noexcept : value_(std::exchange(other.value_, nullptr)) {
    }

    /** Replace one metadata reference.
     * @param other Existing wrapper.
     * @return This lease.
     */
    ContextLease &operator=(const ContextLease &other) noexcept {
        ContextLease copy(other);
        std::swap(value_, copy.value_);

        return *this;
    }

    /** Transfer one metadata reference.
     * @param other Source made empty unless self-assigned.
     * @return This lease.
     */
    ContextLease &operator=(ContextLease &&other) noexcept {
        if (this != &other) {
            ContextLease moved(std::move(other));
            std::swap(value_, moved.value_);
        }

        return *this;
    }

    /** Free scalar metadata only after its last allocation-holding lease releases. */
    ~ContextLease() {
        if (value_ && value_->references.fetch_sub(1) == 1)
            delete value_;
    }

    /** Inspect detached metadata.
     * @return Existing ledger, or null.
     */
    ContextLedger *operator->() const noexcept {
        return value_;
    }

    /** Inspect the nonempty ledger.
     * @return Existing ledger reference.
     */
    ContextLedger &operator*() const noexcept {
        return *value_;
    }

    /** Test lease validity.
     * @return Whether a ledger is retained.
     */
    explicit operator bool() const noexcept {
        return value_ != nullptr;
    }

    /** Compare exact detached scope.
     * @param other Another lease.
     * @return Same backing, including two empty leases.
     */
    bool operator==(const ContextLease &other) const noexcept {
        return value_ == other.value_;
    }
};

/** One generation's pure limits/reservations, independent of callable and producer ports. */
struct ClientBudget {
    std::atomic<std::size_t> references{1};                   ///< Intrusive allocation-holding leases.
    ContextLease context;                                     ///< Aggregate counter lifetime only.
    ClientLimits limits{};                                    ///< Immutable generation maxima.
    std::size_t fixedReservation = 0;                         ///< Complete fixed/maximum reservation.
    std::size_t reservation = 0;                              ///< Complete fixed/maximum reservation.
    std::atomic<std::size_t> fixedLive{sizeof(ClientBudget)}; ///< Actual requested fixed blocks, including this object.
    std::atomic<std::size_t> records{0};                      ///< Actual independent live scopes.
    std::atomic<std::size_t> recordBytes{0};                  ///< Complete charged immutable record object/arena bytes.
    std::atomic<std::size_t> batches{0};                      ///< Actual independent live scopes.
    std::atomic<std::size_t> batchBytes{0}; ///< Complete charged composite object/reference/view capacity bytes.
    bool charged = false;                   ///< Full public generation was activated.
};

/** Free a pure generation after all its actual blocks were deallocated.
 * @param value Detached generation, or null.
 */
void releaseBudget(ClientBudget *value) noexcept {
    if (!value || value->references.fetch_sub(1) != 1)
        return;

    auto context = value->context;
    const auto limits = value->limits;
    const auto reservation = value->reservation;
    const bool charged = value->charged;
    delete value;
    context->clientFixed.fetch_sub(sizeof(ClientBudget));

    if (!charged)
        return;

    context->generations.fetch_sub(1);
    context->reservedBytes.fetch_sub(reservation);
    context->reservedItems.fetch_sub(limits.dataItems);
    context->reservedRecordBytes.fetch_sub(limits.recordBytes);
    context->reservedBatchOwners.fetch_sub(limits.batchOwners);
    context->reservedBatchBytes.fetch_sub(limits.batchBytes);
    context->reservedCommands.fetch_sub(limits.commandSlots);
    context->reservedResults.fetch_sub(limits.resultSlots);
    context->reservedControls.fetch_sub(2);
}

/** Intrusive pure generation lease; never retains a source/callback/owner. */
class BudgetLease {
    ClientBudget *value_ = nullptr; ///< Scalar accounting only.
    public:
    /** Construct an empty lease. */
    BudgetLease() noexcept = default;

    /** Adopt one newly allocated generation reference.
     * @param value New ledger with one initial reference.
     * @return Owning pure lease.
     */
    static BudgetLease adopt(ClientBudget *value) noexcept {
        BudgetLease r;
        r.value_ = value;

        return r;
    }

    /** Retain one independent generation lease.
     * @param other Existing protected wrapper.
     */
    BudgetLease(const BudgetLease &other) noexcept : value_(other.value_) {
        if (value_)
            retainLedger(value_->references);
    }

    /** Transfer one lease.
     * @param other Source made empty.
     */
    BudgetLease(BudgetLease &&other) noexcept : value_(std::exchange(other.value_, nullptr)) {
    }

    /** Replace a lease.
     * @param other Existing protected wrapper.
     * @return This lease.
     */
    BudgetLease &operator=(const BudgetLease &other) noexcept {
        BudgetLease copy(other);
        std::swap(value_, copy.value_);

        return *this;
    }

    /** Transfer a lease.
     * @param other Source made empty unless self-assigned.
     * @return This lease.
     */
    BudgetLease &operator=(BudgetLease &&other) noexcept {
        if (this != &other) {
            BudgetLease moved(std::move(other));
            std::swap(value_, moved.value_);
        }

        return *this;
    }

    /** Release only pure bounded metadata. */
    ~BudgetLease() {
        releaseBudget(value_);
    }

    /** Inspect pure generation counters.
     * @return Ledger, or null.
     */
    ClientBudget *operator->() const noexcept {
        return value_;
    }

    /** Inspect a nonempty generation.
     * @return Ledger reference.
     */
    ClientBudget &operator*() const noexcept {
        return *value_;
    }

    /** Test lease validity.
     * @return Whether a generation is retained.
     */
    explicit operator bool() const noexcept {
        return value_ != nullptr;
    }

    /** Compare exact generation storage.
     * @param other Another lease.
     * @return Same scalar ledger.
     */
    bool operator==(const BudgetLease &other) const noexcept {
        return value_ == other.value_;
    }
};

/** Immutable allocator provenance, containing only pure metadata leases. */
struct AllocationScope {
    ContextLease owner;                       ///< Used for owner-only fixed blocks.
    BudgetLease client;                       ///< Used for generation/data blocks.
    AllocationTag tag = AllocationTag::OWNER; ///< Exact requested-block domain.

    /** Inspect the retained aggregate counter domain.
     * @return Existing pure context.
     */
    ContextLease context() const noexcept {
        return client ? client->context : owner;
    }
};

/** Translate one control request kind.
 * @param tag Exact allocation tag.
 * @return Shared owner kind, or empty for arrays/unique objects.
 */
std::optional<ControlKind> controlKind(AllocationTag tag) noexcept {
    switch (tag) {
    case AllocationTag::ORIGIN:
        return ControlKind::ORIGIN;
    case AllocationTag::PORT:
        return ControlKind::PORT;
    case AllocationTag::COMPLETION:
        return ControlKind::COMPLETION;
    case AllocationTag::RECORD:
        return ControlKind::RECORD;
    case AllocationTag::BATCH:
        return ControlKind::BATCH;
    default:
        return {};
    }
}

/** Admit/release actual standard allocation requests without private ABI-size guesses.
 * @tparam T Actual rebound allocation element selected by the standard library.
 */
template <class T> struct QueueAllocator {
    using value_type = T;                                          ///< Standard allocator element type.
    using propagate_on_container_move_assignment = std::true_type; ///< Preserve exact block ownership during moves.
    AllocationScope scope;                                         ///< Pure counter leases only.
    bool measuring = false;                                        ///< Construction-only no-backing request discovery.
    /** Construct an empty allocator for inactive containers. */
    QueueAllocator() noexcept = default;

    /** Bind exact allocation provenance.
     * @param source Pure scoped counters.
     */
    explicit QueueAllocator(AllocationScope source) noexcept : scope(std::move(source)) {
    }

    /** Preserve exact scope across standard allocator rebinding.
     * @tparam U Original allocation element.
     * @param other Standard allocator copy.
     */
    template <class U>
    QueueAllocator(const QueueAllocator<U> &other) noexcept : scope(other.scope), measuring(other.measuring) {
    }

    /** Allocate exact requested element backing after hard admission.
     * @param count Actual requested element count.
     * @return Owned aligned storage, or typed refusal/bad_alloc without publishing an object.
     */
    T *allocate(std::size_t count) {
        const auto bytes = product(count, sizeof(T));

        if (measuring)
            throw Measurement{bytes};

        auto context = scope.context();

        if (!context)
            throw QueueError::INVALID_CONFIGURATION;

        if (const auto kind = controlKind(scope.tag)) {
            if (context->constructing != kind || context->requestUsed ||
                bytes != context->controlBytes[static_cast<std::size_t>(*kind)])
                throw QueueError::INVALID_CONFIGURATION;

            context->requestUsed = true;
        }

        auto *budget = scope.client ? &*scope.client : nullptr;
        bool ownerCount = false;

        if (scope.tag == AllocationTag::OWNER || scope.tag == AllocationTag::ORIGIN) {
            if (sum(context->fixed.load(), bytes) > context->fixedCapacity)
                throw QueueError::RESOURCE_LIMIT;

            if (!reserve(context->reservedBytes, bytes, context->limits.residentBytes))
                throw QueueError::RESOURCE_LIMIT;

            context->fixed.fetch_add(bytes);
        } else if (scope.tag == AllocationTag::CLIENT || scope.tag == AllocationTag::PORT ||
                   scope.tag == AllocationTag::COMPLETION) {
            const auto next = sum(budget->fixedLive.load(), bytes);

            if (next > budget->fixedReservation)
                throw QueueError::RESOURCE_LIMIT;

            budget->fixedLive.fetch_add(bytes);
            context->clientFixed.fetch_add(bytes);
        } else {
            const bool record = scope.tag == AllocationTag::RECORD || scope.tag == AllocationTag::ARENA;
            ownerCount = scope.tag == AllocationTag::RECORD || scope.tag == AllocationTag::BATCH;
            auto &owners = record ? budget->records : budget->batches;
            auto &charge = record ? budget->recordBytes : budget->batchBytes;
            const auto maximum = record ? budget->limits.recordBytes : budget->limits.batchBytes;
            const auto countMaximum = record ? budget->limits.dataItems : budget->limits.batchOwners;

            if (ownerCount && !reserve(owners, 1, countMaximum))
                throw QueueError::RESOURCE_LIMIT;

            if (!reserve(charge, bytes, maximum)) {
                if (ownerCount)
                    owners.fetch_sub(1);

                throw QueueError::RESOURCE_LIMIT;
            }

            (record ? context->recordBytes : context->batchBytes).fetch_add(bytes);

            if (ownerCount)
                (record ? context->records : context->batches).fetch_add(1);
        }

        try {
            if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
                return static_cast<T *>(::operator new(bytes, std::align_val_t{alignof(T)}));
            else
                return static_cast<T *>(::operator new(bytes));
        } catch (...) {
            uncharge(scope, bytes, ownerCount);
            throw;
        }
    }

    /** Release backing before publishing reusable capacity.
     * @param pointer Original actual allocation.
     * @param count Original element request.
     */
    void deallocate(T *pointer, std::size_t count) noexcept {
        auto retained = scope;
        const auto bytes = count * sizeof(T);

        if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            ::operator delete(pointer, std::align_val_t{alignof(T)});
        else
            ::operator delete(pointer);

        uncharge(retained, bytes, retained.tag == AllocationTag::RECORD || retained.tag == AllocationTag::BATCH);
    }

    /** Compare interchangeable allocation provenance.
     * @tparam U Other standard element.
     * @param other Allocator to compare.
     * @return Same exact scope and tag.
     */
    template <class U> bool operator==(const QueueAllocator<U> &other) const noexcept {
        return scope.tag == other.scope.tag && scope.owner == other.scope.owner && scope.client == other.scope.client &&
               measuring == other.measuring;
    }

    private:
    /** Release exact already-admitted block counters after actual deletion or a failed request.
     * @param retained Pure scope retained through underlying storage disposal.
     * @param bytes Exact original allocation request.
     * @param ownerCount Whether this request owns one logical record/batch.
     */
    static void uncharge(const AllocationScope &retained, std::size_t bytes, bool ownerCount) noexcept {
        auto context = retained.context();
        auto *budget = retained.client ? &*retained.client : nullptr;

        if (retained.tag == AllocationTag::OWNER || retained.tag == AllocationTag::ORIGIN) {
            context->fixed.fetch_sub(bytes);
            context->reservedBytes.fetch_sub(bytes);
        } else if (retained.tag == AllocationTag::CLIENT || retained.tag == AllocationTag::PORT ||
                   retained.tag == AllocationTag::COMPLETION) {
            budget->fixedLive.fetch_sub(bytes);
            context->clientFixed.fetch_sub(bytes);
        } else {
            const bool record = retained.tag == AllocationTag::RECORD || retained.tag == AllocationTag::ARENA;
            (record ? budget->recordBytes : budget->batchBytes).fetch_sub(bytes);
            (record ? context->recordBytes : context->batchBytes).fetch_sub(bytes);

            if (ownerCount) {
                (record ? budget->records : budget->batches).fetch_sub(1);
                (record ? context->records : context->batches).fetch_sub(1);
            }
        }
    }
};
/** Accounted standard contiguous storage.
 * @tparam T Exact element type.
 */
template <class T> using Vector = std::vector<T, QueueAllocator<T>>;

/** Pure fixed owner allocation scope.
 * @param context Existing scalar ledger.
 * @param tag Exact fixed/control tag.
 * @return Allocation provenance without owner/source pointers.
 */
AllocationScope ownerScope(ContextLease context, AllocationTag tag = AllocationTag::OWNER) noexcept {
    return {std::move(context), {}, tag};
}

/** Pure generation/data allocation scope.
 * @param budget Existing scalar generation ledger.
 * @param tag Exact request domain.
 * @return Allocation provenance without callback/protocol pointers.
 */
AllocationScope clientScope(BudgetLease budget, AllocationTag tag = AllocationTag::CLIENT) noexcept {
    return {{}, std::move(budget), tag};
}

/** Construct one shared allocation with a qualified single actual request.
 * @tparam T Complete shared object.
 * @tparam Args Exact constructor argument types.
 * @param scope Pure request scope.
 * @param args Object constructor inputs.
 * @return Complete shared object, or typed refusal before publication.
 */
template <class T, class... Args> std::shared_ptr<T> shared(AllocationScope scope, Args &&...args) {
    auto context = scope.context();
    const auto kind = controlKind(scope.tag);

    if (!context || !kind || context->constructing)
        throw QueueError::INVALID_CONFIGURATION;

    context->constructing = kind;
    context->requestUsed = false;

    /** Clear the construction-only numeric permit on success and every exception. */
    struct Permit {
        ContextLease context; ///< Pure counter lifetime only.

        ~Permit() {
            context->constructing.reset();
            context->requestUsed = false;
        }
    } permit{context};

    auto result = std::allocate_shared<T>(QueueAllocator<T>{std::move(scope)}, std::forward<Args>(args)...);

    if (!context->requestUsed)
        throw QueueError::INVALID_CONFIGURATION;

    return result;
}

/** Discover an actual request before allocation/object construction.
 * @tparam T Shared object type.
 * @tparam Args Actual constructor argument types, identical to the allocation path.
 * @param args Dummy pure inputs; no object constructor executes.
 * @return Actual rebound requested bytes, or unsupported configuration.
 */
template <class T, class... Args> std::size_t measureShared(Args &&...args) {
    QueueAllocator<T> allocator;
    allocator.measuring = true;
    try {
        auto value = std::allocate_shared<T>(allocator, std::forward<Args>(args)...);
        (void)value;
    } catch (const Measurement &m) {
        return m.bytes;
    }
    throw QueueError::INVALID_CONFIGURATION;
}

/** Free a complete typed unique object/array through the same exact allocator.
 * @tparam T Stored element type.
 */
template <class T> struct BlockDelete {
    AllocationScope scope; ///< Pure counters, never an execution owner.
    std::size_t count = 0; ///< Exact original requested elements.
    bool objects = false;  ///< Whether elements require destructor calls.

    /** Release elements and their actual backing before returning capacity.
     * @param pointer Original requested block.
     */
    void operator()(T *pointer) const noexcept {
        if (objects)
            for (std::size_t n = count; n; n--)
                std::destroy_at(pointer + n - 1);

        QueueAllocator<T>{scope}.deallocate(pointer, count);
    }
};
/** Accounted unique library object.
 * @tparam T Exact object type.
 */
template <class T> using Object = std::unique_ptr<T, BlockDelete<T>>;

/** Construct one accounted unique object.
 * @tparam T Exact complete object.
 * @tparam Args Constructor inputs.
 * @param scope Exact request domain.
 * @param args Constructor arguments.
 * @return Complete object or atomic request rollback.
 */
template <class T, class... Args> Object<T> object(AllocationScope scope, Args &&...args) {
    QueueAllocator<T> allocator{scope};
    auto *pointer = allocator.allocate(1);
    try {
        std::construct_at(pointer, std::forward<Args>(args)...);
    } catch (...) {
        allocator.deallocate(pointer, 1);
        throw;
    }

    return Object<T>{pointer, BlockDelete<T>{std::move(scope), 1, true}};
}

/** Opaque origin; allocator retains pure counters only. */
struct QueueOrigin {};

/** Independently retained bounded completion; no command port or callable. */
struct QueueCompletion {
    BudgetLease budget;                     ///< Fixed generation lifetime only.
    mutable std::mutex mutex;               ///< Numeric result publication.
    std::uint64_t id = 0;                   ///< Immutable throughout a caller ticket lifetime.
    std::optional<MuxCommandResult> result; ///< Empty until application/cancellation.

    /** Bind pure generation lifetime.
     * @param source Detached generation.
     */
    explicit QueueCompletion(BudgetLease source) : budget(std::move(source)) {
    }
};
/** Bounded copied command kind. */
enum class QueueCommandKind {
    ADD,     ///< Upsert exact authoritative interests.
    REMOVE,  ///< Retire exact authoritative keys.
    REPLACE, ///< Replace the entire authoritative desired map.
    CLEAR,   ///< Retire every authoritative interest.
};

/** Preallocated copied command slot; no protocol or callback ownership. */
struct QueueCommand {
    QueueCommandKind kind = QueueCommandKind::CLEAR; ///< Exact desired operation.
    Vector<core::SubscriptionRequest> requests;      ///< Fixed request metadata.
    Vector<core::SubscriptionKey> keys;              ///< Fixed removal metadata.
    Vector<char16_t> text;                           ///< Exact fixed UTF16 copied storage.
    std::size_t count = 0;                           ///< Initialized input prefix.
    std::shared_ptr<QueueCompletion> completion;     ///< Pure finite result.
    bool occupied = false;                           ///< Accepted or executing command.

    /** Bind all storage to one fixed generation.
     * @param budget Pure resource scope.
     */
    explicit QueueCommand(BudgetLease budget)
        : requests(QueueAllocator<core::SubscriptionRequest>{clientScope(budget)}),
          keys(QueueAllocator<core::SubscriptionKey>{clientScope(budget)}),
          text(QueueAllocator<char16_t>{clientScope(budget)}) {
    }
};

/** Producer-safe finite command/control port; no callable/source/owner. */
struct QueuePort {
    BudgetLease budget;                               ///< Generation maxima and pure counters.
    std::shared_ptr<const QueueOrigin> origin;        ///< Detached exact origin.
    std::size_t slot = 0;                             ///< Fixed internal slot, not sufficient public identity.
    std::uint64_t generation = 0;                     ///< Non-reused public generation.
    Vector<core::RecordContext> contexts;             ///< Fixed pure exact producer validation.
    Vector<QueueCommand> commands;                    ///< Fixed finite ring.
    Vector<std::shared_ptr<QueueCompletion>> results; ///< Independently retained finite results.
    mutable std::mutex mutex;                         ///< Never held through user callable destruction/invocation.
    std::size_t head = 0;                             ///< Ordinary ring range.
    std::size_t tail = 0;                             ///< Ordinary ring range.
    std::size_t size = 0;                             ///< Ordinary ring range.
    std::uint64_t nextCommand = 1;                    ///< Non-reused command identity.
    std::atomic<std::uint64_t> publicOwnership{1};    ///< High bit permanently retires public acquisition.
    std::atomic<unsigned> stop{0};                    ///< Monotonic reserved stop.
    std::atomic<bool> resume{false};                  ///< Coalesced reserved resume.
    std::atomic<std::uint64_t> ack{0};                ///< Exact reset acknowledgment.
    std::atomic<std::uint64_t> advertisedNotice{0};   ///< Exact reset acknowledgment.
    std::atomic<QueueClientState> state{QueueClientState::SYNC_PENDING}; ///< Public local phase.
    std::optional<QueueResetNotice> notice;                              ///< Reserved independent reset interval.
    std::optional<QueueTerminal> terminal;                               ///< Reserved final immutable result.

    /** Initialize empty allocator-aware fixed containers.
     * @param source Pure generation scope.
     */
    explicit QueuePort(BudgetLease source)
        : budget(source), contexts(QueueAllocator<core::RecordContext>{clientScope(source)}),
          commands(QueueAllocator<QueueCommand>{clientScope(source)}),
          results(QueueAllocator<std::shared_ptr<QueueCompletion>>{clientScope(source)}) {
    }
};

/** Immutable detached record; no source field span escapes. */
struct QueueRecord {
    BudgetLease budget;                                         ///< Pure scope, never protocol/callable ownership.
    std::unique_ptr<std::byte[], BlockDelete<std::byte>> arena; ///< Full exact accounted requested arena.
    std::size_t arenaSize = 0;                                  ///< Whole requested arena capacity.
    MuxRecordView view;                                         ///< Pure detached immutable projection.

    /** Bind the eventual arena deallocator without allocating data.
     * @param source Pure record accounting.
     */
    explicit QueueRecord(BudgetLease source)
        : budget(source), arena(nullptr, BlockDelete<std::byte>{clientScope(source, AllocationTag::ARENA), 0, false}) {
    }
};

/** Composite immutable backing; records are shared without payload recopying. */
struct QueueBatch {
    BudgetLease budget;                               ///< Pure generation resource scope.
    Vector<std::shared_ptr<const QueueRecord>> roots; ///< Exact accounted record references.
    Vector<MuxRecordView> views;                      ///< Exact accounted immutable callback metadata.

    /** Bind empty arrays to the batch byte domain.
     * @param source Pure generation scope.
     */
    explicit QueueBatch(BudgetLease source)
        : budget(source),
          roots(QueueAllocator<std::shared_ptr<const QueueRecord>>{clientScope(source, AllocationTag::BATCH_ARRAY)}),
          views(QueueAllocator<MuxRecordView>{clientScope(source, AllocationTag::BATCH_ARRAY)}) {
    }
};

/** Record identity table entry; full schema ownership stays in actual existing components. */
struct ContextEntry {
    std::int32_t id;             ///< Exact caller local record ID.
    core::RecordContext context; ///< Pure owning exact provenance, independent of caller schema lifetime.
};

/** Measure actual five qualified shared request shapes, with no backing or object construction.
 * @return Numeric request sizes only; no constructor/work pointer survives.
 */
auto controlLayout() {
    BudgetLease empty;

    return std::array<std::size_t, static_cast<std::size_t>(ControlKind::COUNT)>{
        measureShared<QueueOrigin>(), measureShared<QueuePort>(empty), measureShared<QueueCompletion>(empty),
        measureShared<QueueRecord>(empty), measureShared<QueueBatch>(empty)};
}
} // namespace detail

namespace {
/** Non-reusable high bit of public-reference retirement. */
constexpr std::uint64_t PUBLIC_RETIRED = std::uint64_t{1} << 63;

/** Retain an already existing public reference.
 * @param port Existing source's port; copying its same object concurrently is unsupported.
 */
void retainPublic(const std::shared_ptr<detail::QueuePort> &port) noexcept {
    if (!port) {
        return;
    }

    auto current = port->publicOwnership.load(std::memory_order_relaxed);

    for (;;) {
        if (!current || (current & PUBLIC_RETIRED) || current == PUBLIC_RETIRED - 1) {
            std::terminate();
        }

        if (port->publicOwnership.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel)) {
            return;
        }
    }
}

/** Record monotonic allocation-free stop.
 * @param port Detached producer port.
 * @param request Graceful1 or abort2.
 * @return Whether this request changed the fixed control.
 */
bool stop(detail::QueuePort &port, unsigned request) noexcept {
    auto current = port.stop.load(std::memory_order_relaxed);

    while (current < request) {
        if (port.stop.compare_exchange_weak(current, request, std::memory_order_release)) {
            return true;
        }
    }

    return false;
}

/** Release public ownership without source/consumer cleanup.
 * @param port Detached producer port or empty.
 */
void releasePublic(const std::shared_ptr<detail::QueuePort> &port) noexcept {
    if (!port) {
        return;
    }

    auto current = port->publicOwnership.load(std::memory_order_relaxed);

    for (;;) {
        if (!current || (current & PUBLIC_RETIRED)) {
            std::terminate();
        }

        const auto next = current == 1 ? PUBLIC_RETIRED : current - 1;

        if (port->publicOwnership.compare_exchange_weak(current, next, std::memory_order_acq_rel)) {
            if (next == PUBLIC_RETIRED) {
                stop(*port, 2);
            }

            return;
        }
    }
}

/** Copy one producer command into fully reserved buffers.
 * @param port Detached producer port.
 * @param kind Actual desired action.
 * @param requests Borrowed add/replace inputs.
 * @param keys Borrowed removal inputs.
 * @param work Producer-local credits.
 * @return Independent accepted result slot or immediate refusal.
 */
std::expected<std::shared_ptr<detail::QueueCompletion>, QueueError>
submit(const std::shared_ptr<detail::QueuePort> &port, detail::QueueCommandKind kind,
       std::span<const core::SubscriptionRequest> requests, std::span<const core::SubscriptionKey> keys,
       QueueSubmitWork &work) {
    try {
        debit(work.operations, 1);

        if (!port) {
            return std::unexpected(QueueError::INVALID_STATE);
        }

        if (port->stop.load(std::memory_order_acquire) || port->budget->context->stop.load() ||
            port->state.load() == QueueClientState::TERMINAL) {
            return std::unexpected(QueueError::CLOSING);
        }

        const auto size = kind == detail::QueueCommandKind::REMOVE ? keys.size() : requests.size();
        const auto &l = port->budget->limits;
        debit(work.visits,
              sum(sum(l.commandSlots, l.resultSlots), sum(product(size, sum(port->contexts.size(), 2)), 1)));
        debit(work.bytes, sum(sizeof(detail::QueueCommand),
                              product(size, sum(sizeof(core::SubscriptionRequest), sizeof(core::SubscriptionKey)))));

        if (size > l.commandEntries) {
            return std::unexpected(QueueError::RESOURCE_LIMIT);
        }

        std::size_t units = 0;

        for (std::size_t i = 0; i < size; ++i) {
            const auto key = kind == detail::QueueCommandKind::REMOVE ? keys[i] : requests[i].key;
            bool match = false;

            for (const auto &context : port->contexts) {
                match = match || context.matches(key.context);
            }

            if (!match) {
                return std::unexpected(QueueError::INVALID_CONTEXT);
            }

            if (!key.symbol.data()) {
                return std::unexpected(QueueError::INVALID_CONTEXT);
            }

            units = sum(units, sum(key.symbol.size(), 1));
            debit(work.textUnits, key.symbol.size());
        }

        if (units > l.commandSymbolUnits) {
            return std::unexpected(QueueError::RESOURCE_LIMIT);
        }

        debit(work.textUnits, units);
        debit(work.bytes, product(units, sizeof(char16_t)));
        std::lock_guard lock(port->mutex);

        if (port->stop.load(std::memory_order_acquire) || port->budget->context->stop.load() ||
            port->state.load() == QueueClientState::TERMINAL) {
            return std::unexpected(QueueError::CLOSING);
        }

        if (port->size == port->commands.size()) {
            return std::unexpected(QueueError::COMMAND_LIMIT);
        }

        std::shared_ptr<detail::QueueCompletion> completion;

        for (const auto &candidate : port->results) {
            if (candidate.use_count() == 1) {
                completion = candidate;
                break;
            }
        }

        if (!completion) {
            return std::unexpected(QueueError::RESULT_LIMIT);
        }

        if (port->nextCommand == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(QueueError::IDENTITY_EXHAUSTED);
        }

        auto &command = port->commands[port->tail];
        std::size_t offset = 0;

        for (std::size_t i = 0; i < size; ++i) {
            const auto key = kind == detail::QueueCommandKind::REMOVE ? keys[i] : requests[i].key;
            auto *destination = command.text.data() + offset;
            std::copy(key.symbol.begin(), key.symbol.end(), destination);
            destination[key.symbol.size()] = 0;
            const core::SubscriptionKey copied{key.context, std::u16string_view(destination, key.symbol.size()),
                                               key.selector};
            command.keys[i] = copied;
            command.requests[i] = {copied, kind == detail::QueueCommandKind::REMOVE ? std::optional<std::int64_t>{}
                                                                                    : requests[i].historyBound};
            offset += key.symbol.size() + 1;
        }

        {
            std::lock_guard resultLock(completion->mutex);
            completion->id = port->nextCommand++;
            completion->result.reset();
        }
        command.kind = kind;
        command.count = size;
        command.completion = completion;
        command.occupied = true;
        port->tail = (port->tail + 1) % port->commands.size();
        ++port->size;

        return completion;
    } catch (QueueError error) {
        return std::unexpected(error);
    }
}
} // namespace

bool QueueIdentity::matches(const QueueIdentity &other) const noexcept {
    return origin_ && origin_ == other.origin_ && generation_ == other.generation_;
}

std::optional<MuxCommandResult> MuxCommandTicket::result() const {
    if (!state_) {
        return {};
    }

    std::lock_guard lock(state_->mutex);

    return state_->result;
}

std::uint64_t MuxCommandTicket::id() const noexcept {
    return state_ ? state_->id : 0;
}

std::span<const MuxRecordView> OwnedMuxBatch::records() const noexcept {
    return state_ ? std::span<const MuxRecordView>(state_->views) : std::span<const MuxRecordView>{};
}

QueueIdentity OwnedMuxBatch::epoch() const noexcept {
    return state_ && !state_->views.empty() ? state_->views[0].epoch : QueueIdentity{};
}

std::span<const MuxRecordView> BorrowedMuxBatch::records() const noexcept {
    return state_ ? std::span<const MuxRecordView>(state_->views) : std::span<const MuxRecordView>{};
}

OwnedMuxBatch BorrowedMuxBatch::retain() const noexcept {
    OwnedMuxBatch result;
    result.state_ = state_;

    return result;
}

MuxHandle::MuxHandle(std::shared_ptr<detail::QueuePort> port) noexcept : port_(std::move(port)) {
}

MuxHandle::MuxHandle(const MuxHandle &other) noexcept : port_(other.port_) {
    retainPublic(port_);
}

MuxHandle::MuxHandle(MuxHandle &&other) noexcept : port_(std::move(other.port_)) {
}

MuxHandle &MuxHandle::operator=(const MuxHandle &other) noexcept {
    if (this != &other) {
        auto next = other.port_;
        retainPublic(next);
        releasePublic(port_);
        port_ = std::move(next);
    }

    return *this;
}

MuxHandle &MuxHandle::operator=(MuxHandle &&other) noexcept {
    if (this != &other) {
        releasePublic(port_);
        port_ = std::move(other.port_);
    }

    return *this;
}

MuxHandle::~MuxHandle() {
    releasePublic(port_);
}

std::expected<MuxCommandTicket, QueueError> MuxHandle::add(std::span<const core::SubscriptionRequest> inputs,
                                                           QueueSubmitWork &work) {
    auto result = submit(port_, detail::QueueCommandKind::ADD, inputs, {}, work);

    if (!result) {
        return std::unexpected(result.error());
    }

    MuxCommandTicket ticket;
    ticket.state_ = std::move(*result);

    return ticket;
}

std::expected<MuxCommandTicket, QueueError> MuxHandle::remove(std::span<const core::SubscriptionKey> inputs,
                                                              QueueSubmitWork &work) {
    auto result = submit(port_, detail::QueueCommandKind::REMOVE, {}, inputs, work);

    if (!result) {
        return std::unexpected(result.error());
    }

    MuxCommandTicket ticket;
    ticket.state_ = std::move(*result);

    return ticket;
}

std::expected<MuxCommandTicket, QueueError> MuxHandle::replace(std::span<const core::SubscriptionRequest> inputs,
                                                               QueueSubmitWork &work) {
    auto result = submit(port_, detail::QueueCommandKind::REPLACE, inputs, {}, work);

    if (!result) {
        return std::unexpected(result.error());
    }

    MuxCommandTicket ticket;
    ticket.state_ = std::move(*result);

    return ticket;
}

std::expected<MuxCommandTicket, QueueError> MuxHandle::clear(QueueSubmitWork &work) {
    auto result = submit(port_, detail::QueueCommandKind::CLEAR, {}, {}, work);

    if (!result) {
        return std::unexpected(result.error());
    }

    MuxCommandTicket ticket;
    ticket.state_ = std::move(*result);

    return ticket;
}

ControlAdmission MuxHandle::requestStop(StopMode mode) noexcept {
    if (!port_ || port_->state.load() == QueueClientState::TERMINAL) {
        return ControlAdmission::CLOSED;
    }

    return stop(*port_, mode == StopMode::ABORT ? 2U : 1U) ? ControlAdmission::RECORDED : ControlAdmission::COALESCED;
}

ControlAdmission MuxHandle::requestResume() noexcept {
    if (!port_ || port_->stop.load() || port_->state.load() == QueueClientState::TERMINAL) {
        return ControlAdmission::CLOSED;
    }

    return port_->resume.exchange(true, std::memory_order_acq_rel) ? ControlAdmission::COALESCED
                                                                   : ControlAdmission::RECORDED;
}

ControlAdmission MuxHandle::acknowledgeReset(std::uint64_t sequence) noexcept {
    if (!port_) {
        return ControlAdmission::CLOSED;
    }

    if (!sequence || sequence != port_->advertisedNotice.load(std::memory_order_acquire)) {
        return ControlAdmission::STALE;
    }

    auto old = port_->ack.load(std::memory_order_relaxed);

    while (old < sequence) {
        if (port_->ack.compare_exchange_weak(old, sequence, std::memory_order_release)) {
            return ControlAdmission::RECORDED;
        }
    }

    return old == sequence ? ControlAdmission::COALESCED : ControlAdmission::STALE;
}

std::optional<QueueResetNotice> MuxHandle::resetNotice() const {
    if (!port_) {
        return {};
    }

    std::lock_guard lock(port_->mutex);

    return port_->notice;
}

std::optional<QueueTerminal> MuxHandle::terminal() const {
    if (!port_) {
        return {};
    }

    std::lock_guard lock(port_->mutex);

    return port_->terminal;
}

QueueClientState MuxHandle::state() const {
    return port_ ? port_->state.load(std::memory_order_acquire) : QueueClientState::TERMINAL;
}

namespace {
/** Checked fixed arena for one immutable record, without fallback allocations. */
struct RecordArena {
    std::byte *data;      ///< Caller-owned reserved block.
    std::size_t capacity; ///< Requested bytes and committed prefix.
    std::size_t used = 0; ///< Requested bytes and committed prefix.

    /** Reserve aligned raw slots.
     * @tparam T Trivial view/scalar storage type.
     * @param n Slot count.
     * @return Aligned stable address or typed quota failure.
     */
    template <class T> T *take(std::size_t n) {
        const auto aligned = sum(used, alignof(T) - 1) & ~(alignof(T) - 1);
        const auto end = sum(aligned, product(n, sizeof(T)));

        if (end > capacity) {
            throw QueueError::RESOURCE_LIMIT;
        }

        used = end;

        return reinterpret_cast<T *>(data + aligned);
    }

    /** Copy exact non-null text.
     * @param input Borrowed source units.
     * @return Arena-owned non-null text, including empty.
     */
    std::u16string_view text(std::u16string_view input) {
        auto *target = take<char16_t>(sum(input.size(), 1));
        std::copy(input.begin(), input.end(), target);
        target[input.size()] = 0;

        return {target, input.size()};
    }
};

/** Check a per-registration maximum.
 * @param value Proposed actual count.
 * @param maximum Immutable cap.
 */
void bound(std::size_t value, std::size_t maximum) {
    if (value > maximum) {
        throw QueueError::RESOURCE_LIMIT;
    }
}
} // namespace

/** Unique execution state; every actual collector/protocol owner remains in this serialization domain. */
struct MuxQueue::State {
    /** Paired authoritative/view membership; text borrows the owned view's desired state only. */
    struct Mapping {
        core::SubscriptionKey key;                 ///< Stable view-owned exact key until its next controlled mutation.
        core::SubscriptionMembership core;         ///< Actual source-view activation.
        MuxMembership mux;                         ///< Corresponding authoritative activation.
        std::optional<std::int64_t> bound;         ///< Exact current source-view request.
        std::uint64_t sourceRevision = 0;          ///< Independent core revision domain.
        collectors::HistoryEpochToken sourceEpoch; ///< Actual detached HISTORY epoch.
    };

    /** One registration; callbacks live only here, never in ports/identities/retained values. */
    struct Client {
        detail::BudgetLease budget;      ///< Retains pure accounting through unique client-object release.
        std::size_t fixedReferences = 0; ///< Stable actual fixed-backing lease count after construction locals retire.
        std::shared_ptr<detail::QueuePort> port;                            ///< Fixed detached command/control domain.
        std::shared_ptr<MuxConsumer> consumer;                              ///< Guarded owner-only callback reference.
        MuxClient interest;                                                 ///< Actual authoritative client token.
        collectors::TickerViewToken ticker;                                 ///< Actual shared-cache view.
        collectors::HistoryViewToken history;                               ///< Actual history view.
        detail::Vector<std::shared_ptr<const detail::QueueRecord>> records; ///< Fixed queue links.
        detail::Vector<std::uint8_t> selection; ///< Fixed owner eligibility/cleanup scratch.
        detail::Vector<Mapping> mapping;        ///< Fixed paired source-view metadata.
        std::size_t head = 0;                   ///< Stored ranges.
        std::size_t size = 0;                   ///< Stored ranges.
        std::size_t mappingCount = 0;           ///< Stored ranges.
        std::uint64_t desiredRevision = 0;      ///< Authoritative application versus synchronization.
        std::uint64_t mappingRevision = 0;      ///< Authoritative application versus synchronization.
        bool sync = true;                       ///< Owner gates.
        bool paused = false;                    ///< A reserved pause gates later callbacks.
        bool closing = false;                   ///< Owner gates.
        bool sourceClosed = false;              ///< Owner gates.
        bool muxClosed = false;                 ///< Owner gates.
        bool hadGap = false;   ///< Continuity is incomplete because of known or unclassified excluded delivery.
        bool epochGap = false; ///< Owner gates.
        bool unknownEligibility = false;     ///< Lost source eligibility was not classified into exact client copies.
        unsigned stopMode = 0;               ///< Applied graceful1/abort2.
        bool retireBlocked = false;          ///< Actual clear failed irrecoverably; never reuse this slot.
        std::optional<QueueFailure> primary; ///< First non-cancellation failure, preserved through stop and retirement.
        std::optional<QueueFailure> secondary; ///< Later bounded failure; never replaces the primary cause.
        QueueCounts counts;                    ///< Local evidence.

        /** Bind all fixed client arrays to exact requested-block accounting.
         * @param source Pure generation counters.
         */
        explicit Client(detail::BudgetLease source)
            : budget(source),
              records(detail::QueueAllocator<std::shared_ptr<const detail::QueueRecord>>{detail::clientScope(source)}),
              selection(detail::QueueAllocator<std::uint8_t>{detail::clientScope(source)}),
              mapping(detail::QueueAllocator<Mapping>{detail::clientScope(source)}) {
        }
    };

    /** Preserve fixed reservation until the actual serialized client object was deallocated. */
    struct DeleteClient {
        /** Dispose owner metadata after actual view/interest/callable retirement.
         * @param value Complete or partially constructed owner client.
         */
        void operator()(Client *value) const noexcept {
            auto budget = value->budget;
            std::destroy_at(value);
            detail::QueueAllocator<Client>{detail::clientScope(std::move(budget))}.deallocate(value, 1);
        }
    };

    using ClientOwner = std::unique_ptr<Client, DeleteClient>; ///< Fixed empty-deleter owner; no producer callback.
    detail::ContextLease ledger;                   ///< Pure aggregate accounting outlives every requested block.
    detail::Vector<detail::ContextEntry> contexts; ///< Exact local identities; actual components own complete schemas.
    MuxState interest;                             ///< Sole authoritative desired maps/union.
    core::SchemaIdentity schemaIdentity;           ///< Fixed exact provenance, including empty catalogs.
    QueueCapacity capacity;                        ///< Future independently enforced component/count ceilings.
    core::SubscriptionFamily family;               ///< Immutable source family.
    MuxLimits interestLimits;                      ///< Actual source input/mapping capacities.
    QueueLimits limits;                            ///< Queue-domain maxima.
    std::shared_ptr<const detail::QueueOrigin> origin;    ///< Fixed source/run identity.
    detail::Object<collectors::StreamCollector> stream;   ///< One actual union source.
    detail::Object<collectors::TickerCollector> ticker;   ///< One actual shared latest source.
    detail::Object<collectors::HistoryCollector> history; ///< One actual shared indexed source.
    detail::Vector<MuxClient> fixedClients; ///< Empty authoritative slots reused only after clear/retirement.
    detail::Vector<ClientOwner> clients;    ///< Fixed generation slots.
    detail::Vector<collectors::StreamDelivery> offers;                     ///< Fixed source-chunk metadata.
    detail::Vector<core::SubscriptionRequest> requests;                    ///< Fixed owner desired snapshot scratch.
    detail::Vector<std::shared_ptr<const detail::QueueRecord>> candidates; ///< Fixed unpublished fanout slots.
    detail::Vector<std::size_t> candidateCounts;                           ///< Fixed per-client chunk prefix counts.
    detail::Vector<std::uint8_t> candidateFailure;                         ///< Fixed local failure plans.
    std::optional<protocol::LiveEvent> pending; ///< Sole accepted owner; never enters producer metadata.
    enum class Pending {
        NONE,         ///< No accepted event owner remains.
        UNSTAGED,     ///< The admitted owner must stage before ordinary commands.
        ROUTING,      ///< The STREAM source prefix is still routing atomically.
        RELEASE_ONLY, ///< Only serialized owner disposal remains.
    };
    Pending pendingPhase = Pending::NONE;                 ///< Explicit accepted-event continuation.
    std::size_t pendingRecords = 0;                       ///< Actual source records accepted.
    std::optional<protocol::LiveIdentity> sourceIdentity; ///< Serialized pinned actual source epoch.
    QueueState state = QueueState::OPEN;                  ///< Local context lifecycle.
    QueueCounts counts;                                   ///< Context evidence.
    std::optional<QueueFailure> primary;   ///< First non-cancellation failure, preserved through stop and retirement.
    std::optional<QueueFailure> secondary; ///< Later bounded failure; never replaces the primary cause.
    std::size_t failedRegistrations = 0;   ///< Completed lifetime outcomes, not active slots.
    std::size_t abortedRegistrations = 0;  ///< Completed lifetime outcomes, not active slots.
    bool hadGap = false;                   ///< Run integrity survives generation cleanup.
    bool unknownEligibility = false;       ///< Unclassified loss survives retired registrations.
    std::optional<QueueTerminal> terminal; ///< Detached global outcome.
    std::uint64_t nextGeneration = 1;      ///< Non-reused identities.
    std::uint64_t epochGeneration = INITIAL_QUEUE_EPOCH; ///< Non-reused identities.
    std::uint64_t noticeSequence = 0;                    ///< Non-reused identities.
    std::uint64_t resetRequestSequence = 0;              ///< Non-reused identities.
    std::size_t cursor = 0;                              ///< Bounded fair owner cursors.
    std::size_t phase = 0;                               ///< Bounded fair owner cursors.
    bool executing = false;                              ///< Reentrancy and derived-union gates.
    bool unionDirty = false;                             ///< Reentrancy and derived-union gates.
    unsigned stopMode = 0;                               ///< Context reserved stop.
    std::optional<std::size_t> unionOrigin;              ///< Command responsible for a pending shared derived update.
    bool canAdmit = true;        ///< One fair owner cycle follows source acceptance before another admission.
    ResetStatus reset;           ///< Phased accepted reset.
    QueueIdentity resetFrom;     ///< Earliest affected scope.
    std::size_t resetCursor = 0; ///< Finite reset cleanup/publication cursor.
    detail::Vector<std::size_t> resetDiscards; ///< Fixed per-client known queue counts.

    /** Construct metadata after fixed preflight.
     * @param context Pure request accounting retained by every owned allocation.
     * @param mux Actual prepared desired owner.
     * @param contract Immutable family.
     * @param ml Actual desired limits.
     * @param ql Aggregate limits.
     * @param identity Exact retained schema origin, without declarations.
     * @param maxima Original enforced component ceilings, including source desired scopes.
     */
    State(detail::ContextLease context, MuxState &&mux, core::SubscriptionFamily contract, MuxLimits ml, QueueLimits ql,
          core::SchemaIdentity identity, QueueCapacity maxima)
        : ledger(context), contexts(detail::QueueAllocator<detail::ContextEntry>{detail::ownerScope(context)}),
          interest(std::move(mux)), schemaIdentity(std::move(identity)), capacity(maxima), family(contract),
          interestLimits(ml), limits(ql), fixedClients(detail::QueueAllocator<MuxClient>{detail::ownerScope(context)}),
          clients(detail::QueueAllocator<ClientOwner>{detail::ownerScope(context)}),
          offers(detail::QueueAllocator<collectors::StreamDelivery>{detail::ownerScope(context)}),
          requests(detail::QueueAllocator<core::SubscriptionRequest>{detail::ownerScope(context)}),
          candidates(detail::QueueAllocator<std::shared_ptr<const detail::QueueRecord>>{detail::ownerScope(context)}),
          candidateCounts(detail::QueueAllocator<std::size_t>{detail::ownerScope(context)}),
          candidateFailure(detail::QueueAllocator<std::uint8_t>{detail::ownerScope(context)}),
          resetDiscards(detail::QueueAllocator<std::size_t>{detail::ownerScope(context)}) {
    }

    /** Return current detached local epoch.
     * @return Fixed marker and scalar generation only.
     */
    QueueIdentity epoch() const noexcept {
        QueueIdentity result;
        result.origin_ = origin;
        result.generation_ = epochGeneration;

        return result;
    }

    /** Reserve complete fixed owner backing before allocating arrays.
     * @param local Valid input schema.
     * @param contract Immutable family.
     * @param ml Actual desired maxima.
     * @param ql Aggregate capacities.
     * @param work Initialization/cleanup credits.
     * @param capacity Original enforced byte and desired-state capacities.
     * @return Prepared unique state or typed exception.
     */
    static StateOwner create(const core::LocalSchema &local, core::SubscriptionFamily contract, MuxLimits ml,
                             QueueLimits ql, QueueWork &work, QueueCapacity capacity,
                             core::SubscriptionSelectorPolicy policy = core::SubscriptionSelectorPolicy::EXACT_ONLY) {
        if (!ql.routeRecords || !ql.batchRecords || !ql.cleanupRecords || ql.routeRecords > ql.sourceRecords ||
            ml.clients < ql.clients)
            throw QueueError::INVALID_CONFIGURATION;

        const auto shape = detail::controlLayout();
        const auto cells = product(ql.clients, ql.routeRecords);
        auto fixed = sum(sizeof(State), sum(sizeof(detail::ContextLedger),
                                            shape[static_cast<std::size_t>(detail::ControlKind::ORIGIN)]));
        fixed = sum(fixed, product(local.records().size(), sizeof(detail::ContextEntry)));
        fixed = sum(fixed, product(ql.clients, sum(sum(sizeof(ClientOwner), product(sizeof(std::size_t), 2)),
                                                   sum(sizeof(std::uint8_t), sizeof(MuxClient)))));
        fixed = sum(fixed, product(ql.routeRecords, sizeof(collectors::StreamDelivery)));
        fixed = sum(fixed, product(ml.entries, sizeof(core::SubscriptionRequest)));
        fixed = sum(fixed, product(cells, sizeof(std::shared_ptr<const detail::QueueRecord>)));
        fixed = sum(fixed, contract == core::SubscriptionFamily::STREAM
                               ? sizeof(collectors::StreamCollector)
                               : (contract == core::SubscriptionFamily::TICKER ? sizeof(collectors::TickerCollector)
                                                                               : sizeof(collectors::HistoryCollector)));
        bound(fixed, ql.residentBytes);
        const auto slots =
            sum(sum(product(ql.clients, 5), cells),
                sum(sum(ql.routeRecords, ml.entries), product(local.records().size(), local.records().size())));
        spend(work, sum(product(slots, 2), 9), 0, 0, product(fixed, 2));
        auto mux = MuxState::create(local, contract, ml, work.interest, policy);

        if (!mux)
            throw sourceFailure(mux.error());

        auto ledger = detail::ContextLease::adopt(new detail::ContextLedger);
        ledger->limits = ql;
        ledger->controlBytes = shape;
        ledger->fixedCapacity = fixed;
        detail::QueueAllocator<State> allocator{detail::ownerScope(ledger)};
        auto *raw = allocator.allocate(1);
        try {
            std::construct_at(raw, ledger, std::move(*mux), contract, ml, ql, local.identity(), capacity);
        } catch (...) {
            allocator.deallocate(raw, 1);
            throw;
        }
        StateOwner result(raw);
        result->contexts.reserve(local.records().size());

        for (const auto &record : local.records())
            result->contexts.push_back({record.id, local.recordContext(record.id)});

        result->origin = detail::shared<detail::QueueOrigin>(detail::ownerScope(ledger, detail::AllocationTag::ORIGIN));
        result->clients.resize(ql.clients);
        result->fixedClients.reserve(ql.clients);

        for (std::size_t i = 0; i < ql.clients; ++i) {
            auto client = result->interest.createClient(work.interest);

            if (!client)
                throw sourceFailure(client.error());

            result->fixedClients.push_back(std::move(*client));
        }

        result->offers.resize(ql.routeRecords);
        result->requests.resize(ml.entries);
        result->candidates.resize(cells);
        result->candidateCounts.resize(ql.clients);
        result->candidateFailure.resize(ql.clients);
        result->resetDiscards.resize(ql.clients);

        return result;
    } /** Validate worst-case derived desired capacity without private-size guesses.
       * @param dl Actual source core desired limits.
       */

    void checkDesired(core::SubscriptionLimits dl) const {
        const auto entries =
            family == core::SubscriptionFamily::STREAM ? interestLimits.aggregateEntries : interestLimits.entries;

        if (dl.schemaRecords < contexts.size() || dl.entries < entries || dl.symbolUnits < interestLimits.symbolUnits ||
            dl.totalSymbolUnits < interestLimits.totalSymbolUnits || dl.peakEntries < product(entries, 2) ||
            dl.peakSymbolUnits < product(interestLimits.totalSymbolUnits, 2) || dl.commandEntries < entries) {
            throw QueueError::INVALID_CONFIGURATION;
        }
    }

    /** Check client origin without touching source state.
     * @param handle Public registration lease.
     * @return Current retained client, or null for foreign/retired.
     */
    Client *client(const MuxHandle &handle) const noexcept {
        if (!handle.port_ || handle.port_->origin != origin || handle.port_->slot >= clients.size()) {
            return nullptr;
        }

        auto *result = clients[handle.port_->slot].get();

        return result && result->port == handle.port_ ? result : nullptr;
    }

    /** Copy a current detached resource snapshot.
     * @param c Registration owner.
     * @return Known individual backing and occupancy.
     */
    ClientResources resources(const Client &c) const noexcept {
        ClientResources r;
        auto &p = *c.port;
        auto &b = *p.budget;
        r.state = p.state.load();
        r.paused = c.paused;
        r.fixedReservationBytes = b.fixedReservation;
        r.queuedRecords = c.size;
        r.recordOwners = b.records.load();
        r.recordBytes = b.recordBytes.load();
        r.batchOwners = b.batches.load();
        r.batchBytes = b.batchBytes.load();
        std::lock_guard lock(p.mutex);
        r.resetPending = p.notice.has_value();
        r.queuedCommands = p.size;
        r.controlSlots = 2;

        for (const auto &slot : p.results) {
            if (slot.use_count() > 1) {
                ++r.resultSlotsInUse;
            }
        }

        return r;
    }

    /** Preserve primary non-cancellation failure and close callback admission.
     * @param c Affected registration.
     * @param failure Actual local/source cause.
     */
    void fail(Client &c, QueueFailure failure) noexcept {
        if (!c.primary) {
            c.primary = failure;
            count(failedRegistrations, 1, counts.saturated);
        } else if (!c.secondary) {
            c.secondary = failure;
        }

        if (!primary)
            primary = failure;
        else if (!secondary)
            secondary = failure;

        hadGap = true;
        c.hadGap = true;
        c.epochGap = true;
        c.closing = true;
        c.stopMode = 2;
        c.port->state = QueueClientState::CLOSING;
        stop(*c.port, 2);
    }

    /** Mark a shared source failure without allocating or invoking callbacks.
     * @param failure Preserved actual cause.
     */
    void failAll(QueueFailure failure) noexcept {
        if (!primary)
            primary = failure;
        else if (!secondary)
            secondary = failure;

        hadGap = true;
        ledger->stop = 2;

        for (auto &c : clients) {
            if (c && c->port->state.load() != QueueClientState::TERMINAL) {
                fail(*c, failure);
            }
        }

        state = QueueState::CLOSING;
        stopMode = 2;
    }

    /** Publish a registration's current externally visible phase.
     * @param c Current registration.
     */
    void publishState(Client &c) noexcept {
        if (c.closing) {
            c.port->state = QueueClientState::CLOSING;
        } else if (reset.phase != ResetPhase::NONE || c.port->advertisedNotice.load()) {
            c.port->state = QueueClientState::RESET_PENDING;
        } else if (c.sync) {
            c.port->state = QueueClientState::SYNC_PENDING;
        } else {
            c.port->state = c.paused ? QueueClientState::PAUSED : QueueClientState::ACTIVE;
        }
    }

    /** Current exact source mapping, without raw HISTORY time filtering.
     * @param c Registration.
     * @param key Actual projected exact key.
     * @param work Complete lookup allowance.
     * @return Paired current mapping or null for a retired/unrequested projection.
     */
    Mapping *mapping(Client &c, core::SubscriptionKey key, QueueWork &work) {
        spend(work, c.mappingCount, 0, product(c.mappingCount, key.symbol.size()));

        for (std::size_t i = 0; i < c.mappingCount; ++i) {
            auto &m = c.mapping[i];

            if (m.key.selector == key.selector && m.key.context.matches(key.context) && m.key.symbol == key.symbol) {
                auto current = interest.isCurrent(c.interest, m.mux, work.interest);

                if (!current) {
                    throw sourceFailure(current.error());
                }

                return *current ? &m : nullptr;
            }
        }

        return nullptr;
    }

    /** Release a sole pending event only after complete current conservative disposal preflight.
     * @param work Remaining caller work.
     * @param discarded Whether this is explicit loss rather than zero-selection/control release.
     */
    void releasePending(QueueWork &work, bool discarded) {
        if (discarded)
            spend(work, clients.size(), 0, 0, sum(sizeof(QueueCounts), product(clients.size(), sizeof(Client))));

        auto gap = [this](std::size_t records) {
            if (!records)
                return;

            hadGap = true;
            unknownEligibility = true;

            for (auto &c : clients) {
                if (c && c->port->state.load() != QueueClientState::TERMINAL) {
                    c->hadGap = true;
                    c->epochGap = true;
                    c->unknownEligibility = true;
                }
            }
        };

        if (stream && pendingPhase == Pending::ROUTING) {
            auto dropped = stream->discard(work.stream);

            if (!dropped) {
                if (dropped.error() == collectors::StreamError::WORK_LIMIT)
                    throw QueueError::WORK_LIMIT;

                throw sourceFailure(dropped.error());
            }

            if (discarded) {
                count(counts.sourceRecordsDiscarded, *dropped, counts.saturated);
                gap(*dropped);
            }

            pending.reset();
            pendingRecords = 0;
            pendingPhase = Pending::NONE;

            return;
        }

        if (!pending) {
            pendingPhase = Pending::NONE;

            return;
        }

        spend(work, sum(pendingRecords, 1), 0, 0, sum(pending->releaseBytes(), sizeof(protocol::LiveEvent)));

        if (discarded) {
            count(counts.sourceRecordsDiscarded, pendingRecords, counts.saturated);
            gap(pendingRecords);

            if (reset.phase != ResetPhase::NONE) {
                ++reset.discardedPendingFrames;
                reset.discardedPendingRecords = sum(reset.discardedPendingRecords, pendingRecords);
                reset.unknownEligibility = reset.unknownEligibility || pendingRecords != 0;
            }
        }

        pending.reset();
        pendingRecords = 0;
        pendingPhase = Pending::NONE;
    }

    /** Build one immutable exact record with prepaid bounded eventual detached disposal.
     * @tparam Field Exact source generic field-view type.
     * @param c Target registration.
     * @param key Exact actual source key.
     * @param time Optional full raw indexed identity.
     * @param flags Produced delivery flags.
     * @param legacy Actual compact-only values.
     * @param fields Actual tagged generic values.
     * @param membership Captured authoritative activation.
     * @param sourceRevision Actual collector admission revision.
     * @param raw Optional true raw provenance.
     * @param trigger Optional actual HISTORY trigger.
     * @param work Copy/initialization/disposal credits.
     * @return Unpublished complete detached candidate.
     */
    template <class Field>
    std::shared_ptr<const detail::QueueRecord>
    record(Client &c, core::SubscriptionKey key, std::optional<std::int64_t> time, std::int32_t flags,
           std::span<const std::int32_t> legacy, std::span<const Field> fields, MuxMembership membership,
           std::uint64_t sourceRevision, std::optional<RawSourceMetadata> raw,
           std::optional<collectors::HistorySourceMetadata> trigger, QueueWork &work) {
        auto &l = c.port->budget->limits;
        const auto fieldCount = sum(legacy.size(), fields.size());
        std::size_t units = 0, bytes = 0;
        spend(work, 1, product(fields.size(), 2));

        for (const auto &f : fields) {
            if (f.units) {
                units = sum(units, f.units->size());
            }

            if (f.bytes) {
                bytes = sum(bytes, f.bytes->size());
            }
        }

        bound(fieldCount, l.recordFields);
        bound(key.symbol.size(), l.recordSymbolUnits);
        bound(units, l.recordFieldUnits);
        bound(bytes, l.recordByteUnits);
        std::size_t required = 0;
        auto need = [&required](std::size_t n, std::size_t size, std::size_t alignment) {
            required = sum((sum(required, alignment - 1) & ~(alignment - 1)), product(n, size));
        };
        need(sum(key.symbol.size(), 1), sizeof(char16_t), alignof(char16_t));
        need(legacy.size(), sizeof(std::int32_t), alignof(std::int32_t));
        need(fields.size(), sizeof(MuxFieldValueView), alignof(MuxFieldValueView));

        for (const auto &f : fields) {
            if (f.units)
                need(sum(f.units->size(), 1), sizeof(char16_t), alignof(char16_t));

            if (f.bytes)
                need(f.bytes->size(), 1, 1);
        }

        bound(required, l.recordArenaBytes);
        const auto charge =
            sum(c.port->budget->context->controlBytes[static_cast<std::size_t>(detail::ControlKind::RECORD)],
                l.recordArenaBytes);
        bound(charge, l.recordBytes);
        spend(work, sum(fieldCount, 4), product(fieldCount, 2), product(sum(units, key.symbol.size()), 3),
              sum(product(charge, 3), product(bytes, 2)));
        auto out = detail::shared<detail::QueueRecord>(
            detail::clientScope(c.port->budget, detail::AllocationTag::RECORD), c.port->budget);
        out->arenaSize = l.recordArenaBytes;
        const auto arenaScope = detail::clientScope(c.port->budget, detail::AllocationTag::ARENA);
        auto *memory = detail::QueueAllocator<std::byte>{arenaScope}.allocate(l.recordArenaBytes);
        out->arena = {memory, detail::BlockDelete<std::byte>{arenaScope, l.recordArenaBytes, false}};
        RecordArena arena{out->arena.get(), l.recordArenaBytes};
        out->view.family = family;
        out->view.context = key.context;
        out->view.symbol = arena.text(key.symbol);
        out->view.rawTime = time;
        out->view.eventFlags = flags;
        out->view.membership = std::move(membership);
        out->view.revision = c.desiredRevision;
        out->view.admissionRevision = sourceRevision;
        out->view.epoch = epoch();
        out->view.source = raw;
        out->view.trigger = trigger;
        auto *scalars = arena.take<std::int32_t>(legacy.size());
        std::copy(legacy.begin(), legacy.end(), scalars);
        out->view.values = {scalars, legacy.size()};
        auto *values = arena.take<MuxFieldValueView>(fields.size());

        for (std::size_t i = 0; i < fields.size(); ++i) {
            const auto &f = fields[i];
            std::optional<std::u16string_view> copiedUnits;
            std::optional<std::span<const std::byte>> copiedBytes;

            if (f.units) {
                copiedUnits = arena.text(*f.units);
            }

            if (f.bytes) {
                auto *target = arena.take<std::byte>(f.bytes->size());
                std::copy(f.bytes->begin(), f.bytes->end(), target);
                copiedBytes = std::span<const std::byte>(target, f.bytes->size());
            }

            std::construct_at(values + i, MuxFieldValueView{f.type, f.raw, copiedUnits, copiedBytes});
        }

        out->view.typedValues = {values, fields.size()};

        return out;
    }

    /** Publish one fully prepared queue link without allocation.
     * @param c Target client.
     * @param value Complete detached candidate.
     */
    void enqueue(Client &c, std::shared_ptr<const detail::QueueRecord> value) noexcept {
        const auto tail = (c.head + c.size) % c.records.size();
        c.records[tail] = std::move(value);
        ++c.size;
        count(c.counts.queueAdmitted, 1, c.counts.saturated);
        count(counts.queueAdmitted, 1, counts.saturated);
    }

    /** Drop one stored link after its unlink work was reserved.
     * @param c Target client.
     * @param withdrawn Retired membership rather than explicit discard.
     */
    void drop(Client &c, bool withdrawn) noexcept {
        c.records[c.head].reset();
        c.head = (c.head + 1) % c.records.size();
        --c.size;

        if (withdrawn) {
            count(c.counts.withdrawn, 1, c.counts.saturated);
            count(counts.withdrawn, 1, counts.saturated);
        } else {
            count(c.counts.discarded, 1, c.counts.saturated);
            count(counts.discarded, 1, counts.saturated);
        }
    }

    /** Serialized fallback retires actual source owners before publishing detached port completion. */
    ~State();
    /** Synchronize one derived view/union after authoritative application.
     * @param c Current serialized registration.
     * @param work Complete caller credits, including candidate disposal.
     * @return Whether the phase advanced, or typed rejection without fabricated completion.
     */
    std::expected<bool, QueueFailure> sync(Client &c, QueueWork &work);
    /** Prepare/copy/pop one actual TICKER or HISTORY projection atomically.
     * @param c Current serialized registration.
     * @param work Complete caller credits, including candidate disposal.
     * @return Whether the phase advanced, or typed rejection without fabricated completion.
     */
    std::expected<bool, QueueFailure> route(Client &c, QueueWork &work);
    /** Atomically prepare all clients before acknowledging one actual STREAM source chunk.
     * @param work Complete fanout, source ACK and candidate cleanup credits.
     * @return Committed routing, no action, or typed rejection with source prefix preserved.
     */
    std::expected<bool, QueueFailure> routeStream(QueueWork &work);
    /** Build one immutable batch and hold its callable through return/catch.
     * @param c Current serialized registration.
     * @param work Complete caller credits, including candidate disposal.
     * @return Whether the phase advanced, or typed rejection without fabricated completion.
     */
    std::expected<bool, QueueFailure> callback(Client &c, QueueWork &work);
    /** Apply or cancel one whole accepted ordinary command.
     * @param c Current serialized registration.
     * @param work Complete caller credits, including candidate disposal.
     * @return Whether the phase advanced, or typed rejection without fabricated completion.
     */
    std::expected<bool, QueueFailure> command(Client &c, QueueWork &work);
    /** Retire one finite client source/queue/command/callable cleanup phase.
     * @param c Current serialized registration.
     * @param work Complete caller credits, including candidate disposal.
     * @return Whether the phase advanced, or typed rejection without fabricated completion.
     */
    std::expected<bool, QueueFailure> close(Client &c, QueueWork &work);
    /** Advance one accepted continuing reset phase, preserving an actual committed reset across ACK yield.
     * @param work Exact current phase/source credits.
     * @return Real phase evidence, or typed nonterminal yield/failure.
     */
    std::expected<QueueProgress, QueueFailure> resetStep(QueueWork &work);
};

void MuxQueue::StateDelete::operator()(State *value) const noexcept {
    if (!value)
        return;

    auto ledger = value->ledger;
    std::destroy_at(value);
    detail::QueueAllocator<State>{detail::ownerScope(std::move(ledger))}.deallocate(value, 1);
}

MuxQueue::MuxQueue(StateOwner state) noexcept : state_(std::move(state)) {
}

MuxQueue::MuxQueue(MuxQueue &&other) noexcept = default;
MuxQueue &MuxQueue::operator=(MuxQueue &&other) noexcept = default;
MuxQueue::~MuxQueue() = default;

std::expected<MuxQueue, QueueFailure> MuxQueue::createStream(const core::LocalSchema &schema, MuxLimits ml,
                                                             core::SubscriptionLimits desired,
                                                             collectors::StreamLimits source, QueueLimits limits,
                                                             QueueWork &work, core::SubscriptionSelectorPolicy policy) {
    try {
        debit(work.operations, 1);

        if (source.records < limits.sourceRecords || (limits.sourceRecords && !source.frames)) {
            throw QueueError::INVALID_CONFIGURATION;
        }

        const QueueCapacity capacity{limits.residentBytes, ml.residentBytes, source.residentBytes, 1, desired};
        auto state = State::create(schema, core::SubscriptionFamily::STREAM, ml, limits, work, capacity, policy);
        state->checkDesired(desired);
        spend(work, sum(desired.schemaRecords, 1), 0, 0,
              sum(schema.storageBytes(), product(desired.schemaRecords, sizeof(core::LocalRecord))));
        auto demand = core::SubscriptionState::create(schema, core::SubscriptionFamily::STREAM, desired, policy);

        if (!demand) {
            return std::unexpected(sourceFailure(demand.error()));
        }

        auto collector = collectors::StreamCollector::create(std::move(*demand), source, work.stream);

        if (!collector) {
            return std::unexpected(sourceFailure(collector.error()));
        }

        state->stream =
            detail::object<collectors::StreamCollector>(detail::ownerScope(state->ledger), std::move(*collector));

        if (state->ledger->fixed.load() != state->ledger->fixedCapacity)
            throw QueueError::INVALID_CONFIGURATION;

        return MuxQueue(std::move(state));
    } catch (QueueFailure e) {
        return std::unexpected(e);
    } catch (QueueError e) {
        return std::unexpected(QueueFailure{e, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(QueueFailure{QueueError::ALLOCATION_FAILURE, {}});
    }
}

std::expected<MuxQueue, QueueFailure> MuxQueue::createTicker(const core::LocalSchema &schema, MuxLimits ml,
                                                             collectors::TickerLimits source,
                                                             collectors::TickerOptions options, QueueLimits limits,
                                                             QueueWork &work) {
    try {
        debit(work.operations, 1);

        if (source.viewSlots < limits.clients) {
            throw QueueError::INVALID_CONFIGURATION;
        }

        const QueueCapacity capacity{limits.residentBytes, ml.residentBytes, source.residentBytes, source.viewSlots,
                                     source.viewSubscriptions};
        auto state =
            State::create(schema, core::SubscriptionFamily::TICKER, ml, limits, work, capacity, options.selectorPolicy);
        state->checkDesired(source.viewSubscriptions);
        auto copy = schema;
        auto collector = collectors::TickerCollector::create(std::move(copy), source, options, work.ticker);

        if (!collector) {
            return std::unexpected(sourceFailure(collector.error()));
        }

        state->ticker =
            detail::object<collectors::TickerCollector>(detail::ownerScope(state->ledger), std::move(*collector));

        if (state->ledger->fixed.load() != state->ledger->fixedCapacity)
            throw QueueError::INVALID_CONFIGURATION;

        return MuxQueue(std::move(state));
    } catch (QueueFailure e) {
        return std::unexpected(e);
    } catch (QueueError e) {
        return std::unexpected(QueueFailure{e, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(QueueFailure{QueueError::ALLOCATION_FAILURE, {}});
    }
}

std::expected<MuxQueue, QueueFailure> MuxQueue::createHistory(const core::LocalSchema &schema, MuxLimits ml,
                                                              collectors::HistoryLimits source,
                                                              collectors::HistoryOptions options, QueueLimits limits,
                                                              QueueWork &work) {
    try {
        debit(work.operations, 1);

        if (source.viewSlots < limits.clients) {
            throw QueueError::INVALID_CONFIGURATION;
        }

        const QueueCapacity capacity{limits.residentBytes, ml.residentBytes, source.residentBytes, source.viewSlots,
                                     source.viewSubscriptions};
        auto state = State::create(schema, core::SubscriptionFamily::HISTORY, ml, limits, work, capacity,
                                   options.selectorPolicy);
        state->checkDesired(source.viewSubscriptions);
        auto copy = schema;
        auto collector = collectors::HistoryCollector::create(std::move(copy), source, options, work.history);

        if (!collector) {
            return std::unexpected(sourceFailure(collector.error().error));
        }

        state->history =
            detail::object<collectors::HistoryCollector>(detail::ownerScope(state->ledger), std::move(*collector));

        if (state->ledger->fixed.load() != state->ledger->fixedCapacity)
            throw QueueError::INVALID_CONFIGURATION;

        return MuxQueue(std::move(state));
    } catch (QueueFailure e) {
        return std::unexpected(e);
    } catch (QueueError e) {
        return std::unexpected(QueueFailure{e, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(QueueFailure{QueueError::ALLOCATION_FAILURE, {}});
    }
}

std::expected<MuxHandle, QueueFailure> MuxQueue::open(ClientLimits limits, const std::shared_ptr<MuxConsumer> &consumer,
                                                      QueueWork &work) {
    try {
        debit(work.operations, 1);

        if (!state_) {
            throw QueueError::INVALID_STATE;
        }

        auto &s = *state_;

        if (s.executing) {
            throw QueueError::REENTRANT;
        }

        if (s.state != QueueState::OPEN || s.reset.phase != ResetPhase::NONE) {
            throw QueueError::CLOSING;
        }

        if (!consumer || !limits.batchRecords || limits.batchRecords > s.limits.batchRecords ||
            limits.commandEntries > s.interestLimits.commandEntries || limits.resultSlots < limits.commandSlots) {
            throw QueueError::INVALID_CONFIGURATION;
        }

        if (s.nextGeneration == std::numeric_limits<std::uint64_t>::max()) {
            throw QueueError::IDENTITY_EXHAUSTED;
        }

        spend(work, s.clients.size());
        std::size_t slot = s.clients.size();

        for (std::size_t i = 0; i < s.clients.size(); ++i) {
            if (!s.clients[i]) {
                slot = i;
                break;
            }
        }

        if (slot == s.clients.size()) {
            throw QueueError::RESOURCE_LIMIT;
        }

        const auto &layout = s.ledger->controlBytes;
        auto fixed = sum(sizeof(State::Client), sum(sizeof(detail::ClientBudget),
                                                    layout[static_cast<std::size_t>(detail::ControlKind::PORT)]));
        fixed = sum(fixed, product(s.contexts.size(), sizeof(core::RecordContext)));
        fixed = sum(fixed, product(limits.dataItems,
                                   sum(sizeof(std::shared_ptr<const detail::QueueRecord>), sizeof(std::uint8_t))));
        fixed = sum(fixed, product(s.interestLimits.entries, sizeof(State::Mapping)));
        fixed = sum(fixed, product(limits.commandSlots,
                                   sum(sizeof(detail::QueueCommand),
                                       sum(product(limits.commandEntries, sum(sizeof(core::SubscriptionRequest),
                                                                              sizeof(core::SubscriptionKey))),
                                           product(limits.commandSymbolUnits, sizeof(char16_t))))));
        fixed = sum(fixed,
                    product(limits.resultSlots, sum(layout[static_cast<std::size_t>(detail::ControlKind::COMPLETION)],
                                                    sizeof(std::shared_ptr<detail::QueueCompletion>))));
        const auto reservation = sum(fixed, sum(limits.recordBytes, limits.batchBytes));
        const auto initializedSlots = sum(
            sum(s.interestLimits.entries, product(limits.dataItems, 2)),
            sum(product(limits.commandSlots, sum(product(limits.commandEntries, 2), sum(limits.commandSymbolUnits, 1))),
                sum(product(limits.resultSlots, 2), s.contexts.size())));
        constexpr std::size_t fixedPortObjects = 3; ///< Client, budget and port object initialization/disposal.
        constexpr std::size_t reservationAxes = 9;  ///< Independently rolled-back aggregate maxima.
        spend(work,
              sum(sum(product(initializedSlots, 2), limits.commandSlots),
                  product(sum(fixedPortObjects, reservationAxes), 2)),
              0, product(limits.commandSlots, limits.commandSymbolUnits),
              sum(product(sum(fixed, sizeof(detail::ContextLedger)), 2), sizeof(State)));

        struct Charge {
            std::atomic<std::size_t> *counter;
            std::size_t amount, maximum;
            bool done = false;
        };

        auto &l = *s.ledger;
        std::array<Charge, 9> charges{{{&l.generations, 1, s.limits.clients},
                                       {&l.reservedBytes, reservation, s.limits.residentBytes},
                                       {&l.reservedItems, limits.dataItems, s.limits.dataItems},
                                       {&l.reservedRecordBytes, limits.recordBytes, s.limits.recordBytes},
                                       {&l.reservedBatchOwners, limits.batchOwners, s.limits.batchOwners},
                                       {&l.reservedBatchBytes, limits.batchBytes, s.limits.batchBytes},
                                       {&l.reservedCommands, limits.commandSlots, s.limits.commandSlots},
                                       {&l.reservedResults, limits.resultSlots, s.limits.resultSlots},
                                       {&l.reservedControls, 2, s.limits.controlSlots}}};

        struct Rollback {
            std::array<Charge, 9> &charges;
            bool committed = false;

            ~Rollback() {
                if (!committed)
                    for (auto &c : charges)
                        if (c.done)
                            c.counter->fetch_sub(c.amount);
            }
        } rollback{charges};

        for (auto &charge : charges) {
            if (!reserve(*charge.counter, charge.amount, charge.maximum))
                throw QueueError::RESOURCE_LIMIT;

            charge.done = true;
        }

        auto *rawBudget = new detail::ClientBudget;
        auto budget = detail::BudgetLease::adopt(rawBudget);
        budget->context = s.ledger;
        budget->limits = limits;
        budget->fixedReservation = fixed;
        budget->reservation = reservation;
        l.clientFixed.fetch_add(sizeof(detail::ClientBudget));
        State::ClientOwner c;
        {
            detail::QueueAllocator<State::Client> clientAllocator{detail::clientScope(budget)};
            auto *rawClient = clientAllocator.allocate(1);
            try {
                std::construct_at(rawClient, budget);
            } catch (...) {
                clientAllocator.deallocate(rawClient, 1);
                throw;
            }
            c.reset(rawClient);
        }
        auto port = detail::shared<detail::QueuePort>(detail::clientScope(budget, detail::AllocationTag::PORT), budget);
        port->origin = s.origin;
        port->slot = slot;
        port->generation = s.nextGeneration;
        port->contexts.reserve(s.contexts.size());

        for (const auto &entry : s.contexts)
            port->contexts.push_back(entry.context);

        port->commands.reserve(limits.commandSlots);

        for (std::size_t i = 0; i < limits.commandSlots; ++i) {
            port->commands.emplace_back(budget);
            auto &command = port->commands.back();
            command.requests.resize(limits.commandEntries);
            command.keys.resize(limits.commandEntries);
            command.text.resize(limits.commandSymbolUnits);
        }

        port->results.reserve(limits.resultSlots);

        for (std::size_t i = 0; i < limits.resultSlots; ++i)
            port->results.push_back(detail::shared<detail::QueueCompletion>(
                detail::clientScope(budget, detail::AllocationTag::COMPLETION), budget));

        c->port = port;
        c->consumer = consumer;
        c->interest = s.fixedClients[slot];
        c->records.resize(limits.dataItems);
        c->selection.resize(limits.dataItems);
        c->mapping.resize(s.interestLimits.entries);
        c->desiredRevision = s.interest.revision();
        c->mappingRevision = c->desiredRevision;
        c->sync = false;

        if (budget->fixedLive.load() != fixed)
            throw QueueError::INVALID_CONFIGURATION;

        budget = {};
        c->fixedReferences =
            c->budget->references
                .load(); // This is the final fallible component operation; no published MUX mutation needs rollback.

        if (s.ticker) {
            auto view = s.ticker->openView(work.ticker);

            if (!view) {
                return std::unexpected(sourceFailure(view.error()));
            }

            c->ticker = std::move(*view);
        }

        if (s.history) {
            auto view = s.history->openView({true}, work.history);

            if (!view) {
                return std::unexpected(sourceFailure(view.error().error));
            }

            c->history = std::move(*view);
        }

        c->budget->charged = true;
        rollback.committed = true;
        ++s.nextGeneration;
        s.publishState(*c);
        s.clients[slot] = std::move(c);

        return MuxHandle(std::move(port));
    } catch (QueueFailure e) {
        return std::unexpected(e);
    } catch (QueueError e) {
        return std::unexpected(QueueFailure{e, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(QueueFailure{QueueError::ALLOCATION_FAILURE, {}});
    }
}

std::expected<bool, QueueFailure> MuxQueue::State::sync(Client &c, QueueWork &work) {
    if (!c.sync || c.closing) {
        return false;
    }

    spend(work,
          sum(sum(product(interestLimits.entries, interestLimits.entries), product(interestLimits.entries, 4)),
              clients.size()),
          0, product(product(interestLimits.entries, interestLimits.entries), interestLimits.symbolUnits),
          sum(product(interestLimits.entries, product(sum(sizeof(Mapping), sizeof(core::SubscriptionRequest)), 2)),
              product(clients.size(), sizeof(Client))));
    const auto desired = interest.clientEntries(c.interest);

    if (!desired) {
        return std::unexpected(sourceFailure(desired.error()));
    }

    if (stream) {
        if (!unionDirty) {
            c.sync = false;
            c.mappingRevision = c.desiredRevision;
            publishState(c);

            return true;
        }

        const auto entries = interest.aggregateEntries();
        auto result = stream->replace(entries, work.desired);

        if (!result) {
            if (result.error() == core::SubscriptionError::WORK_LIMIT) {
                return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});
            }

            fail(c, sourceFailure(result.error()));

            return true;
        }

        unionDirty = false;
        unionOrigin.reset();

        for (auto &peer : clients) {
            if (peer && !peer->closing) {
                peer->sync = false;
                peer->mappingRevision = peer->desiredRevision;
                publishState(*peer);
            }
        }

        return true;
    }

    for (auto &old : c.mapping)
        old = Mapping{};

    c.mappingCount = 0;

    struct ClearRequests {
        State &s;
        std::size_t count;

        ~ClearRequests() {
            for (std::size_t i = 0; i < count; ++i)
                s.requests[i] = {};
        }
    } clearRequests{*this, desired->size()};

    for (std::size_t i = 0; i < desired->size(); ++i) {
        requests[i] = {(*desired)[i].key, (*desired)[i].historyBound};
    }

    const auto candidate = std::span<const core::SubscriptionRequest>(requests.data(), desired->size());

    if (ticker) {
        auto result = ticker->replace(c.ticker, candidate, work.desired, work.ticker);

        if (!result) {
            if (result.error().error == collectors::TickerError::WORK_LIMIT ||
                result.error().subscription == core::SubscriptionError::WORK_LIMIT) {
                return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});
            }

            fail(c, sourceFailure(result.error().error));

            return true;
        }
    } else {
        auto result = history->replace(c.history, candidate, work.desired, work.history);

        if (!result) {
            if (result.error().error == collectors::HistoryError::WORK_LIMIT ||
                result.error().subscriptionError == core::SubscriptionError::WORK_LIMIT) {
                return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});
            }

            fail(c, sourceFailure(result.error().error));

            return true;
        }
    }

    const auto *view = ticker ? ticker->desired(c.ticker) : history->desired(c.history);

    if (!view) {
        fail(c, {QueueError::INVALID_STATE, {}});

        return true;
    }

    // Source keys own these internal text borrows; no mapping span escapes into detached records.
    c.mappingCount = 0;

    for (const auto &entry : view->entries()) {
        const MuxEntry *match = nullptr;

        for (const auto &wanted : *desired) {
            if (entry.key.selector == wanted.key.selector && entry.key.context.matches(wanted.key.context) &&
                entry.key.symbol == wanted.key.symbol) {
                match = &wanted;
                break;
            }
        }

        if (!match) {
            fail(c, {QueueError::INVALID_STATE, {}});

            return true;
        }

        Mapping m{entry.key, entry.membership, match->membership, entry.historyBound, view->revision(), {}};

        if (history) {
            auto status = history->rangeStatus(c.history, entry.key, work.history);

            if (!status) {
                if (status.error().error == collectors::HistoryError::WORK_LIMIT) {
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});
                }

                fail(c, sourceFailure(status.error().error));

                return true;
            }

            m.sourceEpoch = status->epoch;
        }

        c.mapping[c.mappingCount++] = std::move(m);
    }

    c.sync = false;
    c.mappingRevision = c.desiredRevision;
    publishState(c);

    return true;
}

std::expected<bool, QueueFailure> MuxQueue::State::command(Client &c, QueueWork &work) {
    auto &p = *c.port;

    if (!c.closing && (c.sync || (stream && (unionDirty || pendingPhase == Pending::ROUTING)))) {
        return false;
    }

    detail::QueueCommand *cmd = nullptr;
    {
        std::lock_guard lock(p.mutex);

        if (!p.size) {
            return false;
        }

        cmd = &p.commands[p.head];
    }
    debit(work.commands, 1);
    spend(work, sum(cmd->count, 4), 0, 0,
          sum(sizeof(MuxCommandResult),
              sum(sizeof(Client),
                  product(cmd->count, sum(sizeof(core::SubscriptionKey), sizeof(core::SubscriptionRequest))))));
    MuxCommandResult result{cmd->completion->id, CommandStatus::CANCELLED_CLOSING, interest.revision(), false, {}};

    if (!c.closing && !p.stop.load()) {
        const auto input = std::span<const core::SubscriptionRequest>(cmd->requests.data(), cmd->count);
        const auto keys = std::span<const core::SubscriptionKey>(cmd->keys.data(), cmd->count);
        std::expected<MuxChange, MuxError> applied = std::unexpected(MuxError::INVALID_STATE);

        switch (cmd->kind) {
        case detail::QueueCommandKind::ADD:
            applied = interest.add(c.interest, input, work.interest);
            break;
        case detail::QueueCommandKind::REMOVE:
            applied = interest.remove(c.interest, keys, work.interest);
            break;
        case detail::QueueCommandKind::REPLACE:
            applied = interest.replace(c.interest, input, work.interest);
            break;
        case detail::QueueCommandKind::CLEAR:
            applied = interest.clear(c.interest, work.interest);
            break;
        }

        if (!applied && applied.error() == MuxError::WORK_LIMIT) {
            return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});
        }

        if (applied) {
            result.status = CommandStatus::APPLIED;
            result.revision = applied->revision;
            result.changed = applied->changed;

            if (applied->changed) {
                c.desiredRevision = applied->revision;
                c.sync = true;

                if (stream) {
                    unionDirty = true;
                    unionOrigin = p.slot;
                }

                publishState(c);
            }

            count(c.counts.commandsApplied, 1, c.counts.saturated);
            count(counts.commandsApplied, 1, counts.saturated);
        } else {
            result.status = CommandStatus::REJECTED;
            result.error = applied.error();
            count(c.counts.commandsRejected, 1, c.counts.saturated);
            count(counts.commandsRejected, 1, counts.saturated);
        }
    } else {
        count(c.counts.commandsCancelled, 1, c.counts.saturated);
        count(counts.commandsCancelled, 1, counts.saturated);
    }

    auto completion = cmd->completion;
    {
        std::lock_guard lock(completion->mutex);
        completion->result = result;
    }
    {
        std::lock_guard lock(p.mutex);

        for (std::size_t i = 0; i < cmd->count; ++i) {
            cmd->requests[i] = {};
            cmd->keys[i] = {};
        }

        cmd->count = 0;
        cmd->completion.reset();
        cmd->occupied = false;
        p.head = (p.head + 1) % p.commands.size();
        --p.size;
    }

    return true;
}

std::expected<bool, QueueFailure> MuxQueue::State::route(Client &c, QueueWork &work) {
    if (stream || c.sync || c.sourceClosed || (c.closing && c.stopMode == 2)) {
        return false;
    }

    spend(work, 4, 0, 0, sum(sizeof(Client), sizeof(QueueCounts)));
    try {
        std::shared_ptr<const detail::QueueRecord> value;
        bool unwanted = false, overflow = false;

        if (ticker) {
            auto offer = ticker->front(c.ticker, work.ticker);

            if (!offer) {
                if (offer.error() == collectors::TickerError::WORK_LIMIT) {
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});
                }

                if (offer.error() == collectors::TickerError::STALE) {
                    spend(work, clients.size(), 0, 0, product(clients.size(), sizeof(Client)));
                    failAll(sourceFailure(offer.error()));
                } else {
                    fail(c, sourceFailure(offer.error()));
                }

                return true;
            }

            if (!*offer) {
                return false;
            }

            const auto &o = **offer;
            const auto &r = *o.value.sourceRecord;
            auto *m = mapping(c, {r.context, r.symbol}, work);
            unwanted = !m;

            if (m) {
                try {
                    const RawSourceMetadata raw{r.senderId,    r.cipher,    r.eventFlags,
                                                r.removalTime, r.fieldTime, o.value.bindingGeneration};
                    value = record(c, {r.context, r.symbol}, {}, o.deliveryFlags, r.values, r.typedValues, m->mux,
                                   o.revision, raw, {}, work);
                } catch (QueueError e) {
                    if (e != QueueError::RESOURCE_LIMIT) {
                        throw;
                    }

                    overflow = true;
                }
            }

            auto consumed = ticker->pop(c.ticker, work.ticker);

            if (!consumed) {
                if (consumed.error() == collectors::TickerError::WORK_LIMIT)
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

                fail(c, sourceFailure(consumed.error()));

                return true;
            }
        } else {
            auto offer = history->front(c.history, work.history);

            if (!offer) {
                if (offer.error().error == collectors::HistoryError::WORK_LIMIT) {
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});
                }

                if (offer.error().error == collectors::HistoryError::STALE) {
                    spend(work, clients.size(), 0, 0, product(clients.size(), sizeof(Client)));
                    failAll(sourceFailure(offer.error().error));
                } else {
                    fail(c, sourceFailure(offer.error().error));
                }

                return true;
            }

            if (!*offer) {
                return false;
            }

            const auto &o = **offer;
            const auto &r = o.record;
            auto *m = mapping(c, {r.context, r.symbol}, work);
            unwanted = !m;

            if (m) {
                try {
                    std::optional<RawSourceMetadata> raw;
                    std::span<const std::int32_t> legacy;

                    if (o.sourceRecord) {
                        if (!o.trigger)
                            throw QueueError::INVALID_STATE;

                        const auto &s = *o.sourceRecord;
                        raw =
                            RawSourceMetadata{s.senderId,    s.cipher,    s.eventFlags,
                                              s.removalTime, s.fieldTime, o.trigger ? o.trigger->bindingGeneration : 0};
                        legacy = s.values;
                    }

                    value = record(c, {r.context, r.symbol}, r.rawTime, r.eventFlags, legacy, r.fields, m->mux,
                                   o.admissionRevision, raw, o.trigger, work);
                } catch (QueueError e) {
                    if (e != QueueError::RESOURCE_LIMIT) {
                        throw;
                    }

                    overflow = true;
                }
            }

            auto consumed = history->pop(c.history, work.history);

            if (!consumed) {
                if (consumed.error().error == collectors::HistoryError::WORK_LIMIT)
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

                fail(c, sourceFailure(consumed.error().error));

                return true;
            }
        }

        count(counts.collectorAcknowledged, 1, counts.saturated);
        count(c.counts.collectorAcknowledged, 1, c.counts.saturated);

        if (overflow) {
            fail(c, {QueueError::RESOURCE_LIMIT, {}});
            count(c.counts.discarded, 1, c.counts.saturated);
            count(counts.discarded, 1, counts.saturated);
        } else if (unwanted) {
            count(c.counts.withdrawn, 1, c.counts.saturated);
            count(counts.withdrawn, 1, counts.saturated);
        } else {
            enqueue(c, std::move(value));
        }

        return true;
    } catch (QueueFailure e) {
        return std::unexpected(e);
    } catch (QueueError e) {
        return std::unexpected(QueueFailure{e, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(QueueFailure{QueueError::ALLOCATION_FAILURE, {}});
    }
}

std::expected<bool, QueueFailure> MuxQueue::State::routeStream(QueueWork &work) {
    if (!stream || pendingPhase != Pending::ROUTING) {
        return false;
    }

    struct Clear {
        State &s;
        bool active = false;

        ~Clear() {
            if (active) {
                for (auto &record : s.candidates)
                    record.reset();

                for (auto &offer : s.offers)
                    offer = {};
            }
        }
    } clear{*this};

    try {
        const auto cells = product(clients.size(), limits.routeRecords);
        spend(work, sum(product(cells, 4), sum(product(clients.size(), 4), limits.routeRecords)), 0, 0,
              sum(product(cells, sum(product(sizeof(std::shared_ptr<const detail::QueueRecord>), 4),
                                     sum(sizeof(std::size_t), sizeof(QueueCounts)))),
                  sum(product(clients.size(), sum(sizeof(Client), product(sizeof(QueueCounts), 2))),
                      product(limits.routeRecords, sizeof(collectors::StreamDelivery)))));
        clear.active = true;
        std::fill(candidateCounts.begin(), candidateCounts.end(), 0);
        std::fill(candidateFailure.begin(), candidateFailure.end(), 0);
        auto prepared = stream->prepareBatch(offers, work.stream);

        if (!prepared) {
            if (prepared.error() == collectors::StreamError::WORK_LIMIT)
                return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

            return std::unexpected(sourceFailure(prepared.error()));
        }

        for (std::size_t v = 0; v < clients.size(); ++v) {
            auto *c = clients[v].get();

            if (!c || c->sourceClosed || (c->closing && c->stopMode == 2) || c->port->stop.load() == 2) {
                continue;
            }

            for (const auto &offer : *prepared) {
                const auto &r = *offer.record;
                auto selected = interest.select(c->interest, {r.context, r.symbol}, {}, work.interest);

                if (!selected) {
                    return std::unexpected(sourceFailure(selected.error()));
                }

                if (!*selected) {
                    continue;
                }

                const auto index = candidateCounts[v]++;

                if (candidateFailure[v]) {
                    continue;
                }

                try {
                    const RawSourceMetadata raw{r.senderId,    r.cipher,    r.eventFlags,
                                                r.removalTime, r.fieldTime, offer.bindingGeneration};
                    candidates[v * limits.routeRecords + index] =
                        record(*c, {r.context, r.symbol}, {}, r.eventFlags, r.values, r.typedValues, **selected,
                               offer.revision, raw, {}, work);
                } catch (QueueError e) {
                    if (e != QueueError::RESOURCE_LIMIT) {
                        throw;
                    }

                    candidateFailure[v] = 1;

                    for (std::size_t i = 0; i < index; ++i) {
                        candidates[v * limits.routeRecords + i].reset();
                    }
                }
            }
        }

        auto acknowledged = stream->acknowledgeBatch(work.stream);

        if (!acknowledged) {
            if (acknowledged.error() == collectors::StreamError::WORK_LIMIT)
                return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

            return std::unexpected(sourceFailure(acknowledged.error()));
        }

        count(counts.collectorAcknowledged, acknowledged->records, counts.saturated);
        count(counts.withdrawn, acknowledged->withdrawn, counts.saturated);

        for (std::size_t v = 0; v < clients.size(); ++v) {
            auto *c = clients[v].get();

            if (!c)
                continue;

            count(c->counts.collectorAcknowledged, candidateCounts[v], c->counts.saturated);

            if (candidateFailure[v]) {
                fail(*c, {QueueError::RESOURCE_LIMIT, {}});
                count(c->counts.discarded, candidateCounts[v], c->counts.saturated);
                count(counts.discarded, candidateCounts[v], counts.saturated);
            } else {
                for (std::size_t i = 0; i < candidateCounts[v]; ++i) {
                    enqueue(*c, std::move(candidates[v * limits.routeRecords + i]));
                }
            }
        }

        pendingRecords = stream->queuedRecords();

        if (!pendingRecords) {
            pending.reset();
            pendingPhase = Pending::NONE;
        }

        return true;
    } catch (QueueFailure e) {
        return std::unexpected(e);
    } catch (QueueError e) {
        return std::unexpected(QueueFailure{e, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(QueueFailure{QueueError::ALLOCATION_FAILURE, {}});
    }
}

std::expected<bool, QueueFailure> MuxQueue::State::callback(Client &c, QueueWork &work) {
    if (!c.size || c.sync || c.paused || !c.consumer || reset.phase != ResetPhase::NONE ||
        c.port->advertisedNotice.load() || (c.closing && c.stopMode == 2) || c.port->stop.load() == 2) {
        return false;
    }

    try {
        debit(work.callbacks, 1);
        const auto maximum = std::min(c.port->budget->limits.batchRecords, c.size);
        spend(work, sum(product(c.size, 3), maximum), 0, 0,
              sum(sum(sizeof(Client), product(sizeof(QueueCounts), 2)),
                  product(product(c.size, 3),
                          sum(sizeof(std::shared_ptr<const detail::QueueRecord>), sizeof(std::uint8_t)))));
        std::size_t prefix = 0, selected = 0;

        // Read-only selection first: work failure cannot withdraw an earlier queue prefix.
        for (; prefix < c.size && selected < maximum; ++prefix) {
            const auto &r = c.records[(c.head + prefix) % c.records.size()];
            auto current = interest.isCurrent(c.interest, r->view.membership, work.interest);

            if (!current) {
                return std::unexpected(sourceFailure(current.error()));
            }

            c.selection[prefix] = *current ? 1 : 0;

            if (*current) {
                ++selected;
            }
        }

        if (!selected) {
            for (std::size_t i = 0; i < prefix; ++i) {
                drop(c, true);
            }

            return prefix != 0;
        }

        const auto bytes =
            sum(c.port->budget->context->controlBytes[static_cast<std::size_t>(detail::ControlKind::BATCH)],
                product(selected, sum(sizeof(std::shared_ptr<const detail::QueueRecord>), sizeof(MuxRecordView))));
        auto &budget = *c.port->budget;
        const auto &l = budget.limits;
        bound(bytes, l.batchBytes);
        // The enforced byte ceiling covers actual vector requests and their eventual disposal without an escaping work
        // pointer.
        spend(work, product(selected, 3), 0, 0, l.batchBytes);
        spend(work, 0, 0, 0, l.batchBytes);
        auto batch = detail::shared<detail::QueueBatch>(
            detail::clientScope(c.port->budget, detail::AllocationTag::BATCH), c.port->budget);
        batch->roots.reserve(selected);
        batch->views.reserve(selected); // Reuse the complete preflight selection; no fallible matching remains.

        for (std::size_t i = 0; i < prefix; ++i) {
            const auto &r = c.records[(c.head + i) % c.records.size()];

            if (c.selection[i]) {
                batch->roots.push_back(r);
                batch->views.push_back(r->view);
            }
        }

        // No fallible action remains before queue consumption and invocation.
        std::size_t index = 0;

        for (std::size_t i = 0; i < prefix; ++i) {
            const auto &r = c.records[c.head];
            const bool eligible = index < batch->roots.size() && r == batch->roots[index];

            if (eligible) {
                ++index;
                c.records[c.head].reset();
                c.head = (c.head + 1) % c.records.size();
                --c.size;
            } else {
                drop(c, true);
            }
        }

        count(c.counts.callbackStartedBatches, 1, c.counts.saturated);
        count(c.counts.callbackStartedRecords, selected, c.counts.saturated);
        count(counts.callbackStartedBatches, 1, counts.saturated);
        count(counts.callbackStartedRecords, selected, counts.saturated);
        auto guard = c.consumer;
        BorrowedMuxBatch borrowed;
        borrowed.state_ = batch;
        c.port->resume.exchange(false, std::memory_order_acq_rel);
        executing = true;
        try {
            const auto action = guard->onBatch(borrowed);
            executing = false;
            c.paused = action == BatchAction::PAUSE && !c.port->resume.exchange(false, std::memory_order_acq_rel);
            count(c.counts.callbackCompletedBatches, 1, c.counts.saturated);
            count(c.counts.callbackCompletedRecords, selected, c.counts.saturated);
            count(counts.callbackCompletedBatches, 1, counts.saturated);
            count(counts.callbackCompletedRecords, selected, counts.saturated);

            if (c.paused && c.closing && c.stopMode == 1) {
                c.hadGap = true;
                c.stopMode = 2;
                stop(*c.port, 2);
            }
        } catch (...) {
            executing = false;
            count(c.counts.callbackFailedBatches, 1, c.counts.saturated);
            count(c.counts.callbackFailedRecords, selected, c.counts.saturated);
            count(counts.callbackFailedBatches, 1, counts.saturated);
            count(counts.callbackFailedRecords, selected, counts.saturated);
            fail(c, {QueueError::CALLBACK_FAILURE, {}});
        }
        publishState(c);

        return true;
    } catch (QueueFailure e) {
        return std::unexpected(e);
    } catch (QueueError e) {
        if (e == QueueError::RESOURCE_LIMIT) {
            fail(c, {e, {}});

            return true;
        }

        return std::unexpected(QueueFailure{e, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(QueueFailure{QueueError::ALLOCATION_FAILURE, {}});
    }
}

std::expected<bool, QueueFailure> MuxQueue::State::close(Client &c, QueueWork &work) {
    if (!c.closing) {
        return false;
    }

    if (c.port->state.load() == QueueClientState::TERMINAL) {
        return false;
    }

    if (c.stopMode == 1 && pendingPhase != Pending::NONE) {
        return false;
    }

    bool queued = false;
    {
        std::lock_guard lock(c.port->mutex);
        queued = c.port->size != 0;
    }

    if (queued) {
        return command(c, work);
    }

    spend(work, sum(c.mapping.size(), 4), 0, 0,
          sum(product(c.mapping.size(), sizeof(Mapping)), sum(sizeof(Client), sizeof(QueueTerminal))));

    if (!c.sourceClosed) {
        bool plannedGap = c.hadGap, plannedUnknown = c.unknownEligibility;

        if (c.stopMode == 1) {
            if (c.paused) {
                c.hadGap = true;
                c.stopMode = 2;
                stop(*c.port, 2);

                return true;
            }

            if (!stream) {
                auto routed = route(c, work);

                if (!routed || *routed)
                    return routed;

                if (history) {
                    for (std::size_t i = 0; i < c.mappingCount; ++i) {
                        auto status = history->rangeStatus(c.history, c.mapping[i].key, work.history);

                        if (!status) {
                            if (status.error().error == collectors::HistoryError::WORK_LIMIT)
                                return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

                            fail(c, sourceFailure(status.error().error));
                            break;
                        }

                        if (!status->completeRequestedRange) {
                            plannedGap = true;
                        }
                    }
                }
            }
        }

        if (history && c.stopMode == 2) {
            for (std::size_t i = 0; i < c.mappingCount; ++i) {
                auto status = history->rangeStatus(c.history, c.mapping[i].key, work.history);

                if (!status && status.error().error == collectors::HistoryError::WORK_LIMIT)
                    return std::unexpected(sourceFailure(status.error().error));

                if (!status || !status->completeRequestedRange)
                    plannedGap = true;

                if (!status || status->initialPending)
                    plannedUnknown = true;
            }
        }

        if (ticker) {
            const auto pending = ticker->pendingKeys(c.ticker);
            auto closed = ticker->closeView(c.ticker, work.ticker);

            if (!closed) {
                if (closed.error() == collectors::TickerError::WORK_LIMIT)
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

                spend(work, clients.size(), 0, 0, product(clients.size(), sizeof(Client)));
                failAll(sourceFailure(closed.error()));
                c.retireBlocked = true;
            } else if (pending) {
                count(c.counts.discarded, *pending, c.counts.saturated);
                count(counts.discarded, *pending, counts.saturated);

                if (*pending) {
                    c.hadGap = true;
                    c.epochGap = true;
                }
            } else {
                plannedUnknown = true;
                plannedGap = true;
            }
        }

        if (history) {
            const auto pending = history->pendingCount(c.history);
            auto closed = history->closeView(c.history, work.history);

            if (!closed) {
                if (closed.error().error == collectors::HistoryError::WORK_LIMIT)
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

                spend(work, clients.size(), 0, 0, product(clients.size(), sizeof(Client)));
                failAll(sourceFailure(closed.error().error));
                c.retireBlocked = true;
            } else {
                count(c.counts.discarded, pending, c.counts.saturated);
                count(counts.discarded, pending, counts.saturated);

                if (pending) {
                    c.hadGap = true;
                    c.epochGap = true;
                }
            }
        }

        c.hadGap = c.hadGap || plannedGap;
        c.unknownEligibility = c.unknownEligibility || plannedUnknown;

        for (auto &m : c.mapping) {
            m = Mapping{};
        }

        c.mappingCount = 0;
        c.sourceClosed = true;

        return true;
    }

    if (c.size) {
        if (c.stopMode == 1) {
            return false;
        }

        const auto n = std::min(c.size, limits.cleanupRecords);
        spend(work, n, 0, 0, product(n, sizeof(std::shared_ptr<const detail::QueueRecord>)));
        c.hadGap = true;

        for (std::size_t i = 0; i < n; ++i) {
            drop(c, false);
        }

        return true;
    }

    if (!c.muxClosed && !c.retireBlocked) {
        spend(work, clients.size(), 0, 0, product(clients.size(), sizeof(Client)));
        auto cleared = interest.clear(c.interest, work.interest);

        if (!cleared) {
            if (cleared.error() == MuxError::WORK_LIMIT)
                return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

            c.retireBlocked = true;
            failAll(sourceFailure(cleared.error()));

            return true;
        }

        c.muxClosed = true;

        if (stream && cleared->changed) {
            unionDirty = true;
            unionOrigin = c.port->slot;

            for (auto &peer : clients) {
                if (peer && !peer->closing) {
                    peer->sync = true;
                    publishState(*peer);
                }
            }
        }

        return true;
    }

    // Callable retirement is owner-serialized and deliberately outside every port lock.
    executing = true;
    c.consumer.reset();
    executing = false;
    c.stopMode = std::max(c.stopMode, c.port->stop.load(std::memory_order_acquire));
    QueueTerminal result;
    result.outcome = c.primary ? TerminalOutcome::FAILED
                               : (c.stopMode == 1 && !c.hadGap ? TerminalOutcome::DRAINED : TerminalOutcome::ABORTED);
    result.primary = c.primary;
    result.secondary = c.secondary;
    result.counts = c.counts;
    result.authoritativeRevision = c.desiredRevision;

    if (result.outcome == TerminalOutcome::ABORTED)
        count(abortedRegistrations, 1, counts.saturated);

    hadGap = hadGap || c.hadGap;
    unknownEligibility = unknownEligibility || c.unknownEligibility;
    result.epoch = epoch();
    result.unknownEligibility = c.unknownEligibility;
    result.hadGap = c.hadGap;
    result.unknownUpstreamLoss = c.hadGap;
    result.reset = reset;
    {
        std::lock_guard lock(c.port->mutex);
        c.port->terminal = result;
        c.port->state = QueueClientState::TERMINAL;
    }

    return true;
}

std::expected<void, QueueFailure> MuxQueue::validateBinding(const core::LocalSchema &schema,
                                                            core::SubscriptionFamily family,
                                                            core::SubscriptionSelectorPolicy policy,
                                                            QueueWork &work) const {
    try {
        debit(work.operations, 1);

        if (!state_)
            throw QueueError::INVALID_STATE;

        const auto &s = *state_;

        if (s.executing)
            throw QueueError::REENTRANT;

        if (s.state != QueueState::OPEN || s.stopMode)
            throw QueueError::CLOSING;

        if (s.reset.phase != ResetPhase::NONE || s.epochGeneration != INITIAL_QUEUE_EPOCH || s.sourceIdentity ||
            s.pendingPhase != State::Pending::NONE)
            throw QueueError::INVALID_STATE;

        spend(work, 2, 0, 0, sizeof(core::SchemaIdentity));

        if (!s.schemaIdentity.matches(schema.identity()))
            throw QueueError::INVALID_CONTEXT;

        if (s.family != family || s.interest.selectorPolicy() != policy)
            throw QueueError::INVALID_CONFIGURATION;

        return {};
    } catch (QueueError error) {
        return std::unexpected(QueueFailure{error, {}});
    }
}

std::expected<void, QueueFailure> MuxQueue::validateReceiverBinding(const core::LocalSchema &schema,
                                                                    core::SubscriptionFamily family,
                                                                    core::SubscriptionSelectorPolicy policy,
                                                                    QueueWork &work) const {
    try {
        debit(work.operations, 1);

        if (!state_)
            throw QueueError::INVALID_STATE;

        const auto &s = *state_;

        if (s.executing)
            throw QueueError::REENTRANT;

        if (s.state != QueueState::OPEN || s.stopMode)
            throw QueueError::CLOSING;

        // New receivers share the immutable schema even after source admission or reset.
        // Recovery adoption retains validateBinding's separate pristine-source requirement.
        spend(work, 2, 0, 0, sizeof(core::SchemaIdentity));

        if (!s.schemaIdentity.matches(schema.identity()))
            throw QueueError::INVALID_CONTEXT;

        if (s.family != family || s.interest.selectorPolicy() != policy)
            throw QueueError::INVALID_CONFIGURATION;

        return {};
    } catch (QueueError error) {
        return std::unexpected(QueueFailure{error, {}});
    }
}

QueueCapacity MuxQueue::capacity() const noexcept {
    return state_ ? state_->capacity : QueueCapacity{};
}

std::expected<ResetAdmission, QueueFailure> MuxQueue::resetSource(QueueWork &work) {
    try {
        debit(work.operations, 1);

        if (!state_)
            throw QueueError::INVALID_STATE;

        auto &s = *state_;

        if (s.executing)
            throw QueueError::REENTRANT;

        if (s.state != QueueState::OPEN || s.reset.phase == ResetPhase::FAILED)
            throw QueueError::CLOSING;

        spend(work, 2, 0, 0, sizeof(ResetStatus));

        if (s.resetRequestSequence == std::numeric_limits<std::uint64_t>::max() ||
            s.epochGeneration == std::numeric_limits<std::uint64_t>::max() ||
            s.clients.size() > (std::numeric_limits<std::uint64_t>::max() - s.noticeSequence) / 2)
            throw QueueError::IDENTITY_EXHAUSTED;

        const auto sequence = ++s.resetRequestSequence;

        if (s.reset.phase != ResetPhase::NONE) {
            s.reset.requestSequence = sequence;
            s.reset.coalescedRequests = sum(s.reset.coalescedRequests, 1);

            return ResetAdmission{ResetAdmissionKind::COALESCED, sequence};
        }

        s.reset = ResetStatus{};
        s.reset.phase = ResetPhase::FREEZE_OLD;
        s.reset.requestSequence = sequence;
        s.resetFrom = s.epoch();
        s.resetCursor = 0;
        s.canAdmit = false;

        return ResetAdmission{ResetAdmissionKind::ACCEPTED, sequence};
    } catch (QueueError e) {
        return std::unexpected(QueueFailure{e, {}});
    }
}

std::expected<QueueProgress, QueueFailure> MuxQueue::State::resetStep(QueueWork &work) {
    spend(work, 3, 0, 0, sum(sizeof(ResetStatus), sizeof(QueueProgress)));
    auto progress = [this](QueueProgressKind kind) {
        return QueueProgress{kind, state};
    };
    auto failed = [this, &work](QueueFailure error) -> std::expected<QueueProgress, QueueFailure> {
        reset.failure = error;
        reset.phase = ResetPhase::FAILED;

        for (auto &c : clients) {
            if (!c || c->port->state.load() == QueueClientState::TERMINAL)
                continue;

            QueueResetNotice notice;
            notice.sequence = ++noticeSequence;
            notice.fromEpoch = resetFrom;

            if (reset.sourceCommitted)
                notice.toEpoch = epoch();

            notice.sourceCommitted = reset.sourceCommitted;
            notice.globalAckPending = reset.sourceCommitted && !reset.globalAcknowledged;
            notice.firstCause = error;
            notice.unknownEligibility = reset.unknownEligibility;
            {
                std::lock_guard lock(c->port->mutex);

                if (c->port->notice) {
                    notice.fromEpoch = c->port->notice->fromEpoch;
                }

                c->port->notice = notice;
                c->port->advertisedNotice = notice.sequence;
            }
        }

        failAll(error);

        return QueueProgress{QueueProgressKind::CONTROL, state};
    };

    switch (reset.phase) {
    case ResetPhase::FREEZE_OLD:

        if (resetCursor < clients.size()) {
            const auto v = resetCursor++;
            auto &c = clients[v];
            resetDiscards[v] = 0;

            if (c && c->port->state.load() != QueueClientState::TERMINAL) {
                c->hadGap = true;
                c->epochGap = true;
                publishState(*c);
            }

            return progress(QueueProgressKind::CONTROL);
        }

        resetCursor = 0;
        reset.phase = ResetPhase::DISCARD_OLD_PENDING;

        return progress(QueueProgressKind::CONTROL);
    case ResetPhase::DISCARD_OLD_PENDING:
        releasePending(work, true);
        reset.phase = ResetPhase::DISCARD_OLD_QUEUES;

        return progress(QueueProgressKind::RELEASED);
    case ResetPhase::DISCARD_OLD_QUEUES:

        if (resetCursor < clients.size()) {
            auto &c = clients[resetCursor];

            if (c && c->size) {
                const auto n = std::min(c->size, limits.cleanupRecords);
                spend(work, product(n, 2), 0, 0,
                      product(n, sum(sizeof(std::shared_ptr<const detail::QueueRecord>), sizeof(std::uint8_t))));

                for (std::size_t i = 0; i < n; ++i) {
                    auto current = interest.isCurrent(
                        c->interest, c->records[(c->head + i) % c->records.size()]->view.membership, work.interest);

                    if (!current)
                        return std::unexpected(sourceFailure(current.error()));

                    c->selection[i] = *current ? 1 : 0;
                }

                for (std::size_t i = 0; i < n; ++i) {
                    drop(*c, !c->selection[i]);
                }

                resetDiscards[resetCursor] = sum(resetDiscards[resetCursor], n);

                return progress(QueueProgressKind::CLEANUP);
            }

            ++resetCursor;

            return progress(QueueProgressKind::CONTROL);
        }

        resetCursor = 0;
        reset.phase = ResetPhase::SOURCE_RESET_PENDING;

        return progress(QueueProgressKind::CONTROL);
    case ResetPhase::SOURCE_RESET_PENDING: {
        // Reserve every fixed commit/failure recording write BEFORE irreversible source reset.
        spend(work, sum(product(clients.size(), 2), 4), 0, 0,
              sum(product(clients.size(), sum(sizeof(Client), sizeof(QueueResetNotice))),
                  sum(sum(sizeof(ResetStatus), sizeof(QueueIdentity)),
                      sourceIdentity ? sourceIdentity->releaseBytes() : 0)));
        std::uint64_t sequence = 0;
        std::size_t discarded = 0;
        bool sourceSaturated = false, unknownLoss = true;

        if (stream) {
            auto result = stream->resetSource(work.stream);

            if (!result) {
                if (result.error() == collectors::StreamError::WORK_LIMIT)
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

                return failed(sourceFailure(result.error()));
            }

            sequence = result->sequence;
            discarded = result->discarded;
            sourceSaturated = result->countsSaturated;
            unknownLoss = result->unknownLoss;
        } else if (ticker) {
            auto result = ticker->reset(work.ticker);

            if (!result) {
                if (result.error() == collectors::TickerError::WORK_LIMIT)
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

                return failed(sourceFailure(result.error()));
            }

            sequence = result->sequence;
            discarded = result->discarded;
            sourceSaturated = result->saturated;
            unknownLoss = result->unknownLoss;
        } else {
            auto result = history->reset(work.history);

            if (!result) {
                if (result.error().error == collectors::HistoryError::WORK_LIMIT)
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

                return failed(sourceFailure(result.error().error));
            }

            sequence = result->sequence;
            discarded = result->discardedPending;
            sourceSaturated = result->countsSaturated;
            unknownLoss = result->unknownLoss;
        }

        reset.discardedSourceProjections = discarded;
        reset.sourceCountsSaturated = sourceSaturated;
        reset.unknownSourceLoss = unknownLoss;
        reset.unknownEligibility = reset.unknownEligibility || discarded != 0 || sourceSaturated;
        count(counts.discarded, discarded, counts.saturated);
        counts.saturated = counts.saturated || sourceSaturated;
        reset.actualSourceSequence = sequence;
        reset.sourceCommitted = true;
        // STREAM has no extra collector ACK: its synchronous source barrier is already complete.
        reset.globalAcknowledged = stream != nullptr;
        ++epochGeneration;
        sourceIdentity.reset();
        reset.phase = ResetPhase::NOTICE_PUBLICATION_PENDING;
        resetCursor = 0;

        for (auto &c : clients) {
            if (c && !c->closing) {
                c->sync = true;
                publishState(*c);
            }
        }

        return progress(QueueProgressKind::CONTROL);
    }
    case ResetPhase::NOTICE_PUBLICATION_PENDING:

        if (resetCursor < clients.size()) {
            auto &c = clients[resetCursor];

            if (c && c->port->state.load() != QueueClientState::TERMINAL) {
                spend(work, 2, 0, 0, sizeof(QueueResetNotice));
                QueueResetNotice notice;
                notice.sequence = ++noticeSequence;
                notice.fromEpoch = resetFrom;
                notice.toEpoch = epoch();
                notice.knownDiscardedQueueRecords = resetDiscards[resetCursor];
                notice.sourceCommitted = true;
                notice.globalAckPending = !reset.globalAcknowledged;
                notice.unknownEligibility = reset.unknownEligibility;
                c->unknownEligibility = c->unknownEligibility || reset.unknownEligibility;
                notice.skippedResets = reset.coalescedRequests;
                {
                    std::lock_guard lock(c->port->mutex);

                    if (c->port->notice) {
                        auto &old = *c->port->notice;
                        notice.fromEpoch = old.fromEpoch;
                        count(notice.knownDiscardedQueueRecords, old.knownDiscardedQueueRecords,
                              notice.countsSaturated);
                        count(notice.skippedResets, sum(old.skippedResets, 1), notice.countsSaturated);
                        notice.firstCause = old.firstCause;
                        notice.secondaryCause = old.secondaryCause;
                        notice.unknownEligibility = notice.unknownEligibility || old.unknownEligibility;
                    }

                    c->port->notice = notice;
                    c->port->advertisedNotice.store(notice.sequence, std::memory_order_release);
                }
                ++reset.publishedClients;
            }

            ++resetCursor;

            return progress(QueueProgressKind::CONTROL);
        }

        reset.phase = ResetPhase::GLOBAL_ACK_PENDING;

        return progress(QueueProgressKind::CONTROL);
    case ResetPhase::GLOBAL_ACK_PENDING: {
        // TICKER/HISTORY acknowledge their source resets; STREAM completed its barrier at commit.
        spend(work, sum(product(clients.size(), 2), 2), 0, 0,
              sum(sizeof(ResetStatus), product(clients.size(), sum(sizeof(Client), sizeof(QueueResetNotice)))));

        if (ticker) {
            auto result = ticker->acknowledgeReset(*reset.actualSourceSequence, work.ticker);

            if (!result) {
                if (result.error() == collectors::TickerError::WORK_LIMIT)
                    return std::unexpected(QueueFailure{QueueError::WORK_LIMIT, {}});

                return failed(sourceFailure(result.error()));
            }
        } else if (history) {
            auto result = history->acknowledgeReset(*reset.actualSourceSequence);

            if (!result)
                return failed(sourceFailure(result.error().error));
        }

        reset.globalAcknowledged = true;
        reset.phase = ResetPhase::NONE;

        for (auto &c : clients) {
            if (c) {
                {
                    std::lock_guard lock(c->port->mutex);

                    if (c->port->notice)
                        c->port->notice->globalAckPending = false;
                    else if (!c->closing)
                        c->epochGap = false;
                }
                publishState(*c);
            }
        }

        return progress(QueueProgressKind::CONTROL);
    }
    case ResetPhase::FAILED:
        return progress(QueueProgressKind::CONTROL);
    case ResetPhase::NONE:
        return progress(QueueProgressKind::IDLE);
    }

    return progress(QueueProgressKind::IDLE);
}

std::expected<QueueAdmission, QueueFailure> MuxQueue::admit(protocol::LiveEvent &&event, QueueWork &work) {
    try {
        debit(work.operations, 1);

        if (!state_)
            throw QueueError::INVALID_STATE;

        auto &s = *state_;

        if (s.executing)
            throw QueueError::REENTRANT;

        if (s.state != QueueState::OPEN || s.stopMode)
            throw QueueError::CLOSING;

        if (s.pendingPhase != State::Pending::NONE || !s.canAdmit || s.reset.phase != ResetPhase::NONE || s.unionDirty)
            throw QueueError::BUSY;

        spend(work, s.clients.size() + 2, 0, 0, sizeof(protocol::LiveEvent));

        for (const auto &c : s.clients) {
            if (c && ((c->closing && !c->sourceClosed) || (!c->closing && c->sync)))
                throw QueueError::BUSY;
        }

        if (!event.residentBytes())
            throw QueueError::INVALID_EVENT;

        const bool data = event.kind() == protocol::LiveEventKind::DATA;
        const auto type = s.family == core::SubscriptionFamily::STREAM
                              ? 15
                              : (s.family == core::SubscriptionFamily::TICKER ? 10 : 20);

        if (data && event.messageType() != type)
            throw QueueError::INVALID_EVENT;

        if (event.records().size() > s.limits.sourceRecords)
            throw QueueError::RESOURCE_LIMIT;

        spend(work, product(event.records().size(), s.contexts.size()));

        for (const auto &record : event.records()) {
            bool matched = false;

            for (const auto &entry : s.contexts) {
                if (entry.id == record.localId && entry.context.matches(record.context)) {
                    matched = true;
                    break;
                }
            }

            if (!matched)
                throw QueueError::INVALID_CONTEXT;
        }

        const auto identity = event.snapshot().identity();

        if (s.sourceIdentity && !s.sourceIdentity->sameEpoch(identity))
            throw QueueError::INVALID_EPOCH;

        const auto n = event.records().size();
        const auto kind = data ? AdmissionKind::DATA : AdmissionKind::RELEASE_ONLY;
        s.sourceIdentity = identity;
        s.pending.emplace(std::move(event));
        s.pendingRecords = n;
        s.pendingPhase = data ? State::Pending::UNSTAGED : State::Pending::RELEASE_ONLY;
        s.canAdmit = false;
        count(s.counts.acceptedFrames, 1, s.counts.saturated);
        count(s.counts.acceptedRecords, n, s.counts.saturated);

        return QueueAdmission{kind, n, s.epoch()};
    } catch (QueueError e) {
        return std::unexpected(QueueFailure{e, {}});
    }
}

void MuxQueue::requestStop(StopMode mode) noexcept {
    if (!state_ || state_->terminal)
        return;

    auto &s = *state_;
    const auto requested = mode == StopMode::ABORT ? 2U : 1U;
    s.stopMode = std::max(s.stopMode, requested);
    s.ledger->stop.store(s.stopMode, std::memory_order_release);
    s.state = QueueState::CLOSING;
}

std::expected<QueueProgress, QueueFailure> MuxQueue::progress(QueueWork &work) {
    try {
        debit(work.operations, 1);

        if (!state_)
            throw QueueError::INVALID_STATE;

        auto &s = *state_;

        if (s.executing)
            throw QueueError::REENTRANT;

        if (s.terminal)
            return QueueProgress{QueueProgressKind::TERMINAL, QueueState::TERMINAL};

        spend(work, 3, 0, 0, sum(product(sum(sizeof(s.cursor), sizeof(s.phase)), 2), sizeof(s.canAdmit)));

        if (s.reset.phase != ResetPhase::NONE && s.reset.phase != ResetPhase::FAILED && !s.stopMode)
            return s.resetStep(work);

        auto result = [&s](QueueProgressKind kind) {
            return QueueProgress{kind, s.state};
        };

        if (s.stopMode == 2 && s.pendingPhase != State::Pending::NONE) {
            s.releasePending(work, true);

            return result(QueueProgressKind::RELEASED);
        }

        if (s.pendingPhase == State::Pending::RELEASE_ONLY) {
            s.releasePending(work, false);

            return result(QueueProgressKind::RELEASED);
        }

        if (s.pendingPhase == State::Pending::UNSTAGED) {
            spend(work, s.clients.size() + 2, 0, 0,
                  sum(sizeof(QueueCounts), product(s.clients.size(), sizeof(State::Client))));
            bool staged = false;

            if (s.stream) {
                auto r = s.stream->stage(std::move(*s.pending), work.stream);

                if (!r) {
                    if (r.error() == collectors::StreamError::WORK_LIMIT)
                        throw QueueError::WORK_LIMIT;

                    s.failAll(sourceFailure(r.error()));

                    return result(QueueProgressKind::CONTROL);
                }

                staged = r->selected != 0;
                s.pendingRecords = r->selected;
                s.pendingPhase = staged ? State::Pending::ROUTING : State::Pending::RELEASE_ONLY;
            } else if (s.ticker) {
                auto r = s.ticker->stage(std::move(*s.pending), work.ticker);

                if (!r) {
                    if (r.error() == collectors::TickerError::WORK_LIMIT)
                        throw QueueError::WORK_LIMIT;

                    s.failAll(sourceFailure(r.error()));

                    return result(QueueProgressKind::CONTROL);
                }

                staged = true;
                s.pendingPhase = s.pending->residentBytes() ? State::Pending::RELEASE_ONLY : State::Pending::NONE;
            } else {
                auto r = s.history->stage(std::move(*s.pending), work.history);

                if (!r) {
                    if (r.error().error == collectors::HistoryError::WORK_LIMIT)
                        throw QueueError::WORK_LIMIT;

                    s.failAll(sourceFailure(r.error().error));

                    return result(QueueProgressKind::CONTROL);
                }

                staged = true;
                s.pendingPhase = s.pending->residentBytes() ? State::Pending::RELEASE_ONLY : State::Pending::NONE;

                for (auto &c : s.clients) {
                    if (c && !c->closing && s.history->viewState(c->history) == collectors::HistoryViewState::FAILED) {
                        const auto failure = s.history->viewFailure(c->history);
                        s.fail(*c, {QueueError::RESOURCE_LIMIT,
                                    failure ? std::optional<QueueSourceError>{failure->cause} : std::nullopt});

                        if (failure) {
                            count(c->counts.discarded, failure->pendingItemsDiscarded, c->counts.saturated);
                            count(s.counts.discarded, failure->pendingItemsDiscarded, s.counts.saturated);
                            c->unknownEligibility = failure->initialIncomplete || failure->unknownFollowingLoss;
                        }
                    }
                }
            }

            if (s.pendingPhase == State::Pending::NONE) {
                s.pending.reset();
                s.pendingRecords = 0;
            }

            return result(staged ? QueueProgressKind::STAGED : QueueProgressKind::RELEASED);
        }

        const auto slots = s.clients.size();
        const auto v = slots ? s.cursor : 0;
        auto *c = slots ? s.clients[v].get() : nullptr;
        const auto activePhase = s.phase;
        auto advance = [&s, slots]() {
            if (++s.cursor >= std::max<std::size_t>(slots, 1)) {
                s.cursor = 0;

                if (++s.phase == 5) {
                    s.phase = 0;

                    if (s.pendingPhase == State::Pending::NONE && s.reset.phase == ResetPhase::NONE)
                        s.canAdmit = true;
                }
            }
        };

        // Control flags are fixed; a paused/empty slot is still a debited scheduler visit.
        if (c && c->port->state.load() != QueueClientState::TERMINAL) {
            spend(work, 3, 0, 0, sizeof(State::Client));
            const auto requested = std::max(c->port->stop.load(std::memory_order_acquire), s.stopMode);

            if (requested) {
                c->closing = true;
                c->stopMode = std::max(c->stopMode, requested);

                if (c->sync && c->stopMode == 1) {
                    c->hadGap = true;
                    c->stopMode = 2;
                }
            }

            if (c->paused && c->port->resume.exchange(false, std::memory_order_acq_rel) && !c->closing)
                c->paused = false;

            const auto ack = c->port->ack.load(std::memory_order_acquire);

            if (ack && ack == c->port->advertisedNotice.load(std::memory_order_acquire)) {
                std::lock_guard lock(c->port->mutex);

                if (c->port->notice && c->port->notice->sequence == ack) {
                    c->port->notice.reset();
                    c->port->advertisedNotice = 0;
                    c->port->ack = 0;

                    if (s.reset.globalAcknowledged && s.reset.phase == ResetPhase::NONE && !c->closing)
                        c->epochGap = false;
                }
            }

            s.publishState(*c);
        }

        std::expected<bool, QueueFailure> action = false;
        QueueProgressKind kind = QueueProgressKind::IDLE;

        if (activePhase == 0) {
            if (s.stream) {
                if (s.cursor == 0)
                    action = s.routeStream(work);
            } else if (c && !c->closing)
                action = s.route(*c, work);

            kind = QueueProgressKind::ROUTED;
        } else if (activePhase == 1) {
            if (c && s.pendingPhase != State::Pending::ROUTING && s.reset.phase != ResetPhase::FAILED)
                action = s.command(*c, work);

            kind = QueueProgressKind::COMMAND;
        } else if (activePhase == 2) {
            if (c)
                action = s.callback(*c, work);

            kind = QueueProgressKind::CALLBACK;
        } else if (activePhase == 3) {
            if (c && c->closing)
                action = s.close(*c, work);

            kind = QueueProgressKind::CLEANUP;

            if (c && c->port->state.load() == QueueClientState::TERMINAL && !c->retireBlocked) {
                auto &p = *c->port;
                spend(work, p.results.size() + 1, 0, 0, sizeof(State::Client));
                bool free = p.publicOwnership.load() == PUBLIC_RETIRED && p.budget->records.load() == 0 &&
                            p.budget->batches.load() == 0 && p.budget->references.load() == c->fixedReferences;
                {
                    std::lock_guard lock(p.mutex);

                    for (const auto &ticket : p.results)
                        free = free && ticket.use_count() == 1;
                }

                if (free) {
                    s.clients[v].reset();
                    c = nullptr;
                    action = true;
                }
            }
        } else {
            if (s.stream && s.unionDirty) {
                spend(work, s.clients.size() + s.interest.aggregateEntries().size(), 0, 0,
                      product(s.clients.size(), sizeof(State::Client)));
                auto synced = s.stream->replace(s.interest.aggregateEntries(), work.desired);

                if (!synced) {
                    if (synced.error() == core::SubscriptionError::WORK_LIMIT)
                        throw QueueError::WORK_LIMIT;

                    if (s.unionOrigin && s.clients[*s.unionOrigin] && !s.clients[*s.unionOrigin]->closing)
                        s.fail(*s.clients[*s.unionOrigin], sourceFailure(synced.error()));
                    else
                        s.failAll(sourceFailure(synced.error()));
                } else {
                    s.unionDirty = false;
                    s.unionOrigin.reset();

                    for (auto &peer : s.clients)
                        if (peer && !peer->closing) {
                            peer->sync = false;
                            peer->mappingRevision = peer->desiredRevision;
                            s.publishState(*peer);
                        }
                }

                action = true;
            } else if (c)
                action = s.sync(*c, work);

            kind = QueueProgressKind::SYNCHRONIZED;
        }

        if (!action)
            return std::unexpected(action.error());

        advance();

        if (s.state == QueueState::CLOSING && s.pendingPhase == State::Pending::NONE) {
            spend(work, s.clients.size(), 0, 0, sizeof(QueueTerminal));
            bool done = true;
            std::size_t failed = 0;

            for (const auto &peer : s.clients)
                if (peer) {
                    done = done && peer->port->state.load() == QueueClientState::TERMINAL;

                    if (peer->primary)
                        ++failed;
                }

            if (done) {
                if (s.stream) {
                    auto discarded = s.stream->discard(work.stream);

                    if (!discarded) {
                        if (discarded.error() == collectors::StreamError::WORK_LIMIT)
                            throw QueueError::WORK_LIMIT;

                        throw sourceFailure(discarded.error());
                    }

                    count(s.counts.sourceRecordsDiscarded, *discarded, s.counts.saturated);
                    s.unknownEligibility = s.unknownEligibility || *discarded != 0;
                } else if (s.ticker) {
                    auto cleared = s.ticker->reset(work.ticker);

                    if (!cleared) {
                        if (cleared.error() == collectors::TickerError::WORK_LIMIT)
                            throw QueueError::WORK_LIMIT;

                        throw sourceFailure(cleared.error());
                    }
                } else {
                    auto cleared = s.history->reset(work.history);

                    if (!cleared) {
                        if (cleared.error().error == collectors::HistoryError::WORK_LIMIT)
                            throw QueueError::WORK_LIMIT;

                        throw sourceFailure(cleared.error().error);
                    }
                }

                QueueTerminal terminal;
                terminal.outcome =
                    s.primary ? TerminalOutcome::FAILED
                              : (s.stopMode == 1 && !s.abortedRegistrations && !s.hadGap ? TerminalOutcome::DRAINED
                                                                                         : TerminalOutcome::ABORTED);
                terminal.counts = s.counts;
                terminal.authoritativeRevision = s.interest.revision();
                terminal.epoch = s.epoch();
                terminal.failedRegistrationCount = s.failedRegistrations;
                terminal.reset = s.reset;
                terminal.primary = s.primary;
                terminal.secondary = s.secondary;
                terminal.hadGap = s.hadGap;
                terminal.unknownEligibility = s.unknownEligibility || s.reset.unknownEligibility;
                terminal.unknownUpstreamLoss = s.hadGap;

                for (const auto &peer : s.clients)
                    if (peer && peer->primary && !terminal.primary)
                        terminal.primary = peer->primary;

                s.terminal = terminal;
                s.state = QueueState::TERMINAL;

                return result(QueueProgressKind::TERMINAL);
            }
        }

        return result(*action ? kind : QueueProgressKind::IDLE);
    } catch (QueueFailure e) {
        return std::unexpected(e);
    } catch (QueueError e) {
        return std::unexpected(QueueFailure{e, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(QueueFailure{QueueError::ALLOCATION_FAILURE, {}});
    }
}

std::expected<QueueRangeStatus, QueueFailure> MuxQueue::rangeStatus(const MuxHandle &handle, core::SubscriptionKey key,
                                                                    QueueWork &work) const {
    try {
        debit(work.operations, 1);

        if (!state_ || !state_->history)
            throw QueueError::INVALID_STATE;

        auto &s = *state_;

        if (s.executing)
            throw QueueError::REENTRANT;

        auto *c = s.client(handle);

        if (!c || c->sourceClosed)
            throw QueueError::INVALID_CLIENT;

        auto entries = s.interest.clientEntries(c->interest);

        if (!entries)
            throw sourceFailure(entries.error());

        spend(work, sum(sum(entries->size(), c->size), product(s.interestLimits.entries, 2)), 0,
              product(sum(sum(entries->size(), c->mappingCount), c->size), key.symbol.size()));
        const MuxEntry *wanted = nullptr;

        for (const auto &entry : *entries)
            if (entry.key.selector == key.selector && entry.key.context.matches(key.context) &&
                entry.key.symbol == key.symbol) {
                wanted = &entry;
                break;
            }

        if (!wanted || !wanted->historyBound)
            throw QueueError::INVALID_CONTEXT;

        QueueRangeStatus result;
        result.epoch = s.epoch();
        result.authoritativeRevision = s.interest.revision();
        result.clientDesiredRevision = c->desiredRevision;
        result.mappingDesiredRevision = c->mappingRevision;
        result.membership = wanted->membership;
        result.requestedFrom = *wanted->historyBound;
        result.acceptedUnstagedData = s.pendingPhase == State::Pending::UNSTAGED;
        result.paused = c->paused;
        result.resetPending = s.reset.phase != ResetPhase::NONE || c->port->advertisedNotice.load() != 0;
        result.hadGap = c->epochGap;
        auto source = s.history->rangeStatus(c->history, key, work.history);

        if (source)
            result.source = *source;
        else if (source.error().error == collectors::HistoryError::WORK_LIMIT)
            throw QueueError::WORK_LIMIT;
        else if (source.error().error != collectors::HistoryError::INVALID_KEY)
            throw sourceFailure(source.error().error);

        auto *map = s.mapping(*c, key, work);

        if (map && result.source) {
            const auto *view = s.history->desired(c->history);
            result.mappingCurrent =
                s.state == QueueState::OPEN && !s.stopMode && !c->port->stop.load() && !c->sync && !c->closing &&
                !result.resetPending && !c->epochGap && s.history->state() == collectors::HistoryState::CURRENT &&
                c->mappingRevision == c->desiredRevision && map->bound == wanted->historyBound &&
                result.source->requestedFrom == result.requestedFrom &&
                map->sourceRevision == result.source->revision && map->sourceEpoch.matches(result.source->epoch) &&
                view && view->isCurrent(map->core) && view->isCurrent(result.source->membership);
        }

        for (std::size_t i = 0; i < c->size; ++i) {
            const auto &r = c->records[(c->head + i) % c->records.size()]->view;

            if (r.context.matches(key.context) && r.symbol == key.symbol)
                ++result.queuedForKey;
        }

        result.completeDeliveredRange = result.mappingCurrent && result.source &&
                                        result.source->completeRequestedRange && !result.acceptedUnstagedData &&
                                        !result.queuedForKey && !result.executingForKey;

        return result;
    } catch (QueueFailure e) {
        return std::unexpected(e);
    } catch (QueueError e) {
        return std::unexpected(QueueFailure{e, {}});
    }
}

QueueResources MuxQueue::resources() const noexcept {
    QueueResources r;

    if (!state_)
        return r;

    auto &s = *state_;
    auto &l = *s.ledger;
    r.fixedBytes = l.fixed.load();
    r.residentBytes = l.fixed.load() + l.clientFixed.load() + l.recordBytes.load() + l.batchBytes.load();
    r.reservedBytes = l.reservedBytes.load();
    r.generations = l.generations.load();
    r.dataItems = l.records.load();
    r.recordBytes = l.recordBytes.load();
    r.batchOwners = l.batches.load();
    r.batchBytes = l.batchBytes.load();
    r.reservedItems = l.reservedItems.load();
    r.reservedRecordBytes = l.reservedRecordBytes.load();
    r.reservedBatchOwners = l.reservedBatchOwners.load();
    r.reservedBatchBytes = l.reservedBatchBytes.load();
    r.reservedCommandSlots = l.reservedCommands.load();
    r.reservedResultSlots = l.reservedResults.load();
    r.reservedControlSlots = l.reservedControls.load();
    r.muxBytes = s.interest.storageBytes();

    if (s.stream) {
        r.collectorBytes = s.stream->residentBytes();
        r.eventBytes = s.stream->stagedBytes();
    }

    if (s.ticker) {
        r.collectorBytes = s.ticker->residentBytes();
        r.eventBytes = s.ticker->cacheBytes();
    }

    if (s.history) {
        r.collectorBytes = s.history->residentBytes();
        r.eventBytes = s.history->eventBytes();
    }

    if (s.pending)
        r.eventBytes += s.pending->residentBytes();

    r.pendingRecords = s.pendingRecords;
    r.resetPhase = s.reset.phase;

    return r;
}

std::expected<ClientResources, QueueFailure> MuxQueue::resources(const MuxHandle &handle) const noexcept {
    if (!state_)
        return std::unexpected(QueueFailure{QueueError::INVALID_STATE, {}});

    auto *c = state_->client(handle);

    if (!c)
        return std::unexpected(QueueFailure{QueueError::INVALID_CLIENT, {}});

    return state_->resources(*c);
}

ResetStatus MuxQueue::resetStatus() const noexcept {
    return state_ ? state_->reset : ResetStatus{};
}

std::span<const core::SubscriptionRequest> MuxQueue::aggregateEntries() const noexcept {
    return state_ ? state_->interest.aggregateEntries() : std::span<const core::SubscriptionRequest>{};
}

std::uint64_t MuxQueue::revision() const noexcept {
    return state_ ? state_->interest.revision() : 0;
}

QueueState MuxQueue::state() const noexcept {
    return state_ ? state_->state : QueueState::TERMINAL;
}

std::optional<QueueTerminal> MuxQueue::terminal() const noexcept {
    return state_ ? state_->terminal : std::optional<QueueTerminal>{};
}

MuxQueue::State::~State() {
    if (executing)
        std::terminate();

    executing = true;

    if (ledger)
        ledger->stop = 2;

    // Finite library traversal, without caller credits or application-destructor latency guarantees.
    const auto rawPending = stream && pendingPhase == Pending::ROUTING ? stream->queuedRecords() : pendingRecords;

    for (auto &c : clients) {
        if (!c || c->sourceClosed || c->port->state.load() == QueueClientState::TERMINAL)
            continue;

        std::size_t known = 0;

        if (ticker) {
            const auto n = ticker->pendingKeys(c->ticker);

            if (n)
                known = *n;
            else
                c->unknownEligibility = true;
        }

        if (history) {
            known = history->pendingCount(c->history);
            c->unknownEligibility = true;
        }

        count(c->counts.discarded, known, c->counts.saturated);
        c->unknownEligibility = c->unknownEligibility || rawPending != 0;
        c->hadGap = c->hadGap || known != 0 || c->unknownEligibility;
    }

    pending.reset();
    stream.reset();
    ticker.reset();
    history.reset();
    sourceIdentity.reset();
    {
        auto retired = std::move(interest);
    }

    for (auto &c : clients) {
        if (!c || c->port->state.load() == QueueClientState::TERMINAL)
            continue;

        c->closing = true;
        stop(*c->port, 2);
        c->hadGap = c->hadGap || c->size != 0;

        for (std::size_t i = 0; i < c->size; ++i) {
            c->records[(c->head + i) % c->records.size()].reset();
            count(c->counts.discarded, 1, c->counts.saturated);
        }

        c->size = 0;
        c->consumer.reset();
        QueueTerminal result;
        result.outcome = c->primary ? TerminalOutcome::FAILED : TerminalOutcome::ABORTED;
        result.primary = c->primary;
        result.secondary = c->secondary;
        result.counts = c->counts;
        result.epoch = epoch();
        result.authoritativeRevision = c->desiredRevision;
        result.hadGap = c->hadGap;
        result.unknownEligibility = true;
        result.unknownUpstreamLoss = true;
        result.reset = reset;
        std::lock_guard lock(c->port->mutex);

        for (auto &cmd : c->port->commands)
            if (cmd.occupied) {
                {
                    std::lock_guard completionLock(cmd.completion->mutex);
                    cmd.completion->result = MuxCommandResult{
                        cmd.completion->id, CommandStatus::CANCELLED_CLOSING, c->desiredRevision, false, {}};
                }
                cmd.completion.reset();
                cmd.occupied = false;

                for (std::size_t i = 0; i < cmd.count; ++i) {
                    cmd.requests[i] = {};
                    cmd.keys[i] = {};
                }

                cmd.count = 0;
            }

        c->port->size = 0;
        c->port->terminal = result;
        c->port->state = QueueClientState::TERMINAL;
    }
}

} // namespace org::ttldtor::qdcxx::mux
