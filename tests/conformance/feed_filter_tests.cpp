// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal runner: immutable classifier/production-oracle checks; no endpoint, SDK or network startup.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/feed_filter.hpp"

#include "feed_filter_expression.hpp"

#include <doctest/doctest.h>

#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace api = q::api;
namespace detail = api::detail;

api::EventRegistry registry(bool regional = false) {
    if (!regional) {
        api::EventRegistryWork work;
        auto value = api::EventRegistry::builtIn({}, work);
        REQUIRE(value);

        return std::move(*value);
    }

    auto quote = q::events::defaultQuoteSchema({16, 128, 256, 128, 16384});
    REQUIRE(quote);
    auto composite = quote->records()[0];
    auto region = composite;
    region.id = 1;
    region.name = u"Quote&Q";
    const std::array records{composite, region};
    auto schema =
        q::core::LocalSchema::create(records, {16, 128, 256, 128, 16384}, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const std::array routes{api::EventRegistration{api::EventType::QUOTE, 0},
                            api::EventRegistration{api::EventType::QUOTE, 1}};
    api::EventRegistryWork work;
    auto value = api::EventRegistry::create(*schema, routes, {}, work);
    REQUIRE(value);

    return std::move(*value);
}

std::u16string unescape(std::string_view text) {
    std::u16string value;

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 5 < text.size() && text[i + 1] == 'u') {
            unsigned unit = 0;
            std::istringstream digits(std::string(text.substr(i + 2, 4)));
            digits >> std::hex >> unit;
            REQUIRE(!digits.fail());
            value.push_back(static_cast<char16_t>(unit));
            i += 5;
        } else {
            value.push_back(static_cast<unsigned char>(text[i]));
        }
    }

    return value;
}
} // namespace

TEST_CASE("Compiled expressions match raw pinned Java ordinary and wildcard observations") {
    std::ifstream input(ORG_TTLDTOR_QDCXX_FILTER_FIXTURE);
    REQUIRE(input.good());
    std::string line;
    std::size_t rows = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::vector<std::string> cells;
        std::istringstream split(line);
        std::string cell;

        while (std::getline(split, cell, '\t')) {
            cells.push_back(cell);
        }

        REQUIRE(!cells.empty());

        if (cells[0] == "correlation") {
            REQUIRE(cells.size() == 4);
            const auto expression = detail::FilterExpression::compile(unescape(cells[1]), {});
            const bool all = cells[2] == "ALL";
            INFO(line);
            CHECK(expression.accepts(u"Quote", all ? std::u16string{} : unescape(cells[2]), all) ==
                  (cells[3] == "true"));
            ++rows;
        } else if (cells[0] == "invalid") {
            REQUIRE(cells.size() == 3);
            CHECK_THROWS_AS(static_cast<void>(detail::FilterExpression::compile(unescape(cells[1]), {})),
                            detail::FilterExpressionError);
            ++rows;
        } else if (cells[0] == "match" || cells[0] == "wildcard") {
            const bool wildcard = cells[0] == "wildcard";
            REQUIRE(cells.size() == (wildcard ? 4 : 5));
            const auto expression = detail::FilterExpression::compile(unescape(cells[1]), {});
            const auto symbol = wildcard ? std::u16string{} : unescape(cells[3]);
            INFO(line);
            CHECK(expression.accepts(unescape(cells[2]), symbol, wildcard || symbol == u"*") ==
                  (cells[wildcard ? 3 : 4] == "true"));
            ++rows;
        }
    }

    CHECK(rows == 1342);
}

