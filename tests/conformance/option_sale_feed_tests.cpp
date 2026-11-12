// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/feed.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/option_sale.hpp"
#include "qd/protocol/subscription_writer.hpp"

#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
namespace c = q::core;
namespace e = q::events;
namespace test = q::test;
namespace r = q::test::recovery;
using namespace std::chrono_literals;
constexpr c::SchemaLimits SCHEMA{16, 128, 256, 128, 16384};
constexpr std::int64_t FIRST_IBM_INDEX = -2147483648LL;
constexpr std::int64_t SNIPPED_IBM_INDEX = 2147483647LL;
constexpr std::int64_t SPY_INDEX = -1;

/** @param success Exact test condition.
 * @param message Static test diagnostic.
 */
void require(bool success, const char *message) {
    if (!success) {
        throw std::runtime_error(message);
    }
}

/** Read the complete unmodified QD BinaryQTPComposer capture, including its DESCRIBE_RECORDS.
 * @param path Explicit fixture path supplied by CTest.
 * @return Owned complete bytes, bounded independently of the stream.
 */
std::vector<std::byte> readFixture(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "OptionSale Java fixture could not be opened");
    const std::vector<char> source{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    require(!source.empty() && source.size() <= 65536, "OptionSale Java fixture has an invalid size");
    std::vector<std::byte> bytes;
    bytes.reserve(source.size());

    for (const auto value : source) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }

    return bytes;
}

/** Keep independent event copies after callback retirement. */
class Observation {
    std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<e::OptionSale> values_;

    static bool complete(const std::vector<e::OptionSale> &values) {
        bool begin = false;
        bool snip = false;
        bool emptyOption = false;

        for (const auto &value : values) {
            begin |= value.symbol == u"IBM" && value.index == FIRST_IBM_INDEX && !value.optionSymbol;
            snip |= value.symbol == u"IBM" && value.index == SNIPPED_IBM_INDEX && value.optionSymbol &&
                    *value.optionSymbol == u"\u03a9";
            emptyOption |= value.symbol == u"SPY&Q" && value.index == SPY_INDEX && value.optionSymbol &&
                           value.optionSymbol->empty();
        }

        return begin && snip && emptyOption;
    }

    public:
    /** @param batch Complete callback-borrowed typed events. */
    void append(std::span<const e::OptionSale> batch) {
        {
            std::lock_guard lock(mutex_);
            values_.insert(values_.end(), batch.begin(), batch.end());
        }

        changed_.notify_all();
    }

    /** @return Detached copies after all three captured source values have arrived. */
    std::vector<e::OptionSale> snapshot() {
        std::unique_lock lock(mutex_);
        require(changed_.wait_for(lock, 12s,
                                  [&] {
                                      return complete(values_);
                                  }),
                "OptionSale Java source values did not reach the public callback");

        return values_;
    }
};

/** @param values Independently owned callback copies.
 * @param symbol Exact source symbol.
 * @param index Signed Java business index.
 * @param expectedEventTime Independent FIELD metadata for the original source record.
 * @return Matching retained business event.
 */
const e::OptionSale &findEvent(const std::vector<e::OptionSale> &values, std::u16string_view symbol, std::int64_t index,
                               std::int64_t expectedEventTime) {
    const auto found = std::ranges::find_if(values, [&](const auto &value) {
        return value.symbol == symbol && value.index == index && value.eventTime == expectedEventTime &&
               value.timeSequence != 0;
    });
    require(found != values.end(), "Expected original Java OptionSale event was not retained");

    return *found;
}

/** Exercise the exact Java-produced physical description and HISTORY_DATA over an actual loopback endpoint.
 * @param fixture Complete QD-produced description and data.
 * @param fieldTime True when the fixture declares and transmits the optional FIELD metadata.
 */
