// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/core/subscription.hpp"

#include "../../src/core/subscription_state_access.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace {
namespace c = org::ttldtor::qdcxx::core;

bool allocationFaultActive = false;       ///< Scoped only around the production API call, never assertions.
std::size_t allocationsBeforeFailure = 0; ///< Successful requests permitted in the active fault scope.
std::size_t observedAllocations = 0;      ///< Requests actually observed in the most recent fault scope.

/** Limit ordinary allocations in this isolated single-threaded test executable. */
class AllocationFault {
    public:
    /** Activate a deterministic failure after the selected number of successful requests.
     * @param successful Requests allowed before throwing bad_alloc.
     */
    explicit AllocationFault(std::size_t successful) {
        allocationsBeforeFailure = successful;
        observedAllocations = 0;
        allocationFaultActive = true;
    }

    /** Restore normal allocation before the test framework observes any result. */
    ~AllocationFault() {
        allocationFaultActive = false;
    }
};

/** Supply finite roomy bounds, narrowed independently by boundary scenarios.
 * @return Metadata, resident, peak and command limits in declared units.
 */
c::SubscriptionLimits limits() {
    return {4, 8, 32, 128, 16, 256, 16};
}

/** Begin a fresh explicit turn; this is not an owner lifetime quota.
 * @return Finite remaining command/key/UTF-16 credits.
 */
c::SubscriptionWork turn() {
    return {100, 10000, 10000};
}

/** Supply ordinary and both supported explicit indexed-layout shapes in one immutable context.
 * @return Independently created owned schema, never guessed from record names.
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

/** Create an empty local desired-state owner.
 * @param source Immutable schema whose provenance is captured.
 * @param family Fixed contract family.
 * @param capacities Finite immutable limits.
 * @return Move-only state.
 */
c::SubscriptionState state(const c::LocalSchema &source, c::SubscriptionFamily family = c::SubscriptionFamily::STREAM,
                           c::SubscriptionLimits capacities = limits()) {
    auto result = c::SubscriptionState::create(source, family, capacities);

    REQUIRE(result);

    return std::move(*result);
}

/** Build borrowed exact input with explicit provenance.
 * @param source Immutable source schema.
 * @param symbol Exact borrowed units.
 * @param record Local record selection.
 * @param bound Optional raw signed64 HISTORY lower bound.
 * @return Command input whose text is not retained after application.
 */
c::SubscriptionRequest request(const c::LocalSchema &source, std::u16string_view symbol, std::int32_t record = 0,
                               std::optional<std::int64_t> bound = {}) {
    return {{source.recordContext(record), symbol}, bound};
}

/** Assert a failed command preserved the complete single-entry publication.
 * @param owner State inspected after failure.
 * @param token Previously current activation.
 * @param revision Previously published revision.
 * @param symbol Previously owned exact text.
 */
void unchanged(const c::SubscriptionState &owner, const c::SubscriptionMembership &token, std::uint64_t revision,
               std::u16string_view symbol) {
    REQUIRE(owner.entries().size() == 1);
    CHECK(owner.revision() == revision);
    CHECK(owner.entries()[0].key.symbol == symbol);
    CHECK(owner.isCurrent(token));
}

} // namespace

/** Replace only this executable's ordinary allocation route for scoped failure injection.
 * @param count Requested bytes; zero still yields a deletable pointer.
 * @return Malloc-backed storage, or throws bad_alloc at the observed fault point.
 */
void *operator new(std::size_t count) {
    if (allocationFaultActive) {
        ++observedAllocations;

        if (allocationsBeforeFailure == 0) {
            throw std::bad_alloc{};
        }

        --allocationsBeforeFailure;
    }

    if (auto *storage = std::malloc(count == 0 ? 1 : count)) {
        return storage;
    }

    throw std::bad_alloc{};
}

/** Route array allocation through the same observed fault point.
 * @param count Requested backing bytes including implementation metadata.
 * @return Storage owned by the matching array delete, or throws bad_alloc.
 */