TEST_CASE("First category is global across actual selected delegates and owns caller inputs") {
    auto filter = [&] {
        api::FeedFilter::Builder builder(registry(true));
        std::u16string name = u"regional";
        std::u16string expression = u":Quote[&]Q";
        builder.withCategory(name, expression).withCategory(u"composite", u":Quote");

        return builder.build();
    }();
    CHECK(filter.getCategory(api::EventType::QUOTE, api::SubscriptionSelector::all()) == u"regional");
    CHECK(filter.getCategory(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL")) == u"composite");
    CHECK(filter.getCategory(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL&Q")) == u"regional");
    CHECK(filter.getCategory(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL&N")) == u"composite");
    CHECK_FALSE(filter.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"*")));
    CHECK_FALSE(filter.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"*A")));
    CHECK_FALSE(filter.accept(api::EventType::QUOTE, api::SubscriptionSelector::timeSeries(u"AAPL", 123)));
    CHECK(filter.getCategory(api::EventType::QUOTE, api::SubscriptionSelector::indexed(
                                                        u"AAPL&Q", q::events::IndexedEventSource(42, std::nullopt))) ==
          u"regional");
    q::events::Quote event{};
    event.symbol = u"AAPL&Q";
    CHECK(filter.getEventCategory(event) == u"regional");
    event.symbol = u"*";
    CHECK(filter.getEventCategory(event) == u"composite");
    event.symbol = u"*A";
    CHECK(filter.getEventCategory(event) == u"composite");
    CHECK_FALSE(filter.isDynamic());
    CHECK(filter.getCategories().size() == 2);
}

TEST_CASE("Category grammar and finite refusals leave builder configuration intact") {
    api::FeedFilter::Builder builder(registry());
    CHECK_THROWS_AS(static_cast<void>(builder.build()), api::Error);
    CHECK_THROWS_AS(builder.withCategory(u"", u"*"), api::Error);
    builder.withCategory(u"first", u"A*");
    CHECK_THROWS_AS(builder.withCategory(u"first", u"B*"), api::Error);
    CHECK(builder.getCategories().size() == 1);
    CHECK(builder.build().accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL")));
    api::FeedFilterLimits zero;
    zero.categories = 0;
    CHECK_THROWS_AS(api::FeedFilter::Builder(registry(), api::DeliveryContract::TICKER, zero).withCategory(u"a", u"*"),
                    api::Error);
    detail::FilterExpressionLimits limits;
    limits.depth = 2;
    CHECK_THROWS_AS(static_cast<void>(detail::FilterExpression::compile(u"(((A)))", limits)),
                    detail::FilterExpressionError);
    CHECK_THROWS_AS(static_cast<void>(detail::FilterExpression::compile(u"A*&B*&C*", limits)),
                    detail::FilterExpressionError);
    limits = {};
    limits.nodes = 0;
    CHECK_THROWS_AS(static_cast<void>(detail::FilterExpression::compile(u"A", limits)), detail::FilterExpressionError);
    limits = {};
    limits.patternPositions = 1;
    CHECK_THROWS_AS(static_cast<void>(detail::FilterExpression::compile(u"AB", limits)), detail::FilterExpressionError);
    limits = {};
    limits.textUnits = 1;
    CHECK_THROWS_AS(static_cast<void>(detail::FilterExpression::compile(u"AB", limits)), detail::FilterExpressionError);
}

TEST_CASE("Dynamic resources preserve old snapshots and lazily update a stable tracker") {
    auto context = api::FeedFilterContext::create();
    const std::u16string expression = u"ipf[fixture,update=1d]";
    const std::array first{std::u16string(u"AAPL")};
    const std::array second{std::u16string(u"MSFT")};
    context->replaceSymbols(expression, first, true);
    auto initial =
        api::FeedFilter::Builder(registry()).withResourceContext(context).withCategory(u"ipf", expression).build();
    auto tracker = initial.getTracker();
    CHECK(initial.isDynamic());
    CHECK(initial.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL{x=y}")));
    CHECK_FALSE(initial.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"MSFT")));
    context->replaceSymbols(expression, second, true);
    CHECK(tracker.getCurrentFilter().accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL")));
    std::size_t calls = 0;
    const auto token = tracker.addListener([&](api::FeedFilterTracker current) {
        ++calls;
        CHECK(current.getCurrentFilter().accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL")));
    });
    CHECK(calls == 0);
    CHECK(tracker.getCurrentFilter().accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"MSFT")));
    context->replaceSymbols(expression, first, true);
    CHECK(calls == 1);
    context->replaceSymbols(expression, first, true);
    CHECK(calls == 1);
    CHECK(initial.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL")));
    CHECK(tracker.removeListener(token));
    CHECK_FALSE(tracker.removeListener(token));
    context->replaceSymbols(expression, second, true);
    CHECK(calls == 1);
    CHECK(tracker.getCurrentFilter().accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL")));
}

TEST_CASE("Tracker callback selection survives removal throw reentry and caller retirement") {
    auto context = api::FeedFilterContext::create();
    const std::u16string expression = u"ipf[fixture,update=1d]";
    const std::array first{std::u16string(u"AAPL")};
    const std::array second{std::u16string(u"MSFT")};
    context->replaceSymbols(expression, first, true);
    auto filter =
        api::FeedFilter::Builder(registry()).withResourceContext(context).withCategory(u"ipf", expression).build();
    auto tracker = filter.getTracker();
    std::size_t nesting = 0;
    std::size_t maximumNesting = 0;
    std::size_t headCalls = 0;
    std::size_t tailCalls = 0;
    std::uint64_t tail = 0;
    const auto head = tracker.addListener([&](api::FeedFilterTracker current) {
        ++nesting;
        maximumNesting = std::max(maximumNesting, nesting);
        ++headCalls;

        if (headCalls == 1) {
            CHECK(current.removeListener(tail));
            context->replaceSymbols(expression, first, true);
        }

        --nesting;
        throw std::runtime_error("deliberate contained tracker failure");
    });
    tail = tracker.addListener([&](api::FeedFilterTracker) {
        ++tailCalls;
    });
    context->replaceSymbols(expression, second, true);
    CHECK(headCalls == 2);
    CHECK(tailCalls == 1);
    CHECK(maximumNesting == 1);
    CHECK(tracker.callbackFailures() == 2);
    CHECK(tracker.removeListener(head));
}

TEST_CASE("Retained dynamic generations and selected listener captures consume finite slots") {
    api::FeedFilterContextLimits limits;
    limits.snapshots = 3;
    limits.listeners = 1;
    auto context = api::FeedFilterContext::create(limits);
    const std::u16string expression = u"ipf[fixture,update=1d]";
    const std::array first{std::u16string(u"AAPL")};
    const std::array second{std::u16string(u"MSFT")};
    context->replaceSymbols(expression, first, true);
    auto filter =
        api::FeedFilter::Builder(registry()).withResourceContext(context).withCategory(u"ipf", expression).build();
    auto tracker = filter.getTracker();
    const auto token = tracker.addListener([](api::FeedFilterTracker) {
    });
    CHECK_THROWS_AS(static_cast<void>(tracker.addListener([](api::FeedFilterTracker) {
                    })),
                    api::Error);
    CHECK_THROWS_AS(context->replaceSymbols(expression, second, true), api::Error);
    CHECK(tracker.getCurrentFilter().accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL")));
    CHECK(tracker.removeListener(token));
    CHECK_THROWS_AS(context->replaceSymbols(expression, first, false), api::Error);
}

TEST_CASE("Listener capacity stays charged through reentrant capture retirement") {
    struct Capture {
        std::function<void()> retired;

        ~Capture() {
            retired();
        }
    };

    api::FeedFilterContextLimits limits;
    limits.listeners = 1;
    auto context = api::FeedFilterContext::create(limits);
    const std::u16string expression = u"ipf[fixture,update=1d]";
    const std::array symbols{std::u16string(u"AAPL")};
    context->replaceSymbols(expression, symbols, true);
    auto filter =
        api::FeedFilter::Builder(registry()).withResourceContext(context).withCategory(u"ipf", expression).build();
    auto tracker = filter.getTracker();
    bool retired = false;
    auto capture = std::make_shared<Capture>();
    capture->retired = [&] {
        retired = true;
        CHECK(tracker.getCurrentFilter().isDynamic());
        CHECK_THROWS_AS(static_cast<void>(tracker.addListener([](api::FeedFilterTracker) {
                        })),
                        api::Error);
    };
    const auto token = tracker.addListener([capture](api::FeedFilterTracker) {
    });
    capture.reset();
    CHECK(tracker.removeListener(token));
    CHECK(retired);
    const auto reused = tracker.addListener([](api::FeedFilterTracker) {
    });
    CHECK(tracker.removeListener(reused));
    auto staticTracker = api::FeedFilter::Builder(registry()).withCategory(u"all", u"*").build().getTracker();
    CHECK(staticTracker.addListener({}) == 0);
}

TEST_CASE("Data wildcard cipher is distinct from reserved subscription prefix") {
    auto pattern = api::FeedFilter::Builder(registry()).withCategory(u"a", u"A*").build();
    auto noWildcard = api::FeedFilter::Builder(registry()).withCategory(u"nwc", u"nwc").build();
    q::events::Quote event{};
    event.symbol = u"*";
    CHECK(pattern.acceptEvent(event));
    CHECK_FALSE(noWildcard.acceptEvent(event));
    CHECK_FALSE(pattern.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"*")));
    event.symbol = u"*A";
    CHECK_FALSE(pattern.acceptEvent(event));
    CHECK(noWildcard.acceptEvent(event));
}

TEST_CASE("Concurrent resource replacement exposes one complete immutable classifier generation") {
    auto context = api::FeedFilterContext::create();
    const std::u16string expression = u"ipf[fixture,update=1d]";
    const std::array first{std::u16string(u"AAPL")};
    const std::array second{std::u16string(u"MSFT")};
    context->replaceSymbols(expression, first, true);
    auto initial =
        api::FeedFilter::Builder(registry()).withResourceContext(context).withCategory(u"ipf", expression).build();
    auto tracker = initial.getTracker();
    const auto token = tracker.addListener([](api::FeedFilterTracker) {
    });
    std::barrier boundary(2);
    std::atomic<bool> failed = false;
    std::thread writer([&] {
        for (std::size_t i = 0; i < 32; ++i) {
            boundary.arrive_and_wait();
            try {
                context->replaceSymbols(expression, i % 2 ? first : second, true);
            } catch (...) {
                failed = true;
            }
            boundary.arrive_and_wait();
        }
    });

    for (std::size_t i = 0; i < 32; ++i) {
        boundary.arrive_and_wait();
        try {
            const auto snapshot = tracker.getCurrentFilter();
            const auto a = snapshot.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL"));
            const auto b = snapshot.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"MSFT"));
            CHECK(a != b);
        } catch (...) {
            failed = true;
        }
        boundary.arrive_and_wait();
    }

    writer.join();
    CHECK_FALSE(failed);
    CHECK(initial.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL")));
    CHECK_FALSE(initial.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"MSFT")));
    CHECK(tracker.removeListener(token));
}