void run(const std::filesystem::path &fixture, bool fieldTime) {
    const auto logical = e::defaultOptionSaleSchema(SCHEMA);
    require(bool(logical), "OptionSale standard logical schema refused");
    a::EventRegistryWork work;
    const std::array routes{a::EventRegistration{a::EventType::OPTION_SALE, 0}};
    const auto registry = a::EventRegistry::create(*logical, routes, {}, work);
    require(bool(registry), "OptionSale standard route registration refused");

    a::RuntimeOptions runtimeOptions;
    runtimeOptions.maximumSubscriptionsPerEndpoint = 2;
    runtimeOptions.maximumSymbolsPerSubscription = 2;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    test::TcpPeerScript script;
    script.initialBytes = r::ready();
    script.replyBytes = readFixture(fixture);
    script.readThroughMessage = std::to_underlying(q::protocol::SubscriptionMessage::HISTORY_ADD);
    script.waitForClientClose = true;
    test::OwnedTcpPeer source({{std::move(script)}, 1024 * 1024, 20s});
    require(source.port() != 0, "OptionSale Java peer could not listen");

    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.eventRegistry = *registry;
    options.maximumAttempts = 1;
    options.readTimeout = 15000ms;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto subscription = feed->createSubscription<e::OptionSale>();

    bool rejectedTimeSeries = false;

    try {
        static_cast<void>(feed->createTimeSeriesSubscription<e::OptionSale>());
    } catch (const a::Error &failure) {
        rejectedTimeSeries = failure.diagnostic().code == a::ErrorCode::UNSUPPORTED_PROFILE;
    }

    require(rejectedTimeSeries, "Indexed OptionSale acquired unsupported time-series capability");
    const auto observed = std::make_shared<Observation>();
    subscription->addEventListener([observed](std::span<const e::OptionSale> batch) {
        observed->append(batch);
    });
    subscription->setSymbols({"IBM", "SPY&Q"});
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    const auto events = observed->snapshot();

    // Expectations come from the Java oracle TSV, rather than the native fixture encoder.
    const auto &first = findEvent(events, u"IBM", FIRST_IBM_INDEX, fieldTime ? -3800 : 0);
    require(first.getSource().id() == 0 && first.getTimeSequence() == -1 && first.getTime() == 23 &&
                first.getSequence() == 4194303 && first.getExchangeCode() == u'Q' && first.getPrice() == 0 &&
                first.getSize() == 1.25 && !first.getOptionSymbol() &&
                (first.eventFlags & e::indexed_event_flags::SNAPSHOT_BEGIN),
            "OptionSale BEGIN value differs from the Java capture");

    const auto &snip = findEvent(events, u"IBM", SNIPPED_IBM_INDEX, fieldTime ? 200 : 0);
    require(snip.getTimeSequence() == 9223372036850581505LL && snip.getTime() == 2147483648023LL &&
                snip.getSequence() == 1 && snip.getExchangeCode() == u'Q' &&
                snip.getOptionSymbol() == std::optional<std::u16string>(u"\u03a9") && snip.flags == 20829 &&
                snip.getTradeThroughExempt() == u'Q' && snip.getAggressorSide() == e::Side::SELL &&
                snip.isSpreadLeg() && snip.isExtendedTradingHours() && snip.isValidTick() && snip.isCorrection() &&
                (snip.eventFlags & e::indexed_event_flags::SNAPSHOT_SNIP),
            "OptionSale SNIP value differs from the Java capture");

    const auto &empty = findEvent(events, u"SPY&Q", SPY_INDEX, fieldTime ? -2800 : 0);
    require(empty.getTimeSequence() == std::numeric_limits<std::int64_t>::min() &&
                empty.getTime() == -2147483648000LL && empty.getOptionSymbol() && empty.getOptionSymbol()->empty() &&
                empty.flags == 1 && empty.isCorrection(),
            "OptionSale empty option symbol or signed time differs from the Java capture");
    const auto closed = endpoint->closeAndAwaitTermination();
    require(!closed.deadlineExpired, "OptionSale HISTORY close exceeded its drain deadline");
    require(subscription->getHistorySnapshot().state == a::HistoryObservationState::RETIRED,
            "Retired OptionSale retained a current history observation");
    runtime.closeAndAwaitTermination();
    const auto peer = source.snapshot();
    require(peer.error == test::TcpPeerError::NONE && peer.sessions.front().matched,
            "OptionSale peer did not receive HISTORY ADD");
}
} // namespace

TEST_CASE("Public OptionSale matches the pinned Java HISTORY observations") {
    const std::filesystem::path fixtures(QDCXX_OPTION_SALE_FIXTURES);
    SUBCASE("standard") {
        run(fixtures / "standard" / "option-sale.qtp", false);
    }
    SUBCASE("FIELD") {
        run(fixtures / "field" / "option-sale.qtp", true);
    }
}
