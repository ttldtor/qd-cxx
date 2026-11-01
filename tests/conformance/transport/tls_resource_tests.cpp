// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/transport/tls_resources.hpp"

#include <array>
#include <doctest/doctest.h>
#include <limits>
#include <type_traits>
#include <utility>

namespace {
namespace t = org::ttldtor::qdcxx::transport;
using Count = std::uint64_t;
constexpr Count COUNT_MAX = std::numeric_limits<Count>::max(); ///< Checked coordinate boundary.

/** Make explicit maximal immutable endpoint ceilings.
 * @return Owned input fixture, with no SDK object or application defaults.
 */
t::TlsResourceLimits maximumLimits() noexcept {
    return {524288, 256, 65536, 65536, 255};
}

/** Make synthetic arithmetic-only same-image host maxima.
 * @return Owned twenty-field fixture; these values are not observed state or supported defaults.
 */
t::TlsHostLimits hostFixture() noexcept {
    return {5000, 4, 32, 12, 32, 2048, 128, 8, 64, 16, 4, 8, 2, 2, 4096, 1024, 4096, 1, 1, 1800};
}

/** Locate one named work coordinate.
 * @param vector Owned numeric vector to inspect.
 * @param unit Named coordinate.
 * @return Exact count for that coordinate.
 */
Count unit(const t::TlsWorkVector &vector, t::TlsWorkUnit unit) noexcept {
    return vector.units[static_cast<std::size_t>(unit)];
}

using HostMember = Count t::TlsHostLimits::*; ///< Each independent copied host ceiling.
constexpr std::array<HostMember, 20> HOST_MEMBERS = {&t::TlsHostLimits::namemapGroups,
                                                     &t::TlsHostLimits::queryCount,
                                                     &t::TlsHostLimits::queryBytes,
                                                     &t::TlsHostLimits::propertyNames,
                                                     &t::TlsHostLimits::propertyValues,
                                                     &t::TlsHostLimits::propertyStringBytes,
                                                     &t::TlsHostLimits::propertyStringMaxBytes,
                                                     &t::TlsHostLimits::participatingThreads,
                                                     &t::TlsHostLimits::globalThreadHandlers,
                                                     &t::TlsHostLimits::ownerThreadHandlers,
                                                     &t::TlsHostLimits::ambientThreadPairs,
                                                     &t::TlsHostLimits::ambientGenerators,
                                                     &t::TlsHostLimits::concurrentEntries,
                                                     &t::TlsHostLimits::errorThreads,
                                                     &t::TlsHostLimits::errorCapacityBytes,
                                                     &t::TlsHostLimits::errorSlotCapacityBytes,
                                                     &t::TlsHostLimits::errorDebugBytes,
                                                     &t::TlsHostLimits::failedPrivatePrefixes,
                                                     &t::TlsHostLimits::failedAmbientPrefixes,
                                                     &t::TlsHostLimits::errorTableEntries};
using LimitMember = Count t::TlsResourceLimits::*; ///< Each independent copied endpoint ceiling.
constexpr std::array<LimitMember, 5> LIMIT_MEMBERS = {
    &t::TlsResourceLimits::trustBytes, &t::TlsResourceLimits::anchors, &t::TlsResourceLimits::certificateBytes,
    &t::TlsResourceLimits::certificateRequestBytes, &t::TlsResourceLimits::identityBytes};

static_assert(noexcept(t::tlsRequirements({}, {})));
static_assert(noexcept(t::addTlsWork({}, {})));
static_assert(std::is_trivially_copyable_v<t::TlsWorkShape>);
static_assert(std::is_trivially_copyable_v<t::TlsWorkVector>);
} // namespace

