// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../src/core/shared_record_block.hpp"
#include "../../src/protocol/local_event_owner.hpp"
#include "qd/events/quote.hpp"
#include "qd/mux/striped.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace m = org::ttldtor::qdcxx::mux;
thread_local bool allocationProbeEnabled = false; ///< Test-only ordinary allocator probe, scoped to one owner call.
thread_local std::optional<std::size_t>
    successfulAllocations;                      ///< Explicit remaining requests before injected bad_alloc.
thread_local std::size_t probedAllocations = 0; ///< Actual observed requests in the current finite call.

/** Test-local fault scope; no product hooks or process/environment settings change. */
class AllocationProbe final {
    public:
    /** @param permitted Successful requests before refusal; absent measures the exact successful call. */
    explicit AllocationProbe(std::optional<std::size_t> permitted) noexcept {
        successfulAllocations = permitted;
        probedAllocations = 0;
        allocationProbeEnabled = true;
    }

    /** Restore normal allocation before any assertion, callback or test cleanup. */
    ~AllocationProbe() {
        allocationProbeEnabled = false;
    }
};

constexpr std::size_t MAX_TEST_STEPS = 10000; ///< Finite test failure deadline, not a production capacity.

/** @return Finite child owner turn from the existing qualified queue profile. */
m::QueueWork childTurn() {
    return {100000,
            1000000000,
            1000000000,
            1000000000,
            1000000000,
            10000,
            10000,
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000}};
}

/** @return Finite root, child, submit and event work for one test turn. */
m::StripedWork turn() {
    return {100000,
            100000000,
            100000000,
            1000000000,
            childTurn(),
            {10000, 1000000, 1000000, 10000000},
            {10000, 10000000, 10000000, 100000000}};
}

/** @return Explicit independent root/control and one-operation grants. */
m::StripedLimits rootLimits() {
    return {16 * 1024 * 1024, 8 * 1024 * 1024, 1024 * 1024, 1024 * 1024, 4 * 1024 * 1024};
}

/** @return Existing desired-state capacity profile. */
c::SubscriptionLimits desiredLimits() {
    return {16, 32, 128, 4096, 64, 8192, 64};
}

/** @return Existing independently enforced multi-consumer demand profile. */
m::MuxLimits muxLimits() {
    return {16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024};
}

/** @return Existing independent detached queue profile. */
m::QueueLimits queueLimits() {
    return {4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024};
}

/** @return Explicit per-consumer reservations, independent for each stripe. */
m::ClientLimits clientLimits() {
    return {32, 512 * 1024, 8, 128 * 1024, 4, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096};
}

/** @return Pure local test source epoch capacities; no network work. */
p::LiveLimits sourceLimits() {
    p::LiveLimits result{};
    result.residentBytes = 16 * 1024 * 1024;
    result.owners = 128;
    result.eventBytes = 256 * 1024;

    return result;
}

