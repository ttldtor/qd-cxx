// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "qd/schedule/schedule.hpp"
#include "schedule_allocations.hpp"
#include "schedule_private.hpp"

#include <atomic>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <thread>

using namespace org::ttldtor::qdcxx::schedule;
namespace internal = org::ttldtor::qdcxx::schedule::detail;
namespace allocations = org::ttldtor::qdcxx::schedule::allocation_test;

namespace {
constexpr std::int64_t UPDATE_VERSION = 1780000000000LL;

/** @param value ASCII fixture row.
 * @param delimiter Cell separator.
 * @return All cells, preserving trailing empty cells.
 */
std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> result;

    for (std::size_t at = 0;;) {
        const auto end = value.find(delimiter, at);
        result.emplace_back(value.substr(at, end == value.npos ? end : end - at));

        if (end == value.npos) {
            break;
        }

        at = end + 1;
    }

    return result;
}

/** @param text Borrowed hexadecimal UTF-16 code units.
 * @return Exact owning UTF-16, including unpaired surrogate code units.
 */
std::u16string decode(std::string_view text) {
    if (text == "-") {
        return {};
    }

    std::u16string result;

    for (std::size_t at = 0; at < text.size(); at += 4) {
        result.push_back(static_cast<char16_t>(std::stoul(std::string(text.substr(at, 4)), nullptr, 16)));
    }

    return result;
}

/** @param text Borrowed UTF-16.
 * @return Stable ASCII hexadecimal code units.
 */
std::string hex(std::u16string_view text) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');

    for (auto unit : text) {
        output << std::setw(4) << static_cast<unsigned>(unit);
    }

    return output.str();
}

/** @param output ASCII projection destination.
 * @param value Actual parsed local fields.
 */
void time(std::ostream &output, internal::TimeDefinition value) {
    output << value.day << ':' << value.hour << ':' << value.minute << ':' << value.second;
}

/** @param output ASCII projection destination.
 * @param rules Actual production-owned model.
 * @param index Definition index, or the absent source slot.
 */
void day(std::ostream &output, const internal::Rules &rules, std::size_t index) {
    if (index == internal::NO_DEFINITION) {
        output << '-';

        return;
    }

    const auto &value = rules.definitions.at(index);
    output << '[';
    time(output, value.start);
    output << '>';
    time(output, value.end);
    output << '/';
    time(output, value.reset);
    output << '{';

    for (const auto &session : value.sessions) {
        const auto kind = session.kind == internal::SessionKind::NO_TRADING   ? "NO_TRADING"
                          : session.kind == internal::SessionKind::PRE_MARKET ? "PRE_MARKET"
                          : session.kind == internal::SessionKind::REGULAR    ? "REGULAR"
                                                                              : "AFTER_MARKET";
        output << kind << ':';
        time(output, session.start);
        output << '>';
        time(output, session.end);
        output << ',';
    }

    output << "}]";
}

/** @param schedule Actual public factory result.
 * @return Constructed model projection matching the actual Java field capture schema.
 */
std::string projection(const Schedule &schedule) {
    const auto rules = internal::Access::rules(schedule);
    std::ostringstream output;
    output << std::boolalpha << hex(rules->definition) << '\t' << hex(rules->name) << '\t' << hex(rules->zone) << '\t'
           << rules->dayOffset << '\t' << rules->earlyClose << '\t' << rules->joinNextTradingDay << '\t';

    for (auto value : rules->holidays) {
        output << value << ',';
    }

    output << '\t';

    for (auto value : rules->shortDays) {
        output << value << ',';
    }

    output << '\t';

    for (const auto index : rules->weekDays) {
        day(output, *rules, index);
        output << ';';
    }

    output << '\t';

    for (const auto &strategy : rules->overrides) {
        output << strategy.yearMonthDay << '=' << strategy.forcedTrading << ',' << strategy.closed << ','
               << strategy.earlyClose << ',' << strategy.earlyCloseMillis << ',';
        day(output, *rules, strategy.definition);
        output << ';';
    }

    return output.str();
}

/** Own fixture input backing until the update returns. */
struct TypedEntries {
    std::vector<std::u16string> strings;
    std::vector<CalendarDefaultsEntry> entries;
};

/** @param descriptor Exact typed fixture entries.
 * @return Owning backing and borrowed entry views.
 */
