// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../tools/tape-inspect/sidecar_time.hpp"
#include <doctest/doctest.h>
#include <limits>
#include <string>

namespace {
namespace ti = org::ttldtor::qdcxx::tape_inspect;

/** Submit a string without retaining its bytes.
 * @param scanner Instance under test.
 * @param text Borrowed bytes.
 * @return Synchronous scanner result.
 */
ti::SidecarFeed submit(ti::SidecarTime &scanner, std::string_view text) {
    return scanner.feed(std::as_bytes(std::span(text.data(), text.size())));
}

/** Parse one mark and verify full logical closure.
 * @param text Exact one-line input.
 * @return Independent owning mark.
 */
ti::SidecarMark one(std::string_view text) {
    ti::SidecarTime scanner(ti::SidecarFormat::LONG);
    auto result = submit(scanner, text);

    if (result.status == ti::SidecarStatus::NEED_INPUT) {
        result = scanner.endInput();
    }

    REQUIRE(result.mark);
    const auto mark = *result.mark;
    REQUIRE(scanner.endInput().status == ti::SidecarStatus::END);
    REQUIRE(scanner.finish(std::numeric_limits<std::uint64_t>::max()) == ti::SidecarError::NONE);

    return mark;
}
} // namespace

TEST_CASE("sidecar exact producer grammar signed positions and time projection") {
    CHECK(one("1700000000123:0\n").timestampMs == 1700000000123LL);
    CHECK(one("20231115-011320.123+0300:0").timestampMs == 1700000000123LL);
    CHECK(one("19691231-235959.999+0000:0\r").timestampMs == -1);
    CHECK(one("19691231-235959.999+0000:0\n").packed() == -104857600);
    CHECK(one("9223372036854775807:0\n").timestampMs == std::numeric_limits<std::int64_t>::max());
    CHECK(one("9223372036854775807:0\n").packed() == 9223372035944611840LL);
    CHECK(one("0:-9223372036854775808\n").position == std::numeric_limits<std::int64_t>::min());
    CHECK(one("0:9223372036854775807\n").position == std::numeric_limits<std::int64_t>::max());
    CHECK(one("0:+0\n").position == 0);
    CHECK(one("0:-1\n").reached(0));
    CHECK_FALSE(one("0:1\n").reached(0));
    CHECK(one("0:1\n").reached(1));
    CHECK(one("00010101-000000.000+0000:0\n").timestampMs == -62135596800000LL);
    CHECK(one("20000229-000000.000+0000:0\n").timestampMs == 951782400000LL);
}

TEST_CASE("sidecar chunking CRLF ignored lines final marks and owning publication") {
    const std::string text = "ignored\r\n0:-1\r\n1700000000123:0\n1700000001124:0\r1700000000123:-9";

    for (std::size_t chunk = 1; chunk <= text.size(); ++chunk) {
        ti::SidecarTime scanner(ti::SidecarFormat::TEXT);
        std::vector<ti::SidecarMark> marks;
        std::size_t position = 0;

        while (position < text.size()) {
            auto result = submit(scanner, std::string_view(text).substr(position, chunk));
            REQUIRE(result.status != ti::SidecarStatus::FAILED);
            REQUIRE(result.accepted > 0);
            position += result.accepted;

            if (result.mark) {
                marks.push_back(*result.mark);
            }
        }

        auto last = scanner.endInput();
        REQUIRE(last.mark);
        marks.push_back(*last.mark);
        REQUIRE(scanner.endInput().status == ti::SidecarStatus::END);
        CHECK(scanner.endInput().status == ti::SidecarStatus::END);
        CHECK(scanner.finish(0) == ti::SidecarError::NONE);
        CHECK(scanner.finish(999) == ti::SidecarError::NONE);
        REQUIRE(marks.size() == 4);
        CHECK(marks[0].position == -1);
        CHECK(marks[1].timestampMs == 1700000000123LL);
        CHECK(marks[2].timestampMs == 1700000001124LL);
        CHECK(marks[3].timestampMs == 1700000000123LL);

        for (std::size_t index = 0; index < marks.size(); ++index) {
            CHECK(marks[index].ordinal == index);
        }

        CHECK(scanner.counters().inputBytes == text.size());
        CHECK(scanner.counters().lines == 5);
    }
}