void *operator new[](std::size_t count) {
    return ::operator new(count);
}

/** Release ordinary test storage.
 * @param storage Pointer supplied by the matching allocation route.
 */
void operator delete(void *storage) noexcept {
    std::free(storage);
}

/** Release array test storage.
 * @param storage Pointer supplied by the matching allocation route.
 */
void operator delete[](void *storage) noexcept {
    std::free(storage);
}

/** Release sized ordinary test storage.
 * @param storage Pointer supplied by the matching allocation route.
 * @param count Unused backing byte count.
 */
void operator delete(void *storage, std::size_t count) noexcept {
    static_cast<void>(count);
    std::free(storage);
}

/** Release sized array test storage.
 * @param storage Pointer supplied by the matching allocation route.
 * @param count Unused backing byte count.
 */
void operator delete[](void *storage, std::size_t count) noexcept {
    static_cast<void>(count);
    std::free(storage);
}

TEST_CASE("subscription exact symbols own UTF16 units and apply one revision") {
    const auto source = schema();
    auto owner = state(source);
    auto work = turn();
    std::u16string text{u'A', u'\0', static_cast<char16_t>(0xd800)};
    const std::array inputs{request(source, text), request(source, u""), request(source, u"a*"), request(source, text)};
    const auto applied = owner.add(inputs, work);

    REQUIRE(applied);
    CHECK(applied->changed);
    CHECK(applied->revision == 1);
    REQUIRE(owner.entries().size() == 3);
    CHECK(owner.entries()[0].key.symbol == text);
    CHECK(owner.entries()[1].key.symbol.empty());
    CHECK(owner.entries()[1].key.symbol.data() != nullptr);
    const auto token = owner.entries()[0].membership;
    const auto *published = owner.entries().data();

    text[0] = u'Z';
    CHECK(owner.entries()[0].key.symbol[0] == u'A');
    const std::array identical{c::SubscriptionRequest{owner.entries()[0].key, {}}};
    const auto noop = owner.add(identical, work);

    REQUIRE(noop);
    CHECK_FALSE(noop->changed);
    CHECK(noop->revision == 1);
    CHECK(owner.entries().data() == published);
    CHECK(owner.isCurrent(token));
}

TEST_CASE("subscription remove replace and readd preserve only continuing activations") {
    const auto source = schema();
    auto owner = state(source);
    auto work = turn();
    const std::array initial{request(source, u"A"), request(source, u"B")};
    REQUIRE(owner.add(initial, work));
    const auto a = owner.entries()[0].membership;
    const auto b = owner.entries()[1].membership;
    const std::array reordered{initial[1], initial[0], initial[1]};
    const auto noop = owner.replace(reordered, work);

    REQUIRE(noop);
    CHECK_FALSE(noop->changed);
    CHECK(owner.isCurrent(a));
    CHECK(owner.isCurrent(b));
    const std::array absent{request(source, u"absent").key};
    CHECK_FALSE(owner.remove(absent, work)->changed);
    const std::array removing{initial[0].key, initial[0].key};
    REQUIRE(owner.remove(removing, work));
    CHECK_FALSE(owner.isCurrent(a));
    CHECK(owner.isCurrent(b));
    const std::array readd{initial[0]};
    REQUIRE(owner.add(readd, work));
    CHECK_FALSE(owner.isCurrent(a));
    CHECK(owner.isCurrent(b));
    const auto renewed = owner.entries()[1].membership;
    REQUIRE(owner.replace({}, work));
    CHECK(owner.entries().empty());
    CHECK_FALSE(owner.isCurrent(renewed));
    CHECK(owner.revision() == 4);
    CHECK_FALSE(owner.clear(work)->changed);
}

