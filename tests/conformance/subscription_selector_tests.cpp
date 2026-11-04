// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/subscription_selector.hpp"

#include <array>
#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
using Selector = q::api::SubscriptionSelector;
using Kind = q::api::SubscriptionSymbolKind;

/** @return Explicit known source records, two validated indexed layouts and one nonindexed negative record. */
q::core::LocalSchema schema() {
    const std::array records{
        q::core::LocalRecord{0, u"Order", false, {{8}}, {{u"Index", 0}}},
        q::core::LocalRecord{1, u"Order#NTV", false, {{8}}, {{u"Index", 0}}},
        q::core::LocalRecord{2, u"Order#BYX", false, {{8}}, {{u"Index", 0}}},
        q::core::LocalRecord{
            3, u"Pair", true, {{8}, {8}}, {{u"High", 0}, {u"Low", 1}}, q::core::IndexedTimePairInt32{0, 1}},
        q::core::LocalRecord{4, u"Single", true, {{104}}, {{u"Raw", 0}}, q::core::IndexedTimeSingleInt64{0}},
        q::core::LocalRecord{5, u"Ordinary", false, {{8}}, {{u"Value", 0}}}};
    auto result = q::core::LocalSchema::create(records, {6, 8, 8, 32, 128}, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** @param text One raw corpus line.
 * @return Exact tab-separated cells; captures have no quoting or embedded tabs.
 */
std::vector<std::string> cells(const std::string &text) {
    std::vector<std::string> result;
    std::stringstream stream(text);
    std::string value;

    while (std::getline(stream, value, '\t')) {
        result.push_back(value);
    }

    return result;
}

/** @return Every exact raw production row, without normalizing or replacing the Java discrepancy. */
std::vector<std::vector<std::string>> corpus() {
    std::ifstream input(ORG_TTLDTOR_QDCXX_SELECTOR_FIXTURE);
    REQUIRE(input.good());
    std::vector<std::vector<std::string>> result;
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        result.push_back(cells(line));
    }

    REQUIRE(result.size() == 90);

    return result;
}
} // namespace

TEST_CASE("typed identity preserves Java within-kind semantics and corrects only cross-class symmetry") {
    const auto ies = Selector::indexed(u"AAPL", q::events::IndexedEventSource::DEFAULT);
    const auto other = Selector::indexed(u"AAPL", q::events::IndexedEventSource{1, u"ONE"});
    const auto zero = Selector::timeSeries(u"AAPL", 0);
    const auto minus = Selector::timeSeries(u"AAPL", -1);
    const auto plus = Selector::timeSeries(u"AAPL", 1);
    const auto max = Selector::timeSeries(u"AAPL", std::numeric_limits<std::int64_t>::max());
    const auto min = Selector::timeSeries(u"AAPL", std::numeric_limits<std::int64_t>::min());
    const auto all = Selector::all();
    const std::map<std::string, std::pair<Selector, Selector>> values{
        {"ies-self", {ies, ies}},
        {"ies-same-id-different-name", {ies, Selector::indexed(u"AAPL", {0, u"ALIAS"})}},
        {"ies-same-id-null-name", {ies, Selector::indexed(u"AAPL", {0, std::nullopt})}},
        {"ies-different-source", {ies, other}},
        {"ies-tss-zero", {ies, zero}},
        {"ies-tss-minus", {ies, minus}},
        {"ies-tss-plus", {ies, plus}},
        {"ies-tss-max", {ies, max}},
        {"ies-tss-min", {ies, min}},
        {"other-source-tss", {other, zero}},
        {"tss-zero-minus", {zero, minus}},
        {"tss-zero-plus", {zero, plus}},
        {"tss-zero-max", {zero, max}},
        {"tss-zero-min", {zero, min}},
        {"tss-other-symbol", {zero, Selector::timeSeries(u"MSFT", 0)}},
        {"ies-plain-text", {ies, Selector::plain(u"AAPL")}},
        {"tss-plain-text", {zero, Selector::plain(u"AAPL")}},
        {"wildcard-self", {all, all}},
        {"wildcard-literal-star", {all, Selector::plain(u"*")}},
        {"wildcard-leading-star", {all, Selector::plain(u"*AAPL")}}};
    std::size_t checked = 0;
    std::size_t corrected = 0;

    for (const auto &row : corpus()) {
        if (row[0] != "pair") {
            continue;
        }

        const auto &[left, right] = values.at(row[1]);
        const bool discrepancy = row[1].starts_with("ies-tss-");
        CHECK((left == right) == (discrepancy ? false : row[4] == "true"));
        CHECK((right == left) == (row[5] == "true"));
        CHECK((left == right) == (right == left));
        CHECK(left.hashCode() == std::stoll(row[6]));
        CHECK(right.hashCode() == std::stoll(row[7]));
        corrected += discrepancy;
        ++checked;
    }

    CHECK(checked == 20);
    CHECK(corrected == 5);
    CHECK(zero == max);
    CHECK_FALSE(zero.sameDemand(max));
    CHECK(zero.fromTime() == 0);
    CHECK(max.fromTime() == std::numeric_limits<std::int64_t>::max());
    CHECK(zero.sourceId() == 0);
    CHECK_FALSE(all.sourceId());
    CHECK_FALSE(all.fromTime());
    CHECK(all.eventSymbol().empty());
    std::unordered_set<Selector> forward{ies, zero};
    std::unordered_set<Selector> reverse{zero, ies};
    CHECK(forward.size() == 2);
    CHECK(reverse.size() == 2);
}

