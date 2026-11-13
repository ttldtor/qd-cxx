// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "../../src/mux/interest_transaction.hpp"

#include "qd/events/quote.hpp"
#include "qd/protocol/subscription_writer.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <new>
#include <string>
#include <utility>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace m = org::ttldtor::qdcxx::mux;
namespace p = org::ttldtor::qdcxx::protocol;
namespace e = org::ttldtor::qdcxx::events;
bool faultActive = false;             ///< Scoped only around production API calls, never test assertions.
std::size_t allocationsRemaining = 0; ///< Allowed ordinary allocations during fault injection.
std::size_t allocationCount = 0;      ///< Observed ordinary allocation attempts in the current scope.

/** Restore ordinary allocation before test assertions execute. */
struct AllocationFault {
    /** Activate allocation failure after a finite number of requests.
     * @param permitted Successful allocation requests before failure.
     */
    explicit AllocationFault(std::size_t permitted) {
        allocationsRemaining = permitted;
        allocationCount = 0;
        faultActive = true;
    }

    /** Restore ordinary allocation. */
    ~AllocationFault() {
        faultActive = false;
    }
};

/** Supply explicit finite test turn credits, unrelated to feed lifetime.
 * @return Independent generous turn.
 */
m::MuxWork turn() {
    return {1000, 100000000, 100000000, 100000000};
}

/** Supply small immutable test capacities.
 * @return Complete two-bank reservations, not production defaults.
 */
m::MuxLimits limits() {
    return {4, 4, 16, 16, 32, 256, 16, 1000000};
}

/** Create ordinary and both supported indexed-layout records.
 * @return Independently created immutable schema.
 */
