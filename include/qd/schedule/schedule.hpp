// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/time/time_zone_data.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::schedule {
namespace detail {
struct Context;
struct Identity;
struct Access;
struct ReservedHandle;
struct DayModel;
} // namespace detail

/** Typed parser, transaction and logical-resource failures; no partial update is published. */
enum class ScheduleError {
    INVALID_DEFINITION, ///< Invalid schedule text, time, list reference or recognized strategy.
    MISSING_VENUE,      ///< The requested venue does not occur in the instrument's trading hours.
    INVALID_DEFAULTS,   ///< Invalid or duplicate typed calendar-default entry.
    UNKNOWN_TIME_ZONE,  ///< The exact ID is outside the pinned compatibility identity profile.
    RESOURCE_LIMIT,     ///< A finite input/work/staging/cache/retained-model limit would be exceeded.
    ALLOCATION,         ///< An owning allocation failed before publication.
    CONCURRENT_CHANGE,  ///< A conflicting bounded transaction committed; the caller may explicitly retry.
    NOT_READY,          ///< This parser-only context has no explicitly initialized timezone data.
    OUT_OF_RANGE,       ///< The argument is outside this particular query method's supported range.
    NO_SESSION,         ///< No interval selected by the day/session lookup contains the requested instant.
    TIME_ZONE_DATA      ///< Controlled timezone initialization or an identity/data lookup failed.
};

/** Shared CORE timezone capability; market-calendar generations remain independent. */
using ScheduleTimeZoneData = time::TimeZoneData;
/** Shared finite provider admission bounds. */
using TimeZoneLimits = time::TimeZoneLimits;
/** Shared explicit constructor/raw-reference options; no clock is read by date queries. */
using ScheduleQueryOptions = time::TimeZoneBindingOptions;
/** Ordered source entry in a calendar generation. */
enum class CalendarEntryKind {
    HOLIDAY_DAYS, ///< Named hd list; references may use earlier holiday entries.
    SHORT_DAYS,   ///< Named sd list; references may use earlier short-day entries.
    SPECIAL_DAYS, ///< Named xd list; references may use earlier special-day entries.
    VENUE         ///< Named tv properties, with exact duplicate/chained-key semantics.
};

/** Borrowed typed default entry; strings are copied during the explicit update. */
struct CalendarDefaultsEntry {
    CalendarEntryKind kind;         ///< Entry namespace.
    std::u16string_view name;       ///< Exact list/venue name; not trimmed.
    std::u16string_view definition; ///< Date-list expression or venue property text.
};

/** Explicit calendar data input, separate from timezone transitions and raw resource loading. */
struct CalendarDefaults {
    std::int64_t versionMillis = 0;                 ///< Typed comparison version; older versions are ignored.
    std::span<const CalendarDefaultsEntry> entries; ///< Complete borrowed entries in source order.
};

/** Finite logical limits; bytes describe owned model fields, not exact heap/RSS usage. */
struct ScheduleLimits {
    std::size_t inputUnits = 65536;        ///< Maximum UTF-16 units in one instrument definition.
    std::size_t defaultUnits = 1048576;    ///< Maximum combined UTF-16 units in one typed defaults input.
    std::size_t defaultEntries = 4096;     ///< Maximum named lists plus venues per generation.
    std::size_t properties = 1024;         ///< Maximum distinct merged properties in one schedule.
    std::size_t dateValues = 100000;       ///< Cumulative date-value construction credits per operation.
    std::size_t dayDefinitions = 8192;     ///< Cumulative abstract day-definition construction credits.
    std::size_t sessionsPerDay = 1024;     ///< Maximum abstract sessions including inserted gaps.
    std::size_t work = 16000000;           ///< Cumulative UTF-16/collection inspection and copy credits.
    std::size_t stagingBytes = 33554432;   ///< Cumulative model-construction bytes per operation.
    std::size_t retainedBytes = 67108864;  ///< Shared logical backing of identities/rules/defaults/bundles.
    std::size_t retainedGenerations = 4;   ///< Calendar generations retained by context and reader pins.
    std::size_t retainedSchedules = 256;   ///< Distinct retained exact schedule identities.
    std::size_t retainedRuleModels = 1024; ///< Parsed identity/generation models, including old reader pins.
    std::size_t retainedBundles = 16;      ///< Published/candidate bundles retained across readers.
    std::size_t inactiveCacheTarget =
        64; ///< Inactive collection target on explicit factory turns; retainedSchedules bounds all entries.
    std::size_t queryWork = 100000;          ///< Cumulative inspection/conversion credits per date query.
    std::size_t queryStagingBytes = 4194304; ///< Cumulative logical construction bytes per date query.
    std::size_t retainedDays = 4096;         ///< Distinct retained immutable date-specific day models.
    std::size_t retainedSessions = 65536;    ///< Session records retained inside all live day models.
    std::size_t querySessionsPerDay = 16384; ///< Maximum records in a constructed day, including joined days/gaps.
    std::size_t dayCacheEntries = 1024;      ///< Maximum weak day-cache entries; zero disables the cache.
    std::size_t retainedDayCaches = 16;      ///< Distinct immutable current/candidate/reader cache snapshots.
};

