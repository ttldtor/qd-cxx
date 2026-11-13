// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/subscription_selector.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::api {

enum class EventType; ///< Existing Feed registry enumeration; its definition remains in event_registry.hpp.

inline constexpr std::int64_t TIME_SERIES_FROM_TIME_DISABLED = (std::numeric_limits<std::int64_t>::max)();
///< Initial Java fromTime sentinel: no requested time-series events, not a wall-clock time or wire flag.

/** Value operations required by a time-series decorator.
 * @tparam Symbol Copy-constructible symbol value with equality and a signed Java-style hashCode().
 * Syntax cannot establish semantic laws: callers must provide symmetric equivalence, equal hashes for equal
 * symbols, and stable identity while a value is used as a key. Ownership and exception guarantees follow Symbol.
 */
template <class Symbol>
concept TimeSeriesSymbolValue = std::copy_constructible<Symbol> && requires(const Symbol &a, const Symbol &b) {
    { a == b } -> std::convertible_to<bool>;
    { a.hashCode() } -> std::convertible_to<std::int32_t>;
};

/** Owning time-series decoration; identity and hash exclude its fromTime attribute.
 * @tparam Symbol Owned symbol value satisfying TimeSeriesSymbolValue; the default is the existing typed selector.
 * Nested decorators and caller-defined value types are supported without a Java-object or wire-mapper registry.
 * Native typed-kind equality preserves ADR-0012; it does not reproduce Java's asymmetric IES/TSS relation.
 * Copies/moves follow Symbol's ownership and exception contracts. This value creates no subscription or I/O.
 */
template <TimeSeriesSymbolValue Symbol = SubscriptionSelector> class TimeSeriesSubscriptionSymbol final {
    Symbol symbol_;         ///< Underlying complete value; no textual flattening or implicit decorator parsing.
    std::int64_t fromTime_; ///< Exact signed Unix milliseconds, including both extrema.

    public:
    /** Transfer a complete symbol value and capture one immutable range attribute.
     * @param symbol Underlying owning value, copied/moved according to Symbol's contract.
     * @param fromTime Inclusive lower bound in Unix milliseconds; MAX is the disabled sentinel.
     */
    explicit TimeSeriesSubscriptionSymbol(
        Symbol symbol,
        std::int64_t fromTime = TIME_SERIES_FROM_TIME_DISABLED) noexcept(std::is_nothrow_move_constructible_v<Symbol>)
        : symbol_(std::move(symbol)), fromTime_(fromTime) {
    }

    /** Inspect the underlying value without unwrapping another decorator.
     * @return Borrowed const value until assignment, move or destruction of this object.
     */
    [[nodiscard]] const Symbol &eventSymbol() const noexcept {
        return symbol_;
    }

    /** Inspect the captured range attribute.
     * @return Exact signed Unix milliseconds; reading performs no range mutation.
     */
    [[nodiscard]] std::int64_t fromTime() const noexcept {
        return fromTime_;
    }

    /** Inspect the time-series source identity.
     * @return DEFAULT source ID zero; the underlying symbol's own decorator is retained separately.
     */
    [[nodiscard]] std::int32_t sourceId() const noexcept {
        return 0;
    }

    /** Compare identity independently of this decorator's fromTime.
     * @param other Same native symbol domain; cross-domain erasure is not provided.
     * @return Underlying symbol equality, subject to Symbol's semantic equivalence contract.
     */
    [[nodiscard]] bool operator==(const TimeSeriesSubscriptionSymbol &other) const
        noexcept(noexcept(static_cast<bool>(symbol_ == other.symbol_))) {
        return static_cast<bool>(symbol_ == other.symbol_);
    }

    /** Inspect the identity hash, without mixing the range attribute.
     * @return Underlying signed32 hash; exceptions follow Symbol::hashCode().
     */
    [[nodiscard]] std::int32_t hashCode() const noexcept(noexcept(static_cast<std::int32_t>(symbol_.hashCode()))) {
        return static_cast<std::int32_t>(symbol_.hashCode());
    }

    /** Compare complete demand attributes, separate from key identity.
     * @param other Same native symbol domain.
     * @return Equal outer range and underlying sameDemand() when supplied, otherwise underlying equality.
     */
    [[nodiscard]] bool sameDemand(const TimeSeriesSubscriptionSymbol &other) const {
        if (fromTime_ != other.fromTime_)
            return false;

        if constexpr (requires {
                          { symbol_.sameDemand(other.symbol_) } -> std::convertible_to<bool>;
                      })
            return symbol_.sameDemand(other.symbol_);
        else
            return symbol_ == other.symbol_;
    }
};

/** Explicit business-event marker; raw indexed record layout never implies TIME_SERIES. */
enum class EventSubscriptionCapability { NON_INDEXED, INDEXED, TIME_SERIES };