TEST_CASE("actual production source selection becomes existing desired requests against validated bindings") {
    const auto local = schema();
    std::vector<q::events::PreparedOrderSource> bindings;

    for (std::int32_t id = 0; id < 3; ++id) {
        auto binding = q::events::PreparedOrderSource::prepare(local, id, {8, 8, 1, 1, 1, 32, 128, 10000});
        REQUIRE(binding);
        bindings.push_back(std::move(*binding));
    }

    std::size_t checked = 0;

    for (const auto &row : corpus()) {
        if (row[0] != "source-route") {
            continue;
        }

        const auto id = static_cast<std::int32_t>(std::stoll(row[2]));
        const auto selector = Selector::indexed(u"AAPL", {id, std::nullopt});
        std::vector<q::core::SubscriptionRequest> requests;
        std::string names;

        for (std::size_t index = 0; index < bindings.size(); ++index) {
            auto request = q::api::indexedSelectorRequest(selector, bindings[index]);
            REQUIRE(request);

            if (*request) {
                requests.push_back(**request);

                if (!names.empty()) {
                    names += '|';
                }

                const auto &name = local.records()[index].name;

                for (const auto unit : name) {
                    names.push_back(static_cast<char>(unit));
                }
            }
        }

        CHECK(requests.size() == std::stoul(row[3]));
        CHECK((names.empty() ? "-" : names) == row[4]);
        auto owner =
            q::core::SubscriptionState::create(local, q::core::SubscriptionFamily::STREAM, {6, 3, 4, 12, 6, 24, 3});
        REQUIRE(owner);
        q::core::SubscriptionWork work{10, 100000, 100000};
        REQUIRE(owner->replace(requests, work));
        CHECK(owner->entries().size() == requests.size());
        ++checked;
    }

    CHECK(checked == 7);
    CHECK_FALSE(q::api::indexedSelectorRequest(Selector::all(), bindings[0]));
    CHECK_FALSE(q::api::indexedSelectorRequest(Selector::timeSeries(u"AAPL", 0), bindings[0]));
}

TEST_CASE("production signed time bounds preserve MAX and update same-identity HISTORY demand") {
    const auto local = schema();
    std::size_t checked = 0;

    for (const auto &row : corpus()) {
        if (row[0] != "time") {
            continue;
        }

        const auto selector = Selector::timeSeries(u"AAPL", std::stoll(row[1]));

        for (const std::int32_t record : {3, 4}) {
            const auto bound = selector.qdTime();
            REQUIRE(bound);
            CHECK(*bound == std::stoll(row[2]));
            const q::core::SubscriptionRequest request{{local.recordContext(record), selector.eventSymbol()}, bound};
            auto owner =
                q::core::SubscriptionState::create(local, q::core::SubscriptionFamily::HISTORY, {6, 1, 4, 4, 2, 8, 1});
            REQUIRE(owner);
            q::core::SubscriptionWork work{10, 100000, 100000};
            REQUIRE(owner->replace(std::span(&request, 1), work));
            CHECK(owner->entries()[0].historyBound == request.historyBound);
            const auto revision = owner->revision();
            const auto replacement = Selector::timeSeries(u"AAPL", 1234567);
            REQUIRE(selector == replacement);
            const q::core::SubscriptionRequest next{{local.recordContext(record), replacement.eventSymbol()},
                                                    replacement.qdTime()};
            REQUIRE(owner->replace(std::span(&next, 1), work));
            CHECK(owner->revision() > revision);
            CHECK(owner->entries()[0].historyBound == next.historyBound);
        }

        ++checked;
    }

    CHECK(checked == 13);
    CHECK_FALSE(Selector::plain(u"AAPL").qdTime());
    CHECK_FALSE(Selector::all().qdTime());
    auto owner = q::core::SubscriptionState::create(local, q::core::SubscriptionFamily::HISTORY, {6, 1, 4, 4, 2, 8, 1});
    REQUIRE(owner);
    q::core::SubscriptionWork work{10, 100000, 100000};
    const auto value = Selector::timeSeries(u"AAPL", 0);
    const q::core::SubscriptionRequest nonindexed{{local.recordContext(5), value.eventSymbol()}, value.qdTime()};
    auto refused = owner->replace(std::span(&nonindexed, 1), work);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == q::core::SubscriptionError::INVALID_HISTORY);
    CHECK(owner->entries().empty());
}

TEST_CASE("borrowed source request is copied into an independent desired owner before selector retirement") {
    const auto local = schema();
    auto binding = q::events::PreparedOrderSource::prepare(local, 0, {8, 8, 1, 1, 1, 32, 128, 10000});
    REQUIRE(binding);
    auto selector = Selector::plain(u"AAPL");
    auto request = q::api::indexedSelectorRequest(selector, *binding);
    REQUIRE(request);
    REQUIRE(*request);
    auto owner = q::core::SubscriptionState::create(local, q::core::SubscriptionFamily::STREAM, {6, 1, 4, 4, 2, 8, 1});
    REQUIRE(owner);
    q::core::SubscriptionWork work{10, 100000, 100000};
    REQUIRE(owner->replace(std::span(&**request, 1), work));
    selector = Selector::indexed(u"OTHER", {123, std::nullopt});
    CHECK(owner->entries().size() == 1);
    CHECK(owner->entries()[0].key.symbol == u"AAPL");
    CHECK(owner->entries()[0].key.context.matches(local.recordContext(0)));
}