/** @return Exact indexed synthetic schema used by the pinned collector production oracle. */
c::LocalSchema schema() {
    const c::LocalRecord record{0,
                                u"Oracle#ONE",
                                true,
                                {{8}, {8}, {8}},
                                {{u"TimeHigh", 0}, {u"TimeLow", 1}, {u"Value", 2}},
                                c::IndexedTimePairInt32{0, 1}};
    auto result =
        c::LocalSchema::create(std::span(&record, 1), {16, 128, 128, 128, 4096}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** @param name Fixed test partition.
 * @return Owned production partition with explicit finite grants.
 */
c::SymbolStriper partition(std::u16string_view name = u"byhash4") {
    auto result = c::SymbolStriper::parse(name, {1024, 1024, 1024, 65536});
    REQUIRE(result);

    return std::move(*result);
}

/** Existing multi-consumer callback records exact values and can retain immutable delivery. */
struct Recorder final : m::MuxConsumer {
    std::vector<std::pair<std::u16string, std::int64_t>> values;
    std::vector<m::OwnedMuxBatch> retained;
    std::function<void()> action;
    bool retain = false;
    bool pause = false;

    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        for (const auto &record : batch.records()) {
            if (record.source && record.typedValues.size() >= 3) {
                values.emplace_back(record.symbol, record.typedValues[2].raw);
            }
        }

        if (retain) {
            retained.push_back(batch.retain());
        }

        if (action) {
            action();
        }

        return pause ? m::BatchAction::PAUSE : m::BatchAction::CONTINUE;
    }
};

/** @param root Actual composition.
 * @param target Expected current operation barrier.
 * @return Last truthful borrowed operation cut.
 */
m::StripedProgress complete(m::StripedCollector &root, m::StripePhase target = m::StripePhase::COMPLETE) {
    for (std::size_t step = 0; step < MAX_TEST_STEPS; ++step) {
        auto work = turn();
        const auto result = root.advance(work);
        REQUIRE(result);
        INFO("phase=", static_cast<int>(result->phase),
             " child=", result->failure && result->failure->stripe ? *result->failure->stripe : 999);

        if (result->phase == target) {
            return *result;
        }

        REQUIRE(result->phase != m::StripePhase::FAILED);
    }

    INFO("last operation= ", static_cast<int>(root.progress().operation),
         " phase= ", static_cast<int>(root.progress().phase), " complete= ", root.progress().completedStripes, "/",
         root.progress().requiredStripes);
    FAIL("striped operation did not reach its real barrier");

    return root.progress();
}

/** @param root Actual composition.
 * @param steps Fixed number of bounded fair turns.
 */
void pump(m::StripedCollector &root, std::size_t steps = 256) {
    for (std::size_t step = 0; step < steps; ++step) {
        auto work = turn();
        auto result = root.advance(work);
        REQUIRE(result);
    }
}

/** @param family Actual desired family.
 * @param catalog Immutable provenance.
 * @param striper Existing partition.
 * @return Real per-stripe MuxQueues and their native family collectors.
 */
m::StripedCollector create(c::SubscriptionFamily family, const c::LocalSchema &catalog, c::SymbolStriper striper) {
    auto work = turn();
    std::expected<m::StripedCollector, m::StripeFailure> result =
        std::unexpected(m::StripeFailure{m::StripeError::INVALID_STATE});

    if (family == c::SubscriptionFamily::STREAM) {
        std::vector<m::StreamStripeOptions> options(striper.stripeCount(),
                                                    {muxLimits(),
                                                     desiredLimits(),
                                                     {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
                                                     queueLimits(),
                                                     c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1});
        result = m::StripedCollector::createStream(catalog, striper, options, rootLimits(), work);
    } else if (family == c::SubscriptionFamily::TICKER) {
        std::vector<m::TickerStripeOptions> options(striper.stripeCount(),
                                                    {muxLimits(),
                                                     {4, 32, 64, 4 * 1024 * 1024, 16 * 1024 * 1024, desiredLimits()},
                                                     {false, 100},
                                                     queueLimits()});
        result = m::StripedCollector::createTicker(catalog, striper, options, rootLimits(), work);
    } else {
        std::vector<m::HistoryStripeOptions> options(
            striper.stripeCount(),
            {muxLimits(),
             {32, 256, 128, 64, 4 * 1024 * 1024, 4, 128, 64, 4 * 1024 * 1024, 32 * 1024 * 1024, desiredLimits()},
             {false, 100, 10000},
             queueLimits()});
        result = m::StripedCollector::createHistory(catalog, striper, options, rootLimits(), work);
    }

    REQUIRE(result);

    return std::move(*result);
}

/** @param root Actual graph.
 * @param consumer Existing detached queue consumer.
 * @param filter Optional static leaf.
 * @return Complete logical client after every required child registration.
 */
m::StripeClient open(m::StripedCollector &root, const std::shared_ptr<Recorder> &consumer,
                     std::optional<c::SymbolStripeFilter> filter = {}) {
    std::vector<m::ClientLimits> limits(root.stripeCount(), clientLimits());
    auto work = turn();
    REQUIRE(root.beginOpen(limits, consumer, std::move(filter), work));
    complete(root);
    auto result = root.takeClient();
    REQUIRE(result);

    return std::move(*result);
}

/** @param catalog Exact schema.
 * @param family Existing family.
 * @param symbols Exact borrowed identities.
 * @return Ordinary or raw-bound HISTORY requests.
 */
std::vector<c::SubscriptionRequest> requests(const c::LocalSchema &catalog, c::SubscriptionFamily family,
                                             std::span<const std::u16string_view> symbols) {
    std::vector<c::SubscriptionRequest> result;

    for (const auto symbol : symbols) {
        result.push_back({{catalog.recordContext(0), symbol},
                          family == c::SubscriptionFamily::HISTORY ? std::optional<std::int64_t>{0} : std::nullopt});
    }

    return result;
}

/** @param catalog Exact provenance.
 * @param owner Existing pure local epoch.
 * @param family Actual data family.
 * @param symbols Exact input identities.
 * @param firstValue First deterministic scalar, incremented per input occurrence.
 * @return Complete existing-family event; source indices preserve the input order.
 */
p::LiveEvent event(const c::LocalSchema &catalog, const p::detail::LocalEventOwner &owner, c::SubscriptionFamily family,
                   std::span<const std::u16string_view> symbols, std::int64_t firstValue = 101,
                   bool completeHistory = false) {
    std::vector<std::array<c::FieldValueView, 3>> values(symbols.size());
    std::vector<p::detail::LocalStreamRow> rows;
    rows.reserve(symbols.size());

    for (std::size_t index = 0; index < symbols.size(); ++index) {
        values[index] = {
            {{c::SerialFieldType::COMPACT_INT, 0, {}, {}},
             {c::SerialFieldType::COMPACT_INT, completeHistory ? 0 : static_cast<std::int64_t>(index + 10), {}, {}},
             {c::SerialFieldType::COMPACT_INT, firstValue + static_cast<std::int64_t>(index), {}, {}}}};
        rows.push_back({0, catalog.recordContext(0), symbols[index], values[index], {}, completeHistory ? 12 : 0, {}});
    }

    auto work = turn();
    const auto type = family == c::SubscriptionFamily::STREAM   ? p::LiveDataMessage::STREAM_DATA
                      : family == c::SubscriptionFamily::TICKER ? p::LiveDataMessage::TICKER_DATA
                                                                : p::LiveDataMessage::HISTORY_DATA;
    auto result = owner.stream(rows, work.event, true, type);
    REQUIRE(result);

    return std::move(*result);
}

/** @param recorder Existing owning observation.
 * @return Sorted actual source-value pairs, independent of permitted cross-stripe scheduling.
 */
auto sorted(const Recorder &recorder) {
    auto values = recorder.values;
    std::ranges::sort(values);

    return values;
}

/** @param root Actual graph.
 * @param mode Existing honest stop policy.
 */
void close(m::StripedCollector &root, m::StopMode mode = m::StopMode::GRACEFUL) {
    root.requestStop(mode);

    for (std::size_t step = 0; step < MAX_TEST_STEPS; ++step) {
        auto work = turn();
        const auto result = root.advance(work);
        REQUIRE(result);

        if (result->closed) {
            return;
        }
    }

    FAIL("not every child reached its real terminal barrier");
}

/** Read exact source/value pairs from the production collector fixture.
 * @param family Actual native family.
 * @param spec Physical Java partition spelling.
 * @param phase Actual Java retrieve phase.
 * @return Sorted owning observations, ignoring permitted cross-stripe callback order.
 */
auto javaValues(c::SubscriptionFamily family, std::u16string_view spec, std::string_view phase) {
    std::string name = family == c::SubscriptionFamily::STREAM   ? "Stream:"
                       : family == c::SubscriptionFamily::TICKER ? "Ticker:"
                                                                 : "History:";

    for (const auto unit : spec) {
        name.push_back(static_cast<char>(unit));
    }

    std::ifstream fixture(ORG_TTLDTOR_QDCXX_STRIPE_COLLECTOR_FIXTURE);
    REQUIRE(fixture.good());
    std::vector<std::pair<std::u16string, std::int64_t>> result;
    std::string line;

    while (std::getline(fixture, line)) {
        std::istringstream input(line);
        std::vector<std::string> cells;

        for (std::string cell; std::getline(input, cell, '\t');) {
            cells.push_back(std::move(cell));
        }

        if (cells.size() == 8 && cells[0] == "record" && cells[1] == name && cells[2] == phase) {
            std::u16string symbol;

            for (std::size_t index = 0; index < cells[4].size(); index += 5) {
                symbol.push_back(static_cast<char16_t>(std::stoul(cells[4].substr(index, 4), nullptr, 16)));
            }

            result.emplace_back(std::move(symbol), std::stoll(cells[6]));
        }
    }

    REQUIRE_FALSE(result.empty());
    std::ranges::sort(result);

    return result;
}
} // namespace

/** Replace ordinary allocation only for the isolated explicit test fault scope.
 * @param bytes Requested bytes.
 * @return Ordinary storage or the actual std::bad_alloc boundary.
 */
void *operator new(std::size_t bytes) {
    if (allocationProbeEnabled) {
        ++probedAllocations;

        if (successfulAllocations) {
            if (*successfulAllocations == 0) {
                throw std::bad_alloc{};
            }

            --*successfulAllocations;
        }
    }

    if (auto *pointer = std::malloc(bytes == 0 ? 1 : bytes)) {
        return pointer;
    }

    throw std::bad_alloc{};
}

/** @param pointer Previously allocated replacement storage. */
void operator delete(void *pointer) noexcept {
    std::free(pointer);
}

/** @param pointer Previously allocated replacement storage.
 * @param bytes Original size, unused by free.
 */
void operator delete(void *pointer, std::size_t bytes) noexcept {
    static_cast<void>(bytes);
    std::free(pointer);
}

/** @param bytes Requested array backing.
 * @return Ordinary storage in the same isolated fault scope.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** @param pointer Previously allocated array storage. */
void operator delete[](void *pointer) noexcept {
    ::operator delete(pointer);
}

/** @param pointer Previously allocated array storage.
 * @param bytes Original size.
 */
void operator delete[](void *pointer, std::size_t bytes) noexcept {
    ::operator delete(pointer, bytes);
}

TEST_CASE("actual striped MUX preserves independent multi-consumer demand delivery and close in all native families") {
    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        for (const auto name : {u"by1", u"byhash4", u"byrange-D-M-"}) {
            INFO("family= ", static_cast<int>(family));
            auto catalog = schema();
            auto root = create(family, catalog, partition(name));
            auto first = std::make_shared<Recorder>();
            auto second = std::make_shared<Recorder>();
            const auto a = open(root, first);
            const auto b = open(root, second);
            const std::array<std::u16string_view, 3> firstSymbols{u"AAPL", u"IBM", u"MSFT"};
            const std::array<std::u16string_view, 2> secondSymbols{u"IBM", u"SPY"};
            auto one = requests(catalog, family, firstSymbols);
            auto two = requests(catalog, family, secondSymbols);
            auto work = turn();
            REQUIRE(root.beginReplace(a, one, work));
            complete(root);
            work = turn();
            REQUIRE(root.beginReplace(b, two, work));
            complete(root);
            work = turn();
            auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
            REQUIRE(owner);
            const std::array<std::u16string_view, 5> input{u"AAPL", u"IBM", u"MSFT", u"SPY", u"Z"};
            auto source = event(catalog, *owner, family, input);
            work = turn();
            REQUIRE(root.beginStage(std::move(source), work));
            const auto admitted = complete(root);
            CHECK(admitted.completedStripes == root.stripeCount());
            pump(root);
            CHECK(sorted(*first) == javaValues(family, name, "first"));
            CHECK(sorted(*second) == javaValues(family, name, "second"));
            close(root);
            work = turn();
            const auto observations = root.observations(work);
            REQUIRE(observations);
            REQUIRE(observations->size() == root.stripeCount());
            std::size_t accepted = 0;

            for (const auto &child : *observations) {
                REQUIRE(child.terminal);
                INFO("terminal stripe=", child.stripe, " gap=", child.terminal->hadGap,
                     " unknown=", child.terminal->unknownEligibility,
                     " failures=", child.terminal->failedRegistrationCount,
                     " admitted=", child.terminal->counts.queueAdmitted,
                     " discarded=", child.terminal->counts.discarded,
                     " sourceDiscarded=", child.terminal->counts.sourceRecordsDiscarded,
                     " withdrawn=", child.terminal->counts.withdrawn);
                const bool incompleteHistory =
                    family == c::SubscriptionFamily::HISTORY && child.terminal->counts.queueAdmitted != 0;
                CHECK(child.terminal->outcome ==
                      (incompleteHistory ? m::TerminalOutcome::ABORTED : m::TerminalOutcome::DRAINED));
                CHECK(child.terminal->hadGap == incompleteHistory);
                accepted += child.acceptedSourceRecords;
            }

            CHECK(accepted == input.size());
        }
    }
}