/** Current shared-retention observations; caller-owned handle copies are not distinct models. */
struct ScheduleUsage {
    std::size_t logicalBytes = 0; ///< Charged retained model fields, excluding allocator/control-block overhead.
    std::size_t generations = 0;  ///< Distinct retained calendar generations.
    std::size_t identities = 0;   ///< Distinct retained exact selected definitions.
    std::size_t ruleModels = 0;   ///< Distinct retained parsed definition/generation models.
    std::size_t bundles = 0;      ///< Immutable bundles retained by context, transactions or readers.
    std::size_t days = 0;         ///< Distinct retained date-specific immutable models.
    std::size_t sessions = 0;     ///< Retained session records; handle copies do not create records.
    std::size_t dayCaches = 0;    ///< Retained immutable weak-cache snapshots.
};

/** Result of an explicit typed calendar replacement. */
enum class DefaultsUpdate {
    OLDER_IGNORED, ///< Validated candidate has an older typed version; current generation remains unchanged.
    APPLIED        ///< Complete candidate and all retained live/cached identities were published atomically.
};

/** Explicit independent calendar/cache ownership domain; copies share the same domain.
 * Initialization loads only the embedded pinned market defaults and identity profile, without file,
 * environment, network or timezone-query work. Concurrent readers retain immutable generations.
 * Conflicting writers use one bounded transaction and may return CONCURRENT_CHANGE. Handle moves
 * preserve the source object; replacing one handle requires external synchronization.
 */
class ScheduleDataContext final {
    std::shared_ptr<detail::Context> state_;
    explicit ScheduleDataContext(std::shared_ptr<detail::Context> state) noexcept;
    friend struct detail::Access;

    public:
    /** @param other Shared ownership domain to retain without allocation. */
    ScheduleDataContext(const ScheduleDataContext &other) noexcept = default;
    /** @param other Shared ownership domain; its original handle remains valid. */
    ScheduleDataContext(ScheduleDataContext &&other) noexcept;
    /** @param other Shared ownership domain.
     * @return This handle after replacement; old Schedule handles retain their domain.
     */
    ScheduleDataContext &operator=(const ScheduleDataContext &other) noexcept = default;
    /** @param other Shared ownership domain; its original handle remains valid.
     * @return This handle after replacement.
     */
    ScheduleDataContext &operator=(ScheduleDataContext &&other) noexcept;
    /** Create an independent context using the exact embedded pinned calendar baseline.
     * @param limits Finite logical input/construction/retention limits; zero capacities may refuse initialization.
     * @return Fully initialized context or a typed failure, with no partial domain exposed.
     */
    [[nodiscard]] static std::expected<ScheduleDataContext, ScheduleError> create(ScheduleLimits limits = {});
    /** Create an independent calendar/cache domain sharing an explicitly ready timezone generation.
     * @param timeZones Owning ready capability; no file/native initialization occurs in this factory.
     * @param limits Finite parser/query/retention limits shared by the new domain.
     * @param options Explicit raw-metadata reference; it does not restrict queried dates.
     * @return Fully initialized query-capable context or a typed failure.
     */
    [[nodiscard]] static std::expected<ScheduleDataContext, ScheduleError>
    create(const ScheduleTimeZoneData &timeZones, ScheduleLimits limits = {}, ScheduleQueryOptions options = {});
    /** Prepare a complete typed calendar generation and rebuild all live schedule identities.
     * No raw-byte TimeFormat/date-header loader is implied. Invalid/over-budget/allocation/conflict
     * failures preserve the current generation. Inactive cache entries may be discarded atomically;
     * destruction/retirement occurs outside the commit lock.
     * @param defaults Complete borrowed entries and explicit signed millisecond comparison version.
     * @return Ignored-older/applied status or typed failure; readers observe one complete generation.
     */
    [[nodiscard]] std::expected<DefaultsUpdate, ScheduleError> setDefaults(CalendarDefaults defaults) const;
    /** @return Current typed calendar comparison version; no parsing or timezone work occurs. */
    [[nodiscard]] std::int64_t getDefaultsVersion() const noexcept;
    /** @return Snapshot of the shared logical retention ledger. */
    [[nodiscard]] ScheduleUsage getUsage() const noexcept;
};

