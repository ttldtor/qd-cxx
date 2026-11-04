// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/schedule/schedule.hpp"
#include "schedule_allocations.hpp"
#include "schedule_private.hpp"
#include "schedule_query_projection.hpp"
#include <atomic>
#include <chrono>
#include <doctest/doctest.h>
#include <fstream>
#include <map>
#include <thread>

using namespace org::ttldtor::qdcxx::schedule;
namespace internal = org::ttldtor::qdcxx::schedule::detail;
namespace zone = org::ttldtor::qdcxx::time;
namespace fixture = org::ttldtor::qdcxx::schedule::conformance;

namespace {
/** @tparam T Owning success type.
 * @tparam E Actual typed refusal.
 * @param result Product operation result.
 * @return Owning success, or throw outside product code when the test requires success.
 */
template <class T, class E> T value(std::expected<T, E> result) {
    if (!result) {
        throw std::runtime_error("Unexpected typed refusal");
    }

    return std::move(*result);
}

/** @return One ready process capability shared by independent test domains. */
const ScheduleTimeZoneData &data() {
    static const auto ready = value(ScheduleTimeZoneData::initialize(ORG_TTLDTOR_QDCXX_QUERY_DATA));

    return ready;
}

/** @param limits Explicit finite test-domain bounds.
 * @return Independent domain with a deterministic constructor reference.
 */
ScheduleDataContext domain(ScheduleLimits limits = {}) {
    return value(ScheduleDataContext::create(data(), limits, {1793793600000LL, 128}));
}

/** @param context Explicit owning domain.
 * @param definition Complete accepted rule text.
 * @return Owning live schedule identity required by this test.
 */
Schedule schedule(const ScheduleDataContext &context,
                  std::u16string_view definition = u"(tz=UTC;td=1234567;0=0000+0000)") {
    return value(Schedule::getInstance(context, definition));
}

/** @param path Exact raw LF fixture path.
 * @return Owning rows without modifying fixture bytes; throws on missing/unreadable input.
 */
std::vector<std::string> lines(const char *path) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error("Missing conformance fixture");
    }

    std::vector<std::string> result;

    for (std::string line; std::getline(input, line);) {
        result.push_back(std::move(line));
    }

    if (!input.eof()) {
        throw std::runtime_error("Fixture read failure");
    }

    return result;
}

/** @param context Explicit domain to update.
 * @param version Strictly increasing typed calendar version.
 * @param definition Complete TST venue properties copied by the product.
 */
void update(const ScheduleDataContext &context, std::int64_t version, std::u16string_view definition) {
    const CalendarDefaultsEntry entry{CalendarEntryKind::VENUE, u"TST", definition};
    const auto result = context.setDefaults({version, std::span(&entry, 1)});
    REQUIRE(result);
    CHECK(*result == DefaultsUpdate::APPLIED);
}
} // namespace

TEST_CASE("actual Java filtered navigation observations") {
    const auto cases = lines(ORG_TTLDTOR_QDCXX_FILTER_CASES);
    const auto oracle = lines(ORG_TTLDTOR_QDCXX_FILTER_ORACLE);
    REQUIRE(cases.size() == 6);
    REQUIRE(cases.size() == oracle.size());
    fixture::QueryProjection projection(data());

    for (std::size_t i = 0; i < cases.size(); ++i) {
        CAPTURE(i);
        CHECK(projection.observe(cases[i]) == oracle[i]);
    }
}

TEST_CASE("midnight and day IDs match actual Java observations") {
    const auto cases = lines(ORG_TTLDTOR_QDCXX_MIDNIGHT_CASES);
    const auto oracle = lines(ORG_TTLDTOR_QDCXX_MIDNIGHT_ORACLE);
    REQUIRE(cases.size() == 5);
    REQUIRE(cases.size() == oracle.size());
    fixture::QueryProjection projection(data());

    for (std::size_t index = 0; index < cases.size(); ++index) {
        CAPTURE(index);
        CHECK(projection.observe(cases[index]) == oracle[index]);
    }
}

