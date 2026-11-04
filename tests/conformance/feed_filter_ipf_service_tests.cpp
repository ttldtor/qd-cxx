// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal runner: bounded owning IPF bootstrap, update and retirement checks against controlled sources.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/feed_filter.hpp"

#include "ipf_http_peer.hpp"
#include "qd/ipf/instrument_profile_url_reader.hpp"

#include <doctest/doctest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace api = q::api;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
constexpr std::string_view HEADER = "#STOCK::=TYPE,SYMBOL,TRADING_HOURS\n";

/** Make the existing real standard-event registry.
 * @return Independently owned built-in routes.
 */
api::EventRegistry registry() {
    api::EventRegistryWork work;
    auto result = api::EventRegistry::builtIn({}, work);
    REQUIRE(result);

    return std::move(*result);
}

/** Use finite deterministic transfer timeouts without a live external service.
 * @return Existing full native/HTTP/parser grants with short test deadlines.
 */
q::ipf::IpfTransferOptions transfer() {
    q::ipf::IpfTransferOptions result;
    result.overallTimeout = 1000ms;
    result.connectTimeout = 200ms;
    result.readTimeout = 200ms;
    result.closeTimeout = 100ms;

    return result;
}

/** Make a complete finite HTTP message.
 * @param body Exact controlled IPF text.
 * @return Owning raw response.
 */
std::string reply(std::string body) {
    return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

/** Make one full stock snapshot; the default parser permits missing COMPLETE.
 * @param symbol Exact ASCII test symbol.
 * @param schedule Exact ASCII schedule label.
 * @return Full clean source body.
 */
std::string body(std::string_view symbol, std::string_view schedule = "") {
    return std::string(HEADER) + "STOCK," + std::string(symbol) + "," + std::string(schedule) + "\n";
}

/** Make the full configured resource identity without exposing any real credentials.
 * @param port Owned controlled peer port.
 * @param properties Exact explicit synthetic properties.
 * @return Full UTF-16 resource token.
 */
std::u16string expression(std::uint16_t port, std::string_view properties = "") {
    const auto text = "ipf[http://127.0.0.1:" + std::to_string(port) + "/profiles.ipf" + std::string(properties) + "]";

    return {text.begin(), text.end()};
}

/** Wait for a numeric/owning test observation with a finite deadline.
 * @tparam Predicate Nullary read-only condition.
 * @param predicate Condition over controlled fixture state.
 * @return Whether the condition became true.
 */
template <class Predicate> bool until(Predicate predicate) {
    const auto deadline = Clock::now() + 3s;

    while (Clock::now() < deadline) {
        if (predicate()) {
            return true;
        }

        std::this_thread::sleep_for(1ms);
    }

    return predicate();
}

/** Build through the ordinary session-owned context.
 * @param context Existing resource repository/resolver.
 * @param resource Full resource expression.
 * @return Immutable classifier after automatic finite bootstrap.
 */
api::FeedFilter build(std::shared_ptr<api::FeedFilterContext> context, const std::u16string &resource) {
    return api::FeedFilter::Builder(registry())
        .withResourceContext(std::move(context))
        .withCategory(u"profiles", resource)
        .build();
}

/** Query a real standard-event route.
 * @param filter Immutable current or retained classifier.
 * @param symbol Exact synthetic symbol.
 * @return Whether the standard Quote route accepts the symbol.
 */
bool accepts(const api::FeedFilter &filter, std::u16string symbol) {
    return filter.accept(api::EventType::QUOTE, api::SubscriptionSelector::plain(std::move(symbol)));
}
} // namespace

TEST_CASE("Owning session bootstraps two resources automatically and leaves immutable snapshots") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = api::FeedFilterIpfRuntime::create(*bootstrap, transfer());
    q::test::IpfHttpPeer first({reply(body("AAPL"))});
    q::test::IpfHttpPeer second({reply(body("MSFT"))});
    REQUIRE_FALSE(first.failed());
    REQUIRE_FALSE(second.failed());
    auto context = owner->context();
    auto filter = api::FeedFilter::Builder(registry())
                      .withResourceContext(context)
                      .withCategory(u"first", expression(first.port()))
                      .withCategory(u"second", expression(second.port()))
                      .build();
    CHECK(accepts(filter, u"AAPL{x=y}"));
    CHECK(accepts(filter, u"MSFT"));
    CHECK_FALSE(filter.isDynamic());
    CHECK(first.accepted() == 1);
    CHECK(second.accepted() == 1);
    owner->closeAndAwaitTermination();
    owner.reset();
    CHECK(accepts(filter, u"AAPL"));
    CHECK_THROWS_AS(static_cast<void>(build(context, u"ipf[missing-after-retirement]")), api::Error);
}

