// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/event_registry.hpp"
#include "qd/api/time_series_subscription.hpp"

#include <array>
#include <atomic>
#include <cstdlib>
#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace {
thread_local int allocationCredit = -1; ///< Only the calling test thread elects bounded allocation failure.
}

/** Test-only fault point; unrelated worker allocations stay ordinary.
 * @param bytes Requested ordinary storage.
 * @return Malloc-backed storage or bad_alloc at the elected failure boundary.
 */
void *operator new(std::size_t bytes) {
    if (allocationCredit == 0)
        throw std::bad_alloc();

    if (allocationCredit > 0)
        --allocationCredit;

    if (void *result = std::malloc(bytes ? bytes : 1))
        return result;

    throw std::bad_alloc();
}

/** @param value Ordinary storage, possibly null. */
void operator delete(void *value) noexcept {
    std::free(value);
}

/** @param value Ordinary storage.
 * @param bytes Unused matching size.
 */
void operator delete(void *value, std::size_t bytes) noexcept {
    static_cast<void>(bytes);
    std::free(value);
}

/** @param bytes Array storage request.
 * @return Storage using the same test-owned allocation boundary.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** @param value Array storage, possibly null. */
void operator delete[](void *value) noexcept {
    std::free(value);
}

/** @param value Array storage.
 * @param bytes Unused matching size.
 */
void operator delete[](void *value, std::size_t bytes) noexcept {
    static_cast<void>(bytes);
    std::free(value);
}

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
using Selector = a::SubscriptionSelector;
using Symbol = a::TimeSeriesSubscriptionSymbol<>;
using Capability = a::EventSubscriptionCapability;
using Error = a::TimeSeriesModelError;

/** Conversion failure must propagate even when producing the proxy itself is noexcept. */
struct ConversionFailure {};

/** Distinct explicit/implicit conversion paths detect mismatched noexcept and return expressions. */
struct EqualityProxy {
    bool value;
    bool fail;

    explicit operator bool() const {
        if (fail)
            throw ConversionFailure{};

        return value;
    }

    operator int() const noexcept {
        return value;
    }
};

/** Hash conversion has the same complete-expression exception requirement. */
struct HashProxy {
    std::int32_t value;
    bool fail;

    explicit operator std::int32_t() const {
        if (fail)
            throw ConversionFailure{};

        return value;
    }

    operator std::int64_t() const noexcept {
        return value;
    }
};

/** Valid owning custom domain; capability and wire mapping are unrelated to its value operations. */
struct ProxySymbol {
    std::int32_t value;
    bool failEquality = false;
    bool failHash = false;

    EqualityProxy operator==(const ProxySymbol &other) const noexcept {
        return {value == other.value, failEquality || other.failEquality};
    }

    HashProxy hashCode() const noexcept {
        return {value, failHash};
    }
};

using ProxyDecorator = a::TimeSeriesSubscriptionSymbol<ProxySymbol>;
static_assert(a::TimeSeriesSymbolValue<ProxySymbol>);
static_assert(!noexcept(std::declval<const ProxyDecorator &>() == std::declval<const ProxyDecorator &>()));
static_assert(!noexcept(std::declval<const ProxyDecorator &>().hashCode()));
static_assert(!noexcept(std::hash<ProxyDecorator>{}(std::declval<const ProxyDecorator &>())));
static_assert(noexcept(std::declval<const Symbol &>() == std::declval<const Symbol &>()));
static_assert(noexcept(std::declval<const Symbol &>().hashCode()));

/** Read every literal production row, preserving the Java discrepancy rather than replacing it.
 * @return Owning TSV cells, including empty trailing/adjacent columns.
 */
std::vector<std::vector<std::string>> oracle() {
    std::ifstream input(ORG_TTLDTOR_QDCXX_TIME_SERIES_FIXTURE);
    REQUIRE(input.good());
    std::vector<std::vector<std::string>> result;

    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::vector<std::string> cells;
        std::size_t first = 0;

        for (;;) {
            const auto end = line.find('\t', first);
            cells.push_back(line.substr(first, end == std::string::npos ? end : end - first));

            if (end == std::string::npos)
                break;

            first = end + 1;
        }

        result.push_back(std::move(cells));
    }

    return result;
}