TEST_CASE("filtered navigation preserves conjunction, order and typed failures") {
    const auto context = domain();
    const auto source = schedule(context, u"(tz=UTC;td=12345;0=0900120013001700;hd=20241225;sd=20241224)");
    const auto tuesday = value(source.getDayByYearMonthDay(20241224));
    const auto holiday = value(source.getDayByYearMonthDay(20241225));
    CHECK(DayFilter::TUESDAY.accept(tuesday));
    CHECK(DayFilter::SHORT_DAY.accept(tuesday));
    CHECK(DayFilter::HOLIDAY.accept(holiday));
    CHECK(DayFilter::NON_TRADING.accept(holiday));
    CHECK_FALSE((DayFilter{1U << 2, {}, {}, false}.accept(tuesday)));
    CHECK_FALSE(DayFilter{1U << 0}.accept(tuesday));
    CHECK(value(tuesday.getNextDay(DayFilter::TRADING)).getYearMonthDay() == 20241226);
    CHECK(value(holiday.getPrevDay(DayFilter::TRADING)).getYearMonthDay() == 20241224);
    CHECK(value(tuesday.getFirstSession(SessionFilter::REGULAR)).getType() == SessionType::REGULAR);
    CHECK_FALSE(holiday.findFirstSession(SessionFilter::TRADING));
    const auto absent = holiday.getFirstSession(SessionFilter::TRADING);
    REQUIRE_FALSE(absent);
    CHECK(absent.error() == ScheduleError::NO_MATCH);
    const SessionFilter impossible{SessionType::REGULAR, false};
    CHECK_FALSE(tuesday.findFirstSession(impossible));
    const auto first = value(tuesday.getFirstSession(SessionFilter::REGULAR));
    const auto next = value(first.getNextSession(SessionFilter::REGULAR));
    CHECK(first.getStartTime() == 1735030800000LL);
    CHECK(next.getStartTime() == 1735045200000LL);
    CHECK(next.getStartTime() > first.getStartTime());
    CHECK(value(next.getPrevSession(SessionFilter::REGULAR)) == first);
    CHECK(value(tuesday.getLastSession(SessionFilter::REGULAR)) == next);
    const auto limitDay = value(source.getDayById(2932894));
    const auto range = limitDay.findNextDay(DayFilter::TRADING);
    REQUIRE_FALSE(range);
    CHECK(range.error() == ScheduleError::OUT_OF_RANGE);
}

TEST_CASE("filtered search exhaustion, budgets and retained generations") {
    ScheduleLimits limits;
    limits.queryWork = 10000000;
    limits.queryStagingBytes = 64000000;
    const auto context = domain(limits);
    update(context, 1780000000000LL, u"name=before;tz=UTC;td=12345;0=09001700");
    const auto source = schedule(context, u"TST()");
    const auto friday = value(source.getDayByYearMonthDay(20241227));
    const DayFilter impossible{0, true, {}, true};
    const auto missing = friday.findNextDay(impossible);
    REQUIRE(missing);
    CHECK_FALSE(*missing);
    const auto required = friday.getNextDay(impossible);
    REQUIRE_FALSE(required);
    CHECK(required.error() == ScheduleError::NO_MATCH);
    const SessionFilter impossibleSession{SessionType::REGULAR, false};
    const auto last = friday.getLastSession();
    const auto missingSession = last.findNextSession(impossibleSession);
    REQUIRE(missingSession);
    CHECK_FALSE(*missingSession);
    const auto requiredSession = last.getNextSession(impossibleSession);
    REQUIRE_FALSE(requiredSession);
    CHECK(requiredSession.error() == ScheduleError::NO_MATCH);
    update(context, 1780000001000LL, u"name=after;tz=UTC;td=1234567;0=10001800");
    const auto oldNext = value(friday.getNextDay(DayFilter::TRADING));
    CHECK(oldNext.getYearMonthDay() == 20241230);
    CHECK(oldNext.getDefaultsVersion() == 1780000000000LL);
    CHECK(value(source.getDayByYearMonthDay(20241228)).isTrading());

    ScheduleLimits tight;
    tight.queryWork = 1000;
    const auto tightContext = domain(tight);
    const auto tightSource = schedule(tightContext);
    const auto tightDay = value(tightSource.getDayByYearMonthDay(20241224));
    const auto refused = tightDay.findNextDay(DayFilter{1});
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == ScheduleError::RESOURCE_LIMIT);
}