c::LocalSchema schema() {
    const std::array records{
        c::LocalRecord{0, u"Ordinary", false, {{8}}, {{u"Value", 0}}, {}},
        c::LocalRecord{1, u"Pair", true, {{8}, {8}}, {{u"High", 0}, {u"Low", 1}}, c::IndexedTimePairInt32{0, 1}},
        c::LocalRecord{2, u"Single", true, {{104}}, {{u"Raw", 0}}, c::IndexedTimeSingleInt64{0}}};
    auto result = c::LocalSchema::create(records, {4, 8, 8, 32, 128}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Create the independently reserved MUX owner.
 * @param source Captured immutable schema provenance.
 * @param family Fixed delivery family.
 * @param bounds Explicit immutable capacities.
 * @return Owning empty MUX state.
 */
m::MuxState owner(const c::LocalSchema &source, c::SubscriptionFamily family = c::SubscriptionFamily::STREAM,
                  m::MuxLimits bounds = limits()) {
    auto work = turn();
    auto result = m::MuxState::create(source, family, bounds, work);
    REQUIRE(result);

    return std::move(*result);
}

/** Activate an empty local client.
 * @param state MUX owner to mutate.
 * @return Client identity belonging only to state.
 */
m::MuxClient client(m::MuxState &state) {
    auto work = turn();
    auto result = state.createClient(work);
    REQUIRE(result);

    return *result;
}

/** Build one borrowed exact request.
 * @param source Immutable provenance owner.
 * @param symbol Exact borrowed units.
 * @param bound Optional raw HISTORY lower bound.
 * @param record Explicit local record ID.
 * @return Borrowed command input.
 */
c::SubscriptionRequest request(const c::LocalSchema &source, std::u16string_view symbol,
                               std::optional<std::int64_t> bound = {}, std::int32_t record = 0) {
    return {{source.recordContext(record), symbol}, bound};
}

/** Inspect an explicit independently specified union member.
 * @param state MUX owner inspected without mutation.
 * @param symbol Expected exact symbol.
 * @param bound Expected raw HISTORY bound.
 * @param count Expected complete union cardinality.
 */
void aggregate(const m::MuxState &state, std::u16string_view symbol, std::optional<std::int64_t> bound,
               std::size_t count = 1) {
    const auto entries = state.aggregateEntries();
    REQUIRE(entries.size() == count);
    bool found = false;

    for (const auto &entry : entries) {
        if (entry.key.symbol == symbol && entry.historyBound == bound) {
            found = true;
        }
    }

    CHECK(found);
}

/** Compare actual desired values against an independently executed core desired-state owner.
 * @param state MUX whose exact client map is inspected.
 * @param token Client identity to inspect.
 * @param reference Actual core state, never a copied MUX algorithm.
 */
void sameMap(const m::MuxState &state, const m::MuxClient &token, const c::SubscriptionState &reference) {
    const auto actual = state.clientEntries(token);
    REQUIRE(actual);
    REQUIRE(actual->size() == reference.entries().size());

    for (const auto &expected : reference.entries()) {
        bool found = false;

        for (const auto &entry : *actual) {
            if (entry.key.context.matches(expected.key.context) && entry.key.symbol == expected.key.symbol &&
                entry.historyBound == expected.historyBound) {
                found = true;
            }
        }

        CHECK(found);
    }
}
} // namespace

/** Observe only this executable's ordinary allocation route.
 * @param count Requested bytes; zero still supplies a deletable pointer.
 * @return Malloc storage, or throws at the selected allocation fault.
 */
void *operator new(std::size_t count) {
    if (faultActive) {
        ++allocationCount;

        if (allocationsRemaining == 0) {
            throw std::bad_alloc{};
        }

        --allocationsRemaining;
    }

    if (auto *storage = std::malloc(count == 0 ? 1 : count)) {
        return storage;
    }

    throw std::bad_alloc{};
}

/** Route arrays through the same observed route.
 * @param count Requested array backing bytes.
 * @return Storage for matching array delete, or throws bad_alloc.
 */
void *operator new[](std::size_t count) {
    return ::operator new(count);
}

/** Release ordinary storage.
 * @param storage Matching allocation pointer.
 */
void operator delete(void *storage) noexcept {
    std::free(storage);
}

/** Release array storage.
 * @param storage Matching allocation pointer.
 */
void operator delete[](void *storage) noexcept {
    std::free(storage);
}

/** Release sized ordinary storage.
 * @param storage Matching allocation pointer.
 * @param count Unused implementation supplied size.
 */
void operator delete(void *storage, std::size_t count) noexcept {
    static_cast<void>(count);
    std::free(storage);
}

/** Release sized array storage.
 * @param storage Matching allocation pointer.
 * @param count Unused implementation supplied size.
 */
void operator delete[](void *storage, std::size_t count) noexcept {
    static_cast<void>(count);
    std::free(storage);
}

TEST_CASE("MUX staged replacement publishes only the current inactive bank") {
    const auto source = schema();
    auto state = owner(source);
    auto other = owner(source);
    const auto active = client(state);
    auto work = turn();
    const std::array oldEntries{request(source, u"OLD")};
    const std::array discardedEntries{request(source, u"DISCARDED")};
    const std::array nextEntries{request(source, u"NEXT")};
    REQUIRE(state.add(active, oldEntries, work));
    const auto revision = state.revision();

    auto discarded = m::detail::MuxStateAccess::prepareReplace(state, active, discardedEntries, work);
    REQUIRE(discarded);
    CHECK(state.revision() == revision);
    REQUIRE(state.aggregateEntries().size() == 1);
    CHECK(state.aggregateEntries()[0].key.symbol == u"OLD");

    auto next = m::detail::MuxStateAccess::prepareReplace(state, active, nextEntries, work);
    REQUIRE(next);
    CHECK_FALSE(m::detail::MuxStateAccess::commitReplace(other, *next));
    CHECK_FALSE(m::detail::MuxStateAccess::commitReplace(state, *discarded));
    CHECK(state.revision() == revision);

    const auto committed = m::detail::MuxStateAccess::commitReplace(state, *next);
    REQUIRE(committed);
    CHECK(committed->revision == revision + 1);
    REQUIRE(state.aggregateEntries().size() == 1);
    CHECK(state.aggregateEntries()[0].key.symbol == u"NEXT");
    CHECK_FALSE(m::detail::MuxStateAccess::commitReplace(state, *next));
}

TEST_CASE("MUX overlap preserves private filtering and last removal") {
    const auto source = schema();
    auto state = owner(source);
    const auto a = client(state);
    const auto b = client(state);
    auto work = turn();
    const std::array common{request(source, u"COMMON")};
    const std::array privateA{request(source, u"PRIVATE-A")};
    REQUIRE(state.add(a, common, work));
    REQUIRE(state.add(b, common, work));
    REQUIRE(state.add(a, privateA, work));
    REQUIRE(state.aggregateEntries().size() == 2);
    CHECK(state.select(a, privateA[0].key, {}, work)->has_value());
    CHECK_FALSE(state.select(b, privateA[0].key, {}, work)->has_value());
    const auto aEntries = state.clientEntries(a);
    REQUIRE(aEntries);
    const auto membershipA = (*aEntries)[0].membership;
    CHECK(*state.isCurrent(a, membershipA, work));
    CHECK_FALSE(*state.isCurrent(b, membershipA, work));
    const std::array key{common[0].key};
    REQUIRE(state.remove(a, key, work));
    aggregate(state, u"COMMON", {}, 2);
    CHECK_FALSE(*state.isCurrent(a, membershipA, work));
    CHECK(state.select(b, common[0].key, {}, work)->has_value());
    REQUIRE(state.remove(b, key, work));
    aggregate(state, u"PRIVATE-A", {});
    REQUIRE(state.clear(a, work));
    CHECK(state.aggregateEntries().empty());
}

TEST_CASE("MUX HISTORY production bound sequence is signed minimum and inclusive per client") {
    const auto source = schema();
    auto state = owner(source, c::SubscriptionFamily::HISTORY);
    const auto a = client(state);
    const auto b = client(state);
    auto work = turn();
    constexpr std::int64_t LOWER = (std::int64_t{1} << 32) + 2;
    const std::array first{request(source, u"ALPHA", LOWER, 1)};
    const std::array earlier{request(source, u"ALPHA", LOWER - 1, 1)};
    const std::array later{request(source, u"ALPHA", LOWER + 1, 1)};
    const std::array negative{request(source, u"ALPHA", -1, 1)};
    REQUIRE(state.add(a, first, work));
    aggregate(state, u"ALPHA", LOWER);
    REQUIRE(state.add(b, earlier, work));
    aggregate(state, u"ALPHA", LOWER - 1);
    CHECK_FALSE(state.select(a, first[0].key, LOWER - 1, work)->has_value());
    CHECK(state.select(a, first[0].key, LOWER, work)->has_value());
    CHECK(state.select(b, first[0].key, LOWER - 1, work)->has_value());
    const auto prior = (*state.clientEntries(b))[0].membership;
    REQUIRE(state.add(b, later, work));
    aggregate(state, u"ALPHA", LOWER);
    CHECK_FALSE(*state.isCurrent(b, prior, work));
    const std::array remove{first[0].key};
    REQUIRE(state.remove(a, remove, work));
    aggregate(state, u"ALPHA", LOWER + 1);
    REQUIRE(state.add(b, negative, work));
    aggregate(state, u"ALPHA", -1);
    const std::array minimum{request(source, u"ALPHA", std::numeric_limits<std::int64_t>::min(), 1)};
    REQUIRE(state.add(a, minimum, work));
    aggregate(state, u"ALPHA", std::numeric_limits<std::int64_t>::min());
    CHECK(state.select(a, minimum[0].key, std::numeric_limits<std::int64_t>::min(), work)->has_value());
}

TEST_CASE("MUX real core differential mutations preserve comparable maps and atomic outcomes") {
    const auto source = schema();

    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        auto state = owner(source, family);
        const auto a = client(state);
        const auto b = client(state);
        auto coreA = c::SubscriptionState::create(source, family, {4, 16, 32, 256, 32, 512, 16});
        auto coreB = c::SubscriptionState::create(source, family, {4, 16, 32, 256, 32, 512, 16});
        REQUIRE(coreA);
        REQUIRE(coreB);
        auto work = turn();
        c::SubscriptionWork coreWork{1000, 10000000, 10000000};
        const auto history = family == c::SubscriptionFamily::HISTORY;
        const auto bound = history ? std::optional<std::int64_t>{7} : std::nullopt;
        const auto record = history ? 1 : 0;
        const std::array initial{request(source, u"A", bound, record), request(source, u"B", bound, record),
                                 request(source, u"A", bound, record)};
        const auto mAdd = state.add(a, initial, work);
        const auto cAdd = coreA->add(initial, coreWork);
        REQUIRE(mAdd);
        REQUIRE(cAdd);
        CHECK(mAdd->changed == cAdd->changed);
        REQUIRE(state.add(b, initial, work));
        REQUIRE(coreB->add(initial, coreWork));
        sameMap(state, a, *coreA);
        sameMap(state, b, *coreB);
        const std::array reordered{initial[1], initial[0]};
        const auto oldView = state.clientEntries(a)->data();
        const auto oldRevision = state.revision();
        const auto mReplace = state.replace(a, reordered, work);
        const auto cReplace = coreA->replace(reordered, coreWork);
        REQUIRE(mReplace);
        REQUIRE(cReplace);
        CHECK(mReplace->changed == cReplace->changed);
        CHECK_FALSE(mReplace->changed);
        CHECK(state.revision() == oldRevision);
        CHECK(state.clientEntries(a)->data() == oldView);
        const std::array keys{initial[0].key, initial[0].key};
        REQUIRE(state.remove(a, keys, work));
        REQUIRE(coreA->remove(keys, coreWork));
        sameMap(state, a, *coreA);
        sameMap(state, b, *coreB);
        aggregate(state, u"A", bound, 2);
        REQUIRE(state.remove(a, keys, work));
        REQUIRE(coreA->remove(keys, coreWork));
        sameMap(state, a, *coreA);

        if (history) {
            const std::array bad{request(source, u"C", 8, 1), request(source, u"C", 9, 1)};
            CHECK(state.add(a, bad, work).error() == m::MuxError::CONTRADICTORY_INPUT);
            CHECK(coreA->add(bad, coreWork).error() == c::SubscriptionError::CONTRADICTORY_INPUT);
            sameMap(state, a, *coreA);
        }

        const std::array invalid{request(source, u"*BAD", bound, record)};
        CHECK(state.replace(a, invalid, work).error() == m::MuxError::INVALID_SELECTOR);
        CHECK(coreA->replace(invalid, coreWork).error() == c::SubscriptionError::INVALID_SELECTOR);
        sameMap(state, a, *coreA);
        sameMap(state, b, *coreB);
        REQUIRE(state.clear(a, work));
        REQUIRE(coreA->clear(coreWork));
        sameMap(state, a, *coreA);
        sameMap(state, b, *coreB);
    }
}

