// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"

// The Windows process helper introduces the CALLBACK macro.
#include "connect_process_support.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace peer = q::connect_tests;
namespace process = peer::process;
using process::require;

constexpr q::core::SchemaLimits LIMITS{16, 128, 256, 128, 16384};
constexpr std::int32_t QUOTE = 0;
constexpr std::int32_t TRADE = 2;
constexpr std::int32_t QUOTE_Q = 9 + (u'Q' - u'A');

/** Return the production regional Quote shape with the two exchange fields omitted. */
q::core::LocalRecord regionalQuote(const q::core::LocalRecord &composite) {
    q::core::LocalRecord regional{1, u"Quote&Q", false, {}, {}};

    for (const auto &lookup : composite.lookups) {
        if (lookup.name != u"Quote.Bid.Exchange" && lookup.name != u"Quote.Ask.Exchange" &&
            lookup.name.starts_with(u"Quote.")) {
            regional.lookups.push_back({regional.name + lookup.name.substr(5), regional.fields.size()});
            regional.fields.push_back(composite.fields[lookup.fieldId]);
        }
    }

    require(regional.fields.size() == 6, "six-field regional Quote fixture");

    return regional;
}

/** Construct valid nonzero business fields in the independent peer's schema order. */
peer::Record marketRecord(const q::core::LocalRecord &record, std::u16string symbol, std::int64_t integer) {
    using Type = q::core::SerialFieldType;
    peer::Record result{std::move(symbol), {}, 0, {}, record.id, {}};

    for (const auto &field : record.fields) {
        const auto type = static_cast<Type>(field.serialTypeId);
        std::int64_t raw = 0;

        if (type == Type::WIDE_DECIMAL) {
            raw = (integer << 8) | 128;
        } else if (type == Type::UTF_CHAR) {
            raw = u'Q';
        }

        result.fields.push_back({type, raw});
    }

    return result;
}

void checkMarket(const std::filesystem::path &executable, const std::filesystem::path &captures) {
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    const auto quote = q::events::defaultQuoteSchema(LIMITS);
    const auto trade = q::events::defaultTradeSchema(LIMITS);
    require(quote.has_value() && trade.has_value(), "composite market schemas");
    std::array declarations{quote->records().front(), regionalQuote(quote->records().front()),
                            trade->records().front()};
    declarations[2].id = 2;
    const auto schema = q::core::LocalSchema::create(declarations, LIMITS, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(schema.has_value(), "market peer schema");
    const std::array initial{marketRecord(declarations[0], u"AAPL", 10), marketRecord(declarations[1], u"AAPL", 20),
                             marketRecord(declarations[1], u"AAPL&Q", 30), marketRecord(declarations[2], u"AAPL", 40)};
    std::vector<std::u16string> expected{u"AAPL", u"AAPL", u"AAPL", u"AAPL&Q"};
    std::vector<std::int32_t> expectedIds{QUOTE, TRADE, QUOTE_Q, TRADE};

    // A regional Order source selects that side for all regional delegates. Its symbol stays literal.
    constexpr std::int32_t FIRST_REGIONAL_QUOTE = 9;
    constexpr std::int32_t REGIONAL_QUOTE_COUNT = 26;

    for (auto id = FIRST_REGIONAL_QUOTE; id < FIRST_REGIONAL_QUOTE + REGIONAL_QUOTE_COUNT; ++id) {
        expected.push_back(u"AAPL&Q");
        expectedIds.push_back(id);
    }

    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::TICKER;
    script.describeAllRecords = true;
    script.canonicalSymbols = true;
    script.unorderedAdds = true;
    script.subscriptionRecordIds = expectedIds;
    script.followupAdd = peer::ExpectedSubscription{{u"MSFT", u"MSFT"}, {QUOTE, TRADE}};
    script.afterFollowupAdd = std::vector{marketRecord(declarations[2], u"MSFT", 50)};
    auto removed = peer::ExpectedSubscription{expected, expectedIds, true};
    removed.symbols.insert(removed.symbols.end(), {u"MSFT", u"MSFT"});
    removed.recordIds.insert(removed.recordIds.end(), {QUOTE, TRADE});
    script.removeSequence = std::vector{std::move(removed)};
    script.afterFinalRemove = std::vector<peer::Record>{};
    peer::Peer server(*schema, initial, expected, 8192, peer::PeerMode::HOLD, script);

    std::filesystem::create_directories(captures);
    const auto output = captures / "market.stdout";
    const auto error = captures / "market.stderr";
    process::Child child(executable,
                         {"--run", "--address", "127.0.0.1:" + std::to_string(server.port()), "--duration-ms", "4000"},
                         output, error);
    const auto exit = child.wait();
    const auto stdoutText = process::read(output);

    if (exit != 0 || server.failed()) {
        std::cerr << stdoutText << process::read(error) << " peer adds=" << server.adds()
                  << " peer removes=" << server.removes() << '\n';
    }

    require(exit == 0 && server.adds() == 2 && !server.failed(), "market sample wire ADD, mutation and shutdown");
    require(stdoutText.contains("Quote AAPL bid=10") && stdoutText.contains("Quote AAPL&Q bid=20") &&
                stdoutText.contains("Trade AAPL price=40") && stdoutText.contains("Trade MSFT price=50"),
            "composite, regional and mutation outputs");
    const auto bidSource = "Order AAPL&Q source=" + std::to_string(q::events::OrderSource::REGIONAL_BID.id()) + " ";
    const auto askSource = "Order AAPL&Q source=" + std::to_string(q::events::OrderSource::REGIONAL_ASK.id()) + " ";
    require(stdoutText.contains(bidSource) && !stdoutText.contains(askSource), "explicit regional bid source only");
    const auto compositeSource = "Order AAPL source=" + std::to_string(q::events::OrderSource::COMPOSITE.id()) + " ";
    const auto firstComposite = stdoutText.find(compositeSource);
    require(firstComposite != std::string::npos, "first composite Order");
    const auto secondComposite = stdoutText.find(compositeSource, firstComposite + compositeSource.size());
    require(secondComposite != std::string::npos &&
                stdoutText.find(compositeSource, secondComposite + compositeSource.size()) == std::string::npos,
            "exact composite bid and ask count");
    require(stdoutText.contains("events=7 delivered=7 deliveredAvailability=0") && stdoutText.contains("readBytes=") &&
                !stdoutText.contains("readBytes=0 "),
            "business event and transport statistics");
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        return 2;
    }

    try {
        checkMarket(std::filesystem::absolute(argv[1]), std::filesystem::absolute(argv[2]));
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