TEST_CASE("Unrepresentable listener storage is a detached finite refusal") {
    api::FeedFilterContextLimits limits;
    limits.listeners = std::numeric_limits<std::size_t>::max();
    auto context = api::FeedFilterContext::create(limits);
    const std::u16string expression = u"ipf[fixture,update=1d]";
    const std::array symbols{std::u16string(u"AAPL")};
    context->replaceSymbols(expression, symbols, true);
    try {
        (void)api::FeedFilter::Builder(registry())
            .withResourceContext(context)
            .withCategory(u"ipf", expression)
            .build();
        FAIL("unrepresentable listener vector must refuse");
    } catch (const api::Error &error) {
        CHECK(error.diagnostic().code == api::ErrorCode::RESOURCE_LIMIT);
    }
}

TEST_CASE("Exact finite normalization preserves wildcard membership and typed construction budgets") {
    CHECK_FALSE(detail::FilterExpression::compile(u"A&B", {}).accepts(u"Quote", {}, true));
    CHECK_FALSE(detail::FilterExpression::compile(u"A&!A", {}).accepts(u"Quote", {}, true));
    CHECK_FALSE(detail::FilterExpression::compile(u"(A,B)&!A&!B", {}).accepts(u"Quote", {}, true));
    CHECK(detail::FilterExpression::compile(u"A*&!A*", {}).accepts(u"Quote", {}, true));
    CHECK_FALSE(detail::FilterExpression::compile(u"[*]&A", {}).accepts(u"Quote", {}, true));
    CHECK(detail::FilterExpression::compile(u"[*]&![*]", {}).accepts(u"Quote", {}, true));
    CHECK(detail::FilterExpression::compile(u"([*],A)&([*],B)", {}).accepts(u"Quote", {}, true));
    constexpr std::u16string_view FINITE = u"[A-J][A-J][A-J][A-J]&![A-J][A-J][A-J][A-J]";
    constexpr std::u16string_view NONFINITE = u"[A-K][A-K][A-K][A-K]&![A-K][A-K][A-K][A-K]";
    CHECK_FALSE(detail::FilterExpression::compile(FINITE, {}).accepts(u"Quote", {}, true));
    CHECK(detail::FilterExpression::compile(NONFINITE, {}).accepts(u"Quote", {}, true));
    detail::FilterExpressionLimits limits;
    limits.finiteSymbols = 9999;
    CHECK_THROWS_AS(static_cast<void>(detail::FilterExpression::compile(FINITE, limits)),
                    detail::FilterExpressionError);
    limits.finiteSymbols = 10000;
    CHECK_FALSE(detail::FilterExpression::compile(FINITE, limits).accepts(u"Quote", {}, true));
    limits.normalizationWork = 0;
    CHECK_THROWS_AS(static_cast<void>(detail::FilterExpression::compile(u"A&B", limits)),
                    detail::FilterExpressionError);
    api::FeedFilterLimits publicLimits;
    publicLimits.finiteSymbols = 0;
    try {
        (void)api::FeedFilter::Builder(registry(), api::DeliveryContract::TICKER, publicLimits)
            .withCategory(u"empty", u"A&B")
            .build();
        FAIL("finite candidate refusal must be detached");
    } catch (const api::Error &error) {
        CHECK(error.diagnostic().code == api::ErrorCode::RESOURCE_LIMIT);
    }
    publicLimits = {};
    publicLimits.normalizationWork = 0;
    try {
        (void)api::FeedFilter::Builder(registry(), api::DeliveryContract::TICKER, publicLimits)
            .withCategory(u"empty", u"A&B")
            .build();
        FAIL("finite work refusal must be detached");
    } catch (const api::Error &error) {
        CHECK(error.diagnostic().code == api::ErrorCode::WORK_LIMIT);
    }
}