TEST_CASE("striped command cancellation exposes completed prefix without rolling back or repeating child tickets") {
    auto catalog = schema();
    auto root = create(c::SubscriptionFamily::STREAM, catalog, partition(u"byrange-D-M-"));
    auto recorder = std::make_shared<Recorder>();
    const auto client = open(root, recorder);
    const std::array<std::u16string_view, 3> symbols{u"A", u"IBM", u"MSFT"};
    const auto desired = requests(catalog, c::SubscriptionFamily::STREAM, symbols);
    auto work = turn();
    auto operation = root.beginReplace(client, desired, work);
    REQUIRE(operation);

    for (std::size_t step = 0; step < MAX_TEST_STEPS && root.progress().completedStripes == 0; ++step) {
        work = turn();
        REQUIRE(root.advance(work));
    }

    REQUIRE(root.progress().completedStripes == 1);

    const auto firstTicket = root.progress().children[0].command->id;
    CHECK(root.aggregateEntries(0).size() == 1);
    REQUIRE(root.requestCancel(*operation));
    const auto cancelled = complete(root, m::StripePhase::CANCELLED);
    CHECK(cancelled.completedStripes == 1);
    CHECK(cancelled.children[0].command->id == firstTicket);
    CHECK_FALSE(cancelled.children[1].submitted);
    CHECK_FALSE(cancelled.children[2].submitted);
    CHECK(root.aggregateEntries(0).size() == 1);
    CHECK(root.aggregateEntries(1).empty());
    CHECK(root.aggregateEntries(2).empty());
    close(root);
}

TEST_CASE("healthy indexed snapshots and native observation boundaries close every striped child cleanly") {
    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        INFO("family=", static_cast<int>(family));
        auto catalog = schema();
        auto root = create(family, catalog, partition(u"byrange-D-M-"));
        auto receiver = std::make_shared<Recorder>();
        auto client = open(root, receiver);
        const std::array<std::u16string_view, 3> symbols{u"AAPL", u"IBM", u"MSFT"};
        auto desired = requests(catalog, family, symbols);
        auto work = turn();
        REQUIRE(root.beginReplace(client, desired, work));
        complete(root);
        pump(root);
        work = turn();
        auto before = root.summary(work);
        REQUIRE(before);
        CHECK(before->desiredEntries == 3);
        CHECK_FALSE(before->finalCounts);
        auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
        REQUIRE(owner);
        auto source = event(catalog, *owner, family, symbols, 101, family == c::SubscriptionFamily::HISTORY);
        work = turn();
        REQUIRE(root.beginStage(std::move(source), work));
        complete(root);
        pump(root);
        CHECK(receiver->values.size() == 3);
        work = turn();
        auto ranges = root.rangeSnapshot(client, work);

        if (family == c::SubscriptionFamily::HISTORY) {
            REQUIRE(ranges);
            REQUIRE(ranges->size() == 3);

            for (const auto &row : *ranges) {
                CHECK(row.value.status.mappingCurrent);
                CHECK(row.value.status.completeDeliveredRange);
                CHECK(row.value.status.queuedForKey == 0);
                CHECK(row.value.status.clientDesiredRevision != 0);
                work = turn();
                auto retained =
                    root.retainedHistoryRange(client, {{row.value.context, row.value.symbol}, 0, 0, 1}, work);
                REQUIRE(retained);
                REQUIRE(retained->rows.size() == 1);
                CHECK(retained->rows[0].record.typedValues[2].raw >= 101);
            }
        } else {
            REQUIRE_FALSE(ranges);
            CHECK(ranges.error().error == m::StripeError::UNSUPPORTED_FAMILY);
        }

        std::int64_t latest = 0;
        work = turn();
        const auto visited = root.visitTickerLatest(
            client, {catalog.recordContext(0), u"IBM"}, work, &latest,
            [](void *context, const org::ttldtor::qdcxx::collectors::TickerValue &value) noexcept {
                *static_cast<std::int64_t *>(context) = value.sourceRecord->typedValues[2].raw;
            });

        if (family == c::SubscriptionFamily::TICKER) {
            REQUIRE(visited);
            CHECK(latest == 102);
        } else {
            REQUIRE_FALSE(visited);
            CHECK(visited.error().error == m::StripeError::UNSUPPORTED_FAMILY);
        }

        close(root);
        work = turn();
        auto observation = root.observations(work);
        REQUIRE(observation);

        for (const auto &child : *observation) {
            REQUIRE(child.terminal);
            CHECK(child.terminal->outcome == m::TerminalOutcome::DRAINED);
            CHECK_FALSE(child.terminal->hadGap);
        }

        work = turn();
        auto final = root.summary(work);
        REQUIRE(final);
        REQUIRE(final->finalCounts);
        CHECK(final->terminalStripes == 3);
        CHECK(final->acceptedSourceRecords == 3);
        CHECK(final->finalCounts->callbackCompletedRecords == 3);
        CHECK(final->finalCounts->discarded == 0);
    }
}