TypedEntries entries(std::string_view descriptor) {
    TypedEntries result;

    if (descriptor == "-") {
        return result;
    }

    const auto items = split(descriptor, '|');
    result.strings.reserve(items.size() * 2);

    for (const auto &item : items) {
        const auto cells = split(item, '/');
        result.strings.push_back(decode(cells.at(1)));
        result.strings.push_back(decode(cells.at(2)));
        const auto kind = cells[0] == "H"   ? CalendarEntryKind::HOLIDAY_DAYS
                          : cells[0] == "S" ? CalendarEntryKind::SHORT_DAYS
                          : cells[0] == "X" ? CalendarEntryKind::SPECIAL_DAYS
                                            : CalendarEntryKind::VENUE;
        result.entries.push_back({kind, result.strings[result.strings.size() - 2], result.strings.back()});
    }

    return result;
}

/** @param context Explicit owning domain.
 * @param properties Complete TST property string, copied during update.
 * @param version Explicit typed comparison version.
 * @return Atomic update outcome.
 */
std::expected<DefaultsUpdate, ScheduleError> update(const ScheduleDataContext &context, std::u16string_view properties,
                                                    std::int64_t version = UPDATE_VERSION) {
    const CalendarDefaultsEntry input[] = {{CalendarEntryKind::VENUE, u"TST", properties}};

    return context.setDefaults({version, input});
}

/** @param limits Independent finite context limits.
 * @return Context with one validated test venue.
 */
ScheduleDataContext customContext(ScheduleLimits limits = {}) {
    auto context = ScheduleDataContext::create(limits);
    REQUIRE(context.has_value());
    REQUIRE(update(*context, u"name=before;tz=UTC;0=09001700").has_value());

    return *context;
}
} // namespace

TEST_CASE("schedule factories reproduce the complete captured production grammar family") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_SCHEDULE_CASES);
    std::ifstream expected(ORG_TTLDTOR_QDCXX_SCHEDULE_ORACLE);
    REQUIRE(input.good());
    REQUIRE(expected.good());
    auto context = ScheduleDataContext::create();
    REQUIRE(context.has_value());
    std::string row;
    std::string observation;
    std::size_t count = 0;

    while (std::getline(input, row)) {
        REQUIRE(std::getline(expected, observation));
        const auto cells = split(row, '\t');
        const auto oracle = split(observation, '\t');
        REQUIRE(cells.size() == 6);
        REQUIRE(oracle.size() >= 3);
        INFO("case " << cells[0]);
        CHECK(oracle[1] == cells[0]);
        const auto text = decode(cells[3]);

        if (cells[1] == "V") {
            const auto venues = Schedule::getTradingVenues(text);
            REQUIRE(venues.has_value());
            std::string actual;

            for (const auto &venue : *venues) {
                actual += hex(venue) + ',';
            }

            CHECK(oracle[0] == "VENUES");
            CHECK(actual == oracle[2]);
        } else {
            std::optional<ScheduleError> updateError;

            if (cells[1] == "U") {
                auto typed = entries(cells[5]);
                auto result = context->setDefaults({std::stoll(cells[4]), typed.entries});

                if (!result) {
                    updateError = result.error();
                }
            }

            auto result = updateError       ? std::expected<Schedule, ScheduleError>(std::unexpected(*updateError))
                          : cells[1] == "N" ? Schedule::getInstance(*context, text, decode(cells[2]))
                                            : Schedule::getInstance(*context, text);

            if (oracle[0] == "ERROR") {
                REQUIRE_FALSE(result.has_value());

                if (oracle[2] == "NoSuchElementException") {
                    CHECK(result.error() == ScheduleError::MISSING_VENUE);
                } else {
                    CHECK((result.error() == ScheduleError::INVALID_DEFINITION ||
                           result.error() == ScheduleError::INVALID_DEFAULTS ||
                           result.error() == ScheduleError::UNKNOWN_TIME_ZONE));
                }
            } else {
                if (!result) {
                    INFO("native error " << static_cast<int>(result.error()));
                }

                REQUIRE(result.has_value());
                CHECK(oracle[0] == "OK");
                const auto start = observation.find('\t', observation.find('\t') + 1) + 1;
                CHECK(projection(*result) == observation.substr(start));
            }
        }

        ++count;
    }

    CHECK(count == 867);
    CHECK_FALSE(std::getline(expected, observation));
}