TEST_CASE("MUX opaque identities survive move and never cross clients or reused slots") {
    const auto source = schema();
    auto state = owner(source);
    const auto a = client(state);
    auto work = turn();
    const std::array desired{request(source, u"A")};
    REQUIRE(state.add(a, desired, work));
    const auto membership = (*state.clientEntries(a))[0].membership;
    auto destination = owner(source);
    const auto oldClient = client(destination);
    REQUIRE(destination.add(oldClient, desired, work));
    const auto oldMembership = (*destination.clientEntries(oldClient))[0].membership;
    destination = std::move(state);
    CHECK(*destination.isCurrent(a, membership, work));
    CHECK(destination.clientEntries(oldClient).error() == m::MuxError::INVALID_CLIENT);
    CHECK_FALSE(*destination.isCurrent(a, oldMembership, work));
    CHECK(state.clientEntries(a).error() == m::MuxError::INVALID_STATE);
    CHECK(state.aggregateEntries().empty());
    CHECK(state.storageBytes() == 0);
    CHECK(state.revision() == 0);
    auto *same = &destination;
    const auto revision = destination.revision();
    destination = std::move(*same);
    CHECK(destination.revision() == revision);
    CHECK(*destination.isCurrent(a, membership, work));
    REQUIRE(destination.removeClient(a, work));
    const auto replacement = client(destination);
    CHECK(destination.clientEntries(a).error() == m::MuxError::INVALID_CLIENT);
    REQUIRE(destination.add(replacement, desired, work));
    CHECK_FALSE(*destination.isCurrent(replacement, membership, work));
    const auto newMembership = (*destination.clientEntries(replacement))[0].membership;
    REQUIRE(destination.clear(replacement, work));
    REQUIRE(destination.add(replacement, desired, work));
    CHECK_FALSE(*destination.isCurrent(replacement, newMembership, work));
}