TEST_CASE("real Java stripe hint selects child queues while All still uses the independent actual filter boundary") {
    // Pinned production fixture records these exact representatives and both distinct outcomes.
    std::ifstream fixture(ORG_TTLDTOR_QDCXX_STRIPE_COLLECTOR_FIXTURE);
    REQUIRE(fixture.good());
    std::string line;
    std::size_t hintRows = 0, filteredRows = 0;

    while (std::getline(fixture, line)) {
        if (line.starts_with("record\tbyhash4\thint-only\t")) {
            ++hintRows;
        }

        if (line.starts_with("record\tbyhash4\thint-and-filter\t")) {
            ++filteredRows;
        }
    }

    REQUIRE(hintRows == 2);
    REQUIRE(filteredRows == 1);
    auto catalog = schema();
    auto root = create(c::SubscriptionFamily::STREAM, catalog, partition());
    auto receiver = std::make_shared<Recorder>();
    auto leaf = c::SymbolStripeFilter::parse(u"hash0of8", {1024, 1024, 1024, 65536});
    REQUIRE(leaf);
    auto client = open(root, receiver, *leaf);
    CHECK(client.stripeCount() == 1);
    const c::SubscriptionRequest all{c::allSymbols(catalog.recordContext(0)), {}};
    auto work = turn();
    REQUIRE(root.beginAdd(client, std::span(&all, 1), work));
    complete(root);
    pump(root);
    work = turn();
    auto summary = root.summary(work);
    REQUIRE(summary);
    CHECK(summary->desiredEntries == 1);
    const std::array<std::u16string_view, 2> symbols{u"SYM2", u"SYM5"};
    auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
    REQUIRE(owner);
    auto source = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols, 301);
    work = turn();
    REQUIRE(root.beginStage(std::move(source), work));
    complete(root);
    pump(root);
    CHECK(sorted(*receiver) == std::vector<std::pair<std::u16string, std::int64_t>>{{u"SYM2", 301}, {u"SYM5", 302}});
    close(root);
}

TEST_CASE("each physical stripe reconnect preserves desired demand and exact local ACK across all three families") {
    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        INFO("family=", static_cast<int>(family));
        auto catalog = schema();
        auto root = create(family, catalog, partition(u"byrange-D-M-"));
        auto receiver = std::make_shared<Recorder>();
        receiver->retain = true;
        auto client = open(root, receiver);
        const std::array<std::u16string_view, 3> symbols{u"AAPL", u"IBM", u"MSFT"};
        auto desired = requests(catalog, family, symbols);
        auto work = turn();
        REQUIRE(root.beginReplace(client, desired, work));
        complete(root);
        pump(root);
        auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
        REQUIRE(owner);
        auto source = event(catalog, *owner, family, symbols, 101, family == c::SubscriptionFamily::HISTORY);
        work = turn();
        REQUIRE(root.beginStage(std::move(source), work));
        complete(root);
        pump(root);
        REQUIRE(receiver->values.size() == 3);
        REQUIRE_FALSE(receiver->retained.empty());
        const auto saved = receiver->retained.front();
        const auto oldValue = saved.records()[0].typedValues[2].raw;
        work = turn();
        REQUIRE(root.beginReset(work));
        complete(root);
        pump(root);
        work = turn();
        auto observations = root.observations(work);
        REQUIRE(observations);

        for (const auto &child : *observations) {
            CHECK(child.reset.sourceCommitted);
            CHECK(child.reset.globalAcknowledged);
            CHECK(child.desiredEntries == 1);
        }

        std::vector<std::uint64_t> notices(root.stripeCount());

        for (std::size_t stripe = 0; stripe < root.stripeCount(); ++stripe) {
            const auto notice = client.resetNotice(stripe);
            REQUIRE(notice);
            REQUIRE(notice->toEpoch);
            CHECK_FALSE(notice->fromEpoch.matches(*notice->toEpoch));
            CHECK(notice->unknownUpstreamLoss);
            notices[stripe] = notice->sequence;
            CHECK(client.acknowledgeReset(stripe, notice->sequence + 1) == m::ControlAdmission::STALE);
        }

        auto freshOwner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
        REQUIRE(freshOwner);
        auto fresh = event(catalog, *freshOwner, family, symbols, 201, family == c::SubscriptionFamily::HISTORY);
        work = turn();
        REQUIRE(root.beginStage(std::move(fresh), work));
        complete(root);
        pump(root);
        CHECK(receiver->values.size() == 3);

        for (std::size_t stripe = 0; stripe < root.stripeCount(); ++stripe) {
            CHECK(client.acknowledgeReset(stripe, notices[stripe]) == m::ControlAdmission::RECORDED);
        }

        pump(root);
        CHECK(receiver->values.size() == 6);
        CHECK(saved.records()[0].typedValues[2].raw == oldValue);
        close(root, m::StopMode::ABORT);
        CHECK(saved.records()[0].typedValues[2].raw == oldValue);
    }
}

TEST_CASE(
    "root work refusal keeps source and prior child effects and cancellation reports accepted command truthfully") {
    auto catalog = schema();
    auto root = create(c::SubscriptionFamily::STREAM, catalog, partition(u"byrange-D-M-"));
    auto receiver = std::make_shared<Recorder>();
    auto client = open(root, receiver);
    const std::array<std::u16string_view, 3> symbols{u"AAPL", u"IBM", u"MSFT"};
    auto desired = requests(catalog, c::SubscriptionFamily::STREAM, symbols);
    auto work = turn();
    auto operation = root.beginReplace(client, desired, work);
    REQUIRE(operation);
    work = turn();
    REQUIRE_FALSE(root.beginClear(client, work));
    work = turn();
    work.visits = 1;
    const auto unfunded = root.advance(work);
    REQUIRE_FALSE(unfunded);
    CHECK(unfunded.error().error == m::StripeError::WORK_LIMIT);
    CHECK_FALSE(root.progress().children[0].submitted);
    work = turn();
    REQUIRE(root.advance(work));
    REQUIRE(root.progress().children[0].submitted);
    REQUIRE_FALSE(root.progress().children[0].completed);
    REQUIRE(root.requestCancel(*operation));
    auto cancelled = complete(root, m::StripePhase::CANCELLED);
    REQUIRE(cancelled.children[0].command);
    CHECK(cancelled.children[0].command->status == m::CommandStatus::APPLIED);
    CHECK(cancelled.children[0].completed);
    CHECK(root.aggregateEntries(0).size() == 1);
    CHECK_FALSE(cancelled.children[1].submitted);
    work = turn();
    auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
    REQUIRE(owner);
    auto source = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols);
    work = turn();
    work.bytes = 0;
    const auto refused = root.beginStage(std::move(source), work);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().error == m::StripeError::WORK_LIMIT);
    CHECK(source.records().size() == 3);
    work = turn();
    auto staged = root.beginStage(std::move(source), work);
    REQUIRE(staged);
    REQUIRE(root.requestCancel(*staged));
    complete(root, m::StripePhase::CANCELLED);
    work = turn();
    auto summary = root.summary(work);
    REQUIRE(summary);
    CHECK(summary->unsubmittedSourceRecords == 3);
    CHECK(summary->acceptedSourceRecords == 0);
    close(root, m::StopMode::ABORT);
}