TEST_CASE("subscription history raw bounds and explicit layout are validated atomically") {
    const auto source = schema();
    auto owner = state(source, c::SubscriptionFamily::HISTORY);
    auto work = turn();
    const std::array initial{request(source, u"X", 1, std::numeric_limits<std::int64_t>::min())};
    REQUIRE(owner.add(initial, work));
    const auto old = owner.entries()[0].membership;
    const std::array updated{request(source, u"X", 1, std::numeric_limits<std::int64_t>::max())};
    REQUIRE(owner.add(updated, work));
    CHECK_FALSE(owner.isCurrent(old));
    CHECK(owner.entries()[0].historyBound == std::numeric_limits<std::int64_t>::max());
    const auto current = owner.entries()[0].membership;
    const std::array contradictory{updated[0], request(source, u"X", 1, 0)};
    CHECK(owner.replace(contradictory, work).error() == c::SubscriptionError::CONTRADICTORY_INPUT);
    unchanged(owner, current, 2, u"X");
    const std::array noLayout{request(source, u"Y", 0, 0)};
    CHECK(owner.add(noLayout, work).error() == c::SubscriptionError::INVALID_HISTORY);
    const std::array missing{request(source, u"Y", 1)};
    CHECK(owner.add(missing, work).error() == c::SubscriptionError::INVALID_HISTORY);
    unchanged(owner, current, 2, u"X");
    const std::array single{request(source, u"Y", 2, -1)};
    REQUIRE(owner.add(single, work));
    CHECK(owner.entries()[1].historyBound == -1);
    const std::array withoutTime{updated[0].key};
    REQUIRE(owner.remove(withoutTime, work));
    CHECK_FALSE(owner.isCurrent(current));
}

TEST_CASE("subscription HISTORY removal rejects ordinary records before publishing any removal") {
    const auto source = schema();
    auto owner = state(source, c::SubscriptionFamily::HISTORY);
    auto work = turn();
    const std::array initial{request(source, u"A", 1, 0), request(source, u"B", 2, -1)};
    REQUIRE(owner.add(initial, work));
    const auto a = owner.entries()[0].membership;
    const auto b = owner.entries()[1].membership;
    const auto *published = owner.entries().data();
    const auto *aText = owner.entries()[0].key.symbol.data();
    const auto *bText = owner.entries()[1].key.symbol.data();
    const auto ordinary = request(source, u"ordinary", 0).key;

    SUBCASE("absent ordinary key still requires explicit indexed metadata") {
        const std::array removing{ordinary};
        const auto result = owner.remove(removing, work);

        REQUIRE_FALSE(result);
        CHECK(result.error() == c::SubscriptionError::INVALID_HISTORY);
    }

    SUBCASE("valid indexed removal followed by ordinary key rejects the entire command") {
        const std::array removing{initial[0].key, ordinary};
        const auto result = owner.remove(removing, work);

        REQUIRE_FALSE(result);
        CHECK(result.error() == c::SubscriptionError::INVALID_HISTORY);
    }

    REQUIRE(owner.entries().size() == 2);
    CHECK(owner.revision() == 1);
    CHECK(owner.entries().data() == published);
    CHECK(owner.entries()[0].key.symbol.data() == aText);
    CHECK(owner.entries()[1].key.symbol.data() == bText);
    CHECK(owner.entries()[0].key.symbol == u"A");
    CHECK(owner.entries()[1].key.symbol == u"B");
    CHECK(owner.isCurrent(a));
    CHECK(owner.isCurrent(b));
}