TEST_CASE("schedule ownership and context generations preserve live identities and old rule pins") {
    auto context = customContext();
    std::u16string definition = u"TST()";
    auto schedule = Schedule::getInstance(context, definition);
    REQUIRE(schedule.has_value());
    const auto identity = *schedule;
    const auto oldRules = internal::Access::rules(*schedule);
    definition.assign(u"destroyed caller backing");
    CHECK(schedule->getDefinition() == u"TST()");
    CHECK(schedule->getName() == u"before");
    auto moved = std::move(*schedule);
    CHECK(*schedule == moved);
    REQUIRE(update(context, u"name=after;tz=GMT+05:30;0=10001800").has_value());
    CHECK(moved == identity);
    CHECK(moved.getName() == u"after");
    CHECK(moved.getTimeZoneId() == u"GMT+05:30");
    CHECK(oldRules->name == u"before");
    CHECK(oldRules->zone == u"UTC");
    CHECK(oldRules->definitions[oldRules->weekDays[1]].sessions[1].start.hour == 9);
    CHECK(context.getUsage().generations == 2);
    auto repeated = Schedule::getInstance(context, u"TST()");
    REQUIRE(repeated.has_value());
    CHECK(*repeated == moved);
    const auto ignored = update(context, u"name=older;tz=UTC;0=09001700", UPDATE_VERSION - 1);
    REQUIRE(ignored.has_value());
    CHECK(*ignored == DefaultsUpdate::OLDER_IGNORED);
    CHECK(moved.getName() == u"after");
    CHECK_FALSE(update(context, u"tz=UNKNOWN;0=09001700", UPDATE_VERSION - 1));
    CHECK(moved.getName() == u"after");
    auto independent = customContext();
    auto other = Schedule::getInstance(independent, u"TST()");
    REQUIRE(other.has_value());
    CHECK_FALSE(*other == moved);
    CHECK(other->getName() == u"before");
}

TEST_CASE("schedule handles and internal immutable query pins outlive their original context handles") {
    auto retained = [] {
        auto context = customContext();

        return Schedule::getInstance(context, u"TST()").value();
    }();
    CHECK(retained.getName() == u"before");
    const auto pin = internal::Access::rules(retained);
    CHECK(pin->zone == u"UTC");
}

TEST_CASE("Schedule copy move assignment and destruction count handles independently from bundle owners") {
    auto context = customContext();
    auto original = Schedule::getInstance(context, u"TST()");
    REQUIRE(original.has_value());
    const auto identity = [&] {
        const auto bundle = internal::Access::context(context)->current.load(std::memory_order_acquire);

        for (const auto &entry : bundle->entries) {
            if (entry.identity->definition == u"TST()") {
                return entry.identity;
            }
        }

        return std::shared_ptr<const internal::Identity>{};
    }();
    REQUIRE(identity != nullptr);
    CHECK(identity->liveHandles.load() == 1);

    {
        auto copy = *original;
        auto moved = std::move(copy);
        auto replacement = Schedule::getInstance(context, u"TST(name=other)");
        REQUIRE(replacement.has_value());
        *replacement = *original;
        *replacement = std::move(moved);
        *replacement = *replacement;
        *replacement = std::move(*replacement);
        CHECK(identity->liveHandles.load() == 4);
        CHECK(copy.getName() == u"before");
        CHECK(moved.getName() == u"before");
    }

    CHECK(identity->liveHandles.load() == 1);
    original = std::unexpected(ScheduleError::INVALID_DEFINITION);
    CHECK(identity->liveHandles.load() == 0);
}