TEST_CASE("MUX validation keeps foreign provenance and failed partial input atomic") {
    const auto source = schema();
    const auto foreign = schema();
    auto state = owner(source);
    const auto a = client(state);
    auto work = turn();
    const std::array desired{request(source, u"A")};
    REQUIRE(state.add(a, desired, work));
    const auto old = state.clientEntries(a)->data();
    const auto revision = state.revision();
    const std::array mixed{request(source, u"B"), request(foreign, u"A")};
    CHECK(state.add(a, mixed, work).error() == m::MuxError::INVALID_CONTEXT);
    CHECK(state.clientEntries(a)->data() == old);
    CHECK(state.revision() == revision);
    aggregate(state, u"A", {});
    CHECK(state.select(a, mixed[1].key, {}, work).error() == m::MuxError::INVALID_CONTEXT);
    const std::array null{request(source, std::u16string_view{})};
    CHECK(state.add(a, null, work).error() == m::MuxError::INVALID_SELECTOR);
    const std::array wrongBound{request(source, u"B", 1)};
    CHECK(state.add(a, wrongBound, work).error() == m::MuxError::INVALID_HISTORY);
    auto history = owner(source, c::SubscriptionFamily::HISTORY);
    const auto h = client(history);
    const std::array untimed{request(source, u"A", 1)};
    CHECK(history.add(h, untimed, work).error() == m::MuxError::INVALID_HISTORY);
    const std::array untimedKey{untimed[0].key};
    CHECK(history.remove(h, untimedKey, work).error() == m::MuxError::INVALID_HISTORY);
    CHECK(state.clientEntries(m::MuxClient{}).error() == m::MuxError::INVALID_CLIENT);
    const auto defaultMembership = m::MuxMembership{};
    CHECK_FALSE(*state.isCurrent(a, defaultMembership, work));
}