TEST_CASE("callback destruction and replacement retain only the serialized operation lease") {
    for (const bool replace : {false, true}) {
        auto catalog = schema();
        auto root = std::make_unique<m::StripedCollector>(
            create(c::SubscriptionFamily::STREAM, catalog, partition(u"byrange-D-M-")));
        auto receiver = std::make_shared<Recorder>();
        receiver->retain = true;
        auto client = open(*root, receiver);
        const std::array<std::u16string_view, 1> symbols{u"AAPL"};
        auto desired = requests(catalog, c::SubscriptionFamily::STREAM, symbols);
        auto work = turn();
        REQUIRE(root->beginAdd(client, desired, work));
        complete(*root);
        pump(*root);
        auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
        REQUIRE(owner);
        auto source = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols);
        work = turn();
        REQUIRE(root->beginStage(std::move(source), work));
        complete(*root);
        std::optional<m::StripedCollector> replacement;

        if (replace) {
            replacement.emplace(create(c::SubscriptionFamily::STREAM, catalog, partition(u"by1")));
        }

        receiver->action = [&] {
            if (replace) {
                *root = std::move(*replacement);
            } else {
                root.reset();
            }
        };

        for (std::size_t step = 0; step < MAX_TEST_STEPS && receiver->values.empty(); ++step) {
            work = turn();
            REQUIRE(root);
            const auto advanced = root->advance(work);
            REQUIRE(advanced);

            if (!receiver->values.empty()) {
                CHECK(advanced->children.empty());
                CHECK(advanced->closing);
            }
        }

        REQUIRE(receiver->values.size() == 1);
        REQUIRE(receiver->retained.size() == 1);
        CHECK(receiver->retained[0].records()[0].symbol == u"AAPL");

        for (std::size_t stripe = 0; stripe < 3; ++stripe) {
            REQUIRE(client.terminal(stripe));
        }

        if (replace) {
            CHECK(root->stripeCount() == 1);
            close(*root, m::StopMode::ABORT);
        } else
            CHECK_FALSE(root);

        // Retained client data and ports survive on an unrelated thread after the operational root is gone.
        auto retained = std::move(receiver->retained);
        receiver->action = {};
        std::thread release([values = std::move(retained), client]() mutable {
            CHECK(values.front().records()[0].typedValues[2].raw == 101);
            values.clear();
        });
        release.join();
    }
}

TEST_CASE("a single stripe client overflow keeps healthy stripe and independent consumer delivery progressing") {
    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        INFO("family=", static_cast<int>(family));
        auto catalog = schema();
        auto root = create(family, catalog, partition(u"byrange-D-M-"));
        auto limited = std::make_shared<Recorder>();
        limited->retain = true;
        auto healthy = std::make_shared<Recorder>();
        std::vector<m::ClientLimits> perStripe(3, clientLimits());
        perStripe[0].dataItems = 1;
        perStripe[0].batchOwners = 1;
        perStripe[0].batchRecords = 1;
        auto work = turn();
        REQUIRE(root.beginOpen(perStripe, limited, {}, work));
        complete(root);
        auto maybe = root.takeClient();
        REQUIRE(maybe);
        auto small = std::move(*maybe);
        auto peer = open(root, healthy);
        const std::array<std::u16string_view, 3> symbols{u"AAPL", u"IBM", u"MSFT"};
        auto desired = requests(catalog, family, symbols);
        work = turn();
        REQUIRE(root.beginReplace(small, desired, work));
        complete(root);
        work = turn();
        REQUIRE(root.beginReplace(peer, desired, work));
        complete(root);
        pump(root);
        auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
        REQUIRE(owner);
        auto initial = event(catalog, *owner, family, symbols, 101, family == c::SubscriptionFamily::HISTORY);
        work = turn();
        REQUIRE(root.beginStage(std::move(initial), work));
        complete(root);
        pump(root);
        REQUIRE(limited->values.size() == 3);
        REQUIRE(healthy->values.size() == 3);
        auto next = event(catalog, *owner, family, symbols, 201, family == c::SubscriptionFamily::HISTORY);
        work = turn();
        REQUIRE(root.beginStage(std::move(next), work));
        complete(root);
        pump(root);
        CHECK(healthy->values.size() == 6);
        CHECK(limited->values.size() == 5);
        const auto failed = small.terminal(0);
        REQUIRE(failed);
        REQUIRE(failed->primary);
        CHECK(failed->outcome == m::TerminalOutcome::FAILED);
        CHECK(failed->primary->error == m::QueueError::RESOURCE_LIMIT);
        CHECK_FALSE(small.terminal(1));
        CHECK_FALSE(small.terminal(2));
        CHECK(limited->retained.front().records()[0].typedValues[2].raw == 101);
        work = turn();
        auto summary = root.summary(work);
        REQUIRE(summary);
        CHECK(summary->acceptedSourceRecords == 6);
        CHECK_FALSE(summary->finalCounts);
        close(root, m::StopMode::ABORT);
        work = turn();
        summary = root.summary(work);
        REQUIRE(summary);
        REQUIRE(summary->finalCounts);
        CHECK(summary->finalCounts->callbackCompletedRecords == 11);
        CHECK(summary->hadGap);
    }
}