TEST_CASE("schedule quotas refuse before publication and inactive collection permits continued reuse") {
    ScheduleLimits limits;
    limits.retainedSchedules = 3;
    limits.inactiveCacheTarget = 0;
    auto context = customContext(limits);
    auto live = Schedule::getInstance(context, u"TST()");
    REQUIRE(live.has_value());

    for (int i = 0; i < 100; ++i) {
        const auto suffix = std::to_string(i);
        const auto name = std::u16string(suffix.begin(), suffix.end());
        auto value = Schedule::getInstance(context, u"(tz=UTC;td=;name=" + name + u")");
        REQUIRE(value.has_value());
        CHECK(context.getUsage().identities <= limits.retainedSchedules);
        CHECK(live->getName() == u"before");
    }

    auto noInput = ScheduleLimits{};
    noInput.inputUnits = 1;
    auto small = ScheduleDataContext::create(noInput);
    REQUIRE(small.has_value());
    auto refused = Schedule::getInstance(*small, u"XNYS()");
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error() == ScheduleError::RESOURCE_LIMIT);
    CHECK(small->getUsage().identities == 0);
    auto noRetainedBytes = ScheduleLimits{};
    noRetainedBytes.retainedBytes = 1;
    const auto bytesFailure = ScheduleDataContext::create(noRetainedBytes);
    REQUIRE_FALSE(bytesFailure.has_value());
    CHECK(bytesFailure.error() == ScheduleError::RESOURCE_LIMIT);
    auto noWork = ScheduleLimits{};
    noWork.work = 0;
    const auto workFailure = ScheduleDataContext::create(noWork);
    REQUIRE_FALSE(workFailure.has_value());
    CHECK(workFailure.error() == ScheduleError::RESOURCE_LIMIT);
    auto noStaging = ScheduleLimits{};
    noStaging.stagingBytes = 0;
    const auto stagingFailure = ScheduleDataContext::create(noStaging);
    REQUIRE_FALSE(stagingFailure.has_value());
    CHECK(stagingFailure.error() == ScheduleError::RESOURCE_LIMIT);
    auto oneRule = ScheduleLimits{};
    oneRule.retainedRuleModels = 1;
    auto one = customContext(oneRule);
    auto handle = Schedule::getInstance(one, u"TST()");
    REQUIRE(handle.has_value());
    const auto before = one.getUsage();
    auto failed = update(one, u"name=unpublished;tz=UTC;0=10001800", UPDATE_VERSION + 1);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error() == ScheduleError::RESOURCE_LIMIT);
    CHECK(handle->getName() == u"before");
    CHECK(one.getDefaultsVersion() == UPDATE_VERSION);
    CHECK(one.getUsage().logicalBytes == before.logicalBytes);
    CHECK(one.getUsage().generations == before.generations);
}

TEST_CASE("schedule allocation failures preserve defaults and live models") {
    allocations::arm(0);
    auto measured = ScheduleDataContext::create();
    const auto createCalls = allocations::disarm();
    REQUIRE(measured.has_value());

    for (const auto ordinal : {std::size_t{1}, std::size_t{2}, std::size_t{10}, std::size_t{100}, std::size_t{1000}}) {
        if (ordinal > createCalls) {
            continue;
        }

        allocations::arm(ordinal);
        auto failed = ScheduleDataContext::create();
        allocations::disarm();
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error() == ScheduleError::ALLOCATION);
    }

    auto context = customContext();
    auto handle = Schedule::getInstance(context, u"TST()");
    REQUIRE(handle.has_value());
    const auto before = context.getUsage();
    const CalendarDefaultsEntry next[] = {{CalendarEntryKind::VENUE, u"TST", u"name=after;tz=UTC;0=10001800"}};

    for (const auto ordinal : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{5}, std::size_t{10}}) {
        allocations::arm(ordinal);
        auto failed = context.setDefaults({UPDATE_VERSION + 1, next});
        allocations::disarm();
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error() == ScheduleError::ALLOCATION);
        CHECK(handle->getName() == u"before");
        CHECK(context.getDefaultsVersion() == UPDATE_VERSION);
        CHECK(context.getUsage().logicalBytes == before.logicalBytes);
    }

    for (const auto ordinal : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{5}, std::size_t{10}}) {
        allocations::arm(ordinal);
        auto failed = Schedule::getInstance(context, u"TST(name=other)");
        allocations::disarm();
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.error() == ScheduleError::ALLOCATION);
        CHECK(handle->getName() == u"before");
        CHECK(context.getUsage().logicalBytes == before.logicalBytes);
    }
}

TEST_CASE("old publication pins are not live Schedule handles during typed replacement") {
    auto context = customContext();
    auto live = Schedule::getInstance(context, u"(tz=UTC;td=;name=live)");
    REQUIRE(live.has_value());

    {
        auto unused = Schedule::getInstance(context, u"TST()");
        REQUIRE(unused.has_value());
    }

    const auto oldBundle = internal::Access::context(context)->current.load(std::memory_order_acquire);
    auto another = Schedule::getInstance(context, u"(tz=UTC;td=;name=another)");
    REQUIRE(another.has_value());
    const CalendarDefaultsEntry replacement[] = {{CalendarEntryKind::VENUE, u"OTHER", u"tz=UTC;0=09001700"}};
    const auto result = context.setDefaults({UPDATE_VERSION + 1, replacement});
    REQUIRE(result.has_value());
    CHECK(*result == DefaultsUpdate::APPLIED);
    CHECK(live->getName() == u"live");
    CHECK(another->getName() == u"another");
    CHECK(oldBundle->defaults->version == UPDATE_VERSION);
    CHECK_FALSE(Schedule::getInstance(context, u"TST()"));
}