TEST_CASE("COMPLETE and FLUSH do not publish an incomplete HTTP source or malformed tail") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto options = transfer();
    options.readTimeout = 50ms;
    options.overallTimeout = 300ms;
    auto owner = api::FeedFilterIpfRuntime::create(*bootstrap, options);
    const auto prefix = body("AAPL") + "##FLUSH\n##COMPLETE\n";
    q::test::IpfHttpPeer held({"HTTP/1.1 200 OK\r\nContent-Length: 99999\r\n\r\n" + prefix}, 4096, true);
    auto context = api::FeedFilterContext::create();
    const auto resource = expression(held.port());
    auto source = owner->start(context, resource, {});
    CHECK_FALSE(source->awaitInitialSnapshot(Clock::now() + 2s));
    REQUIRE(until([&] {
        return source->observation().retired;
    }));
    CHECK_FALSE(source->observation().initialSnapshotReady);
    CHECK(source->observation().diagnostic.has_value());
    CHECK_THROWS_AS(static_cast<void>(build(context, resource)), api::Error);
    q::test::IpfHttpPeer malformed({reply(prefix + "STOCK,\"unterminated\n")});
    const auto malformedResource = expression(malformed.port());
    auto failed = owner->start(context, malformedResource, {});
    CHECK_FALSE(failed->awaitInitialSnapshot(Clock::now() + 2s));
    CHECK_FALSE(failed->observation().initialSnapshotReady);
    CHECK_THROWS_AS(static_cast<void>(build(context, malformedResource)), api::Error);
    owner->closeAndAwaitTermination();
}

TEST_CASE("Updates are lazy full snapshots with identical suppression error retention and callback reentry") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    std::atomic<bool> suppress{true};
    std::atomic<unsigned> scheduleChecks{0};
    api::FeedFilterIpfSchedule schedule;
    schedule.currentTimeMillis = [] {
        return std::int64_t{0};
    };
    schedule.isTrading = [&](std::u16string_view, std::int64_t) {
        ++scheduleChecks;

        return suppress.load();
    };
    auto owner = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), {}, schedule);
    const auto invalid = body("GOOG") + "##COMPLETE\nSTOCK,\"unterminated\n";
    q::test::IpfHttpPeer peer(
        {reply(body("AAPL")), reply(body("MSFT")), reply(body("MSFT")), reply(invalid), reply(body("AMZN"))});
    auto context = owner->context();
    const auto resource = expression(peer.port(), ",update=0.02s,schedule=GATE");
    auto source = owner->start(context, resource, {});
    REQUIRE(source->awaitInitialSnapshot(Clock::now() + 2s));
    auto original = build(context, resource);
    auto tracker = original.getTracker();
    std::this_thread::sleep_for(80ms);
    CHECK(source->observation().attempts == 1);
    CHECK(scheduleChecks == 0);
    std::atomic<unsigned> headCalls{0};
    std::atomic<unsigned> tailCalls{0};
    std::atomic<unsigned> selfWaitRefusals{0};
    const auto head = tracker.addListener([&](api::FeedFilterTracker) {
        ++headCalls;
        throw std::runtime_error("synthetic contained callback");
    });
    const auto tail = tracker.addListener([&](api::FeedFilterTracker current) {
        try {
            static_cast<void>(source->awaitInitialSnapshot(Clock::now() + 1s));
        } catch (const api::Error &error) {
            if (error.diagnostic().stage == "filter-ipf-worker-self-wait") {
                ++selfWaitRefusals;
            }
        }

        try {
            owner->closeAndAwaitTermination();
        } catch (const api::Error &error) {
            if (error.diagnostic().code == api::ErrorCode::WRONG_THREAD) {
                ++selfWaitRefusals;
            }
        }

        try {
            static_cast<void>(build(owner->context(), u"ipf[recursive-missing-resource]"));
        } catch (const api::Error &error) {
            if (error.diagnostic().stage == "filter-ipf-worker-self-wait") {
                ++selfWaitRefusals;
            }
        }

        ++tailCalls;

        if (accepts(current.getCurrentFilter(), u"AMZN")) {
            source->close();
        }
    });
    suppress = false;
    REQUIRE(until([&] {
        return source->observation().retired;
    }));
    CHECK(source->observation().attempts == 5);
    CHECK(headCalls == 2);
    CHECK(tailCalls == 2);
    CHECK(selfWaitRefusals == 6);
    CHECK(tracker.callbackFailures() == 2);
    CHECK(accepts(tracker.getCurrentFilter(), u"AMZN"));
    CHECK_FALSE(accepts(tracker.getCurrentFilter(), u"GOOG"));
    CHECK(accepts(original, u"AAPL"));
    CHECK(tracker.removeListener(head));
    CHECK(tracker.removeListener(tail));
    owner->closeAndAwaitTermination();
}