TEST_CASE("MUX nonnull empty and exact raw UTF16 survive zero text budgets and aliases") {
    const auto source = schema();
    auto bounds = limits();
    bounds.symbolUnits = 0;
    bounds.totalSymbolUnits = 0;
    auto state = owner(source, c::SubscriptionFamily::STREAM, bounds);
    const auto a = client(state);
    auto work = turn();
    const std::array empty{request(source, u"")};
    REQUIRE(state.add(a, empty, work));
    const auto entry = (*state.clientEntries(a))[0];
    CHECK(entry.key.symbol.empty());
    CHECK(entry.key.symbol.data() != nullptr);
    CHECK(state.aggregateEntries()[0].key.symbol.data() != nullptr);
    auto core = c::SubscriptionState::create(source, c::SubscriptionFamily::STREAM, {4, 16, 0, 0, 32, 0, 16});
    REQUIRE(core);
    c::SubscriptionWork coreWork{100, 10000, 10000};
    REQUIRE(core->add(empty, coreWork));
    sameMap(state, a, *core);
    auto raw = owner(source);
    const auto b = client(raw);
    const char16_t units[]{u'A', 0, 0xd800};
    const std::array exact{request(source, {units, 3})};
    REQUIRE(raw.add(b, exact, work));
    const auto borrowed = (*raw.clientEntries(b))[0];
    const std::array alias{c::SubscriptionRequest{borrowed.key, {}}};
    const auto revision = raw.revision();
    REQUIRE(raw.replace(b, alias, work));
    CHECK(raw.revision() == revision);
    CHECK((*raw.clientEntries(b))[0].key.symbol == std::u16string_view(units, 3));
}

TEST_CASE("MUX capacities count clients aggregate slots both banks and work rollback") {
    const auto source = schema();
    auto base = owner(source);
    const auto exactBytes = base.storageBytes();
    auto bounds = limits();
    bounds.residentBytes = exactBytes;
    auto state = owner(source, c::SubscriptionFamily::STREAM, bounds);
    auto work = turn();
    --bounds.residentBytes;
    CHECK(m::MuxState::create(source, c::SubscriptionFamily::STREAM, bounds, work).error() ==
          m::MuxError::RESOURCE_LIMIT);
    bounds = limits();
    bounds.clients = 2;
    bounds.entries = 2;
    bounds.aggregateEntries = 1;
    bounds.symbolUnits = 1;
    bounds.totalSymbolUnits = 2;
    bounds.commandEntries = 2;
    auto small = owner(source, c::SubscriptionFamily::STREAM, bounds);
    const auto a = client(small);
    const auto b = client(small);
    CHECK(small.createClient(work).error() == m::MuxError::RESOURCE_LIMIT);
    const std::array first{request(source, u"A")};
    const std::array second{request(source, u"B")};
    REQUIRE(small.add(a, first, work));
    REQUIRE(small.add(b, first, work));
    const auto revision = small.revision();
    const auto view = small.clientEntries(a)->data();
    CHECK(small.replace(a, second, work).error() == m::MuxError::RESOURCE_LIMIT);
    CHECK(small.revision() == revision);
    CHECK(small.clientEntries(a)->data() == view);
    aggregate(small, u"A", {});
    auto noWork = turn();
    noWork.visits = 0;
    CHECK(small.clear(a, noWork).error() == m::MuxError::WORK_LIMIT);
    CHECK(noWork.operations == 999);
    CHECK(small.revision() == revision);
    noWork = turn();
    noWork.bytes = 0;
    CHECK(small.clear(a, noWork).error() == m::MuxError::WORK_LIMIT);
    noWork = turn();
    noWork.symbolUnits = 0;
    CHECK(small.select(a, first[0].key, {}, noWork).error() == m::MuxError::WORK_LIMIT);
    noWork = turn();
    noWork.visits = 0;
    CHECK(small.isCurrent(a, (*small.clientEntries(a))[0].membership, noWork).error() == m::MuxError::WORK_LIMIT);
    CHECK(small.revision() == revision);
    CHECK(small.clientEntries(a)->data() == view);
}

TEST_CASE("MUX empty client axes consume work even with zero entry and symbol capacities") {
    const auto source = schema();
    auto bounds = limits();
    bounds.clients = 128;
    bounds.entries = 0;
    bounds.aggregateEntries = 0;
    bounds.symbolUnits = 0;
    bounds.totalSymbolUnits = 0;
    auto state = owner(source, c::SubscriptionFamily::STREAM, bounds);
    auto noWork = turn();
    noWork.visits = 127;
    CHECK(state.createClient(noWork).error() == m::MuxError::WORK_LIMIT);
    CHECK(state.revision() == 0);
    const auto a = client(state);
    auto work = turn();
    const auto revision = state.revision();
    REQUIRE(state.clear(a, work));
    CHECK(state.revision() == revision);
    CHECK(work.visits < turn().visits);
    REQUIRE(state.removeClient(a, work));
    CHECK(state.revision() == revision + 1);
    CHECK(state.aggregateEntries().empty());
}

