// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "evidence_cells.hpp"
#include "time_series_query_policy.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace d = q::api::detail;
namespace e = q::events;

struct TestEvent {
    std::int64_t index = 0;
    std::int32_t eventFlags = 0;
    std::u16string payload;

    [[nodiscard]] std::int64_t getIndex() const noexcept {
        return index;
    }

    [[nodiscard]] std::int64_t getTime() const noexcept {
        return e::getTimeFromTimeSequence(index);
    }
};

std::int64_t index(std::int32_t seconds, std::uint32_t millis, std::uint32_t sequence) {
    const auto bits = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(seconds)) << 32) |
                      (static_cast<std::uint64_t>(millis) << 22) | sequence;

    return std::bit_cast<std::int64_t>(bits);
}
} // namespace

TEST_CASE("Inclusive business boundaries retain every sequence and overlapping packed millisecond") {
    for (std::int64_t time = -1025; time <= 1025; ++time) {
        const auto range = d::makeTimeSeriesQueryRange(time, time);
        REQUIRE_FALSE(range.empty);

        for (const auto seconds : {-3, -2, -1, 0, 1, 2, 3}) {
            for (const auto millis : {0U, 1U, 23U, 24U, 999U, 1000U, 1001U, 1023U}) {
                for (const auto sequence : {0U, 1U, static_cast<std::uint32_t>(e::MAX_EVENT_SEQUENCE)}) {
                    const auto raw = index(seconds, millis, sequence);
                    const auto business = static_cast<std::int64_t>(seconds) * 1000 + millis;
                    CHECK(range.contains(raw) == (business == time));

                    if (business == time) {
                        CHECK(raw >= range.fromIndex);
                        CHECK(raw <= range.toIndex);
                    }
                }
            }
        }
    }

    const auto boundary = d::makeTimeSeriesQueryRange(1000, 1000);
    CHECK(boundary.fromIndex == index(0, 1000, 0));
    CHECK(boundary.subscriptionFromTime == 0);
    CHECK(e::withTimeSequenceTime(0, boundary.subscriptionFromTime) <= boundary.fromIndex);
    CHECK(boundary.toIndex == index(1, 0, e::MAX_EVENT_SEQUENCE));
    CHECK(boundary.contains(index(1, 0, 123)));
    CHECK(boundary.contains(index(0, 1000, e::MAX_EVENT_SEQUENCE)));
    CHECK_FALSE(boundary.contains(index(0, 1001, 0)));

    for (std::int64_t overlap = 0; overlap <= 23; ++overlap) {
        const auto range = d::makeTimeSeriesQueryRange(1000 + overlap, 1000 + overlap);
        CHECK(range.subscriptionFromTime == 0);
        CHECK(e::withTimeSequenceTime(0, range.subscriptionFromTime) <= range.fromIndex);
        CHECK(range.contains(index(0, static_cast<std::uint32_t>(1000 + overlap), e::MAX_EVENT_SEQUENCE)));
        CHECK(range.contains(index(1, static_cast<std::uint32_t>(overlap), e::MAX_EVENT_SEQUENCE)));
    }

    const auto beyondOverlap = d::makeTimeSeriesQueryRange(1024, 1024);
    CHECK(beyondOverlap.subscriptionFromTime == 1000);
    CHECK(e::withTimeSequenceTime(0, beyondOverlap.subscriptionFromTime) <= beyondOverlap.fromIndex);
}

TEST_CASE("Saturated and reversed boundaries do not overflow or invent candidates") {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto all = d::makeTimeSeriesQueryRange(minimum, maximum);
    REQUIRE_FALSE(all.empty);
    CHECK(all.fromIndex == minimum);
    CHECK(all.toIndex == maximum);
    CHECK(all.contains(minimum));
    CHECK(all.contains(maximum));
    constexpr std::int64_t FIRST = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) * 1000;
    constexpr std::int64_t LAST = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) * 1000 + 1023;
    CHECK(d::makeTimeSeriesQueryRange(minimum, FIRST - 1).empty);
    CHECK(d::makeTimeSeriesQueryRange(LAST + 1, maximum).empty);
    CHECK(d::makeTimeSeriesQueryRange(9, 8).empty);
    const auto last = d::makeTimeSeriesQueryRange(LAST, LAST);
    REQUIRE_FALSE(last.empty);
    CHECK(last.fromIndex == index(std::numeric_limits<std::int32_t>::max(), 1023, 0));
    CHECK(last.toIndex == maximum);
}

TEST_CASE("Business filtering precedes complete output limit and sorts with deterministic index ties") {
    std::vector<TestEvent> candidates(5);
    candidates[0].index = index(1, 0, 3);
    candidates[0].payload = u"owned";
    candidates[1].index = index(0, 1001, 0); // Raw envelope false positive at time 1001.
    candidates[2].index = index(0, 1000, 7);
    candidates[3].index = index(1, 0, 1);
    candidates[4].index = index(0, 999, 0);

    for (auto &event : candidates)
        event.eventFlags = 7;

    d::TimeSeriesQueryWork work{5, 64};
    const auto range = d::makeTimeSeriesQueryRange(1000, 1000);
    auto result = d::selectTimeSeriesSnapshot(std::move(candidates), range, 3, work);
    REQUIRE(result);
    REQUIRE(result->size() == 3);
    CHECK((*result)[0].index == index(0, 1000, 7));
    CHECK((*result)[1].index == index(1, 0, 1));
    CHECK((*result)[2].index == index(1, 0, 3));
    CHECK((*result)[2].payload == u"owned");

    for (const auto &event : *result)
        CHECK(event.eventFlags == 0);

    CHECK(work.visits == 0);
}