TEST_CASE("One inactive resource does not starve another tracked resource") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = api::FeedFilterIpfRuntime::create(*bootstrap, transfer());
    q::test::IpfHttpPeer first({reply(body("AAPL")), reply(body("GOOG"))});
    q::test::IpfHttpPeer second({reply(body("MSFT")), reply(body("AMZN"))});
    auto context = owner->context();
    const auto a = expression(first.port(), ",update=0.02s");
    const auto b = expression(second.port(), ",update=0.02s");
    auto sourceA = owner->start(context, a, {});
    auto sourceB = owner->start(context, b, {});
    REQUIRE(sourceA->awaitInitialSnapshot(Clock::now() + 2s));
    REQUIRE(sourceB->awaitInitialSnapshot(Clock::now() + 2s));
    auto filterA = build(context, a);
    auto filterB = build(context, b);
    auto trackerA = filterA.getTracker();
    auto trackerB = filterB.getTracker();
    std::atomic<unsigned> callsB{0};
    const auto listenerB = trackerB.addListener([&](api::FeedFilterTracker) {
        ++callsB;
        sourceB->close();
    });
    REQUIRE(until([&] {
        return callsB.load() == 1;
    }));
    CHECK(sourceA->observation().attempts == 1);
    CHECK(sourceB->observation().attempts == 2);
    REQUIRE(until([&] {
        return sourceB->observation().retired;
    }));
    std::atomic<unsigned> callsA{0};
    std::atomic<bool> retiredWait{false};
    const auto listenerA = trackerA.addListener([&](api::FeedFilterTracker) {
        retiredWait = sourceB->awaitInitialSnapshot(Clock::now() + 1s);
        ++callsA;
        sourceA->close();
    });
    REQUIRE(until([&] {
        return callsA.load() == 1;
    }));
    CHECK(sourceA->observation().attempts == 2);
    CHECK(retiredWait.load());
    CHECK(trackerA.callbackFailures() == 0);
    CHECK(trackerA.removeListener(listenerA));
    CHECK(trackerB.removeListener(listenerB));
    owner->closeAndAwaitTermination();
}