TEST_CASE("MUX factory failures publish nothing and commands never allocate") {
    const auto source = schema();
    bool reachedSuccess = false;

    for (std::size_t allowed = 0; allowed < 20; ++allowed) {
        auto work = turn();
        std::optional<std::expected<m::MuxState, m::MuxError>> created;
        {
            AllocationFault fault(allowed);
            created.emplace(m::MuxState::create(source, c::SubscriptionFamily::STREAM, limits(), work));
        }

        if (*created) {
            reachedSuccess = true;
            break;
        }

        CHECK(created->error() == m::MuxError::ALLOCATION_FAILURE);
    }

    REQUIRE(reachedSuccess);
    auto state = owner(source);
    const auto a = client(state);
    auto work = turn();
    const std::array desired{request(source, u"A")};
    std::optional<std::expected<m::MuxChange, m::MuxError>> added;
    {
        AllocationFault fault(0);
        added.emplace(state.add(a, desired, work));
    }
    REQUIRE(*added);
    CHECK(allocationCount == 0);
    {
        AllocationFault fault(0);
        added.emplace(state.add(a, desired, work));
    }
    REQUIRE(*added);
    CHECK_FALSE((*added)->changed);
    CHECK(allocationCount == 0);
    const std::array invalid{request(source, u"*BAD")};
    {
        AllocationFault fault(0);
        added.emplace(state.replace(a, invalid, work));
    }
    CHECK(added->error() == m::MuxError::INVALID_SELECTOR);
    CHECK(allocationCount == 0);
    {
        AllocationFault fault(0);
        added.emplace(state.clear(a, work));
    }
    REQUIRE(*added);
    CHECK(allocationCount == 0);
}

TEST_CASE("MUX offline desired snapshot feeds actual writer with configured full Quote") {
    auto source = e::defaultQuoteSchema({1, 8, 24, 64, 512});
    REQUIRE(source);
    auto state = owner(*source);
    const auto a = client(state);
    const auto b = client(state);
    auto work = turn();
    const std::array first{request(*source, u"AAPL")};
    const std::array discarded{request(*source, u"MSFT")};
    const std::array last{request(*source, u"IBM")};
    REQUIRE(state.add(a, first, work));
    REQUIRE(state.add(b, discarded, work));
    REQUIRE(state.clear(b, work));
    REQUIRE(state.add(b, last, work));
    aggregate(state, u"AAPL", {}, 2);
    aggregate(state, u"IBM", {}, 2);
    p::OutgoingRecord layout{41, 0, u"Quote", {}};

    for (std::size_t i = 0; i < e::QUOTE_FIELD_NAMES.size(); ++i) {
        const auto name = e::QUOTE_FIELD_NAMES[i];
        layout.fields.push_back({std::u16string(name.begin(), name.end()),
                                 static_cast<std::int32_t>(e::QUOTE_FIELD_TYPES[i]), static_cast<std::int32_t>(i)});
    }

    p::SubscriptionWriterWork writerWork{100, 1000000, 1000000, 1000000};
    const std::array layouts{layout};
    auto writer = p::SubscriptionWriter::create(*source, layouts, {{1, 8, 24, 64, 512}, 1, 8, 64, 512},
                                                {65536, 65541, 16, 32, {1024, 128}, 131082, 131082, 4}, writerWork);
    REQUIRE(writer);
    auto snapshot = writer->compose(p::SubscriptionMessage::STREAM_ADD, state.aggregateEntries(), writerWork);
    REQUIRE(snapshot);
    const std::array expected{first[0], last[0]};
    auto independentlySpecified = writer->compose(p::SubscriptionMessage::STREAM_ADD, expected, writerWork);
    REQUIRE(independentlySpecified);
    REQUIRE(snapshot->bytes().size() == independentlySpecified->bytes().size());
    CHECK(std::equal(snapshot->bytes().begin(), snapshot->bytes().end(), independentlySpecified->bytes().begin()));
    CHECK(state.select(a, last[0].key, {}, work)->has_value() == false);
}

TEST_CASE("MUX zero-axis rewrite metadata requires more than array-only byte credits") {
    const auto source = schema();
    auto bounds = limits();
    bounds.clients = 1;
    bounds.entries = 0;
    bounds.aggregateEntries = 0;
    bounds.symbolUnits = 0;
    bounds.totalSymbolUnits = 0;
    auto state = owner(source, c::SubscriptionFamily::STREAM, bounds);
    const auto a = client(state);
    auto work = turn();
    work.bytes = 32;
    const auto revision = state.revision();
    const auto result = state.clear(a, work);
    REQUIRE_FALSE(result);
    CHECK(result.error() == m::MuxError::WORK_LIMIT);
    CHECK(state.revision() == revision);
}