class Day;
class Session;

/** Date-specific session kinds, including zero-duration trading sessions. */
enum class SessionType {
    NO_TRADING,  ///< No trading activity.
    PRE_MARKET,  ///< Trading before the regular market.
    REGULAR,     ///< Regular market trading.
    AFTER_MARKET ///< Trading after the regular market.
};

/** @param type Session kind to inspect.
 * @return Whether this kind is trading; duration does not affect its trading type.
 */
[[nodiscard]] bool isTrading(SessionType type) noexcept;

/** Owning Java-like schedule identity in an explicit context.
 * Metadata uses the current atomically published calendar generation. Day and Session results own
 * immutable generation snapshots and retain this live identity. Input views are never retained.
 * Ordinary getters and queries perform no file/network discovery, callbacks or worker progress.
 */
class Schedule final {
    std::shared_ptr<detail::Context> context_;
    std::shared_ptr<const detail::Identity> identity_;
    Schedule(std::shared_ptr<detail::Context> context, std::shared_ptr<const detail::Identity> identity) noexcept;
    Schedule(std::shared_ptr<detail::Context> context, std::shared_ptr<const detail::Identity> identity,
             detail::ReservedHandle) noexcept;
    friend struct detail::Access;

    public:
    /** @param other Identity/context to share without allocation. */
    Schedule(const Schedule &other) noexcept;
    /** @param other Identity/context; its original handle remains valid. */
    Schedule(Schedule &&other) noexcept;
    /** @param other Identity/context to share.
     * @return This handle after replacement; borrowed definition views can be invalidated.
     */
    Schedule &operator=(const Schedule &other) noexcept;
    /** @param other Identity/context; its original handle remains valid.
     * @return This handle after replacement.
     */
    Schedule &operator=(Schedule &&other) noexcept;
    /** Release one live handle; other handles, caches and immutable reader pins retain their models. */
    ~Schedule();
    /** Select the first schedule portion and parse the complete pinned grammar/default rules.
     * @param context Explicit calendar/cache domain.
     * @param instrumentDefinition Borrowed exact UTF-16 trading-hours text.
     * @return Owning schedule identity, reused within the domain when retained, or typed failure.
     */
    [[nodiscard]] static std::expected<Schedule, ScheduleError> getInstance(const ScheduleDataContext &context,
                                                                            std::u16string_view instrumentDefinition);
    /** Select the first exact matching venue using the production portion-selection behavior.
     * @param context Explicit calendar/cache domain.
     * @param instrumentDefinition Borrowed complete UTF-16 trading-hours text.
     * @param venue Exact requested venue; empty venue is accepted.
     * @return Owning schedule identity or MISSING_VENUE/another typed refusal.
     */
    [[nodiscard]] static std::expected<Schedule, ScheduleError> getInstance(const ScheduleDataContext &context,
                                                                            std::u16string_view instrumentDefinition,
                                                                            std::u16string_view venue);
    /** Enumerate venue matches with the reference's separate ASCII-word regex-find semantics.
     * @param instrumentDefinition Borrowed exact UTF-16 trading-hours text.
     * @param limits Finite input/work/construction limits for this owning result.
     * @return Ordered owning venue names, including duplicates, or typed failure.
     */
    [[nodiscard]] static std::expected<std::vector<std::u16string>, ScheduleError>
    getTradingVenues(std::u16string_view instrumentDefinition, ScheduleLimits limits = {});
    /** @return Borrowed exact selected definition; stable while this handle is unchanged/alive. */
    [[nodiscard]] std::u16string_view getDefinition() const noexcept;
    /** @return Owning current generation name.
     * @throws std::bad_alloc The result copy cannot be allocated.
     */
    [[nodiscard]] std::u16string getName() const;
    /** Return the validated requested identity, without implementing a timezone query object.
     * @return Owning exact current-generation timezone ID, including accepted aliases/custom IDs.
     * @throws std::bad_alloc The result copy cannot be allocated.
     */
    [[nodiscard]] std::u16string getTimeZoneId() const;
    /** @param dayId DayUtil label in the inclusive method-specific range [-719160, 2932894].
     * @return Owning day pinned to one complete current calendar generation, or typed refusal.
     */
    [[nodiscard]] std::expected<Day, ScheduleError> getDayById(std::int32_t dayId) const;
    /** @param yearMonthDay Packed YMD in [10103, 99991229]; invalid labels normalize to the lowest later label.
     * @return Owning day pinned to one current generation, or typed refusal.
     */
    [[nodiscard]] std::expected<Day, ScheduleError> getDayByYearMonthDay(std::int32_t yearMonthDay) const;
    /** @param time Signed Unix milliseconds in [-62135424000000, 253402128000000], both ends inclusive.
     * @return The owning day containing time; its label can exceed the direct-label factory's bounds.
     */
    [[nodiscard]] std::expected<Day, ScheduleError> getDayByTime(std::int64_t time) const;
    /** @param time Signed Unix milliseconds, with the same method range as getDayByTime.
     * @return Owning nonempty session containing time, or typed refusal.
     */
    [[nodiscard]] std::expected<Session, ScheduleError> getSessionByTime(std::int64_t time) const;
    /** @param other Schedule identity to compare.
     * @return True only for the same retained identity in the same context; defaults updates preserve it.
     */
    [[nodiscard]] bool operator==(const Schedule &other) const noexcept;
};

