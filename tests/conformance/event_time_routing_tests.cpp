// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "event_time_fixture.hpp"
#include "receiving_profile.hpp"

#include <doctest/doctest.h>

#include <limits>

namespace routing_allocations {
/** Arm the isolated allocation-call witness after fixture/profile setup. */
void begin() noexcept;
/** Disarm before any assertion/diagnostic allocation.
 * @return Allocation calls made by the selected routing operation.
 */
std::size_t finish() noexcept;
} // namespace routing_allocations

namespace {
namespace q = org::ttldtor::qdcxx;
namespace detail = q::api::detail;
namespace f = event_time_fixture;
std::filesystem::path fixture; ///< Explicit immutable original Java oracle, selected by CTest.

/** Prepare the actual caller subset under the unchanged finite receiving ceilings, without native activation.
 * @param scenario Exact generated logical fields and aliases.
 * @param regional Include Q/z regional registrations; false selects only the composite Quote and Profile.
 * @return Independently owned immutable receiving plan or a diagnosed test failure.
 */
detail::ReceivingProfile profile(const f::Scenario &scenario, bool regional) {
    auto schema = q::core::LocalSchema::create(scenario.records, {16, 128, 256, 128, 16384},
                                               q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    std::vector routes{q::api::EventRegistration{q::api::EventType::QUOTE, 0},
                       q::api::EventRegistration{q::api::EventType::PROFILE, 3}};

    if (regional) {
        routes.push_back({q::api::EventType::QUOTE, 1});
        routes.push_back({q::api::EventType::QUOTE, 2});
    }

    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*schema, routes, {}, work);
    REQUIRE(registry);
    q::api::EndpointOptions options;
    options.maximumAttempts = 1;
    options.eventRegistry = *registry;
    q::api::RuntimeOptions runtime;
    runtime.maximumSubscriptionsPerEndpoint = 1;
    runtime.maximumSymbolsPerSubscription = 5;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});

    return detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);
}
} // namespace

TEST_CASE("routing exactly matches actual production delegate-set regional selection and composite fallback") {
    const auto corpus = f::read(fixture);
    const auto &scenario = corpus.at("default");
    REQUIRE(scenario.selections.size() == 14);
    auto regional = profile(scenario, true);
    auto fallback = profile(scenario, false);

    for (const auto &row : scenario.selections) {
        const auto &selected = row[0] == "0" ? regional : fallback;
        const auto &plan = detail::receivingEventPlan(selected, detail::EventSelection::QUOTE);
        std::array symbols{detail::ReceivingSymbol{f::units(row[1])}};
        detail::prepareReceivingSymbols(selected, plan, symbols);
        std::size_t matches = 0;

        for (std::size_t offset = 0; offset < plan.bindingCount; ++offset) {
            const auto index = plan.bindingIndices[offset];

            if (detail::receivingSymbolMatches(selected, index, symbols[0])) {
                const auto id = selected.bindings[index].localId;
                CHECK(selected.schema.records()[static_cast<std::size_t>(id)].name ==
                      std::u16string(row[2].begin(), row[2].end()));
                const auto &physical = selected.bindingExchanges[index] ? symbols[0].physical : symbols[0].logical;
                CHECK(physical == f::units(row[3]));
                ++matches;
            }
        }

        CHECK(matches == 1);
    }

    const auto &mixed = detail::receivingEventPlan(regional, detail::EventSelection::BOTH);
    std::array symbols{detail::ReceivingSymbol{u"AAPL&Q{x=y}"}};
    detail::prepareReceivingSymbols(regional, mixed, symbols);
    CHECK(symbols[0].logical == u"AAPL&Q{x=y}");
    CHECK(symbols[0].physical == u"AAPL{x=y}");
    CHECK(symbols[0].regional[0]);
    CHECK_FALSE(symbols[0].regional[1]);
    CHECK_FALSE(detail::receivingSymbolMatches(regional, 0, symbols[0]));
    CHECK(detail::receivingSymbolMatches(regional, 1, symbols[0]));
    CHECK_FALSE(detail::receivingSymbolMatches(regional, 2, symbols[0]));
    CHECK(detail::receivingSymbolMatches(regional, 3, symbols[0]));
}