TEST_CASE("Explicit schedules suppress checks at the supplied boundary and contain callback throws") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    std::atomic<std::int64_t> now{99};
    std::atomic<unsigned> scheduleCalls{0};
    api::FeedFilterIpfSchedule schedule;
    schedule.currentTimeMillis = [&] {
        return now.load();
    };
    schedule.isTrading = [&](std::u16string_view name, std::int64_t time) {
        CHECK(name == u"TRADING");
        ++scheduleCalls;

        return time < 100;
    };
    auto owner = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), {}, schedule);
    q::test::IpfHttpPeer peer({reply(body("AAPL")), reply(body("MSFT"))});
    const auto resource = expression(peer.port(), ",update=0.02s,schedule=TRADING");
    auto source = owner->start(owner->context(), resource, {});
    REQUIRE(source->awaitInitialSnapshot(Clock::now() + 2s));
    auto tracker = build(owner->context(), resource).getTracker();
    const auto listener = tracker.addListener([&](api::FeedFilterTracker) {
        source->close();
    });
    REQUIRE(until([&] {
        return scheduleCalls.load() > 0;
    }));
    CHECK(source->observation().attempts == 1);
    now = 100;
    REQUIRE(until([&] {
        return source->observation().retired;
    }));
    CHECK(source->observation().attempts == 2);
    CHECK(accepts(tracker.getCurrentFilter(), u"MSFT"));
    CHECK(tracker.removeListener(listener));
    owner->closeAndAwaitTermination();

    schedule.isTrading = [](std::u16string_view, std::int64_t) -> bool {
        throw std::runtime_error("synthetic schedule failure");
    };
    auto throwing = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), {}, schedule);
    q::test::IpfHttpPeer next({reply(body("AAPL", "TRADING"))});
    const auto automatic = expression(next.port(), ",update=0.01s,schedule");
    auto failed = throwing->start(throwing->context(), automatic, {});
    REQUIRE(failed->awaitInitialSnapshot(Clock::now() + 2s));
    auto stable = build(throwing->context(), automatic).getTracker();
    const auto token = stable.addListener([](api::FeedFilterTracker) {
    });
    REQUIRE(until([&] {
        return failed->observation().retired;
    }));
    REQUIRE(failed->observation().diagnostic);
    CHECK(failed->observation().diagnostic->code == api::ErrorCode::CALLBACK_FAILURE);
    CHECK(accepts(stable.getCurrentFilter(), u"AAPL"));
    CHECK(stable.removeListener(token));
    throwing->closeAndAwaitTermination();
}

TEST_CASE("Finite source snapshot work and capability bounds refuse without partial publication") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    api::FeedFilterIpfLimits limits;
    limits.sources = 1;
    limits.snapshotSymbols = 1;
    auto owner = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), limits);
    q::test::IpfHttpPeer peer({reply(std::string(HEADER) + "STOCK,AAPL,\nSTOCK,MSFT,\n")});
    auto source = owner->start(owner->context(), expression(peer.port()), {});
    CHECK_FALSE(source->awaitInitialSnapshot(Clock::now() + 2s));
    REQUIRE(source->observation().diagnostic);
    CHECK(source->observation().diagnostic->code == api::ErrorCode::RESOURCE_LIMIT);
    CHECK_THROWS_AS(static_cast<void>(owner->start(owner->context(), u"ipf[another]", {})), api::Error);
    CHECK_THROWS_AS(
        static_cast<void>(owner->start(owner->context(), u"ipf[http://127.0.0.1/path,update=1s,schedule=TRADING]", {})),
        api::Error);
    q::ipf::IpfUrlRequest request;
    request.authorization = *q::ipf::HttpAuthorization::bearer("synthetic");
    CHECK_THROWS_AS(
        static_cast<void>(owner->start(owner->context(), u"ipf[https://invalid.example/path,user=synthetic]", request)),
        api::Error);
    owner->closeAndAwaitTermination();
    limits = {};
    limits.copyWork = 1;
    auto small = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), limits);
    q::test::IpfHttpPeer second({reply(body("AAPL"))});
    auto refused = small->start(small->context(), expression(second.port()), {});
    CHECK_FALSE(refused->awaitInitialSnapshot(Clock::now() + 2s));
    REQUIRE(refused->observation().diagnostic);
    CHECK(refused->observation().diagnostic->code == api::ErrorCode::WORK_LIMIT);
    small->closeAndAwaitTermination();
}

TEST_CASE("Close cuts an active source and its handles survive joined runtime retirement") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto owner = api::FeedFilterIpfRuntime::create(*bootstrap, transfer());
    q::test::IpfHttpPeer peer({"HTTP/1.1 200 OK\r\nContent-Length: 99999\r\n\r\n" + body("AAPL")}, 4096, true);
    auto source = owner->start(owner->context(), expression(peer.port()), {});
    REQUIRE(until([&] {
        return peer.accepted() > 0;
    }));
    source->close();
    const auto before = Clock::now();
    owner->closeAndAwaitTermination();
    CHECK(Clock::now() - before < 2s);
    owner.reset();
    CHECK(source->observation().closed);
    CHECK(source->observation().retired);
    CHECK_FALSE(source->observation().initialSnapshotReady);
    CHECK_FALSE(source->awaitInitialSnapshot(Clock::now() + 1s));

    auto interruptedOwner = api::FeedFilterIpfRuntime::create(*bootstrap, transfer());
    q::test::IpfHttpPeer interruptedPeer({"HTTP/1.1 200 OK\r\nContent-Length: 99999\r\n\r\n" + body("MSFT")}, 4096,
                                         true);
    auto context = interruptedOwner->context();
    const auto resource = expression(interruptedPeer.port());
    std::atomic<api::ErrorCode> interrupted{api::ErrorCode::NONE};
    std::atomic<bool> returnedFilter{false};
    std::jthread builder([&] {
        try {
            static_cast<void>(build(context, resource));
            returnedFilter = true;
        } catch (const api::Error &error) {
            interrupted = error.diagnostic().code;
        } catch (...) {
            interrupted = api::ErrorCode::INVALID_STATE;
        }
    });
    REQUIRE(until([&] {
        return interruptedPeer.accepted() > 0;
    }));
    interruptedOwner->closeAndAwaitTermination();
    builder.join();
    CHECK_FALSE(returnedFilter.load());
    CHECK(interrupted.load() == api::ErrorCode::CLOSED);
}