TEST_CASE("subscription invalid selectors contexts and family attributes never partially apply") {
    const auto source = schema();
    const auto foreign = schema();
    auto owner = state(source);
    auto work = turn();
    const std::array initial{request(source, u"A")};
    REQUIRE(owner.add(initial, work));
    const auto token = owner.entries()[0].membership;
    const std::array invalids{request(source, std::u16string_view{}),
                              request(source, u"*"),
                              request(source, u"*literal"),
                              request(foreign, u"X"),
                              request(source, u"X", 9),
                              request(source, u"X", 0, 0)};
    const std::array errors{c::SubscriptionError::INVALID_SELECTOR, c::SubscriptionError::INVALID_SELECTOR,
                            c::SubscriptionError::INVALID_SELECTOR, c::SubscriptionError::INVALID_CONTEXT,
                            c::SubscriptionError::INVALID_CONTEXT,  c::SubscriptionError::INVALID_HISTORY};

    for (std::size_t i = 0; i < invalids.size(); ++i) {
        const std::array command{request(source, u"valid-prefix"), invalids[i]};
        const auto result = owner.add(command, work);

        REQUIRE_FALSE(result);
        CHECK(result.error() == errors[i]);
        unchanged(owner, token, 1, u"A");
    }

    auto copied = source.boundedCopy({4, 8, 8, 32, 128});
    REQUIRE(copied);
    const std::array sameProvenance{request(*copied, u"A")};
    CHECK_FALSE(owner.add(sameProvenance, work)->changed);
    CHECK_FALSE(c::SubscriptionState::create(source, static_cast<c::SubscriptionFamily>(99), limits()));
}

TEST_CASE("subscription move transfers identity and cross-owner tokens cannot match") {
    static_assert(!std::is_copy_constructible_v<c::SubscriptionState>);
    static_assert(!std::is_copy_assignable_v<c::SubscriptionState>);
    static_assert(std::is_nothrow_move_constructible_v<c::SubscriptionState>);
    const auto source = schema();
    auto first = state(source);
    auto second = state(source);
    auto work = turn();
    const std::array inputs{request(source, u"A")};
    REQUIRE(first.add(inputs, work));
    REQUIRE(second.add(inputs, work));
    const auto firstToken = first.entries()[0].membership;
    const auto secondToken = second.entries()[0].membership;
    CHECK_FALSE(second.isCurrent(firstToken));
    CHECK_FALSE(first.isCurrent(secondToken));
    auto moved = std::move(first);
    CHECK(moved.isCurrent(firstToken));
    CHECK(first.entries().empty());
    CHECK(first.revision() == 0);
    CHECK(first.add(inputs, work).error() == c::SubscriptionError::INVALID_STATE);
    c::SubscriptionWork noCommands{0, 100, 100};
    CHECK(first.clear(noCommands).error() == c::SubscriptionError::WORK_LIMIT);
    second = std::move(moved);
    CHECK(second.isCurrent(firstToken));
    CHECK_FALSE(second.isCurrent(secondToken));
    CHECK_FALSE(second.isCurrent(c::SubscriptionMembership{}));
    second = std::move(second);
    CHECK(second.isCurrent(firstToken));
}

TEST_CASE("subscription exact capacity and old plus candidate peaks are enforced") {
    const auto source = schema();
    auto capacities = limits();
    capacities.entries = 1;
    capacities.symbolUnits = 1;
    capacities.totalSymbolUnits = 1;
    capacities.peakEntries = 2;
    capacities.peakSymbolUnits = 2;
    auto owner = state(source, c::SubscriptionFamily::STREAM, capacities);
    auto work = turn();
    const std::array a{request(source, u"A")};
    REQUIRE(owner.add(a, work));
    const auto token = owner.entries()[0].membership;
    const std::array two{request(source, u"B")};
    CHECK(owner.add(two, work).error() == c::SubscriptionError::RESOURCE_LIMIT);
    unchanged(owner, token, 1, u"A");
    const std::array tooLong{request(source, u"AB")};
    CHECK(owner.replace(tooLong, work).error() == c::SubscriptionError::RESOURCE_LIMIT);
    REQUIRE(owner.replace(two, work));
    CHECK(owner.entries()[0].key.symbol == u"B");

    capacities.peakEntries = 1;
    auto entryPeak = state(source, c::SubscriptionFamily::STREAM, capacities);
    REQUIRE(entryPeak.add(a, work));
    CHECK(entryPeak.replace(two, work).error() == c::SubscriptionError::RESOURCE_LIMIT);
    REQUIRE(entryPeak.clear(work));

    capacities.peakEntries = 2;
    capacities.peakSymbolUnits = 1;
    auto unitPeak = state(source, c::SubscriptionFamily::STREAM, capacities);
    REQUIRE(unitPeak.add(a, work));
    CHECK(unitPeak.replace(two, work).error() == c::SubscriptionError::RESOURCE_LIMIT);

    auto zero = capacities;
    zero.entries = 0;
    zero.totalSymbolUnits = 0;
    zero.commandEntries = 0;
    auto empty = state(source, c::SubscriptionFamily::STREAM, zero);
    CHECK(empty.add(a, work).error() == c::SubscriptionError::RESOURCE_LIMIT);
    CHECK_FALSE(empty.clear(work)->changed);
    zero.schemaRecords = 0;
    CHECK(c::SubscriptionState::create(source, c::SubscriptionFamily::STREAM, zero).error() ==
          c::SubscriptionError::RESOURCE_LIMIT);
}