TEST_CASE("MUX queries preserve published views and account exact bounded turn failures") {
    const auto source = schema();
    auto state = owner(source);
    const auto a = client(state);
    auto work = turn();
    const std::array desired{request(source, u"A"), request(source, u"B")};
    REQUIRE(state.add(a, desired, work));
    const auto entries = state.clientEntries(a);
    REQUIRE(entries);
    const auto pointer = entries->data();
    const auto unionPointer = state.aggregateEntries().data();
    const auto revision = state.revision();
    const auto membership = (*entries)[0].membership;
    auto measured = turn();
    REQUIRE(state.select(a, desired[0].key, {}, measured));
    const auto spentVisits = turn().visits - measured.visits;
    const auto spentText = turn().symbolUnits - measured.symbolUnits;
    auto exact = m::MuxWork{1, spentVisits, spentText, 0};
    REQUIRE(state.select(a, desired[0].key, {}, exact));
    CHECK(exact.operations == 0);
    CHECK(exact.visits == 0);
    CHECK(exact.symbolUnits == 0);
    exact = {1, spentVisits - 1, spentText, 0};
    CHECK(state.select(a, desired[0].key, {}, exact).error() == m::MuxError::WORK_LIMIT);
    auto current = m::MuxWork{1, entries->size() + 1, 0, 0};
    CHECK(*state.isCurrent(a, membership, current));
    CHECK(current.visits == 0);
    CHECK(state.revision() == revision);
    CHECK(state.clientEntries(a)->data() == pointer);
    CHECK(state.aggregateEntries().data() == unionPointer);
    REQUIRE(state.clear(a, work));
    current = {1, 0, 0, 0};
    CHECK(state.isCurrent(a, membership, current).error() == m::MuxError::WORK_LIMIT);
}

TEST_CASE("MUX packed symbol and entry limits preserve all clients after failed growth") {
    const auto source = schema();
    auto bounds = limits();
    bounds.clients = 2;
    bounds.entries = 2;
    bounds.aggregateEntries = 2;
    bounds.symbolUnits = 2;
    bounds.totalSymbolUnits = 3;
    bounds.commandEntries = 2;
    auto state = owner(source, c::SubscriptionFamily::STREAM, bounds);
    const auto a = client(state);
    const auto b = client(state);
    auto work = turn();
    const std::array aMap{request(source, u"AA")};
    const std::array bMap{request(source, u"B")};
    REQUIRE(state.add(a, aMap, work));
    REQUIRE(state.add(b, bMap, work));
    const auto revision = state.revision();
    const auto pointer = state.aggregateEntries().data();
    const std::array tooMany{request(source, u"C")};
    CHECK(state.add(b, tooMany, work).error() == m::MuxError::RESOURCE_LIMIT);
    const std::array tooLong{request(source, u"CCC")};
    CHECK(state.replace(b, tooLong, work).error() == m::MuxError::RESOURCE_LIMIT);
    const std::array tooPacked{request(source, u"BB")};
    CHECK(state.replace(b, tooPacked, work).error() == m::MuxError::RESOURCE_LIMIT);
    CHECK(state.revision() == revision);
    CHECK(state.aggregateEntries().data() == pointer);
    aggregate(state, u"AA", {}, 2);
    aggregate(state, u"B", {}, 2);
    const std::array duplicateOverflow{aMap[0], aMap[0], aMap[0]};
    CHECK(state.add(a, duplicateOverflow, work).error() == m::MuxError::RESOURCE_LIMIT);
}

TEST_CASE("MUX captured provenance survives schema declarations and constructor move") {
    std::optional<m::MuxState> retained;
    c::SubscriptionRequest desired;
    {
        const auto source = schema();
        retained.emplace(owner(source));
        desired = request(source, u"A");
    }
    auto moved = std::move(*retained);
    const auto a = client(moved);
    auto work = turn();
    const std::array input{desired};
    REQUIRE(moved.add(a, input, work));
    aggregate(moved, u"A", {});
    CHECK(moved.select(a, desired.key, {}, work)->has_value());
    CHECK(retained->createClient(work).error() == m::MuxError::INVALID_STATE);
    auto zero = turn();
    zero.operations = 0;
    CHECK(retained->clear(a, zero).error() == m::MuxError::WORK_LIMIT);
    auto bounds = limits();
    bounds.entries = std::numeric_limits<std::size_t>::max();
    const auto source = schema();
    CHECK(m::MuxState::create(source, c::SubscriptionFamily::STREAM, bounds, work).error() ==
          m::MuxError::INVALID_CONFIGURATION);
    CHECK(m::MuxState::create(source, static_cast<c::SubscriptionFamily>(-1), limits(), work).error() ==
          m::MuxError::INVALID_STATE);
}