TEST_CASE("TLS exact source-profile maximum backing and complete work vectors") {
    const auto result = t::tlsRequirements(maximumLimits(), hostFixture());
    REQUIRE(result);
    const auto &requirements = *result;
    CHECK(requirements.shape.profileRevision == t::TLS_RESOURCE_PROFILE_REVISION);
    CHECK(requirements.shape.limits == maximumLimits());
    CHECK(requirements.shape.host == hostFixture());
    CHECK(requirements.sdk.instanceBytes == 601607223);
    CHECK(requirements.sdk.sharedBytes == 44048771);
    CHECK(requirements.sdk.totalBytes == 645655994);
    CHECK(requirements.sdk.failedPrivatePrefixBytes == 40865186);
    CHECK(requirements.sdk.failedAmbientPrefixBytes == 2536);

    // Independently evaluated source-profile table; coordinate order is the public named-unit order.
    const t::TlsWorkVector factory{{1,
                                    257,
                                    256,
                                    256,
                                    213159936,
                                    256,
                                    256,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    524288,
                                    255,
                                    25,
                                    25579192329ULL,
                                    76737576987ULL,
                                    76737576987ULL,
                                    76737576987ULL,
                                    153475153974ULL,
                                    849561714823077ULL,
                                    153475153974ULL,
                                    5658168501559458ULL}};
    const t::TlsWorkVector initial{{0,
                                    0,
                                    1772,
                                    1772,
                                    1475466432,
                                    1773,
                                    0,
                                    1,
                                    1,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    177055974245ULL,
                                    531167922735ULL,
                                    531167922735ULL,
                                    531167922735ULL,
                                    1062335845470ULL,
                                    5880560072599185ULL,
                                    1062335845470ULL,
                                    39165135614942490ULL}};
    const t::TlsWorkVector entry{{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 110684, 0, 0, 0, 17408, 0, 2}};
    const t::TlsWorkVector retirement{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1}};
    CHECK(requirements.factory == factory);
    CHECK(requirements.initialHandshake == initial);
    CHECK(requirements.entry == entry);
    CHECK(requirements.retirement == retirement);
    CHECK(unit(factory, t::TlsWorkUnit::RNG_EVENT) + unit(initial, t::TlsWorkUnit::RNG_EVENT) == 202635166574ULL);
    auto close = entry;
    close.units[static_cast<std::size_t>(t::TlsWorkUnit::CLOSE_CONTROL)] = 1;
    CHECK(requirements.close == close);
    const auto factoryAdmission = t::addTlsWork(factory, retirement);
    const auto initialStart = t::addTlsWork(initial, entry);
    REQUIRE(factoryAdmission);
    REQUIRE(initialStart);
    CHECK(requirements.factoryAdmission == *factoryAdmission);
    CHECK(requirements.initialStart == *initialStart);
}

TEST_CASE("TLS all indivisible phase coordinates refuse atomically and preserve excess credits") {
    const auto result = t::tlsRequirements(maximumLimits(), hostFixture());
    REQUIRE(result);
    const std::array phases{
        result->factoryAdmission, result->initialHandshake, result->initialStart, result->entry, result->close,
        result->retirement};
    std::size_t oneShortCases = 0;

    for (const auto &need : phases) {
        t::TlsWork zero{result->shape, {}};
        const auto refused = t::admitTlsWork(zero, result->shape, need);
        REQUIRE_FALSE(refused);
        CHECK(refused.error().error == t::TlsResourceError::WORK_LIMIT);
        CHECK(zero.remaining == t::TlsWorkVector{});
        CHECK(zero.shape == result->shape);

        for (std::size_t index = 0; index < t::TLS_WORK_UNIT_COUNT; ++index) {
            if (need.units[index] == 0) {
                continue;
            }

            ++oneShortCases;
            auto shortNeed = need;
            --shortNeed.units[index];
            t::TlsWork work{result->shape, shortNeed};
            const auto failure = t::admitTlsWork(work, result->shape, need);
            REQUIRE_FALSE(failure);
            CHECK(failure.error().error == t::TlsResourceError::WORK_LIMIT);
            CHECK(work.remaining == shortNeed);
            CHECK(work.shape == result->shape);
        }

        t::TlsWork exact{result->shape, need};
        REQUIRE(t::admitTlsWork(exact, result->shape, need));
        CHECK(exact.remaining == t::TlsWorkVector{});
        auto excess = need;

        for (auto &coordinate : excess.units) {
            coordinate += 7;
        }

        t::TlsWork sufficient{result->shape, excess};
        REQUIRE(t::admitTlsWork(sufficient, result->shape, need));

        for (const Count remaining : sufficient.remaining.units) {
            CHECK(remaining == 7);
        }
    }

    CHECK(oneShortCases == 63);
}