/** Decode only the harness's exact UTF-16 hexadecimal notation.
 * @param hex Four digits per code unit, without Unicode normalization.
 * @return Owning exact units, including lone surrogates and embedded NUL.
 */
std::u16string units(std::string_view hex) {
    REQUIRE(hex.size() % 4 == 0);
    std::u16string result;

    for (std::size_t index = 0; index < hex.size(); index += 4)
        result.push_back(static_cast<char16_t>(std::stoul(std::string(hex.substr(index, 4)), nullptr, 16)));

    return result;
}

/** Prepare an explicit caller-declared business identity, not a schema-derived capability.
 * @param id Stable exact identity.
 * @param capability Explicit business marker.
 * @return Independent descriptor or a test failure.
 */
a::TimeSeriesEventDescriptor descriptor(std::u16string id = u"example:TimeAndSale",
                                        Capability capability = Capability::TIME_SERIES) {
    auto result = a::TimeSeriesEventDescriptor::declared(std::move(id), capability);
    REQUIRE(result);

    return std::move(*result);
}

/** Create a pure model; no SDK bootstrap, endpoint or protocol owner exists.
 * @param limits Independent finite model bounds.
 * @return Prepared logical state.
 */
a::TimeSeriesSubscriptionModel model(a::TimeSeriesModelLimits limits = {}) {
    const auto type = descriptor();
    auto result = a::TimeSeriesSubscriptionModel::create(std::span(&type, 1), limits);
    REQUIRE(result);

    return std::move(*result);
}
} // namespace

TEST_CASE("time-series owning identity matches actual Java signed bounds and exact UTF16 hashes") {
    std::size_t compared = 0;

    for (const auto &row : oracle()) {
        if (row[0] != "symbol")
            continue;

        const auto comma = row[1].rfind(',');
        const auto text = units(std::string_view(row[1]).substr(4, comma - 4));
        const auto fromTime = std::stoll(row[1].substr(comma + 1));
        const Symbol value(Selector::plain(text), fromTime);
        CHECK(value.eventSymbol().eventSymbol() == text);
        CHECK(value.sourceId() == std::stoi(row[2]));
        CHECK(value.hashCode() == std::stol(row[3]));
        CHECK(value.fromTime() == fromTime);
        const Symbol other(Selector::plain(text), fromTime == 0 ? 1 : 0);
        CHECK(value == other);
        CHECK_FALSE(value.sameDemand(other));
        CHECK(std::hash<Symbol>{}(value) == std::hash<Symbol>{}(other));
        ++compared;
    }

    CHECK(compared == 30);
}

TEST_CASE("time-series generic equality and hash propagate proxy conversion failures") {
    const ProxyDecorator first(ProxySymbol{7}, 0);
    const ProxyDecorator same(ProxySymbol{7}, 1);
    CHECK(first == same);
    CHECK(first.hashCode() == 7);
    CHECK(std::hash<ProxyDecorator>{}(first) == 7);
    const ProxyDecorator badEquality(ProxySymbol{7, true, false}, 0);
    const ProxyDecorator badHash(ProxySymbol{7, false, true}, 0);
    CHECK_THROWS_AS(static_cast<void>(badEquality == same), ConversionFailure);
    CHECK_THROWS_AS(static_cast<void>(badHash.hashCode()), ConversionFailure);
    CHECK_THROWS_AS(std::hash<ProxyDecorator>{}(badHash), ConversionFailure);
}