TEST_CASE("MUX factory charges captured-record fill and allocation-failure cleanup independently") {
    constexpr std::size_t RECORDS = 8;
    std::array<c::LocalRecord, RECORDS> records;

    for (std::size_t i = 0; i < RECORDS; ++i) {
        records[i] = {static_cast<std::int32_t>(i),
                      u"Record" + std::u16string(1, static_cast<char16_t>(u'A' + i)),
                      false,
                      {{8}},
                      {{u"Value", 0}},
                      {}};
    }

    auto source = c::LocalSchema::create(records, {RECORDS, RECORDS, RECORDS, 16, 256});
    REQUIRE(source);
    const auto context = source->recordContext(0);
    auto bounds = limits();
    bounds.schemaRecords = RECORDS;
    bounds.clients = 0;
    bounds.entries = 0;
    bounds.aggregateEntries = 0;
    bounds.symbolUnits = 0;
    bounds.totalSymbolUnits = 0;
    auto under = turn();
    under.visits = 3 * RECORDS - 1;
    const auto rejected = m::MuxState::create(*source, c::SubscriptionFamily::STREAM, bounds, under);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == m::MuxError::WORK_LIMIT);
    auto work = turn();
    std::optional<std::expected<m::MuxState, m::MuxError>> failed;
    {
        AllocationFault fault(4);
        failed.emplace(m::MuxState::create(*source, c::SubscriptionFamily::STREAM, bounds, work));
    }
    REQUIRE_FALSE(*failed);
    CHECK(failed->error() == m::MuxError::ALLOCATION_FAILURE);
    CHECK(source->records().size() == RECORDS);
    CHECK(source->recordContext(0).matches(context));
    REQUIRE(m::MuxState::create(*source, c::SubscriptionFamily::STREAM, bounds, work));
}

TEST_CASE("MUX real core differential effective replace and HISTORY bound upserts") {
    const auto source = schema();

    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        const bool history = family == c::SubscriptionFamily::HISTORY;
        const auto record = history ? 2 : 0;
        const auto firstBound =
            history ? std::optional<std::int64_t>{std::numeric_limits<std::int64_t>::max()} : std::nullopt;
        const auto changedBound = history ? std::optional<std::int64_t>{-7} : std::nullopt;
        auto state = owner(source, family);
        const auto a = client(state);
        auto reference = c::SubscriptionState::create(source, family, {4, 16, 32, 256, 32, 512, 16});
        REQUIRE(reference);
        auto work = turn();
        c::SubscriptionWork coreWork{1000, 10000000, 10000000};
        const std::array initial{request(source, u"A", firstBound, record)};
        REQUIRE(state.add(a, initial, work));
        REQUIRE(reference->add(initial, coreWork));
        sameMap(state, a, *reference);
        const auto priorMembership = (*state.clientEntries(a))[0].membership;
        const std::array changed{request(source, u"A", changedBound, record)};
        const auto mChanged = state.add(a, changed, work);
        const auto cChanged = reference->add(changed, coreWork);
        REQUIRE(mChanged);
        REQUIRE(cChanged);
        CHECK(mChanged->changed == cChanged->changed);
        CHECK(mChanged->changed == history);
        sameMap(state, a, *reference);
        CHECK(*state.isCurrent(a, priorMembership, work) == !history);
        const std::array replacement{request(source, u"B", changedBound, record),
                                     request(source, u"C", changedBound, record)};
        const auto mReplaced = state.replace(a, replacement, work);
        const auto cReplaced = reference->replace(replacement, coreWork);
        REQUIRE(mReplaced);
        REQUIRE(cReplaced);
        CHECK(mReplaced->changed == cReplaced->changed);
        CHECK(mReplaced->changed);
        sameMap(state, a, *reference);
        aggregate(state, u"B", changedBound, 2);
        aggregate(state, u"C", changedBound, 2);
        const std::array<c::SubscriptionRequest, 0> empty{};
        REQUIRE(state.replace(a, empty, work));
        REQUIRE(reference->replace(empty, coreWork));
        sameMap(state, a, *reference);
        CHECK(state.aggregateEntries().empty());
        const auto mNoop = state.clear(a, work);
        const auto cNoop = reference->clear(coreWork);
        REQUIRE(mNoop);
        REQUIRE(cNoop);
        CHECK(mNoop->changed == cNoop->changed);
        CHECK_FALSE(mNoop->changed);
    }
}