TEST_CASE("Context normalization work is shared across successful and refused construction attempts") {
    api::FeedFilterContextLimits limits;
    limits.updateWork = 100000;
    auto context = api::FeedFilterContext::create(limits);
    const std::u16string resource = u"ipf[fixture,update=1d]";
    const std::array symbols{std::u16string(u"AAPL")};
    context->replaceSymbols(resource, symbols, true);
    auto held = api::FeedFilter::Builder(registry()).withResourceContext(context).withCategory(u"held", u"*").build();
    api::FeedFilterLimits refused;
    refused.finiteSymbols = 0;
    std::size_t finiteRefusals = 0;
    bool workExhausted = false;

    for (std::size_t attempt = 0; attempt < 1024 && !workExhausted; ++attempt) {
        try {
            (void)api::FeedFilter::Builder(registry(), api::DeliveryContract::TICKER, refused)
                .withResourceContext(context)
                .withCategory(u"empty", u"A&B")
                .build();
            FAIL("zero finite candidates must refuse");
        } catch (const api::Error &failure) {
            if (failure.diagnostic().code == api::ErrorCode::WORK_LIMIT) {
                workExhausted = true;
            } else {
                CHECK(failure.diagnostic().code == api::ErrorCode::RESOURCE_LIMIT);
                ++finiteRefusals;
            }
        }
    }

    CHECK(finiteRefusals > 0);
    CHECK(workExhausted);

    for (std::size_t attempt = 0; attempt < 2; ++attempt) {
        try {
            (void)api::FeedFilter::Builder(registry())
                .withResourceContext(context)
                .withCategory(u"empty", u"A&B")
                .build();
            FAIL("exhausted context normalization work must not reset");
        } catch (const api::Error &failure) {
            CHECK(failure.diagnostic().code == api::ErrorCode::WORK_LIMIT);
        }
    }

    CHECK(held.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(u"AAPL")));
}