TEST_CASE("time-series nested decorators preserve ADR12 typed identity and support arbitrary native value depth") {
    const auto rawIndexed = Selector::indexed(u"IBM", q::events::IndexedEventSource::DEFAULT);
    const auto rawTimed = Selector::timeSeries(u"IBM", 7);
    const Symbol indexed(rawIndexed, 0);
    const Symbol timed(rawTimed, 0);
    CHECK_FALSE(indexed == timed);
    CHECK_FALSE(timed == indexed);
    CHECK(indexed.hashCode() == timed.hashCode());
    CHECK((std::unordered_set<Symbol>{indexed, timed}.size() == 2));
    CHECK((std::unordered_set<Symbol>{timed, indexed}.size() == 2));
    const Symbol nestedFirst(Selector::timeSeries(u"IBM", 7), -1);
    const Symbol nestedSecond(Selector::timeSeries(u"IBM", 8), a::TIME_SERIES_FROM_TIME_DISABLED);
    CHECK(nestedFirst == nestedSecond);
    CHECK_FALSE(nestedFirst.sameDemand(nestedSecond));
    const a::TimeSeriesSubscriptionSymbol<Symbol> deeperFirst(nestedFirst, 5);
    const a::TimeSeriesSubscriptionSymbol<Symbol> deeperSecond(nestedSecond, 6);
    CHECK(deeperFirst == deeperSecond);
    CHECK(deeperFirst.hashCode() == nestedFirst.hashCode());
    CHECK_FALSE(deeperFirst.sameDemand(deeperSecond));
    bool rawDiscrepancy = false;

    for (const auto &row : oracle()) {
        if (row[0] == "pair" && row[1] == "nested-ADR12-cross-kind") {
            CHECK(row[4] == "true");
            CHECK(row[5] == "false");
            CHECK(row[8] == "2");
            CHECK(row[9] == "1");
            rawDiscrepancy = true;
        }
    }

    CHECK(rawDiscrepancy);
    CHECK_FALSE(Symbol(Selector::all(), 0) == Symbol(Selector::plain(u"*"), 0));
}

TEST_CASE("time-series capability is explicit extensible business metadata distinct from native receiver support") {
    CHECK_FALSE(a::TimeSeriesEventDescriptor::declared(u"", Capability::TIME_SERIES));
    CHECK_FALSE(a::TimeSeriesEventDescriptor::declared(u"x", static_cast<Capability>(99)));
    std::size_t inherited = 0;

    for (const auto &row : oracle()) {
        if (row[0] != "capability")
            continue;

        const auto marker = row[3] == "true"   ? Capability::TIME_SERIES
                            : row[2] == "true" ? Capability::INDEXED
                                               : Capability::NON_INDEXED;
        auto value = descriptor(std::u16string(row[1].begin(), row[1].end()), marker);
        const auto prepared = a::TimeSeriesSubscriptionModel::create(std::span(&value, 1));
        CHECK(static_cast<bool>(prepared) == (row[3] == "true"));

        if (!prepared)
            CHECK(prepared.error() == Error::UNSUPPORTED_EVENT_TYPE);

        CHECK_FALSE(a::hasTimeSeriesFeedReceiver(value));

        if (row[1] == "custom.NuamTimeAndSale" || row[1] == "custom-inherited") {
            CHECK(prepared);
            ++inherited;
        }
    }

    CHECK(inherited == 2);

    for (const auto type : {a::EventType::QUOTE, a::EventType::PROFILE, a::EventType::TRADE, a::EventType::TRADE_ETH,
                            a::EventType::SUMMARY, a::EventType::TEXT_MESSAGE, a::EventType::TEXT_CONFIGURATION,
                            a::EventType::MESSAGE, a::EventType::CONFIGURATION, static_cast<a::EventType>(99)})
        CHECK_FALSE(a::isTimeSeriesEventType(type));

    const auto custom = descriptor(u"not-a-Java-class-name", Capability::TIME_SERIES);
    REQUIRE(a::TimeSeriesSubscriptionModel::create(std::span(&custom, 1)));
    const std::array conflict{custom, descriptor(u"not-a-Java-class-name", Capability::INDEXED)};
    CHECK(a::TimeSeriesSubscriptionModel::create(conflict).error() == Error::CONFLICTING_EVENT_TYPE);
    const std::array duplicate{custom, custom};
    auto normalized = a::TimeSeriesSubscriptionModel::create(duplicate);
    REQUIRE(normalized);
    CHECK(normalized->snapshot()->eventTypes.size() == 1);
    CHECK(a::TimeSeriesSubscriptionModel::create({}).error() == Error::UNSUPPORTED_EVENT_TYPE);
}