TEST_CASE("Output and independent work limits refuse without a partial result") {
    const auto range = d::makeTimeSeriesQueryRange(0, 1000);
    std::vector<TestEvent> rows(2);
    rows[0].index = index(0, 1, 1);
    rows[1].index = index(0, 1, 2);
    auto tooMany = rows;
    d::TimeSeriesQueryWork ample{2, 10};
    CHECK(d::selectTimeSeriesSnapshot(std::move(tooMany), range, 1, ample) ==
          std::unexpected(d::TimeSeriesQueryError::RESULT_LIMIT));
    auto scan = rows;
    d::TimeSeriesQueryWork noScan{1, 10};
    CHECK(d::selectTimeSeriesSnapshot(std::move(scan), range, 2, noScan) ==
          std::unexpected(d::TimeSeriesQueryError::WORK_LIMIT));
    CHECK(noScan.visits == 0);
    d::TimeSeriesQueryWork noSort{2, 0};
    CHECK(d::selectTimeSeriesSnapshot(std::move(rows), range, 2, noSort) ==
          std::unexpected(d::TimeSeriesQueryError::WORK_LIMIT));
}

TEST_CASE("Variant projection preserves ordering and owning values") {
    using Value = std::variant<TestEvent>;
    std::vector<Value> rows;
    rows.emplace_back(TestEvent{index(1, 0, 3), 7, u"later"});
    rows.emplace_back(TestEvent{index(0, 1000, 1), 7, u"earlier"});
    d::TimeSeriesQueryWork work{2, 8};
    const auto project = [](const Value &value) -> const TestEvent & {
        return std::get<TestEvent>(value);
    };
    auto result = d::selectTimeSeriesSnapshot(
        std::move(rows), d::makeTimeSeriesQueryRange(1000, 1000), 2, work,
        [&](const Value &value) {
            return project(value).getIndex();
        },
        [&](const Value &value) {
            return project(value).getTime();
        },
        [](Value &value) {
            std::get<TestEvent>(value).eventFlags = 0;
        });
    REQUIRE(result);
    REQUIRE(result->size() == 2);
    CHECK(project((*result)[0]).payload == u"earlier");
    CHECK(project((*result)[1]).payload == u"later");
    CHECK(project((*result)[0]).eventFlags == 0);
}

#ifndef ORG_TTLDTOR_QDCXX_TIME_SERIES_QUERY_FIXTURE
#    define ORG_TTLDTOR_QDCXX_TIME_SERIES_QUERY_FIXTURE "tests/fixtures/time-series-query/oracle.tsv"
#endif

TEST_CASE("Time-series raw and business values match pinned Java QD") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_TIME_SERIES_QUERY_FIXTURE);
    REQUIRE(input);
    std::size_t events = 0, inclusive = 0, order = 0;
    std::int64_t previous = std::numeric_limits<std::int64_t>::min();

    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const auto cells = q::test_support::split(line);

        if (cells[0] == "event") {
            REQUIRE(cells.size() == 5);
            const auto raw = std::stoll(cells[1]);
            const auto time = std::stoll(cells[2]);
            CHECK(e::getTimeFromTimeSequence(raw) == time);
            CHECK(e::getSequenceFromTimeSequence(raw) == std::stoi(cells[3]));
            CHECK(time == std::stoll(cells[4]));
            const auto range = d::makeTimeSeriesQueryRange(time, time);
            REQUIRE_FALSE(range.empty);
            CHECK(range.contains(raw));
            CHECK(raw >= range.fromIndex);
            CHECK(raw <= range.toIndex);
            ++events;
        } else if (cells[0] == "inclusive") {
            REQUIRE(cells.size() == 6);
            const auto time = std::stoll(cells[1]);
            const auto javaUpper = std::stoll(cells[2]);
            const auto raw = std::stoll(cells[3]);
            CHECK(std::stoll(cells[4]) == time);
            CHECK(cells[5] == "false"); // Java's encoded upper excludes this same-time nonzero sequence.
            CHECK(raw > javaUpper);
            const auto range = d::makeTimeSeriesQueryRange(time, time);
            CHECK(range.contains(raw));
            CHECK(raw <= range.toIndex);
            ++inclusive;
        } else if (cells[0] == "order") {
            REQUIRE(cells.size() == 3);
            const auto raw = std::stoll(cells[1]);
            CHECK(raw >= previous);
            CHECK(e::getTimeFromTimeSequence(raw) == std::stoll(cells[2]));
            previous = raw;
            ++order;
        }
    }

    CHECK(events == 168);
    CHECK(inclusive == 1);
    CHECK(order == 3);
}
