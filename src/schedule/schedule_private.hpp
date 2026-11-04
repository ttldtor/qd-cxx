// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/schedule/schedule.hpp"

#include <array>
#include <atomic>
#include <compare>
#include <limits>
#include <mutex>
#include <utility>

namespace org::ttldtor::qdcxx::schedule::detail {
inline constexpr std::size_t NO_DEFINITION = std::numeric_limits<std::size_t>::max();
inline constexpr std::int64_t DAY_MILLIS = 86400000;

/** Internal checked construction failure, translated at the owning public boundary. */
struct Failure {
    ScheduleError error;
};

/** One operation's cumulative finite inspection and logical construction counters. */
struct Construction {
    const ScheduleLimits &limits;
    std::size_t work = 0;
    std::size_t bytes = 0;
    std::size_t dateValues = 0;
    std::size_t dayDefinitions = 0;
    /** @param amount Inspection/copy work to charge before performing it. */
    void inspect(std::size_t amount);
    /** @param amount Logical owned-field bytes to charge before construction. */
    void allocate(std::size_t amount);
    /** @param value Borrowed UTF-16 to copy after charging work/backing.
     * @return Independent owned text.
     */
    std::u16string copy(std::u16string_view value);
};

/** Parsed local calendar fields; 24:00 and direct TimeDef source quirks remain unnormalized. */
struct TimeDefinition {
    std::int32_t day = 0;
    std::int32_t hour = 0;
    std::int32_t minute = 0;
    std::int32_t second = 0;
    /** @return Signed local offset in milliseconds, without timezone conversion. */
    std::int64_t offset() const noexcept;
    std::strong_ordering operator<=>(const TimeDefinition &other) const noexcept;
    bool operator==(const TimeDefinition &) const = default;
};

/** Abstract session kinds; this is not a constructed date-specific Session. */
enum class SessionKind { NO_TRADING, PRE_MARKET, REGULAR, AFTER_MARKET };

/** An abstract local-field pair, independent of timezone transition queries. */
struct SessionDefinition {
    SessionKind kind;
    TimeDefinition start;
    TimeDefinition end;
};

/** Complete local day definition including explicit NO_TRADING gaps. */
struct DayDefinition {
    TimeDefinition start;
    TimeDefinition end;
    TimeDefinition reset;
    std::vector<SessionDefinition> sessions;
};

/** Ordered property value; repeated/chained keys are removed and appended on replacement. */
using Properties = std::vector<std::pair<std::u16string, std::u16string>>;

/** An ordered named immutable date list. */
struct NamedDays {
    std::u16string name;
    std::vector<std::int32_t> days;
};

/** Exact venue properties, preserving source insertion order. */
struct VenueDefaults {
    std::u16string name;
    Properties properties;
};

enum class RetainedKind { GENERATION, IDENTITY, RULES, BUNDLE };
struct Ledger;

/** Shared logical charge, released independently of the context commit lock. */
struct Lease {
    std::shared_ptr<Ledger> ledger;
    RetainedKind kind;
    std::size_t bytes;
    ~Lease();
};

/** Immutable calendar generation, independently retained from timezone-provider data. */
struct Defaults {
    std::shared_ptr<const Lease> lease;
    std::int64_t version = 0;
    std::vector<NamedDays> holidays;
    std::vector<NamedDays> shortDays;
    std::vector<NamedDays> specialDays;
    std::vector<VenueDefaults> venues;
    std::size_t modelBytes = 0;
};

/** Parsed final per-date override after ordered strategy/default composition. */
struct DayStrategy {
    std::int32_t yearMonthDay = 0;
    std::size_t definition = NO_DEFINITION;
    bool forcedTrading = false;
    bool closed = false;
    bool earlyClose = false;
    std::int64_t earlyCloseMillis = 0;
};

/** Owned parsed rules; the same model drives metadata and the future date-query boundary. */
struct Rules {
    std::shared_ptr<const Lease> lease;
    std::shared_ptr<const Defaults> defaults;
    std::u16string definition;
    std::u16string name;
    std::u16string zone;
    Properties properties;
    std::vector<std::int32_t> holidays;
    std::vector<std::int32_t> shortDays;
    std::int64_t dayOffset = 0;
    std::int64_t earlyClose = 0;
    std::int32_t joinNextTradingDay = 0;
    std::array<std::size_t, 9> weekDays;
    std::vector<DayDefinition> definitions;
    std::vector<DayStrategy> overrides;
    std::size_t modelBytes = 0;
};

/** One exact selected definition; no context pointer/cycle is retained here. */
struct Identity {
    std::shared_ptr<const Lease> lease;
    std::u16string definition;
    mutable std::atomic<std::size_t> liveHandles{0}; ///< Handles/reservations only; bundle pins are excluded.
};

/** Only formed after a live-handle reservation under the context commit lock. */
struct ReservedHandle {};

/** Current identity/model association. */
struct Entry {
    std::shared_ptr<const Identity> identity;
    std::shared_ptr<const Rules> rules;
};

/** One complete immutable publication unit for all metadata readers. */
struct Bundle {
    std::shared_ptr<const Lease> lease;
    std::shared_ptr<const Defaults> defaults;
    std::vector<Entry> entries;
};

/** Thread-safe distinct-model retention ledger; it never invokes application code. */
struct Ledger {
    std::mutex mutex;
    ScheduleLimits limits;
    ScheduleUsage usage;
};

/** Context commit lock protects publication/cache-hit acquisition, not construction/destruction. */
struct Context {
    ScheduleLimits limits;
    std::shared_ptr<Ledger> ledger;
    std::mutex commitMutex;
    std::atomic<std::shared_ptr<const Bundle>> current;
};

/** Private bridge shared by public metadata methods and future date-specific queries. */
struct Access {
    /** @param context Public owning domain.
     * @return Shared internal domain, without allocation.
     */
    static const std::shared_ptr<Context> &context(const ScheduleDataContext &context) noexcept;
    /** @param schedule Public owning identity.
     * @return Immutable current rules, pinned for the entire caller operation.
     */
    static std::shared_ptr<const Rules> rules(const Schedule &schedule);
    /** @param context Owning domain.
     * @param identity Complete retained identity.
     * @return Public handle retaining both objects.
     */
    static Schedule make(std::shared_ptr<Context> context, std::shared_ptr<const Identity> identity) noexcept;
    /** @param context Owning domain.
     * @param identity Identity with one protected handle reservation.
     * @return Public handle adopting that reservation without allocation or throwing.
     */
    static Schedule makeReserved(std::shared_ptr<Context> context, std::shared_ptr<const Identity> identity) noexcept;
};

/** @param text Exact selected definition.
 * @param defaults Pinned calendar generation.
 * @param construction Operation-wide finite counters.
 * @return Fully owning parsed candidate; failure exposes no model.
 */
std::shared_ptr<Rules> parseRules(std::u16string_view text, const std::shared_ptr<const Defaults> &defaults,
                                  Construction &construction);
/** @param input Complete typed source entries.
 * @param construction Operation-wide counters.
 * @return Fully validated owning generation, without a retention lease yet.
 */
std::shared_ptr<Defaults> parseDefaults(CalendarDefaults input, Construction &construction);
/** @param construction Operation-wide counters.
 * @return Typed baseline extracted only from the exact embedded resource; no raw loader is exposed.
 */
std::shared_ptr<Defaults> builtinDefaults(Construction &construction);
/** @param text Complete instrument text.
 * @param venue Optional exact named selection.
 * @param construction Operation-wide counters.
 * @return Borrowed selected portion; caller input must remain alive until the owning copy is made.
 */
std::u16string_view selectDefinition(std::u16string_view text, const std::u16string_view *venue,
                                     Construction &construction);
/** @param ledger Shared retention domain.
 * @param kind Distinct model category.
 * @param bytes Complete logical owned-field backing.
 * @param construction Operation-wide staging counters, charged before lease allocation.
 * @return Lease acquired before publication, or typed quota/allocation failure.
 */
std::shared_ptr<const Lease> acquire(const std::shared_ptr<Ledger> &ledger, RetainedKind kind, std::size_t bytes,
                                     Construction &construction);
} // namespace org::ttldtor::qdcxx::schedule::detail