TEST_CASE("subscription aggregate and zero UTF16 storage are independent finite limits") {
    const auto source = schema();
    auto capacities = limits();
    capacities.totalSymbolUnits = 1;
    auto aggregate = state(source, c::SubscriptionFamily::STREAM, capacities);
    auto work = turn();
    const std::array initial{request(source, u"A")};
    REQUIRE(aggregate.add(initial, work));
    const auto token = aggregate.entries()[0].membership;
    const std::array extra{request(source, u"B")};
    CHECK(aggregate.add(extra, work).error() == c::SubscriptionError::RESOURCE_LIMIT);
    unchanged(aggregate, token, 1, u"A");

    capacities.symbolUnits = 0;
    capacities.totalSymbolUnits = 0;
    capacities.peakSymbolUnits = 0;
    auto zero = state(source, c::SubscriptionFamily::STREAM, capacities);
    const std::array empty{request(source, u"")};
    REQUIRE(zero.add(empty, work));
    CHECK(zero.entries()[0].key.symbol.empty());
    CHECK(zero.entries()[0].key.symbol.data() != nullptr);
    CHECK(zero.add(initial, work).error() == c::SubscriptionError::RESOURCE_LIMIT);
}

TEST_CASE("subscription duplicates consume input and turn work without lifetime quotas") {
    const auto source = schema();
    auto capacities = limits();
    capacities.commandEntries = 1;
    auto owner = state(source, c::SubscriptionFamily::STREAM, capacities);
    const auto input = request(source, u"A");
    const std::array duplicates{input, input};
    auto work = turn();
    CHECK(owner.add(duplicates, work).error() == c::SubscriptionError::RESOURCE_LIMIT);
    CHECK(work.commands == 99);
    CHECK(owner.entries().empty());
    const std::array inputs{input};
    c::SubscriptionWork noCommands{0, 100, 100};
    CHECK(owner.add(inputs, noCommands).error() == c::SubscriptionError::WORK_LIMIT);
    c::SubscriptionWork shortTurn{2, 2, 0};
    CHECK(owner.add(inputs, shortTurn).error() == c::SubscriptionError::WORK_LIMIT);
    CHECK(shortTurn.commands == 1);
    CHECK(shortTurn.keyVisits == 0);
    CHECK(owner.add(inputs, shortTurn).error() == c::SubscriptionError::WORK_LIMIT);
    CHECK(shortTurn.commands == 0);
    CHECK(owner.revision() == 0);

    for (int i = 0; i < 32; ++i) {
        auto fresh = turn();
        REQUIRE(owner.add(inputs, fresh));
        REQUIRE(owner.clear(fresh));
    }

    CHECK(owner.revision() == 64);
}