TEST_CASE("routing workspace is separate finite storage with an attainable exact work grant") {
    const auto corpus = f::read(fixture);
    auto selected = profile(corpus.at("millis"), true);
    const auto &limits = selected.routing;
    CHECK(selected.bindingExchanges.size() == detail::MAXIMUM_RECEIVING_BINDINGS);
    CHECK(selected.eventPlans.size() == 4);
    CHECK(limits.retainedMetadataBytes ==
          sizeof(selected.bindingExchanges) + sizeof(selected.bindingSources) + sizeof(selected.bindingTimeEncodings) +
              sizeof(selected.versionBindings) + sizeof(selected.snapshotRetransmitRecords) +
              selected.eventPlans.capacity() * sizeof(detail::ReceivingEventPlan) +
              sizeof(std::array<q::client::PreparedSubscriptionBinding, detail::MAXIMUM_RECEIVING_BINDINGS>));
    CHECK(limits.symbols == 5);
    CHECK(limits.temporaryTextUnits == 2 * 5 * 128);
    CHECK(limits.temporaryBytes >= 5 * sizeof(detail::ReceivingSymbol));
    CHECK(selected.clientLimits.commandSymbolUnits == selected.clientLimits.commandEntries * 129);
    auto fresh = selected.submitWork;
    REQUIRE(detail::prepayReceivingRouting(selected, fresh));
    CHECK(fresh.operations == selected.submitWork.operations);
    CHECK(fresh.bytes < selected.submitWork.bytes);

    for (const auto coordinate : {0, 1, 2}) {
        auto exact = limits.work;
        exact.operations = 1;
        auto below = exact;

        if (coordinate == 0) {
            --below.visits;
        } else if (coordinate == 1) {
            --below.textUnits;
        } else {
            --below.bytes;
        }

        const auto original = below;
        CHECK_FALSE(detail::prepayReceivingRouting(selected, below));
        CHECK(below.visits == original.visits);
        CHECK(below.textUnits == original.textUnits);
        CHECK(below.bytes == original.bytes);
        CHECK(detail::prepayReceivingRouting(selected, exact));
        CHECK(exact.visits == 0);
        CHECK(exact.textUnits == 0);
        CHECK(exact.bytes == 0);
        CHECK(exact.operations == 1);
    }

    q::api::RuntimeOptions huge;
    huge.maximumSymbolsPerSubscription = std::numeric_limits<std::size_t>::max();
    q::api::EndpointOptions options;

    try {
        static_cast<void>(detail::prepareReceivingProfile(huge, options, "127.0.0.1", 12345));
        FAIL("Unrepresentable capacity must refuse before allocating a routing workspace");
    } catch (const q::api::Error &error) {
        CHECK(error.diagnostic().code == q::api::ErrorCode::RESOURCE_LIMIT);
        CHECK(error.diagnostic().stage == "profile-arithmetic");
    }
}

TEST_CASE("long attributed regional physical text uses one allocation instead of a growing prefix") {
    const auto corpus = f::read(fixture);
    auto selected = profile(corpus.at("default"), true);
    const auto &plan = detail::receivingEventPlan(selected, detail::EventSelection::QUOTE);
    const auto logical = std::u16string(80, u'A') + u"&Q{" + std::u16string(44, u'x') + u"}";
    REQUIRE(logical.size() == 128);
    std::array symbols{detail::ReceivingSymbol{logical}};
    routing_allocations::begin();

    try {
        detail::prepareReceivingSymbols(selected, plan, symbols);
    } catch (...) {
        static_cast<void>(routing_allocations::finish());

        throw;
    }

    const auto allocations = routing_allocations::finish();
    CHECK(symbols[0].physical.size() == 126);
    CHECK(allocations == 1);
    CHECK(symbols[0].logical == logical);
}

TEST_CASE("regional exact representability matches actual production delegate roundtrips") {
    const auto rows = f::representations(fixture.parent_path() / "representability.tsv");

    for (const auto &row : rows) {
        CHECK(detail::regionalSymbolRepresentable(f::units(row[1])) == (row[4] == "true"));
    }
}

/** Run only the explicitly selected production fixture.
 * @param argc Program name, fixture filename and optional doctest arguments.
 * @param argv Borrowed process arguments, retained only through test execution.
 * @return Native doctest result, or usage exit2 if no fixture is supplied.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    fixture = argv[1];
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