/** Pure-model refusal; no accepted state is changed on failure. */
enum class TimeSeriesModelError {
    INVALID_STATE,          ///< Moved-from model.
    INVALID_LIMITS,         ///< Zero or nonrepresentable finite model capacities.
    INVALID_CAPABILITY,     ///< Empty stable identity or unknown marker enumeration.
    UNSUPPORTED_EVENT_TYPE, ///< Descriptor does not declare TimeSeriesEvent business capability.
    CONFLICTING_EVENT_TYPE, ///< One stable business identity has incompatible marker declarations.
    RESOURCE_LIMIT,         ///< Finite event/symbol count or text bound exceeded.
    ALLOCATION_FAILURE,     ///< Required owning storage could not be allocated; old state survives.
    REVISION_EXHAUSTED      ///< A changed state cannot receive a fresh owner-local revision.
};

/** Explicit, extensible business identity and marker declaration; not a verified wire mapper.
 * Stable identity is caller-defined data, not parsed as a class name. A caller must not alias distinct business
 * types with one identity. Custom/inherited types need no new enum or subclass here. This descriptor retains no
 * schema, implementation class, callback or runtime and grants no native Feed HISTORY availability.
 */
class TimeSeriesEventDescriptor final {
    std::u16string identity_;                ///< Exact nonempty stable business identity, independently owned.
    EventSubscriptionCapability capability_; ///< Explicit business marker, independent of record-time fields.
    TimeSeriesEventDescriptor(std::u16string identity, EventSubscriptionCapability capability) noexcept;

    public:
    /** Validate and retain a caller-declared business capability.
     * @param stableIdentity Exact nonempty owned identity, never inferred from its spelling.
     * @param capability Explicit marker; TIME_SERIES includes IndexedEvent semantics.
     * @return Owning descriptor or INVALID_CAPABILITY; allocation failures during argument copying propagate.
     */
    [[nodiscard]] static std::expected<TimeSeriesEventDescriptor, TimeSeriesModelError>
    declared(std::u16string stableIdentity, EventSubscriptionCapability capability);
    /** @return Borrowed exact stable identity until assignment, move or destruction. */
    [[nodiscard]] std::u16string_view stableIdentity() const noexcept;
    /** @return Declared business marker; this is not an assertion of decoder or transport support. */
    [[nodiscard]] EventSubscriptionCapability capability() const noexcept;
    /** Compare stable business identity, separately from declaration consistency.
     * @param other Independent descriptor.
     * @return True when exact stable identities match.
     */
    [[nodiscard]] bool operator==(const TimeSeriesEventDescriptor &other) const noexcept;
};

/** Query current registered native types without treating raw record time as business capability.
 * @param type Existing EventType or unknown enum value.
 * @return False for all current nine types and unknown values; none declares TimeSeriesEvent.
 */
[[nodiscard]] bool isTimeSeriesEventType(EventType type) noexcept;

/** Separate logical declaration from actual Feed HISTORY support.
 * @param descriptor Caller-declared pure-model capability.
 * @return False: this component supplies no HISTORY receiver, mapper, wire activation or runtime handle.
 */
[[nodiscard]] bool hasTimeSeriesFeedReceiver(const TimeSeriesEventDescriptor &descriptor) noexcept;

/** Independent finite pure-model bounds; text charges count UTF-16 units, not process RSS. */
struct TimeSeriesModelLimits {
    std::size_t eventTypes = 16;          ///< Maximum supplied type declarations, including duplicates.
    std::size_t typeIdentityUnits = 256;  ///< Maximum exact units in one stable business identity.
    std::size_t symbols = 1024;           ///< Maximum retained distinct raw selector identities.
    std::size_t inputSymbols = 4096;      ///< Maximum entries in one supplied batch, including duplicates.
    std::size_t symbolUnits = 1024;       ///< Maximum exact event-symbol text units per selector.
    std::size_t totalSymbolUnits = 65536; ///< Maximum total retained event-symbol text units.
};

/** One committed owner-local logical change; not a transport, source or callback acknowledgement. */
struct TimeSeriesModelChange {
    std::uint64_t revision = 0;   ///< Accepted logical model revision; cannot compare unrelated model owners.
    bool symbolsChanged = false;  ///< Effective raw symbol set or its retained demand attributes changed.
    bool fromTimeChanged = false; ///< Exact lower-bound property changed, including while the set is empty.
};

/** Detached owning model cut; copies retain no model, runtime, schema or callable. */
struct TimeSeriesSubscriptionSnapshot {
    std::uint64_t revision = 0; ///< One coherent model-local revision for every field below.
    std::int64_t fromTime = TIME_SERIES_FROM_TIME_DISABLED; ///< Inclusive Unix milliseconds, MAX disabled.
    std::vector<TimeSeriesEventDescriptor> eventTypes; ///< Normalized owning business declarations, first-seen order.
    std::vector<SubscriptionSelector> symbols; ///< Undecorated raw native values, preserving their typed decorators.
    std::vector<TimeSeriesSubscriptionSymbol<>> decoratedSymbols; ///< One outer time-series wrapper per raw symbol.
};

/** Thread-safe, bounded logical range/set model, independent of Feed and Candle-only use cases.
 * This is not a live subscription: no SDK bootstrap, callbacks, close lifecycle, history completeness or network
 * progress exists. All methods serialize through one mutex; snapshots allocate bounded independent copies.
 * Moving/destruction require the caller to exclude concurrent access to that object, as for ordinary C++ owners.
 */