TEST_CASE("sidecar malformed unsupported overflow and terminal guarantees") {
    for (const auto text :
         {"bad:0\n", "0:bad\n", "0:9223372036854775808\n", "0:-9223372036854775809\n", "9223372036854775808:0\n",
          "20230229-000000.000+0000:0\n", "20231114-221320.123:0\n", "+1700000000123:0\n", "00:0\n", "0:\n"}) {
        ti::SidecarTime scanner(ti::SidecarFormat::LONG);
        REQUIRE(submit(scanner, text).status == ti::SidecarStatus::FAILED);
        const auto error = scanner.diagnostic().error;
        const auto bytes = scanner.counters().inputBytes;
        CHECK(submit(scanner, "0:0\n").accepted == 0);
        CHECK(scanner.endInput().status == ti::SidecarStatus::FAILED);
        CHECK(scanner.finish(0) == error);
        CHECK(scanner.counters().inputBytes == bytes);
    }

    for (const auto text : {"-1:0\n", "-9223372036854775808:0\n", "-1d:0\n"}) {
        ti::SidecarTime scanner(ti::SidecarFormat::TEXT);
        CHECK(submit(scanner, text).status == ti::SidecarStatus::FAILED);
        CHECK(scanner.diagnostic().error == ti::SidecarError::UNSUPPORTED_TIME);
    }

    ti::SidecarTime invalid(static_cast<ti::SidecarFormat>(99));
    CHECK(invalid.diagnostic().error == ti::SidecarError::INVALID_ARGUMENT);
    CHECK(submit(invalid, "0:0\n").accepted == 0);
    CHECK(invalid.counters().workBytes == 0);

    ti::SidecarTime early(ti::SidecarFormat::LONG);
    CHECK(early.finish(0) == ti::SidecarError::NOT_FINISHED);
    CHECK(early.endInput().status == ti::SidecarStatus::FAILED);

    ti::SidecarTime future(ti::SidecarFormat::LONG);
    REQUIRE(submit(future, "0:2\n").mark);
    REQUIRE(future.endInput().status == ti::SidecarStatus::END);
    CHECK(future.finish(1) == ti::SidecarError::POSITION_AFTER_DATA);
    CHECK(future.finish(2) == ti::SidecarError::POSITION_AFTER_DATA);

    ti::SidecarTime final(ti::SidecarFormat::LONG);
    REQUIRE(submit(final, "0:0").status == ti::SidecarStatus::NEED_INPUT);
    REQUIRE(final.endInput().mark);
    CHECK(submit(final, "x").status == ti::SidecarStatus::FAILED);
}