/** Owning immutable day snapshot; navigation retains its original calendar/provider generation.
 * The contained live Schedule identity keeps getSchedule valid, while the model pins old rules.
 * Moves preserve the source handle. Copies share models; concurrent replacement of one handle
 * requires external synchronization. Identity equality follows Schedule identity plus day-id.
 */
class Day final {
    Schedule schedule_;
    std::shared_ptr<const detail::DayModel> model_;
    Day(Schedule schedule, std::shared_ptr<const detail::DayModel> model) noexcept;
    friend struct detail::Access;

    public:
    /** @param other Identity and immutable snapshot to share without allocation. */
    Day(const Day &other) noexcept = default;
    /** @param other Snapshot to share; the original handle remains valid. */
    Day(Day &&other) noexcept;
    /** @param other Snapshot to share.
     * @return This handle after replacement.
     */
    Day &operator=(const Day &other) noexcept = default;
    /** @param other Snapshot to share; the original handle remains valid.
     * @return This handle after replacement.
     */
    Day &operator=(Day &&other) noexcept;
    /** @return Owning live Schedule identity; direct queries through it observe current defaults. */
    [[nodiscard]] Schedule getSchedule() const noexcept;
    /** @return Original pinned calendar comparison version, independent of live Schedule metadata. */
    [[nodiscard]] std::int64_t getDefaultsVersion() const noexcept;
    /** @return Consecutive DayUtil day-id label; independent of physical interval duration. */
    [[nodiscard]] std::int32_t getDayId() const noexcept;
    /** @return Packed proleptic Gregorian YMD label. */
    [[nodiscard]] std::int32_t getYearMonthDay() const noexcept;
    /** @return Gregorian year of the day label. */
    [[nodiscard]] std::int32_t getYear() const noexcept;
    /** @return Month from 1 through 12. */
    [[nodiscard]] std::int32_t getMonthOfYear() const noexcept;
    /** @return Day-of-month label. */
    [[nodiscard]] std::int32_t getDayOfMonth() const noexcept;
    /** @return ISO weekday from Monday1 through Sunday7. */
    [[nodiscard]] std::int32_t getDayOfWeek() const noexcept;
    /** @return Whether the pinned holiday list contains this label; joined final days retain source handling. */
    [[nodiscard]] bool isHoliday() const noexcept;
    /** @return Whether the pinned short-day list contains this label. */
    [[nodiscard]] bool isShortDay() const noexcept;
    /** @return Whether any session has a trading type, including zero-duration sessions. */
    [[nodiscard]] bool isTrading() const noexcept;
    /** @return Inclusive start in signed Unix milliseconds. */
    [[nodiscard]] std::int64_t getStartTime() const noexcept;
    /** @return Exclusive end in signed Unix milliseconds. */
    [[nodiscard]] std::int64_t getEndTime() const noexcept;
    /** @return Reset instant in signed Unix milliseconds. */
    [[nodiscard]] std::int64_t getResetTime() const noexcept;
    /** @param time Signed Unix milliseconds.
     * @return Membership in [start,end); empty days contain no instant.
     */
    [[nodiscard]] bool containsTime(std::int64_t time) const noexcept;
    /** @return Ordered owning sessions; zero-duration records are preserved.
     * @throws std::bad_alloc The caller-owned output cannot be allocated.
     */
    [[nodiscard]] std::vector<Session> getSessions() const;
    /** @param time Signed Unix milliseconds; this method searches only this day's existing records.
     * @return Owning containing session or NO_SESSION; empty sessions never contain time.
     */
    [[nodiscard]] std::expected<Session, ScheduleError> getSessionByTime(std::int64_t time) const;
    /** @return Owning first session, with the unfiltered source ANY behavior. */
    [[nodiscard]] Session getFirstSession() const noexcept;
    /** @return Owning last session, with the unfiltered source ANY behavior. */
    [[nodiscard]] Session getLastSession() const noexcept;
    /** @return Previous labelled day in the pinned generation; direct day-id method bounds apply. */
    [[nodiscard]] std::expected<Day, ScheduleError> getPrevDay() const;
    /** @return Next labelled day in the pinned generation; direct day-id method bounds apply. */
    [[nodiscard]] std::expected<Day, ScheduleError> getNextDay() const;
    /** @param other Day identity to compare, possibly from a different calendar snapshot.
     * @return True for the same Schedule identity and day-id, as in the source equality contract.
     */
    [[nodiscard]] bool operator==(const Day &other) const noexcept;
};