TEST_CASE(
    "per-stripe QueueOptions remain independent and an explicit source refusal preserves exact unsubmitted progress") {
    auto catalog = schema();
    auto striper = partition(u"byrange-D-M-");
    std::vector<m::StreamStripeOptions> options(3, {muxLimits(),
                                                    desiredLimits(),
                                                    {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
                                                    queueLimits(),
                                                    c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1});
    options[0].queue.sourceRecords = 1;
    options[1].queue.sourceRecords = 2;
    options[2].queue.sourceRecords = 3;

    for (auto &option : options) {
        option.queue.routeRecords = 1;
    }

    for (std::size_t stripe = 0; stripe < 3; ++stripe) {
        options[stripe].queue.residentBytes = (16 + stripe) * 1024 * 1024;
    }

    auto work = turn();
    auto created = m::StripedCollector::createStream(catalog, striper, options, rootLimits(), work);
    REQUIRE(created);
    auto root = std::move(*created);
    auto receiver = std::make_shared<Recorder>();
    auto client = open(root, receiver);
    const std::array<std::u16string_view, 4> symbols{u"AAPL", u"AMD", u"IBM", u"MSFT"};
    auto desired = requests(catalog, c::SubscriptionFamily::STREAM, symbols);
    work = turn();
    REQUIRE(root.beginReplace(client, desired, work));
    complete(root);
    pump(root);
    work = turn();
    auto observed = root.observations(work);
    REQUIRE(observed);

    for (std::size_t stripe = 0; stripe < 3; ++stripe) {
        CHECK((*observed)[stripe].capacity.queueBytes == (16 + stripe) * 1024 * 1024);
    }

    auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
    REQUIRE(owner);
    auto source = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols);
    work = turn();
    auto id = root.beginStage(std::move(source), work);
    REQUIRE(id);
    work = turn();
    auto refused = root.advance(work);
    REQUIRE(refused);
    REQUIRE(refused->failure);
    CHECK(refused->phase == m::StripePhase::FAILED);
    CHECK(refused->failure->stripe == 0);
    REQUIRE(refused->children[0].failure);
    REQUIRE(refused->children[0].failure->child);
    CHECK(refused->children[0].failure->child->error == m::QueueError::RESOURCE_LIMIT);
    CHECK_FALSE(refused->children[0].submitted);
    CHECK_FALSE(refused->children[1].submitted);
    REQUIRE(root.requestCancel(*id));
    complete(root, m::StripePhase::CANCELLED);
    const std::array<std::u16string_view, 2> healthy{u"IBM", u"MSFT"};
    auto next = event(catalog, *owner, c::SubscriptionFamily::STREAM, healthy, 201);
    work = turn();
    REQUIRE(root.beginStage(std::move(next), work));
    complete(root);
    pump(root);
    CHECK(receiver->values.size() == 2);
    work = turn();
    auto summary = root.summary(work);
    REQUIRE(summary);
    CHECK(summary->unsubmittedSourceRecords == 4);
    CHECK(summary->acceptedSourceRecords == 2);
    close(root, m::StopMode::ABORT);
}

TEST_CASE("source cancellation never repeats a completed child stage and keeps its already accepted delivery") {
    auto catalog = schema();
    auto root = create(c::SubscriptionFamily::STREAM, catalog, partition(u"byrange-D-M-"));
    auto receiver = std::make_shared<Recorder>();
    auto client = open(root, receiver);
    const std::array<std::u16string_view, 3> symbols{u"AAPL", u"IBM", u"MSFT"};
    auto desired = requests(catalog, c::SubscriptionFamily::STREAM, symbols);
    auto work = turn();
    REQUIRE(root.beginReplace(client, desired, work));
    complete(root);
    pump(root);
    auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
    REQUIRE(owner);
    auto source = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols);
    work = turn();
    auto operation = root.beginStage(std::move(source), work);
    REQUIRE(operation);

    for (std::size_t step = 0; step < MAX_TEST_STEPS && root.progress().completedStripes == 0; ++step) {
        work = turn();
        REQUIRE(root.advance(work));
    }

    REQUIRE(root.progress().completedStripes == 1);
    REQUIRE(root.progress().children[0].stagedRecords == 1);
    REQUIRE(root.requestCancel(*operation));
    const auto cancelled = complete(root, m::StripePhase::CANCELLED);
    CHECK(cancelled.children[0].completed);
    CHECK_FALSE(cancelled.children[1].submitted);
    CHECK_FALSE(cancelled.children[2].submitted);
    pump(root);
    CHECK(receiver->values == std::vector<std::pair<std::u16string, std::int64_t>>{{u"AAPL", 101}});
    work = turn();
    auto summary = root.summary(work);
    REQUIRE(summary);
    CHECK(summary->acceptedSourceRecords == 1);
    CHECK(summary->stagedSourceRecords == 1);
    CHECK(summary->unsubmittedSourceRecords == 2);
    auto fresh = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols, 201);
    work = turn();
    REQUIRE(root.beginStage(std::move(fresh), work));
    complete(root);
    pump(root);
    CHECK(sorted(*receiver) == std::vector<std::pair<std::u16string, std::int64_t>>{
                                   {u"AAPL", 101}, {u"AAPL", 201}, {u"IBM", 202}, {u"MSFT", 203}});
    close(root);
}

TEST_CASE("last public client release aborts its child ports while a current operation retains only metadata") {
    auto catalog = schema();
    auto root = create(c::SubscriptionFamily::STREAM, catalog, partition(u"byrange-D-M-"));
    auto receiver = std::make_shared<Recorder>();
    auto client = open(root, receiver);
    const std::array<std::u16string_view, 3> symbols{u"AAPL", u"IBM", u"MSFT"};
    auto desired = requests(catalog, c::SubscriptionFamily::STREAM, symbols);
    auto work = turn();
    const auto operation = root.beginReplace(client, desired, work);
    REQUIRE(operation);
    work = turn();
    REQUIRE(root.advance(work));
    REQUIRE(root.progress().children[0].submitted);
    client = {};
    REQUIRE(root.requestCancel(*operation));
    const auto cancelled = complete(root, m::StripePhase::CANCELLED);
    REQUIRE(cancelled.children[0].command);
    CHECK(cancelled.children[0].command->status == m::CommandStatus::CANCELLED_CLOSING);
    CHECK_FALSE(cancelled.children[1].submitted);
    pump(root);
    work = turn();
    const auto observation = root.observations(work);
    REQUIRE(observation);

    for (const auto &child : *observation) {
        CHECK(child.desiredEntries == 0);
        CHECK(child.resources.generations == 0);
    }

    CHECK(receiver->values.empty());
    close(root);
}

TEST_CASE("zero root operation capacity rejects reset before accepting any child action") {
    auto catalog = schema();
    auto striper = partition(u"by1");
    const m::StreamStripeOptions child{
        muxLimits(), desiredLimits(), {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, queueLimits()};
    auto limits = rootLimits();
    limits.operationBytes = 0;
    auto work = turn();
    auto root = m::StripedCollector::createStream(catalog, striper, std::span(&child, 1), limits, work);
    REQUIRE(root);
    work = turn();
    auto refused = root->beginReset(work);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().error == m::StripeError::RESOURCE_LIMIT);
    CHECK(root->progress().operationId == 0);
    CHECK(root->progress().phase == m::StripePhase::IDLE);
    close(*root);
}