TEST_CASE("actual Java queries with explicit per-case pinned-data differences") {
    const auto cases = lines(ORG_TTLDTOR_QDCXX_QUERY_CASES);
    const auto oracle = lines(ORG_TTLDTOR_QDCXX_QUERY_ORACLE);
    std::map<std::string, std::string> contextual;

    for (const auto &line : lines(ORG_TTLDTOR_QDCXX_QUERY_CONTEXTUAL)) {
        const auto cells = fixture::splitCells(line, '\t');
        REQUIRE(contextual.emplace(std::string(cells[1]), line).second);
    }

    REQUIRE(cases.size() == 2303);
    REQUIRE(cases.size() == oracle.size());
    REQUIRE(contextual.size() == 36);
    fixture::QueryProjection projection(data());
    std::size_t used = 0;

    for (std::size_t index = 0; index < cases.size(); ++index) {
        const auto cells = fixture::splitCells(oracle[index], '\t');
        CAPTURE(cells[1]);
        auto expected = oracle[index];
        auto actual = projection.observe(cases[index]);

        if (const auto replacement = contextual.find(std::string(cells[1])); replacement != contextual.end()) {
            expected = replacement->second;
            ++used;
        } else if (cells[1] == "empty-day-time") {
            expected = "ERROR\tempty-day-time\tNoSuchElementException";
        } else if (cells[0] == "RAW") {
            expected.clear();

            for (std::size_t field = 0; field < 4; ++field) {
                if (field != 0) {
                    expected += '\t';
                }

                expected += cells[field];
            }
        } else if (cells[0] == "HOT") {
            const auto projected = fixture::splitCells(actual, '\t');
            REQUIRE(cells.size() == 11);
            REQUIRE(projected.size() == 12);

            for (std::size_t field = 0; field < 9; ++field) {
                CHECK(projected[field] == cells[field]);
            }

            CHECK(projected[9] == cells[9]);
            CHECK(projected[10] == cells[9]);
            CHECK(projected[11] == cells[10]);
            continue;
        }

        CHECK(actual == expected);
    }

    CHECK(used == 36);
}

TEST_CASE("parser-only queries refuse without ambient timezone initialization") {
    const auto context = value(ScheduleDataContext::create());
    const auto source = schedule(context);
    const auto day = source.getDayById(0);
    REQUIRE_FALSE(day);
    CHECK(day.error() == ScheduleError::NOT_READY);
}

TEST_CASE("day and session handles own generations and do not duplicate retained records") {
    auto context = domain();
    update(context, 1780000000000LL, u"name=before;tz=UTC;td=1234567;0=09001700");
    auto source = schedule(context, u"TST()");
    auto old = value(source.getDayByYearMonthDay(20241224));
    auto session = old.getFirstSession();
    const auto before = old.getStartTime();
    const auto next = value(old.getNextDay()).getStartTime();
    const auto usage = context.getUsage();
    auto copied = old;
    auto moved = std::move(copied);
    auto sessionCopy = std::move(session);
    CHECK(copied.getStartTime() == before);
    CHECK(session.getDay() == old);
    CHECK(moved == old);
    CHECK(context.getUsage().days == usage.days);
    CHECK(context.getUsage().sessions == usage.sessions);
    update(context, 1780000001000LL, u"name=after;tz=UTC;td=1234567;0=10001800");
    const auto current = value(source.getDayByYearMonthDay(20241224));
    CHECK(current.getSessions()[1].getStartTime() != old.getSessions()[1].getStartTime());
    CHECK(old.getStartTime() == before);
    CHECK(value(old.getNextDay()).getStartTime() == next);
    CHECK(old.getSchedule().getName() == u"after");
    CHECK(sessionCopy.getDay().getStartTime() == before);
    CHECK(old.getDefaultsVersion() == 1780000000000LL);
    CHECK(current.getDefaultsVersion() == 1780000001000LL);
}