TEST_CASE("subscription collapsed duplicates still consume examined UTF16 work") {
    const auto source = schema();
    auto single = state(source);
    auto duplicate = state(source);
    auto baseline = turn();
    const std::array one{request(source, u"A")};
    const std::array repeated{one[0], one[0]};
    REQUIRE(single.add(one, baseline));
    const auto symbolCost = turn().symbolUnits - baseline.symbolUnits;
    c::SubscriptionWork constrained{1, 10000, symbolCost};
    const auto result = duplicate.add(repeated, constrained);

    REQUIRE_FALSE(result);
    CHECK(result.error() == c::SubscriptionError::WORK_LIMIT);
    CHECK(duplicate.entries().empty());
    CHECK(duplicate.revision() == 0);
}

TEST_CASE("subscription captured schema identity survives declaration owner destruction") {
    auto source = schema();
    const auto context = source.recordContext(1);
    auto owner = state(source, c::SubscriptionFamily::HISTORY);
    source = schema();
    auto work = turn();
    const std::array retained{c::SubscriptionRequest{{context, u"A"}, 0}};
    REQUIRE(owner.add(retained, work));
    const std::array replacementContext{request(source, u"A", 1, 0)};
    CHECK(owner.add(replacementContext, work).error() == c::SubscriptionError::INVALID_CONTEXT);
    CHECK(owner.entries().size() == 1);
}

TEST_CASE("subscription candidate may safely copy borrowed current symbols") {
    const auto source = schema();
    auto owner = state(source);
    auto work = turn();
    const std::array initial{request(source, u"retained")};
    REQUIRE(owner.add(initial, work));
    const auto token = owner.entries()[0].membership;
    const std::array aliased{c::SubscriptionRequest{owner.entries()[0].key, {}}, request(source, u"new")};
    REQUIRE(owner.replace(aliased, work));
    CHECK(owner.entries()[0].key.symbol == u"retained");
    CHECK(owner.isCurrent(token));
}

TEST_CASE("staged subscription replacement owns inputs and publishes with no allocation") {
    using Access = c::detail::SubscriptionStateAccess;
    const auto source = schema();
    auto owner = state(source);
    auto work = turn();
    const std::array initial{request(source, u"kept"), request(source, u"removed")};
    REQUIRE(owner.add(initial, work));
    const auto kept = owner.entries()[0].membership;
    const auto removed = owner.entries()[1].membership;
    const auto *published = owner.entries().data();
    std::u16string incoming = u"added";
    const std::array replacement{c::SubscriptionRequest{owner.entries()[0].key, {}}, request(source, incoming)};

    auto candidate = Access::prepareReplace(owner, replacement, work);
    REQUIRE(candidate);
    incoming.assign(u"changed by caller");
    CHECK(owner.revision() == 1);
    CHECK(owner.entries().data() == published);
    CHECK(owner.isCurrent(kept));
    CHECK(owner.isCurrent(removed));
    REQUIRE(Access::revision(owner, *candidate) == 2);
    {
        const auto staged = Access::entries(owner, *candidate);
        REQUIRE(staged.size() == 2);
        CHECK(staged[1].key.symbol == u"added");
        CHECK(Access::isCurrent(owner, *candidate, kept));
        CHECK_FALSE(Access::isCurrent(owner, *candidate, removed));
        CHECK(Access::isCurrent(owner, *candidate, staged[1].membership));
        CHECK_FALSE(owner.isCurrent(staged[1].membership));
    }

    const auto change = [&] {
        AllocationFault fault(0);

        return Access::commitReplace(owner, std::move(*candidate));
    }();
    REQUIRE(change);
    CHECK(observedAllocations == 0);
    CHECK(change->changed);
    CHECK(change->revision == 2);
    REQUIRE(owner.entries().size() == 2);
    CHECK(owner.entries()[1].key.symbol == u"added");
    CHECK(owner.isCurrent(kept));
    CHECK_FALSE(owner.isCurrent(removed));

    const auto reused = Access::commitReplace(owner, std::move(*candidate));
    REQUIRE_FALSE(reused);
    CHECK(reused.error() == c::SubscriptionError::INVALID_STATE);
}