TEST_CASE("time-series model range and decorated snapshots match actual Java transitions without a facade lifecycle") {
    auto state = model();
    const std::array raw{Selector::plain(u"IBM"), Selector::plain(u"AAPL")};
    CHECK(*state.getFromTime() == a::TIME_SERIES_FROM_TIME_DISABLED);
    REQUIRE(state.addSymbols(raw));
    const auto before = state.snapshot();
    REQUIRE(before);
    CHECK(before->decoratedSymbols[0].fromTime() == a::TIME_SERIES_FROM_TIME_DISABLED);
    REQUIRE(state.setFromTime(0));
    const auto zero = state.snapshot();
    REQUIRE(zero);
    const auto unchanged = state.setFromTime(0);
    REQUIRE(unchanged);
    CHECK_FALSE(unchanged->fromTimeChanged);
    CHECK(unchanged->revision == zero->revision);
    const std::array reverse{raw[1], raw[0]};
    CHECK_FALSE(state.setSymbols(reverse)->symbolsChanged);
    REQUIRE(state.setFromTime(std::numeric_limits<std::int64_t>::min()));
    CHECK(*state.getFromTime() == std::numeric_limits<std::int64_t>::min());
    REQUIRE(state.setFromTime(a::TIME_SERIES_FROM_TIME_DISABLED));
    CHECK(state.snapshot()->symbols == zero->symbols);
    CHECK(before->decoratedSymbols[0] == zero->decoratedSymbols[0]);
    CHECK_FALSE(before->decoratedSymbols[0].sameDemand(zero->decoratedSymbols[0]));
    CHECK(before->fromTime == a::TIME_SERIES_FROM_TIME_DISABLED);
    CHECK(zero->fromTime == 0);
    bool initial = false, noOp = false;

    for (const auto &row : oracle()) {
        if (row[0] == "state" && row[1] == "initial") {
            CHECK(std::stoll(row[2]) == before->fromTime);
            initial = true;
        }

        if (row[0] == "state" && row[1] == "range-zero-noop") {
            CHECK(std::stoll(row[2]) == zero->fromTime);
            noOp = true;
        }
    }

    CHECK(initial);
    CHECK(noOp);
    const std::array wrappers{Selector::indexed(u"IBM", q::events::IndexedEventSource::DEFAULT),
                              Selector::timeSeries(u"IBM", 7), Selector::all(), Selector::plain(u"*")};
    REQUIRE(state.setSymbols(wrappers));
    CHECK(state.snapshot()->symbols.size() == 4);
    REQUIRE(state.removeSymbol(Selector::timeSeries(u"IBM", 123)));
    CHECK(state.snapshot()->symbols.size() == 3);
    REQUIRE(state.clearSymbols());
    REQUIRE(state.setFromTime(-1));
    CHECK(state.snapshot()->symbols.empty());
    CHECK(*state.getFromTime() == -1);
}

TEST_CASE("time-series model rejects bounded batches atomically and snapshots outlive moved and destroyed owners") {
    a::TimeSeriesModelLimits limits;
    limits.symbols = 2;
    limits.inputSymbols = 3;
    limits.symbolUnits = 4;
    limits.totalSymbolUnits = 5;
    auto state = model(limits);
    const std::array seed{Selector::plain(u"AB"), Selector::timeSeries(u"CD", 0)};
    REQUIRE(state.setSymbols(seed));
    const auto old = state.snapshot();
    REQUIRE(old);
    const std::array tooLong{Selector::plain(u"ABCDE")};
    CHECK(state.setSymbols(tooLong).error() == Error::RESOURCE_LIMIT);
    const std::array tooMany{seed[0], seed[1], Selector::plain(u"X")};
    CHECK(state.addSymbols(tooMany).error() == Error::RESOURCE_LIMIT);
    const std::array tooLarge{Selector::plain(u"ABC"), Selector::plain(u"DEF")};
    CHECK(state.setSymbols(tooLarge).error() == Error::RESOURCE_LIMIT);
    CHECK(state.snapshot()->revision == old->revision);
    CHECK(state.snapshot()->symbols == old->symbols);
    const std::array replacement{Selector::timeSeries(u"CD", 7)};
    REQUIRE(state.addSymbols(replacement));
    CHECK(state.snapshot()->symbols[1].fromTime() == 7);
    CHECK(old->symbols[1].fromTime() == 0);
    auto moved = std::move(state);
    CHECK_FALSE(state.valid());
    CHECK(state.getFromTime().error() == Error::INVALID_STATE);
    CHECK(state.snapshot().error() == Error::INVALID_STATE);
    CHECK(state.setFromTime(0).error() == Error::INVALID_STATE);
    CHECK(moved.valid());
    moved = std::move(moved);
    CHECK(moved.valid());
    moved = model();
    CHECK(old->symbols.size() == 2);
    CHECK(old->decoratedSymbols[1].eventSymbol().fromTime() == 0);
    limits.symbols = 0;
    const auto type = descriptor();
    CHECK(a::TimeSeriesSubscriptionModel::create(std::span(&type, 1), limits).error() == Error::INVALID_LIMITS);
}