TEST_CASE("day retention, record, cache, staging and work limits fail finitely") {
    SUBCASE("live handles bound models, and release permits a new model") {
        ScheduleLimits limits;
        limits.retainedDays = 1;
        limits.dayCacheEntries = 0;
        auto context = domain(limits);
        auto source = schedule(context);
        {
            auto held = value(source.getDayById(0));
            auto denied = source.getDayById(1);
            REQUIRE_FALSE(denied);
            CHECK(denied.error() == ScheduleError::RESOURCE_LIMIT);
            CHECK(held.getDayId() == 0);
        }
        CHECK(context.getUsage().days == 0);
        CHECK(source.getDayById(1));
    }
    SUBCASE("records are charged independently from copied handles") {
        ScheduleLimits limits;
        limits.retainedSessions = 0;
        auto source = schedule(domain(limits));
        auto denied = source.getDayById(0);
        REQUIRE_FALSE(denied);
        CHECK(denied.error() == ScheduleError::RESOURCE_LIMIT);
    }
    SUBCASE("zero cache is a supported caller choice") {
        ScheduleLimits limits;
        limits.dayCacheEntries = 0;
        limits.retainedDayCaches = 0;
        auto context = domain(limits);
        auto source = schedule(context);
        auto day = value(source.getDayById(0));
        CHECK(day.getDayId() == 0);
        CHECK(context.getUsage().dayCaches == 0);
    }
    SUBCASE("zero work refuses before date construction") {
        ScheduleLimits limits;
        limits.queryWork = 0;
        auto source = schedule(domain(limits));
        auto denied = source.getDayById(0);
        REQUIRE_FALSE(denied);
        CHECK(denied.error() == ScheduleError::RESOURCE_LIMIT);
    }
    SUBCASE("zero staging refuses before allocation") {
        ScheduleLimits limits;
        limits.queryStagingBytes = 0;
        auto source = schedule(domain(limits));
        auto denied = source.getDayById(0);
        REQUIRE_FALSE(denied);
        CHECK(denied.error() == ScheduleError::RESOURCE_LIMIT);
    }
    SUBCASE("empty days remain valid but a time lookup must contain its instant") {
        ScheduleLimits limits;
        limits.queryWork = 100;
        auto source = schedule(domain(limits), u"(tz=UTC;td=1234567;0=d00000000)");
        const auto empty = value(source.getDayById(0));
        REQUIRE(empty.getStartTime() == empty.getEndTime());
        auto denied = source.getDayByTime(0);
        REQUIRE_FALSE(denied);
        CHECK(denied.error() == ScheduleError::NO_SESSION);
        const auto missing = source.getSessionByTime(0);
        REQUIRE_FALSE(missing);
        CHECK(missing.error() == ScheduleError::NO_SESSION);
    }
}

TEST_CASE("ordinary query allocation failure retains existing day and generation") {
    ScheduleLimits limits;
    limits.dayCacheEntries = 0;
    auto context = domain(limits);
    auto source = schedule(context);
    auto held = value(source.getDayById(0));
    allocation_test::arm(0);
    auto baseline = source.getDayById(1);
    const auto count = allocation_test::disarm();
    REQUIRE(baseline);
    REQUIRE(count > 0);
    baseline = std::unexpected(ScheduleError::ALLOCATION);

    for (std::size_t ordinal = 1; ordinal <= count; ++ordinal) {
        allocation_test::arm(ordinal);
        auto result = source.getDayById(1);
        allocation_test::disarm();
        CAPTURE(ordinal);
        REQUIRE_FALSE(result);
        CHECK(result.error() == ScheduleError::ALLOCATION);
        CHECK(held.getDayId() == 0);
        CHECK(context.getUsage().days == 1);
        CHECK(context.getUsage().sessions == held.getSessions().size());
    }

    CHECK(source.getDayById(1));
    allocation_test::arm(1);
    bool failed = false;
    try {
        (void)held.getSessions();
    } catch (const std::bad_alloc &) {
        failed = true;
    }
    allocation_test::disarm();
    CHECK(failed);
    CHECK(held.getFirstSession().getDay() == held);
}

TEST_CASE("raw reference metadata is separate from final pre1900 actual offset") {
    const auto binding = value(data().bind(u"Europe/Dublin", {-2208988800000LL, 128}));
    CHECK(binding.getRawOffsetSeconds() == -1521);
    CHECK(value(binding.getOffset(-5364662400000LL)).totalSeconds == 0);
    CHECK(value(binding.getOffset(-5364662400000LL)).referenceRawSeconds == -1521);
    CHECK(value(binding.getOffset(std::numeric_limits<std::int64_t>::min())).totalSeconds == 0);
    const auto context = value(ScheduleDataContext::create(data(), {}, {-2208988800000LL, 128}));
    const auto source = schedule(context, u"(tz=Europe/Dublin;td=1234567;0=0000+0000)");
    const auto day = value(source.getDayByYearMonthDay(18000101));
    CHECK(day.getStartTime() == -5364662400000LL);
}