TEST_CASE("sidecar finite zero exact excess and representability bounds") {
    for (unsigned selected = 0; selected < 6; ++selected) {
        ti::SidecarLimits limits;

        switch (selected) {
        case 0:
            limits.inputBytes = 0;
            break;
        case 1:
            limits.lineBytes = 0;
            break;
        case 2:
            limits.lines = 0;
            break;
        case 3:
            limits.marks = 0;
            break;
        case 4:
            limits.workBytes = 0;
            break;
        case 5:
            limits.retainedBytes = 0;
            break;
        }

        ti::SidecarTime empty(ti::SidecarFormat::LONG, limits);
        CHECK(empty.endInput().status == ti::SidecarStatus::END);
        CHECK(empty.finish(0) == ti::SidecarError::NONE);
        ti::SidecarTime nonempty(ti::SidecarFormat::LONG, limits);
        CHECK(submit(nonempty, "0:0\n").status == ti::SidecarStatus::FAILED);
    }

    for (std::size_t allowance : {std::size_t{2}, std::size_t{3}, std::size_t{4}}) {
        ti::SidecarLimits limits;
        limits.lineBytes = allowance;
        limits.retainedBytes = allowance;
        ti::SidecarTime scanner(ti::SidecarFormat::LONG, limits);
        auto result = submit(scanner, "0:0\n");
        CHECK((result.status == ti::SidecarStatus::MARK) == (allowance >= 3));
    }

    ti::SidecarLimits exact;
    exact.inputBytes = 4;
    exact.workBytes = 7;
    exact.lines = 1;
    exact.marks = 1;
    ti::SidecarTime scanner(ti::SidecarFormat::LONG, exact);
    REQUIRE(submit(scanner, "0:0\n").mark);
    CHECK(scanner.counters().workBytes == 7);
    REQUIRE(scanner.endInput().status == ti::SidecarStatus::END);
    CHECK(scanner.finish(0) == ti::SidecarError::NONE);

    ti::SidecarLimits huge;
    huge.lineBytes = std::numeric_limits<std::size_t>::max();
    huge.retainedBytes = huge.lineBytes;
    ti::SidecarTime unrepresentable(ti::SidecarFormat::LONG, huge);
    CHECK(unrepresentable.diagnostic().error == ti::SidecarError::INVALID_ARGUMENT);
    CHECK(unrepresentable.counters().inputBytes == 0);
}

TEST_CASE("sidecar independent allowance boundaries remain literal and cumulative") {
    for (unsigned selected = 0; selected < 5; ++selected) {
        for (int delta : {-1, 0, 1}) {
            ti::SidecarLimits limits;

            switch (selected) {
            case 0:
                limits.inputBytes = static_cast<std::uint64_t>(4 + delta);
                break;
            case 1:
                limits.workBytes = static_cast<std::uint64_t>(7 + delta);
                break;
            case 2:
                limits.lines = static_cast<std::uint64_t>(1 + delta);
                break;
            case 3:
                limits.marks = static_cast<std::uint64_t>(1 + delta);
                break;
            case 4:
                limits.lineBytes = 3;
                limits.retainedBytes = static_cast<std::size_t>(3 + delta);
                break;
            }

            ti::SidecarTime scanner(ti::SidecarFormat::TEXT, limits);
            const auto result = submit(scanner, "0:0\n");
            CHECK((result.status == ti::SidecarStatus::MARK) == (delta >= 0));
        }
    }

    ti::SidecarLimits finiteMax;
    finiteMax.inputBytes = std::numeric_limits<std::uint64_t>::max();
    finiteMax.workBytes = std::numeric_limits<std::uint64_t>::max();
    finiteMax.lines = std::numeric_limits<std::uint64_t>::max();
    finiteMax.marks = std::numeric_limits<std::uint64_t>::max();
    finiteMax.retainedBytes = std::numeric_limits<std::size_t>::max();
    ti::SidecarTime bounded(ti::SidecarFormat::LONG, finiteMax);
    REQUIRE(submit(bounded, "0:0\n").mark);
    CHECK(bounded.counters().inputBytes == 4);
    CHECK(bounded.counters().workBytes == 7);

    ti::SidecarLimits twoLines;
    twoLines.marks = 1;
    ti::SidecarTime cumulative(ti::SidecarFormat::LONG, twoLines);
    const std::string text = "0:0\n0:0\n";
    const auto first = submit(cumulative, text);
    REQUIRE(first.mark);
    const auto second = submit(cumulative, std::string_view(text).substr(first.accepted));
    CHECK(second.status == ti::SidecarStatus::FAILED);
    CHECK(cumulative.diagnostic().error == ti::SidecarError::MARK_LIMIT);
    CHECK(cumulative.counters().marks == 1);
    CHECK(cumulative.counters().lines == 2);
}