TEST_CASE("TLS work identity includes every immutable field and profile") {
    const auto result = t::tlsRequirements(maximumLimits(), hostFixture());
    REQUIRE(result);

    for (const auto member : HOST_MEMBERS) {
        auto shape = result->shape;
        ++(shape.host.*member);
        t::TlsWork work{shape, result->entry};
        const auto failure = t::admitTlsWork(work, result->shape, result->entry);
        REQUIRE_FALSE(failure);
        CHECK(failure.error().error == t::TlsResourceError::SHAPE_MISMATCH);
        CHECK(work.remaining == result->entry);
        CHECK(work.shape == shape);
    }

    for (const auto member : LIMIT_MEMBERS) {
        auto shape = result->shape;
        ++(shape.limits.*member);
        t::TlsWork work{shape, result->entry};
        CHECK_FALSE(t::admitTlsWork(work, result->shape, result->entry));
        CHECK(work.remaining == result->entry);
    }

    auto oldProfile = result->shape;
    ++oldProfile.profileRevision;
    t::TlsWork old{oldProfile, result->entry};
    CHECK_FALSE(t::admitTlsWork(old, oldProfile, result->entry));
    CHECK(old.remaining == result->entry);
    t::TlsWork mismatched{result->shape, result->entry};
    CHECK_FALSE(t::admitTlsWork(mismatched, oldProfile, {}));
    CHECK(mismatched.remaining == result->entry);
}

TEST_CASE("TLS admission handles aliased values and detached copies") {
    auto limits = maximumLimits();
    auto host = hostFixture();
    const auto result = t::tlsRequirements(limits, host);
    REQUIRE(result);
    limits.trustBytes = 1;
    host.namemapGroups = 1;
    CHECK(result->shape.limits == maximumLimits());
    CHECK(result->shape.host == hostFixture());
    t::TlsWork work{result->shape, result->factoryAdmission};
    const auto ownedRetirement = result->retirement;
    REQUIRE(t::admitTlsWork(work, work.shape, work.remaining));
    CHECK(work.remaining == t::TlsWorkVector{});
    CHECK(unit(ownedRetirement, t::TlsWorkUnit::RETIRE_GRAPH) == 1);
    CHECK(unit(ownedRetirement, t::TlsWorkUnit::ABORT_CONTROL) == 1);
    REQUIRE(t::admitTlsWork(work, result->shape, {}));
    CHECK(work.remaining == t::TlsWorkVector{});
}

TEST_CASE("TLS lower input ceilings remain finite and host ceilings are independent") {
    auto limits = maximumLimits();
    limits.trustBytes = 1;
    limits.anchors = 1;
    limits.identityBytes = 1;
    const auto result = t::tlsRequirements(limits, hostFixture());
    const auto maximum = t::tlsRequirements(maximumLimits(), hostFixture());
    REQUIRE(result);
    REQUIRE(maximum);
    CHECK(result->sdk.instanceBytes == 239531473);
    CHECK(result->sdk.totalBytes == 283580244);
    CHECK(result->sdk.instanceBytes < maximum->sdk.instanceBytes);
    CHECK(result->sdk.sharedBytes == maximum->sdk.sharedBytes);
    CHECK(unit(result->factory, t::TlsWorkUnit::TRUST_READ) == 2);
    CHECK(unit(result->factory, t::TlsWorkUnit::FAMILY_ATTEMPT) == 832656);
    CHECK(unit(result->factory, t::TlsWorkUnit::RNG_EVENT) == 99918729);
    CHECK(unit(result->factory, t::TlsWorkUnit::RNG_AES_BLOCKS) == 3318600746277ULL);
    CHECK(unit(result->factory, t::TlsWorkUnit::GETRANDOM_ATTEMPT_BOUND) == 22102222692258ULL);
    CHECK(result->initialHandshake == maximum->initialHandshake);
    CHECK(result->entry == maximum->entry);
    auto loose = hostFixture();
    loose.ownerThreadHandlers = 1000;
    loose.errorSlotCapacityBytes = 8192;
    loose.propertyStringMaxBytes = 4096;
    REQUIRE(t::tlsRequirements(limits, loose));
}

TEST_CASE("TLS failed-prefix storage debt is persistent and charged at shared scope") {
    const auto original = t::tlsRequirements(maximumLimits(), hostFixture());
    REQUIRE(original);
    auto host = hostFixture();
    ++host.failedPrivatePrefixes;
    const auto privateFailure = t::tlsRequirements(maximumLimits(), host);
    REQUIRE(privateFailure);
    CHECK(privateFailure->sdk.sharedBytes - original->sdk.sharedBytes == 40865186);
    CHECK(privateFailure->sdk.instanceBytes == original->sdk.instanceBytes);
    ++host.failedAmbientPrefixes;
    const auto ambientFailure = t::tlsRequirements(maximumLimits(), host);
    REQUIRE(ambientFailure);
    CHECK(ambientFailure->sdk.sharedBytes - privateFailure->sdk.sharedBytes == 2536);
    CHECK(ambientFailure->factory == original->factory);
    CHECK(ambientFailure->retirement == original->retirement);
    host.failedPrivatePrefixes = 0;
    host.failedAmbientPrefixes = 0;
    const auto withoutDebt = t::tlsRequirements(maximumLimits(), host);
    REQUIRE(withoutDebt);
    CHECK(withoutDebt->sdk.sharedBytes > 0);
    CHECK(original->sdk.sharedBytes - withoutDebt->sdk.sharedBytes == 40867722);
}