/** Owning session handle; its Day pin and record index avoid model cycles and borrowed references. */
class Session final {
    Day day_;
    std::size_t index_;
    Session(Day day, std::size_t index) noexcept;
    friend struct detail::Access;

    public:
    /** @return Owning immutable day snapshot. */
    [[nodiscard]] Day getDay() const noexcept;
    /** @return Source session kind, independent of its duration. */
    [[nodiscard]] SessionType getType() const noexcept;
    /** @return Whether its type is trading; zero-duration trading sessions remain trading. */
    [[nodiscard]] bool isTrading() const noexcept;
    /** @return Whether start is at or after end, including permitted inverted no-trading records. */
    [[nodiscard]] bool isEmpty() const noexcept;
    /** @return Inclusive signed Unix millisecond start. */
    [[nodiscard]] std::int64_t getStartTime() const noexcept;
    /** @return Exclusive signed Unix millisecond end. */
    [[nodiscard]] std::int64_t getEndTime() const noexcept;
    /** @param time Signed Unix milliseconds.
     * @return Membership in [start,end); an empty session contains no instant.
     */
    [[nodiscard]] bool containsTime(std::int64_t time) const noexcept;
    /** @return Previous session in source ordering, crossing a day boundary in the pinned generation. */
    [[nodiscard]] std::expected<Session, ScheduleError> getPrevSession() const;
    /** @return Next session in source ordering, crossing a day boundary in the pinned generation. */
    [[nodiscard]] std::expected<Session, ScheduleError> getNextSession() const;
    /** @param other Session to compare.
     * @return Source equality: day identity, type and exact interval endpoints.
     */
    [[nodiscard]] bool operator==(const Session &other) const noexcept;
};
} // namespace org::ttldtor::qdcxx::schedule
