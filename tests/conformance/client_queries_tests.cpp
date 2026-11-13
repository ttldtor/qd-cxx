// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "connect_peer.hpp"
#include "history_order_input.hpp"
#include "qd/api/feed.hpp"
#include "qd/events/event_flags.hpp"

// The Windows process helper defines CALLBACK; include it after SDK headers.
#include "connect_process_support.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace peer = q::connect_tests;
namespace process = peer::process;
using process::require;
constexpr q::core::SchemaLimits LIMITS{16, 128, 256, 128, 16384};

peer::Record record(const q::core::LocalRecord &shape, std::u16string symbol, std::int32_t id, std::int32_t flags) {
    peer::Record result{std::move(symbol), {}, flags, {}, id, {}};

    for (const auto &field : shape.fields)
        result.fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});

    return result;
}

void verify(const std::filesystem::path &executable, const std::filesystem::path &captures) {
    q::api::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(runtimeOptions);
    auto quote = q::events::defaultQuoteSchema(LIMITS);
    auto candle = q::events::defaultCandleSchema(LIMITS);
    auto [order, orderFields] = history_test::orderInput();
    require(quote && candle, "standard schemas");
    const auto &quoteRecord = quote->records().front();
    auto quoteValue = record(quoteRecord, u"AAPL", 0, 0);

    for (auto &value : quoteValue.fields) {
        if (value.type == q::core::SerialFieldType::WIDE_DECIMAL)
            value.raw = 10 * 256 + 128;
    }

    peer::PeerScript tickerScript;
    tickerScript.family = q::core::SubscriptionFamily::TICKER;
    tickerScript.canonicalSymbols = true;
    tickerScript.afterRemove = std::vector<peer::Record>{};
    const std::array tickValues{quoteValue};
    const std::array tickerSymbols{std::u16string(u"AAPL")};
    peer::Peer ticker(*quote, tickValues, tickerSymbols, 8192, peer::PeerMode::HOLD, tickerScript);

    auto candleShape = candle->records().front();
    auto orderShape = order.records().front();
    candleShape.id = 0;
    orderShape.id = 1;
    const std::array shapes{candleShape, orderShape};
    const auto historySchema = q::core::LocalSchema::create(shapes, LIMITS, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(historySchema.has_value(), "history peer schema");
    namespace flags = q::events::indexed_event_flags;
    auto candleValue = record(candleShape, u"AAPL{=d}", 0, flags::SNAPSHOT_BEGIN);
    candleValue.fields[0].raw = 1;
    candleValue.fields[1].raw = 1;
    auto orderValue = peer::Record{u"AAPL", std::move(orderFields), flags::SNAPSHOT_BEGIN, {}, 1, {}};
    orderValue.fields[1].raw = 1;
    orderValue.fields[4].raw = 100 * 256 + 128;
    orderValue.fields[6].raw = 10 * 256 + 128;
    orderValue.fields[10].raw = 7;
    const std::array values{candleValue,
                            peer::Record{u"AAPL{=d}", {}, flags::REMOVE_EVENT | flags::SNAPSHOT_END, 0, 0, {}},
                            orderValue, peer::Record{u"AAPL", {}, flags::REMOVE_EVENT | flags::SNAPSHOT_END, 0, 1, {}}};
    const std::array historySymbols{std::u16string(u"AAPL{=d}"), std::u16string(u"AAPL")};
    peer::PeerScript historyScript;
    historyScript.family = q::core::SubscriptionFamily::HISTORY;
    historyScript.historyBound = 0;
    historyScript.canonicalSymbols = true;
    historyScript.describeAllRecords = true;
    historyScript.unorderedAdds = true;
    historyScript.subscriptionRecordIds = {10, 0};
    historyScript.afterRemove = std::vector<peer::Record>{};
    historyScript.removeSequence = std::vector{peer::ExpectedSubscription{{u"AAPL{=d}", u"AAPL"}, {10, 0}, true}};
    historyScript.afterFinalRemove = std::vector<peer::Record>{};
    peer::Peer history(*historySchema, values, historySymbols, 8192, peer::PeerMode::HOLD, historyScript);
    std::filesystem::create_directories(captures);
    const auto out = captures / "queries.stdout";
    const auto err = captures / "queries.stderr";
    process::Child child(executable,
                         {"--run", "127.0.0.1:" + std::to_string(ticker.port()),
                          "127.0.0.1:" + std::to_string(history.port()), "24", "2001"},
                         out, err);
    const auto result = child.wait();
    const auto text = process::read(out);

    if (result != 0)
        std::cerr << text << process::read(err) << " ticker-failed=" << ticker.failed()
                  << " history-failed=" << history.failed() << " ticker-adds=" << ticker.adds()
                  << " ticker-removes=" << ticker.removes() << " history-adds=" << history.adds()
                  << " history-removes=" << history.removes() << '\n';

    require(result == 0, "sample success and shutdown");
    require(text.contains("Quote bid=10") && text.contains("Candle snapshot state=COMPLETE count=1") &&
                text.contains("Candle time=1000") && text.contains("Order snapshot state=COMPLETE bids=1"),
            "sample source values and completed snapshots");
    require(text.contains("Retired ticker final=1 active=0 history final=1 active=0"),
            "both endpoints actually retired their native and capture ownership");
    const auto diagnostic = process::read(err);
    require(diagnostic.empty() || diagnostic.contains("Close: drained="), "reported close diagnostics");
    require(ticker.adds() >= 1 && history.adds() >= 1 && !ticker.failed() && !history.failed(),
            "two actual subscriptions and no peer failure");
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;

    try {
        verify(argv[1], argv[2]);

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