TEST_CASE("Generic event coordinates follow actual delegates and ignore nullable payload") {
    auto filter = api::FeedFilter::Builder(registry())
                      .withCategory(u"decorated", u"TOPIC[&]Q")
                      .withCategory(u"message", u":TextMessage*,:Message*")
                      .withCategory(u"configuration", u":TextConfiguration*,:Configuration*")
                      .build();
    std::ifstream input(ORG_TTLDTOR_QDCXX_FILTER_FIXTURE);
    REQUIRE(input.good());
    std::string line;
    std::size_t checked = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream stream(line);
        std::vector<std::string> cells;
        std::string cell;

        while (std::getline(stream, cell, '\t')) {
            cells.push_back(cell);
        }

        if (cells.empty() || (cells[0] != "text-event" && cells[0] != "text-subscription" &&
                              cells[0] != "attachment-event" && cells[0] != "attachment-subscription")) {
            // Java null symbol/model observations have no representation in the owning native value API.
            continue;
        }

        INFO(line);
        const bool attachment = cells[0].starts_with("attachment-");
        const bool configuration = cells[1] == (attachment ? "Configuration" : "TextConfiguration");
        REQUIRE((configuration || cells[1] == (attachment ? "Message" : "TextMessage")));
        const auto type = attachment
                              ? (configuration ? api::EventType::CONFIGURATION : api::EventType::MESSAGE)
                              : (configuration ? api::EventType::TEXT_CONFIGURATION : api::EventType::TEXT_MESSAGE);

        if (cells[0] == "text-event" || cells[0] == "attachment-event") {
            REQUIRE(cells.size() == 6);
            const auto symbol = unescape(cells[2]);
            const api::RegisteredEvent event = [&]() -> api::RegisteredEvent {
                if (attachment) {
                    q::events::SerializedAttachment payload;

                    if (cells[3] != "null") {
                        // Arbitrary serialized bytes must remain opaque to the classifier.
                        const std::array bytes{std::byte{0}, std::byte{0xff}};
                        const auto body = cells[3] == "empty-object" ? std::span<const std::byte>{}
                                                                     : std::span<const std::byte>(bytes);
                        auto value = q::events::SerializedAttachment::create(q::core::SerialFieldType::SERIAL_OBJECT,
                                                                             body, bytes.size());
                        REQUIRE(value);
                        payload = std::move(*value);
                    }

                    const auto time = cells[3] == "opaque-object" ? std::numeric_limits<std::int64_t>::min() : 0;
                    const auto version = cells[3] == "opaque-object" ? std::numeric_limits<std::int32_t>::min() : 0;

                    if (configuration) {
                        return q::events::Configuration{symbol, time, version, payload};
                    }

                    return q::events::Message{symbol, time, payload};
                }

                const auto text = cells[3] == "null"
                                      ? std::optional<std::u16string>{}
                                      : std::optional<std::u16string>(cells[3] == "empty" ? u"" : u"body");

                if (configuration) {
                    return q::events::TextConfiguration{symbol, 0, 0, 0, text};
                }

                return q::events::TextMessage{symbol, 0, 0, text};
            }();
            CHECK(filter.acceptEvent(event) == (cells[4] == "true"));
            CHECK(filter.getEventCategory(event) == unescape(cells[5]));
        } else {
            REQUIRE(cells.size() == 5);
            auto selector = api::SubscriptionSelector::plain(u"TOPIC&Q");

            if (cells[2] == "indexed0" || cells[2] == "indexed1" || cells[2] == "indexed-negative") {
                const auto id = cells[2] == "indexed0" ? 0 : cells[2] == "indexed1" ? 1 : -1;
                selector =
                    api::SubscriptionSelector::indexed(u"TOPIC&Q", q::events::IndexedEventSource(id, std::nullopt));
            } else if (cells[2] == "time-series") {
                selector = api::SubscriptionSelector::timeSeries(u"TOPIC&Q", 123);
            } else if (cells[2] == "all") {
                selector = api::SubscriptionSelector::all();
            } else if (cells[2] == "star" || cells[2] == "star-prefix") {
                selector = api::SubscriptionSelector::plain(cells[2] == "star" ? u"*" : u"*A");
            } else {
                REQUIRE(cells[2] == "plain");
            }

            CHECK(filter.accept(type, selector) == (cells[3] == "true"));
            const auto category = filter.getCategory(type, selector);

            if (cells[4] == "null") {
                CHECK_FALSE(category);
            } else {
                CHECK(category == unescape(cells[4]));
            }
        }

        ++checked;
    }

    CHECK(checked == 92);
}