TEST_CASE("factory failure may collect only inactive cache while preserving live calendar state") {
    bool observedCollectionBeforeFailure = false;

    for (std::size_t ordinal = 1; ordinal <= 30 && !observedCollectionBeforeFailure; ++ordinal) {
        ScheduleLimits limits;
        limits.inactiveCacheTarget = 0;
        auto context = customContext(limits);
        auto live = Schedule::getInstance(context, u"TST()");
        REQUIRE(live.has_value());

        {
            auto unused = Schedule::getInstance(context, u"(tz=UTC;td=;name=unused)");
            REQUIRE(unused.has_value());
        }

        const auto before = context.getUsage();
        allocations::arm(ordinal);
        auto result = Schedule::getInstance(context, u"(tz=UTC;td=;name=new)");
        allocations::disarm();

        if (!result) {
            REQUIRE(result.error() == ScheduleError::ALLOCATION);
            CHECK(live->getName() == u"before");
            CHECK(live->getTimeZoneId() == u"UTC");
            CHECK(context.getDefaultsVersion() == UPDATE_VERSION);
            observedCollectionBeforeFailure = context.getUsage().identities < before.identities;
        }
    }

    CHECK(observedCollectionBeforeFailure);
}

TEST_CASE("schedule publication remains coherent during concurrent metadata readers and bounded writers") {
    auto context = customContext();
    auto handle = Schedule::getInstance(context, u"TST()");
    REQUIRE(handle.has_value());
    std::atomic<bool> done{false};
    std::atomic<std::size_t> invalid{0};
    std::atomic<std::size_t> applied{0};
    std::vector<std::thread> readers;

    for (int i = 0; i < 3; ++i) {
        readers.emplace_back([&] {
            while (!done.load(std::memory_order_acquire)) {
                const auto rules = internal::Access::rules(*handle);
                const bool before = rules->name == u"before" && rules->zone == u"UTC";
                const bool first = rules->name == u"first" && rules->zone == u"GMT+05:30";
                const bool second = rules->name == u"second" && rules->zone == u"UTC";

                if (!before && !first && !second) {
                    invalid.fetch_add(1, std::memory_order_relaxed);
                }

                auto identity = Schedule::getInstance(context, u"TST()");

                if (identity && identity->getDefinition() != u"TST()") {
                    invalid.fetch_add(1, std::memory_order_relaxed);
                }

                // No permanently live handle protects this identity: cache-hit reservation races
                // with its last release and defaults publication dropping inactive entries.
                auto ephemeral = Schedule::getInstance(context, u"(tz=UTC;td=;name=ephemeral)");

                if (ephemeral) {
                    auto copied = *ephemeral;
                    auto moved = std::move(copied);

                    if (copied.getName() != u"ephemeral" || moved.getTimeZoneId() != u"UTC") {
                        invalid.fetch_add(1, std::memory_order_relaxed);
                    }
                } else if (ephemeral.error() != ScheduleError::CONCURRENT_CHANGE &&
                           ephemeral.error() != ScheduleError::RESOURCE_LIMIT) {
                    invalid.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    auto writer = [&](bool first) {
        for (int i = 0; i < 100; ++i) {
            auto result =
                update(context, first ? u"name=first;tz=GMT+05:30;0=09001700" : u"name=second;tz=UTC;0=10001800",
                       UPDATE_VERSION + i);

            if (result && *result == DefaultsUpdate::APPLIED) {
                applied.fetch_add(1, std::memory_order_relaxed);
            } else if (!result && result.error() != ScheduleError::CONCURRENT_CHANGE &&
                       result.error() != ScheduleError::RESOURCE_LIMIT) {
                invalid.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };
    std::thread first(writer, true);
    std::thread second(writer, false);
    first.join();
    second.join();
    done.store(true, std::memory_order_release);

    for (auto &reader : readers) {
        reader.join();
    }

    CHECK(invalid.load() == 0);
    CHECK(applied.load() != 0);
    CHECK(context.getUsage().generations <= 4);
}