TEST_CASE("actual allocation refusal at every source preparation request preserves input root and completed demand") {
    auto catalog = schema();
    auto root = create(c::SubscriptionFamily::STREAM, catalog, partition(u"byrange-D-M-"));
    auto receiver = std::make_shared<Recorder>();
    auto client = open(root, receiver);
    const std::array<std::u16string_view, 3> symbols{u"AAPL", u"IBM", u"MSFT"};
    auto desired = requests(catalog, c::SubscriptionFamily::STREAM, symbols);
    auto work = turn();
    REQUIRE(root.beginReplace(client, desired, work));
    complete(root);
    pump(root);
    auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
    REQUIRE(owner);
    auto measured = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols);
    std::expected<std::uint64_t, m::StripeFailure> operation =
        std::unexpected(m::StripeFailure{m::StripeError::INVALID_STATE});
    std::size_t requestsObserved = 0;
    work = turn();
    {
        AllocationProbe probe{std::nullopt};
        operation = root.beginStage(std::move(measured), work);
        requestsObserved = probedAllocations;
    }
    REQUIRE(operation);
    REQUIRE(requestsObserved > 0);
    REQUIRE(root.requestCancel(*operation));
    complete(root, m::StripePhase::CANCELLED);
    const auto previousOperation = root.progress().operationId;
    const auto baseline = root.residentBytes();

    for (std::size_t failAfter = 0; failAfter < requestsObserved; ++failAfter) {
        INFO("source preparation allocation=", failAfter, "/", requestsObserved);
        auto original = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols);
        const auto resident = original.residentBytes();
        work = turn();
        {
            AllocationProbe probe{failAfter};
            operation = root.beginStage(std::move(original), work);
        }
        REQUIRE_FALSE(operation);
        CHECK((operation.error().error == m::StripeError::ALLOCATION_FAILURE ||
               operation.error().event == p::LiveError::ALLOCATION_FAILURE));
        CHECK(original.records().size() == 3);
        CHECK(original.residentBytes() == resident);
        CHECK(root.progress().operationId == previousOperation);
        CHECK(root.residentBytes() == baseline);

        for (std::size_t stripe = 0; stripe < root.stripeCount(); ++stripe) {
            CHECK(root.aggregateEntries(stripe).size() == 1);
        }
    }

    auto fresh = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols);
    work = turn();
    REQUIRE(root.beginStage(std::move(fresh), work));
    complete(root);
    pump(root);
    CHECK(receiver->values.size() == 3);
    close(root);
}

TEST_CASE("STREAM All fans out across every physical child and clearing it preserves an independent exact consumer") {
    auto catalog = schema();
    auto root = create(c::SubscriptionFamily::STREAM, catalog, partition(u"byrange-D-M-"));
    auto wildcard = std::make_shared<Recorder>();
    auto exact = std::make_shared<Recorder>();
    auto allClient = open(root, wildcard);
    auto exactClient = open(root, exact);
    const c::SubscriptionRequest all{c::allSymbols(catalog.recordContext(0)), {}};
    const c::SubscriptionRequest ibm{{catalog.recordContext(0), u"IBM"}, {}};
    auto work = turn();
    REQUIRE(root.beginAdd(allClient, std::span(&all, 1), work));
    complete(root);
    work = turn();
    REQUIRE(root.beginAdd(exactClient, std::span(&ibm, 1), work));
    complete(root);
    pump(root);

    for (std::size_t stripe = 0; stripe < root.stripeCount(); ++stripe) {
        CHECK(std::ranges::any_of(root.aggregateEntries(stripe), [](const auto &entry) {
            return entry.key.selector == c::SubscriptionSelectorKind::ALL;
        }));
    }

    const std::array<std::u16string_view, 3> symbols{u"AAPL", u"IBM", u"MSFT"};
    auto owner = p::detail::LocalEventOwner::create(sourceLimits(), work.event);
    REQUIRE(owner);
    auto first = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols);
    work = turn();
    REQUIRE(root.beginStage(std::move(first), work));
    complete(root);
    pump(root);
    CHECK(sorted(*wildcard) ==
          std::vector<std::pair<std::u16string, std::int64_t>>{{u"AAPL", 101}, {u"IBM", 102}, {u"MSFT", 103}});
    CHECK(exact->values == std::vector<std::pair<std::u16string, std::int64_t>>{{u"IBM", 102}});
    work = turn();
    REQUIRE(root.beginClear(allClient, work));
    complete(root);
    pump(root);
    work = turn();
    auto summary = root.summary(work);
    REQUIRE(summary);
    CHECK(summary->desiredEntries == 1);
    auto second = event(catalog, *owner, c::SubscriptionFamily::STREAM, symbols, 201);
    work = turn();
    REQUIRE(root.beginStage(std::move(second), work));
    complete(root);
    pump(root);
    CHECK(wildcard->values.size() == 3);
    CHECK(exact->values == std::vector<std::pair<std::u16string, std::int64_t>>{{u"IBM", 102}, {u"IBM", 202}});
    close(root);

    for (const auto family : {c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        auto unsupported = create(family, catalog, partition(u"byrange-D-M-"));
        auto sink = std::make_shared<Recorder>();
        auto client = open(unsupported, sink);
        work = turn();
        auto operation = unsupported.beginAdd(client, std::span(&all, 1), work);
        REQUIRE(operation);

        for (std::size_t step = 0; step < MAX_TEST_STEPS && unsupported.progress().phase != m::StripePhase::FAILED;
             ++step) {
            work = turn();
            REQUIRE(unsupported.advance(work));
        }

        REQUIRE(unsupported.progress().phase == m::StripePhase::FAILED);
        REQUIRE(unsupported.progress().children[0].command);
        CHECK(unsupported.progress().children[0].command->status == m::CommandStatus::REJECTED);
        CHECK_FALSE(unsupported.progress().children[1].submitted);
        REQUIRE(unsupported.requestCancel(*operation));
        complete(unsupported, m::StripePhase::CANCELLED);
        close(unsupported);
    }
}