TEST_CASE("Text mappings preserve symbols for regional-looking custom records and valid absent types") {
    auto message = q::events::defaultTextMessageSchema({16, 128, 256, 128, 16384});
    REQUIRE(message);
    auto record = message->records()[0];
    record.name = u"MessageRoute&Q";
    const std::array records{record};
    auto schema =
        q::core::LocalSchema::create(records, {16, 128, 256, 128, 16384}, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const std::array routes{api::EventRegistration{api::EventType::TEXT_MESSAGE, 0}};
    api::EventRegistryWork work;
    auto registered = api::EventRegistry::create(*schema, routes, {}, work);
    REQUIRE(registered);
    auto filter = api::FeedFilter::Builder(*registered)
                      .withCategory(u"physical", u"TOPIC[&]Q")
                      .withCategory(u"record", u":MessageRoute[&]Q")
                      .build();
    q::events::TextMessage event{u"TOPIC&Q", 0, 0, std::nullopt};
    CHECK(filter.getEventCategory(event) == u"physical");
    CHECK(filter.getCategory(api::EventType::TEXT_MESSAGE, api::SubscriptionSelector::plain(u"TOPIC&Q")) ==
          u"physical");
    CHECK_FALSE(filter.accept(api::EventType::TEXT_CONFIGURATION, api::SubscriptionSelector::plain(u"TOPIC&Q")));
    q::events::TextConfiguration absent{u"TOPIC&Q", 0, 0, -1, std::nullopt};
    CHECK_FALSE(filter.acceptEvent(absent));
}

TEST_CASE("Attachment mappings preserve regional-looking symbols and valid absent event types") {
    const std::array types{api::EventType::MESSAGE, api::EventType::CONFIGURATION};

    for (const auto type : types) {
        const bool configuration = type == api::EventType::CONFIGURATION;
        auto model = configuration ? q::events::defaultConfigurationSchema({16, 128, 256, 128, 16384})
                                   : q::events::defaultMessageSchema({16, 128, 256, 128, 16384});
        REQUIRE(model);
        auto record = model->records()[0];
        record.name = u"AttachmentRoute&Q";
        const std::array records{record};
        auto schema =
            q::core::LocalSchema::create(records, {16, 128, 256, 128, 16384}, q::core::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);
        const std::array routes{api::EventRegistration{type, 0}};
        api::EventRegistryWork work;
        auto registered = api::EventRegistry::create(*schema, routes, {}, work);
        REQUIRE(registered);
        auto filter = api::FeedFilter::Builder(*registered)
                          .withCategory(u"physical", u"TOPIC[&]Q")
                          .withCategory(u"record", u":AttachmentRoute[&]Q")
                          .build();
        const api::RegisteredEvent event = configuration
                                               ? api::RegisteredEvent(q::events::Configuration{u"TOPIC&Q", 0, 0, {}})
                                               : api::RegisteredEvent(q::events::Message{u"TOPIC&Q", 0, {}});
        const api::RegisteredEvent absent = configuration
                                                ? api::RegisteredEvent(q::events::Message{u"TOPIC&Q", 0, {}})
                                                : api::RegisteredEvent(q::events::Configuration{u"TOPIC&Q", 0, 0, {}});
        CHECK(filter.getEventCategory(event) == u"physical");
        CHECK(filter.getCategory(type, api::SubscriptionSelector::plain(u"TOPIC&Q")) == u"physical");
        CHECK_FALSE(filter.accept(configuration ? api::EventType::MESSAGE : api::EventType::CONFIGURATION,
                                  api::SubscriptionSelector::plain(u"TOPIC&Q")));
        CHECK_FALSE(filter.acceptEvent(absent));
    }
}