TEST_CASE("Session attempt lifetime scheduler and initial-wait bounds remain finite") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    api::FeedFilterIpfLimits limits;
    limits.attempts = 2;
    auto owner = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), limits);
    q::test::IpfHttpPeer peer({reply(body("AAPL")), reply(body("MSFT"))});
    const auto resource = expression(peer.port(), ",update=0.01s");
    auto source = owner->start(owner->context(), resource, {});
    REQUIRE(source->awaitInitialSnapshot(Clock::now() + 2s));
    auto tracker = build(owner->context(), resource).getTracker();
    const auto listener = tracker.addListener([](api::FeedFilterTracker) {
    });
    REQUIRE(until([&] {
        return source->observation().retired;
    }));
    CHECK(source->observation().attempts == 2);
    REQUIRE(source->observation().diagnostic);
    CHECK(source->observation().diagnostic->code == api::ErrorCode::WORK_LIMIT);
    CHECK(accepts(tracker.getCurrentFilter(), u"MSFT"));
    CHECK(tracker.removeListener(listener));
    owner->closeAndAwaitTermination();

    limits = {};
    limits.lifetime = 40ms;
    auto finite = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), limits);
    q::test::IpfHttpPeer held({"HTTP/1.1 200 OK\r\nContent-Length: 99999\r\n\r\n" + body("AAPL")}, 4096, true);
    auto bounded = finite->start(finite->context(), expression(held.port()), {});
    CHECK_FALSE(bounded->awaitInitialSnapshot(Clock::now() + 2s));
    REQUIRE(until([&] {
        return bounded->observation().retired;
    }));
    CHECK_FALSE(bounded->observation().initialSnapshotReady);
    finite->closeAndAwaitTermination();

    limits = {};
    limits.schedulerTurns = 1;
    auto scheduler = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), limits);
    std::this_thread::sleep_for(50ms);
    CHECK_THROWS_AS(static_cast<void>(scheduler->start(scheduler->context(), u"ipf[after-scheduler-cutoff]", {})),
                    api::Error);
    scheduler->closeAndAwaitTermination();

    limits = {};
    limits.initialTimeout = 10ms;
    auto initial = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), limits);
    q::test::IpfHttpPeer slow({"HTTP/1.1 200 OK\r\nContent-Length: 99999\r\n\r\n" + body("AAPL")}, 4096, true);
    const auto before = Clock::now();
    CHECK_THROWS_AS(static_cast<void>(build(initial->context(), expression(slow.port()))), api::Error);
    CHECK(Clock::now() - before < 1s);
    initial->closeAndAwaitTermination();

    limits = {};
    limits.schedulerInterval = 0ms;
    CHECK_THROWS_AS(static_cast<void>(api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), limits)), api::Error);
    limits.schedulerInterval = 5s;
    auto sparse = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), limits);
    q::test::IpfHttpPeer ready({reply(body("AAPL"))});
    auto dormant = sparse->start(sparse->context(), expression(ready.port(), ",update=0.01s"), {});
    REQUIRE(dormant->awaitInitialSnapshot(Clock::now() + 2s));
    std::this_thread::sleep_for(20ms);
    dormant->close();
    REQUIRE(until([&] {
        return dormant->observation().retired;
    }));
    const auto closeStarted = Clock::now();
    sparse->closeAndAwaitTermination();
    CHECK(Clock::now() - closeStarted < 1s);
}