TEST_CASE("time-series allocation failure preserves the complete accepted range set and revision") {
    auto state = model();
    const std::array seed{Selector::plain(std::u16string(40, u'A'))};
    REQUIRE(state.setSymbols(seed));
    REQUIRE(state.setFromTime(-1));
    const auto old = state.snapshot();
    REQUIRE(old);
    const std::array desired{Selector::plain(std::u16string(50, u'B')), Selector::plain(std::u16string(60, u'C'))};
    bool sawFailure = false, sawSuccess = false;

    for (int credit = 0; credit < 12; ++credit) {
        allocationCredit = credit;
        const auto changed = state.setSymbols(desired);
        allocationCredit = -1;

        if (changed) {
            sawSuccess = true;
            break;
        }

        CHECK(changed.error() == Error::ALLOCATION_FAILURE);
        const auto after = state.snapshot();
        REQUIRE(after);
        CHECK(after->revision == old->revision);
        CHECK(after->symbols == old->symbols);
        CHECK(after->fromTime == old->fromTime);
        sawFailure = true;
    }

    CHECK(sawFailure);
    CHECK(sawSuccess);
    allocationCredit = 0;
    const auto failedSnapshot = state.snapshot();
    allocationCredit = -1;
    CHECK(failedSnapshot.error() == Error::ALLOCATION_FAILURE);
    CHECK(*state.getFromTime() == -1);
    const auto type = descriptor();
    allocationCredit = 0;
    const auto failedCreation = a::TimeSeriesSubscriptionModel::create(std::span(&type, 1));
    allocationCredit = -1;
    CHECK(failedCreation.error() == Error::ALLOCATION_FAILURE);
}

TEST_CASE("time-series model snapshots reconcile concurrent range and symbol mutations") {
    auto state = model();
    const std::array first{Selector::plain(u"IBM")};
    const std::array second{Selector::plain(u"AAPL"), Selector::timeSeries(u"MSFT", -1)};
    REQUIRE(state.setSymbols(first));
    std::atomic<bool> coherent{true};
    std::atomic<unsigned> observations{0};
    std::jthread reader([&](std::stop_token stop) {
        std::uint64_t previous = 0;

        while (!stop.stop_requested()) {
            const auto cut = state.snapshot();

            if (!cut || cut->revision < previous || cut->symbols.size() != cut->decoratedSymbols.size()) {
                coherent = false;
                break;
            }

            for (std::size_t index = 0; index < cut->symbols.size(); ++index) {
                if (!cut->symbols[index].sameDemand(cut->decoratedSymbols[index].eventSymbol()) ||
                    cut->fromTime != cut->decoratedSymbols[index].fromTime())
                    coherent = false;
            }

            previous = cut->revision;
            ++observations;
            std::this_thread::yield();
        }
    });

    for (unsigned index = 0; index < 100; ++index) {
        REQUIRE(state.setFromTime(index));
        REQUIRE(state.setSymbols(index % 2 ? std::span<const Selector>(first) : std::span<const Selector>(second)));
    }

    while (!observations.load() && coherent.load())
        std::this_thread::yield();

    reader.request_stop();
    reader.join();
    CHECK(coherent.load());
    CHECK(observations.load() > 0);
}