TEST_CASE("staged subscription reservation is released on rejection and abandonment") {
    using Access = c::detail::SubscriptionStateAccess;
    const auto source = schema();
    auto owner = state(source);
    auto foreign = state(source);
    auto work = turn();
    const std::array initial{request(source, u"old")};
    const std::array replacement{request(source, u"new")};
    REQUIRE(owner.add(initial, work));
    REQUIRE(foreign.add(initial, work));
    const auto token = owner.entries()[0].membership;
    const auto foreignToken = foreign.entries()[0].membership;

    {
        auto candidate = Access::prepareReplace(owner, replacement, work);
        REQUIRE(candidate);
        const auto competing = Access::prepareReplace(owner, replacement, work);
        REQUIRE_FALSE(competing);
        CHECK(competing.error() == c::SubscriptionError::INVALID_STATE);
        const auto ordinary = owner.clear(work);
        REQUIRE_FALSE(ordinary);
        CHECK(ordinary.error() == c::SubscriptionError::INVALID_STATE);
        unchanged(owner, token, 1, u"old");
    }

    auto candidate = Access::prepareReplace(owner, replacement, work);
    REQUIRE(candidate);
    CHECK_FALSE(Access::revision(foreign, *candidate));
    CHECK(Access::entries(foreign, *candidate).empty());
    CHECK_FALSE(Access::isCurrent(foreign, *candidate, foreignToken));
    const auto wrong = Access::commitReplace(foreign, std::move(*candidate));
    REQUIRE_FALSE(wrong);
    CHECK(wrong.error() == c::SubscriptionError::INVALID_STATE);
    unchanged(foreign, foreignToken, 1, u"old");
    unchanged(owner, token, 1, u"old");
    REQUIRE(owner.replace(replacement, work));
    CHECK(owner.revision() == 2);
}

TEST_CASE("staged subscription replacement follows its logical owner through move") {
    using Access = c::detail::SubscriptionStateAccess;
    const auto source = schema();
    auto owner = state(source);
    auto work = turn();
    const std::array original{request(source, u"old")};
    const std::array replacement{request(source, u"new")};
    REQUIRE(owner.add(original, work));
    const auto token = owner.entries()[0].membership;

    auto candidate = Access::prepareReplace(owner, replacement, work);
    REQUIRE(candidate);
    auto moved = std::move(owner);
    CHECK(owner.entries().empty());
    unchanged(moved, token, 1, u"old");
    auto change = Access::commitReplace(moved, std::move(*candidate));
    REQUIRE(change);
    CHECK(change->revision == 2);
    CHECK_FALSE(moved.isCurrent(token));

    const auto *published = moved.entries().data();
    auto noOp = Access::prepareReplace(moved, replacement, work);
    REQUIRE(noOp);
    CHECK(Access::revision(moved, *noOp) == 2);
    CHECK(Access::entries(moved, *noOp).data() == published);
    change = Access::commitReplace(moved, std::move(*noOp));
    REQUIRE(change);
    CHECK_FALSE(change->changed);
    CHECK(change->revision == 2);
    CHECK(moved.entries().data() == published);
}

TEST_CASE("staged subscription replacement cannot commit after owner identity is replaced") {
    using Access = c::detail::SubscriptionStateAccess;
    const auto source = schema();
    auto owner = state(source);
    auto incoming = state(source);
    auto work = turn();
    const std::array initial{request(source, u"old")};
    const std::array replacement{request(source, u"new")};
    REQUIRE(owner.add(initial, work));
    REQUIRE(incoming.add(initial, work));
    const auto incomingToken = incoming.entries()[0].membership;

    auto retired = Access::prepareReplace(owner, replacement, work);
    REQUIRE(retired);
    auto transferred = Access::prepareReplace(incoming, replacement, work);
    REQUIRE(transferred);
    owner = std::move(incoming);

    const auto stale = Access::commitReplace(owner, std::move(*retired));
    REQUIRE_FALSE(stale);
    CHECK(stale.error() == c::SubscriptionError::INVALID_STATE);
    unchanged(owner, incomingToken, 1, u"old");
    const auto competing = owner.clear(work);
    REQUIRE_FALSE(competing);
    CHECK(competing.error() == c::SubscriptionError::INVALID_STATE);
    REQUIRE(Access::commitReplace(owner, std::move(*transferred)));
    CHECK(owner.revision() == 2);
}