TEST_CASE("Owning file sources load initially and update after lazy listener activation") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto path = std::filesystem::temp_directory_path() /
                      ("qdcxx-filter-ipf-" + std::to_string(Clock::now().time_since_epoch().count()) + ".ipf");
    REQUIRE_FALSE(std::filesystem::exists(path));

    struct OwnedFile {
        std::filesystem::path path;

        ~OwnedFile() {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    } file{path};

    {
        std::ofstream output(path, std::ios::binary);
        output << body("AAPL");
        REQUIRE(output.good());
    }
    std::u16string resource = u"ipf[";

    for (const auto unit : path.generic_u16string()) {
        if (unit == u',' || unit == u'[' || unit == u']' || unit == u'(' || unit == u')' || unit == u'\\') {
            resource.push_back(u'\\');
        }

        resource.push_back(unit);
    }

    resource += u",update=0.02s]";
    auto owner = api::FeedFilterIpfRuntime::create(*bootstrap, transfer());
    auto source = owner->start(owner->context(), resource, {});
    REQUIRE(source->awaitInitialSnapshot(Clock::now() + 2s));
    auto original = build(owner->context(), resource);
    auto tracker = original.getTracker();
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << body("MSFT");
        REQUIRE(output.good());
    }
    const auto token = tracker.addListener([&](api::FeedFilterTracker) {
        source->close();
    });
    REQUIRE(until([&] {
        return source->observation().retired;
    }));
    CHECK(accepts(original, u"AAPL"));
    CHECK(accepts(tracker.getCurrentFilter(), u"MSFT"));
    CHECK(source->observation().attempts == 2);
    CHECK(tracker.removeListener(token));
    owner->closeAndAwaitTermination();
    owner.reset();
    std::atomic<bool> retiredReady{false};
    std::atomic<bool> retiredThrew{false};
    std::jthread waiter([&] {
        try {
            retiredReady = source->awaitInitialSnapshot(Clock::now() + 1s);
        } catch (...) {
            retiredThrew = true;
        }
    });
    waiter.join();
    CHECK(retiredReady.load());
    CHECK_FALSE(retiredThrew.load());
}

TEST_CASE("Automatic schedule identity compares the global hours set independently of profile association") {
    auto bootstrap = q::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    api::FeedFilterIpfSchedule schedule;
    schedule.currentTimeMillis = [] {
        return std::int64_t{0};
    };
    schedule.isTrading = [](std::u16string_view, std::int64_t) {
        return false;
    };
    auto owner = api::FeedFilterIpfRuntime::create(*bootstrap, transfer(), {}, schedule);
    const auto initial = std::string(HEADER) + "STOCK,AAPL,A\nSTOCK,MSFT,B\n";
    const auto association = std::string(HEADER) + "STOCK,AAPL,B\nSTOCK,MSFT,A\n";
    const auto duplicates = std::string(HEADER) + "STOCK,MSFT,A\nSTOCK,AAPL,B\nSTOCK,AAPL,B\n";
    const auto changed = std::string(HEADER) + "STOCK,AAPL,C\nSTOCK,MSFT,B\n";
    q::test::IpfHttpPeer peer({reply(initial), reply(association), reply(duplicates), reply(changed)});
    const auto resource = expression(peer.port(), ",update=0.01s,schedule");
    auto source = owner->start(owner->context(), resource, {});
    REQUIRE(source->awaitInitialSnapshot(Clock::now() + 2s));
    auto original = build(owner->context(), resource);
    auto tracker = original.getTracker();
    std::atomic<unsigned> calls{0};
    const auto token = tracker.addListener([&](api::FeedFilterTracker current) {
        CHECK(accepts(current.getCurrentFilter(), u"AAPL"));
        CHECK(accepts(current.getCurrentFilter(), u"MSFT"));
        ++calls;
        source->close();
    });
    REQUIRE(until([&] {
        return source->observation().retired;
    }));
    CHECK(source->observation().attempts == 4);
    CHECK(calls.load() == 1);
    CHECK(accepts(original, u"AAPL"));
    CHECK(accepts(original, u"MSFT"));
    CHECK(tracker.removeListener(token));
    owner->closeAndAwaitTermination();
}