TEST_CASE("TLS immutable input and host representation errors are typed and static") {
    for (const auto member : LIMIT_MEMBERS) {
        auto limits = maximumLimits();
        limits.*member = 0;
        const auto result = t::tlsRequirements(limits, hostFixture());
        REQUIRE_FALSE(result);
        CHECK(result.error().error == t::TlsResourceError::INVALID_CONFIGURATION);
        CHECK(result.error().stage == "tls.resource.limits");
        limits = maximumLimits();
        ++(limits.*member);
        CHECK_FALSE(t::tlsRequirements(limits, hostFixture()));
    }

    const std::array<std::pair<HostMember, Count>, 15> invalid = {
        {{&t::TlsHostLimits::namemapGroups, 2147483648ULL},
         {&t::TlsHostLimits::propertyNames, 2147483648ULL},
         {&t::TlsHostLimits::propertyValues, 2147483648ULL},
         {&t::TlsHostLimits::participatingThreads, 2147483648ULL},
         {&t::TlsHostLimits::errorSlotCapacityBytes, 2147483648ULL},
         {&t::TlsHostLimits::concurrentEntries, 0},
         {&t::TlsHostLimits::errorThreads, 0},
         {&t::TlsHostLimits::errorThreads, 9},
         {&t::TlsHostLimits::errorTableEntries, 1772},
         {&t::TlsHostLimits::queryCount, 1},
         {&t::TlsHostLimits::queryBytes, 15},
         {&t::TlsHostLimits::propertyNames, 5},
         {&t::TlsHostLimits::propertyValues, 21},
         {&t::TlsHostLimits::ambientGenerators, 9},
         {&t::TlsHostLimits::participatingThreads, 1}}};

    for (const auto &[member, value] : invalid) {
        auto host = hostFixture();
        host.*member = value;
        const auto result = t::tlsRequirements(maximumLimits(), host);
        REQUIRE_FALSE(result);
        CHECK(result.error().error == t::TlsResourceError::INVALID_CONFIGURATION);
        CHECK_FALSE(result.error().stage.empty());
    }
}

TEST_CASE("TLS checked additions and actual requirement intermediates cannot wrap") {
    for (std::size_t index = 0; index < t::TLS_WORK_UNIT_COUNT; ++index) {
        t::TlsWorkVector left;
        t::TlsWorkVector right;
        left.units[index] = COUNT_MAX;
        right.units[index] = 1;
        const auto result = t::addTlsWork(left, right);
        REQUIRE_FALSE(result);
        CHECK(result.error().error == t::TlsResourceError::ARITHMETIC_OVERFLOW);
        CHECK(left.units[index] == COUNT_MAX);
        CHECK(right.units[index] == 1);
        right.units[index] = 0;
        const auto boundary = t::addTlsWork(left, right);
        REQUIRE(boundary);
        CHECK(boundary->units[index] == COUNT_MAX);
    }

    const std::array overflowing{&t::TlsHostLimits::queryCount,
                                 &t::TlsHostLimits::queryBytes,
                                 &t::TlsHostLimits::propertyStringBytes,
                                 &t::TlsHostLimits::propertyStringMaxBytes,
                                 &t::TlsHostLimits::globalThreadHandlers,
                                 &t::TlsHostLimits::ownerThreadHandlers,
                                 &t::TlsHostLimits::ambientThreadPairs,
                                 &t::TlsHostLimits::concurrentEntries,
                                 &t::TlsHostLimits::errorCapacityBytes,
                                 &t::TlsHostLimits::errorDebugBytes,
                                 &t::TlsHostLimits::failedPrivatePrefixes,
                                 &t::TlsHostLimits::failedAmbientPrefixes,
                                 &t::TlsHostLimits::errorTableEntries};

    for (const auto member : overflowing) {
        auto host = hostFixture();
        host.*member = COUNT_MAX;
        const auto result = t::tlsRequirements(maximumLimits(), host);
        REQUIRE_FALSE(result);
        CHECK(result.error().error == t::TlsResourceError::ARITHMETIC_OVERFLOW);
    }
}