TEST_CASE("subscription work failure throughout candidate construction preserves every membership") {
    const auto source = schema();
    auto owner = state(source);
    auto work = turn();
    const std::array initial{request(source, u"A"), request(source, u"B")};
    REQUIRE(owner.add(initial, work));
    const auto a = owner.entries()[0].membership;
    const auto b = owner.entries()[1].membership;
    const auto *published = owner.entries().data();
    const std::array replacement{initial[0], request(source, u"C")};
    bool completed = false;

    for (std::size_t credits = 0; credits < 256 && !completed; ++credits) {
        c::SubscriptionWork constrained{1, credits, 10000};
        const auto result = owner.replace(replacement, constrained);

        if (result) {
            completed = true;
            CHECK(owner.isCurrent(a));
            CHECK_FALSE(owner.isCurrent(b));
            CHECK(result->revision == 2);
        } else {
            CHECK(result.error() == c::SubscriptionError::WORK_LIMIT);
            CHECK(owner.revision() == 1);
            CHECK(owner.entries().data() == published);
            CHECK(owner.isCurrent(a));
            CHECK(owner.isCurrent(b));
        }
    }

    CHECK(completed);
}

TEST_CASE("subscription allocation failure at each actual candidate allocation rolls back") {
    const auto source = schema();
    auto owner = state(source);
    auto work = turn();
    const std::array initial{request(source, u"A")};
    REQUIRE(owner.add(initial, work));
    const auto token = owner.entries()[0].membership;
    const auto *published = owner.entries().data();
    const std::array replacement{request(source, u"different")};

    for (std::size_t allowed = 0; allowed < 2; ++allowed) {
        const auto beforeCommands = work.commands;
        const auto result = [&] {
            AllocationFault fault(allowed);

            return owner.replace(replacement, work);
        }();

        REQUIRE_FALSE(result);
        CHECK(result.error() == c::SubscriptionError::ALLOCATION_FAILURE);
        CHECK(observedAllocations == allowed + 1);
        CHECK(work.commands == beforeCommands - 1);
        CHECK(owner.entries().data() == published);
        unchanged(owner, token, 1, u"A");
    }

    const auto success = [&] {
        AllocationFault fault(2);

        return owner.replace(replacement, work);
    }();

    REQUIRE(success);
    CHECK(observedAllocations == 2);
    CHECK(success->revision == 2);
    CHECK_FALSE(owner.isCurrent(token));
    CHECK(owner.entries()[0].key.symbol == u"different");
}

TEST_CASE("subscription creation allocation failures and allocation-free clear are explicit") {
    const auto source = schema();

    for (std::size_t allowed = 0; allowed < 2; ++allowed) {
        const auto result = [&] {
            AllocationFault fault(allowed);

            return c::SubscriptionState::create(source, c::SubscriptionFamily::STREAM, limits());
        }();

        REQUIRE_FALSE(result);
        CHECK(result.error() == c::SubscriptionError::ALLOCATION_FAILURE);
        CHECK(observedAllocations == allowed + 1);
    }

    auto owner = state(source);
    auto work = turn();
    const std::array inputs{request(source, u"A")};
    REQUIRE(owner.add(inputs, work));
    const auto result = [&] {
        AllocationFault fault(0);

        return owner.clear(work);
    }();

    REQUIRE(result);
    CHECK(result->changed);
    CHECK(observedAllocations == 0);
}