TEST_CASE("root preflight refuses schema-dependent allocation before resident and work admission") {
    auto small = schema();
    std::vector<c::LocalRecord> records(24, small.records()[0]);

    for (std::size_t index = 0; index < records.size(); ++index) {
        records[index].id = static_cast<std::int32_t>(index);
        records[index].name.append(index + 16, u'X');
    }

    auto large = c::LocalSchema::create(records, {64, 1024, 1024, 128, 16384}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(large);
    auto striper = partition(u"by1");
    const m::StreamStripeOptions child{
        muxLimits(), desiredLimits(), {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, queueLimits()};

    for (const auto *catalog : {&small, &*large}) {
        for (const bool residentRefusal : {true, false}) {
            auto limits = rootLimits();
            auto work = turn();

            if (residentRefusal) {
                limits.residentBytes = 0;
            } else {
                // Only the fixed ledger control is affordable; the schema and root are not.
                work.bytes = org::ttldtor::qdcxx::core::detail::RecordByteBudget::metadataAllocationBytes();
            }

            std::expected<m::StripedCollector, m::StripeFailure> result =
                std::unexpected(m::StripeFailure{m::StripeError::INVALID_STATE});
            std::size_t observed = 0;
            {
                AllocationProbe probe{1};
                result = m::StripedCollector::createStream(*catalog, striper, std::span(&child, 1), limits, work);
                observed = probedAllocations;
            }
            REQUIRE_FALSE(result);
            CHECK(result.error().error ==
                  (residentRefusal ? m::StripeError::RESOURCE_LIMIT : m::StripeError::WORK_LIMIT));
            CHECK(observed == 1);
        }
    }
}

TEST_CASE("retirement retries fund every request visit before scanning command storage") {
    auto catalog = schema();
    auto root = create(c::SubscriptionFamily::STREAM, catalog, partition(u"by1"));
    auto receiver = std::make_shared<Recorder>();
    auto client = open(root, receiver);
    const std::array<std::u16string_view, 8> symbols{u"A", u"B", u"C", u"D", u"E", u"F", u"G", u"H"};
    auto desired = requests(catalog, c::SubscriptionFamily::STREAM, symbols);
    auto work = turn();
    REQUIRE(root.beginReplace(client, desired, work));

    for (std::size_t step = 0; step < MAX_TEST_STEPS && root.progress().completedStripes != 1; ++step) {
        work = turn();
        REQUIRE(root.advance(work));
    }

    REQUIRE(root.progress().completedStripes == 1);
    REQUIRE(root.progress().phase == m::StripePhase::RUNNING);
    const auto operation = root.progress().operationId;

    for (std::size_t retry = 0; retry < 3; ++retry) {
        work = turn();
        work.operations = 1;
        work.visits = 3 + 1 + desired.size(); // Fixed one-stripe advance plus complete disposal walk.
        work.bytes = 0;
        const auto refused = root.advance(work);
        REQUIRE_FALSE(refused);
        CHECK(refused.error().error == m::StripeError::WORK_LIMIT);
        CHECK(work.visits == 0);
        CHECK(root.progress().operationId == operation);
        CHECK(root.progress().completedStripes == 1);
        CHECK(root.progress().phase == m::StripePhase::RUNNING);
    }

    complete(root);
    CHECK(root.aggregateEntries(0).size() == desired.size());
    close(root);
}

TEST_CASE("HISTORY combined snapshot uses exact reservation and funds both linear observation passes") {
    auto catalog = schema();
    auto root = create(c::SubscriptionFamily::HISTORY, catalog, partition(u"byrange-D-M-"));
    auto receiver = std::make_shared<Recorder>();
    auto client = open(root, receiver);
    const std::array<std::u16string_view, 3> symbols{u"AAPL", u"IBM", u"MSFT"};
    auto desired = requests(catalog, c::SubscriptionFamily::HISTORY, symbols);
    auto work = turn();
    REQUIRE(root.beginReplace(client, desired, work));
    complete(root);
    pump(root);
    work = turn();
    auto completeRows = root.rangeSnapshot(client, work);
    REQUIRE(completeRows);
    REQUIRE(completeRows->size() == 3);
    auto newRowBytes = completeRows->size() * sizeof(m::StripedCollector::RangeObservation);

    for (const auto &row : *completeRows) {
        newRowBytes += (row.value.symbol.capacity() + 1) * sizeof(char16_t);
    }

    work = turn();
    work.visits = 6; // Only the sizing pass is funded; no final child observation can start.
    const auto visitRefusal = root.rangeSnapshot(client, work);
    REQUIRE_FALSE(visitRefusal);
    CHECK(visitRefusal.error().error == m::StripeError::WORK_LIMIT);
    work = turn();
    work.bytes = newRowBytes; // Exact final storage suffices; there is no repeated prefix relocation.
    const auto exactBytes = root.rangeSnapshot(client, work);
    REQUIRE(exactBytes);
    CHECK(exactBytes->size() == 3);
    CHECK(work.bytes == 0);
    work = turn();
    work.visits = 11; // Exactly one less than two passes over three children and three rows.
    const auto oneVisitShort = root.rangeSnapshot(client, work);
    REQUIRE_FALSE(oneVisitShort);
    CHECK(oneVisitShort.error().error == m::StripeError::WORK_LIMIT);
    work = turn();
    work.bytes = newRowBytes - 1;
    const auto oneByteShort = root.rangeSnapshot(client, work);
    REQUIRE_FALSE(oneByteShort);
    CHECK(oneByteShort.error().error == m::StripeError::WORK_LIMIT);
    work = turn();
    work.visits = 12;
    work.bytes = newRowBytes;
    const auto finalRows = root.rangeSnapshot(client, work);
    REQUIRE(finalRows);
    CHECK(finalRows->size() == 3);
    CHECK(finalRows->capacity() == finalRows->size());
    CHECK(work.visits == 0);
    CHECK(work.bytes == 0);
    const auto nativeOperations = turn().child.operations - work.child.operations;
    REQUIRE(nativeOperations > 0);
    work = turn();
    work.child.operations = nativeOperations - 1;
    const auto oneNativeOperationShort = root.rangeSnapshot(client, work);
    REQUIRE_FALSE(oneNativeOperationShort);
    CHECK(oneNativeOperationShort.error().error == m::StripeError::WORK_LIMIT);

    std::expected<std::vector<m::StripedCollector::RangeObservation>, m::StripeFailure> measured =
        std::unexpected(m::StripeFailure{m::StripeError::INVALID_STATE});
    std::size_t allocationRequests = 0;
    work = turn();
    {
        AllocationProbe probe{std::nullopt};
        measured = root.rangeSnapshot(client, work);
        allocationRequests = probedAllocations;
    }
    REQUIRE(measured);
    REQUIRE(allocationRequests > 0);
    CHECK(measured->capacity() == measured->size());
    const auto rootBytes = root.residentBytes();
    const auto operationId = root.progress().operationId;

    for (std::size_t failAfter = 0; failAfter < allocationRequests; ++failAfter) {
        work = turn();
        {
            AllocationProbe probe{failAfter};
            measured = root.rangeSnapshot(client, work);
        }
        REQUIRE_FALSE(measured);
        CHECK((measured.error().error == m::StripeError::ALLOCATION_FAILURE ||
               (measured.error().child && measured.error().child->error == m::QueueError::ALLOCATION_FAILURE)));
        CHECK(root.residentBytes() == rootBytes);
        CHECK(root.progress().operationId == operationId);
    }

    // Reserved stop is producer-safe and changes statuses; applied keys require owner progress to change.
    client.requestStop(m::StopMode::ABORT);
    work = turn();
    const auto stoppedRows = root.rangeSnapshot(client, work);
    REQUIRE(stoppedRows);
    CHECK(stoppedRows->size() == 3);

    for (const auto &row : *stoppedRows) {
        CHECK_FALSE(row.value.status.mappingCurrent);
        CHECK_FALSE(row.value.status.completeDeliveredRange);
    }

    close(root, m::StopMode::ABORT);
}

TEST_CASE("immediate stopped child command preserves CLOSING under CHILD_FAILURE") {
    auto catalog = schema();
    auto root = create(c::SubscriptionFamily::STREAM, catalog, partition(u"by1"));
    auto receiver = std::make_shared<Recorder>();
    auto client = open(root, receiver);
    client.requestStop(m::StopMode::ABORT);
    auto work = turn();
    const auto operation = root.beginClear(client, work);
    REQUIRE(operation);
    work = turn();
    const auto result = root.advance(work);
    REQUIRE(result);
    REQUIRE(result->failure);
    CHECK(result->failure->error == m::StripeError::CHILD_FAILURE);
    CHECK(result->failure->submission == m::QueueError::CLOSING);
    CHECK(result->failure->stripe == 0);
    CHECK(result->phase == m::StripePhase::FAILED);
    REQUIRE(root.requestCancel(*operation));
    complete(root, m::StripePhase::CANCELLED);
    close(root, m::StopMode::ABORT);
}
