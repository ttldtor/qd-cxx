// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
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
} // namespace detail

/** Typed parser, transaction and logical-resource failures; no partial update is published. */
enum class ScheduleError {
    INVALID_DEFINITION, ///< Invalid schedule text, time, list reference or recognized strategy.
    MISSING_VENUE,      ///< The requested venue does not occur in the instrument's trading hours.
    INVALID_DEFAULTS,   ///< Invalid or duplicate typed calendar-default entry.
    UNKNOWN_TIME_ZONE,  ///< The exact ID is outside the pinned compatibility identity profile.
    RESOURCE_LIMIT,     ///< A finite input/work/staging/cache/retained-model limit would be exceeded.
    ALLOCATION,         ///< An owning allocation failed before publication.
    CONCURRENT_CHANGE   ///< A conflicting bounded transaction committed; the caller may explicitly retry.
};

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
};

/** Current shared-retention observations; caller-owned handle copies are not distinct models. */
struct ScheduleUsage {
    std::size_t logicalBytes = 0; ///< Charged retained model fields, excluding allocator/control-block overhead.
    std::size_t generations = 0;  ///< Distinct retained calendar generations.
    std::size_t identities = 0;   ///< Distinct retained exact selected definitions.
    std::size_t ruleModels = 0;   ///< Distinct retained parsed definition/generation models.
    std::size_t bundles = 0;      ///< Immutable bundles retained by context, transactions or readers.
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

/** Owning Java-like schedule identity in an explicit context, without Day/Session queries.
 * Getters use the current completely published calendar generation; copies share identity and
 * retain the context. Input views are never retained. Metadata returns owning values where a
 * concurrent defaults replacement could otherwise invalidate a borrowed view. No callbacks,
 * implicit source discovery, timezone querying or worker/progress work occurs in getters.
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
    /** @param other Schedule identity to compare.
     * @return True only for the same retained identity in the same context; defaults updates preserve it.
     */
    [[nodiscard]] bool operator==(const Schedule &other) const noexcept;
};
} // namespace org::ttldtor::qdcxx::schedule