class TimeSeriesSubscriptionModel final {
    struct Impl;
    std::unique_ptr<Impl> impl_; ///< Unique pure-data owner; no operational capability can escape.
    explicit TimeSeriesSubscriptionModel(std::unique_ptr<Impl> impl) noexcept;

    public:
    /** Transfer pure ownership; existing detached snapshots remain independent.
     * @param other Source becomes invalid, with no implicit range reset or other-owner mutation.
     */
    TimeSeriesSubscriptionModel(TimeSeriesSubscriptionModel &&other) noexcept;
    /** Transfer pure ownership after releasing this model's old data.
     * @param other Source becomes invalid; self-move preserves validity.
     * @return This model, without callbacks or SDK work.
     */
    TimeSeriesSubscriptionModel &operator=(TimeSeriesSubscriptionModel &&other) noexcept;
    /** Release pure model data; detached snapshots remain usable. Concurrent access must already have ended. */
    ~TimeSeriesSubscriptionModel();
    /** Create an empty disabled-range model with explicit business capability declarations.
     * @param eventTypes Nonempty borrowed declarations; each must declare TIME_SERIES. Exact duplicates collapse;
     * conflicting declarations for one stable identity are rejected before publication.
     * @param limits Finite independent count/unit bounds; retained for this model lifetime.
     * @return Prepared pure model or typed refusal; inputs remain unchanged.
     */
    [[nodiscard]] static std::expected<TimeSeriesSubscriptionModel, TimeSeriesModelError>
    create(std::span<const TimeSeriesEventDescriptor> eventTypes, TimeSeriesModelLimits limits = {});
    /** @return Whether this object still owns its pure model; does not inspect delivery readiness. */
    [[nodiscard]] bool valid() const noexcept;
    /** Observe the current range without allocating or advancing another component.
     * @return Exact signed milliseconds or INVALID_STATE after move.
     */
    [[nodiscard]] std::expected<std::int64_t, TimeSeriesModelError> getFromTime() const;
    /** Atomically change the range for every symbol without changing any symbol identity.
     * @param fromTime Any signed64 Unix-millisecond value; MAX disables demand and is the initial default.
     * @return Committed change; equal values are a no-op. Failure preserves the old range, set and revision.
     */
    [[nodiscard]] std::expected<TimeSeriesModelChange, TimeSeriesModelError> setFromTime(std::int64_t fromTime);
    /** Replace the complete raw symbol set using typed identity, retaining the last duplicate's attributes.
     * @param symbols Borrowed native selector values, copied before atomic commit; no text is parsed or flattened.
     * @return Effective logical change or typed refusal preserving the entire old model.
     */
    [[nodiscard]] std::expected<TimeSeriesModelChange, TimeSeriesModelError>
    setSymbols(std::span<const SubscriptionSelector> symbols);
    /** Add/replace raw identities without withdrawing other symbols.
     * @param symbols Complete bounded borrowed batch, copied before commit.
     * @return Effective logical change or typed refusal with no partial mutation.
     */
    [[nodiscard]] std::expected<TimeSeriesModelChange, TimeSeriesModelError>
    addSymbols(std::span<const SubscriptionSelector> symbols);
    /** Remove one typed identity, ignoring its mutable fromTime attribute.
     * @param symbol Borrowed identity; an absent identity is a no-op.
     * @return Effective change or typed refusal; remaining raw values retain their order and attributes.
     */
    [[nodiscard]] std::expected<TimeSeriesModelChange, TimeSeriesModelError>
    removeSymbol(const SubscriptionSelector &symbol);
    /** Remove all symbols while preserving the range and event declarations.
     * @return Effective change or typed refusal; an empty set is a no-op.
     */
    [[nodiscard]] std::expected<TimeSeriesModelChange, TimeSeriesModelError> clearSymbols();
    /** Copy one coherent raw/decorated cut with no progress, callbacks or model capture.
     * @return Independently owning snapshot or typed allocation/owner refusal.
     */
    [[nodiscard]] std::expected<TimeSeriesSubscriptionSnapshot, TimeSeriesModelError> snapshot() const;
};

} // namespace org::ttldtor::qdcxx::api

namespace std {
/** Standard hash adapter preserving the decorator's range-independent identity.
 * @tparam Symbol Same value/hash semantic requirements as the native time-series decorator.
 */
template <org::ttldtor::qdcxx::api::TimeSeriesSymbolValue Symbol>
struct hash<org::ttldtor::qdcxx::api::TimeSeriesSubscriptionSymbol<Symbol>> {
    /** @param value Owning decorator; its fromTime is not hashed.
     * @return Underlying signed32 hash converted to size_t, subject to Symbol's hash/equality laws.
     */
    size_t operator()(const org::ttldtor::qdcxx::api::TimeSeriesSubscriptionSymbol<Symbol> &value) const
        noexcept(noexcept(value.hashCode())) {
        return static_cast<size_t>(value.hashCode());
    }
};
} // namespace std
