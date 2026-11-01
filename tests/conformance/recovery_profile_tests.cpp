// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner transport_recovery_profile: unqualified preparation without allocation or input adoption.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_configuration.hpp"
#include "recovery_copy_profile.hpp"
#include "recovery_test_support.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> rejectAllocations{false};
std::atomic<std::size_t> rejectedAllocations{0};
} // namespace

/** Reject allocation only during the isolated preparation call.
 * @param size Requested byte count, with zero treated as one byte.
 * @return Owned storage or a standard allocation exception.
 */
void *operator new(std::size_t size) {
    if (rejectAllocations.load()) {
        rejectedAllocations.fetch_add(1);
        throw std::bad_alloc{};
    }

    if (auto *value = std::malloc(size == 0 ? 1 : size)) {
        return value;
    }

    throw std::bad_alloc{};
}

void *operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void *value) noexcept {
    std::free(value);
}

void operator delete[](void *value) noexcept {
    std::free(value);
}

void operator delete(void *value, std::size_t) noexcept {
    std::free(value);
}

void operator delete[](void *value, std::size_t) noexcept {
    std::free(value);
}

TEST_CASE("an unknown copy implementation refuses preparation before allocating or spending input work") {
    namespace root = org::ttldtor::qdcxx;
    namespace detail = root::transport::detail;
    namespace support = root::test::recovery;
    REQUIRE(detail::recoveryCopyProfile() == 0);
    auto local = support::schema();
    auto endpoint = support::endpoint(local, support::settings(1));
    auto blueprint = support::blueprint();
    const auto *backing = blueprint.data();
    const auto capacity = blueprint.capacity();
    root::transport::RecoveryQueues queues;
    root::transport::RecoveryPreparationWork work{};
    std::optional<std::expected<detail::RecoveryConfiguration, root::transport::RecoveryDiagnostic>> result;
    rejectedAllocations.store(0);
    rejectAllocations.store(true);

    try {
        result.emplace(detail::RecoveryConfiguration::prepare(
            endpoint, blueprint, queues, {}, root::client::EndpointSecurity::CREDENTIAL_FREE_PLAIN, work));
    } catch (...) {
        rejectAllocations.store(false);
        throw;
    }

    rejectAllocations.store(false);
    REQUIRE(result);
    REQUIRE_FALSE(*result);
    CHECK(result->error().error == root::transport::RecoveryError::UNSUPPORTED_PROFILE);
    CHECK(rejectedAllocations.load() == 0);
    CHECK(blueprint.data() == backing);
    CHECK(blueprint.capacity() == capacity);
    REQUIRE(blueprint.size() == 1);
    CHECK(blueprint.front().wireName == u"R");
    CHECK_FALSE(queues.ticker);
    CHECK_FALSE(queues.stream);
    CHECK_FALSE(queues.history);
    CHECK(work.recovery.operations == 0);
    CHECK(work.recovery.visits == 0);
    CHECK(work.recovery.textUnits == 0);
    CHECK(work.recovery.bytes == 0);
}