TEST_CASE("reference clock is captured only for new models and once per atomic rebuild") {
    auto context = value(ScheduleDataContext::create(data()));
    const auto a = schedule(context);
    const auto b = schedule(context, u"(tz=Europe/Dublin;td=1234567;0=0000+0000)");
    const auto before = internal::Access::rules(a)->rawOffsetReferenceTime;
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    const auto cached = schedule(context);
    CHECK(internal::Access::rules(cached)->rawOffsetReferenceTime == before);
    update(context, 1780000001000LL, u"tz=UTC;0=09001700");
    const auto first = internal::Access::rules(a)->rawOffsetReferenceTime;
    CHECK(first == internal::Access::rules(b)->rawOffsetReferenceTime);
    CHECK(first >= before);
    const auto day = value(a.getDayById(0));
    CHECK(internal::Access::rules(a)->rawOffsetReferenceTime == first);
}

TEST_CASE("concurrent weak-cache readers preserve day snapshots across atomic defaults") {
    ScheduleLimits limits;
    limits.retainedGenerations = 16;
    auto context = domain(limits);
    update(context, 1780000000000LL, u"name=before;tz=UTC;td=1234567;0=09001700");
    auto source = schedule(context, u"TST()");
    const auto held = value(source.getDayById(20081));
    const auto saved = held.getStartTime();
    std::atomic<bool> start = false;
    std::atomic<int> failures = 0;
    auto read = [&] {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        for (int index = 0; index < 100; ++index) {
            const auto day = source.getDayById(20081 + index % 2);

            if (!day || day->getEndTime() < day->getStartTime() || held.getStartTime() != saved) {
                ++failures;
            }
        }
    };
    std::thread first(read), second(read);
    start.store(true, std::memory_order_release);

    for (int index = 0; index < 20; ++index) {
        const CalendarDefaultsEntry entry{CalendarEntryKind::VENUE, u"TST",
                                          index % 2 == 0 ? u"tz=UTC;td=1234567;0=10001800"
                                                         : u"tz=UTC;td=1234567;0=09001700"};
        const auto changed = context.setDefaults({1780000001000LL + index, std::span(&entry, 1)});

        if (!changed && changed.error() != ScheduleError::CONCURRENT_CHANGE) {
            ++failures;
        }
    }

    first.join();
    second.join();
    CHECK(failures.load() == 0);
    CHECK(held.getStartTime() == saved);
}

TEST_CASE("shared binding owns its input and translates ordinary construction failures") {
    std::u16string id = u"America/New_York";
    auto binding = value(data().bind(id, {0, 128}));
    id.assign(u"destroyed input");
    CHECK(binding.getId() == u"America/New_York");
    auto copy = binding;
    auto moved = std::move(copy);
    CHECK(copy.getId() == moved.getId());
    CHECK(value(binding.getOffset(std::numeric_limits<std::int64_t>::min())).totalSeconds == -18000);
    CHECK_FALSE(binding.getOffset(std::numeric_limits<std::int64_t>::max()));
    const auto bounds = binding.getQueryBounds();
    CHECK(binding.getOffset(bounds.maximumUtcMillis));
    const auto beyond = binding.getOffset(bounds.maximumUtcMillis + 1);
    REQUIRE_FALSE(beyond);
    CHECK(beyond.error() == zone::TimeZoneError::OUT_OF_RANGE);
    allocation_test::arm(0);
    auto baseline = data().bind(u"America/New_York", {0, 128});
    const auto count = allocation_test::disarm();
    REQUIRE(baseline);
    REQUIRE(count > 0);

    for (std::size_t ordinal = 1; ordinal <= count; ++ordinal) {
        allocation_test::arm(ordinal);
        auto failed = data().bind(u"America/New_York", {0, 128});
        allocation_test::disarm();
        REQUIRE_FALSE(failed);
        CHECK(failed.error() == zone::TimeZoneError::ALLOCATION);
        CHECK(binding.getId() == u"America/New_York");
    }

    allocation_test::arm(1);
    auto refused = data().bind(u"America/New_York", {0, 1});
    const auto attempted = allocation_test::disarm();
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == zone::TimeZoneError::RESOURCE_LIMIT);
    CHECK(attempted == 0);
}
