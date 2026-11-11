// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/derived_order.hpp"
#include "qd/events/market_maker.hpp"
#include <cstdlib>
#include <doctest/doctest.h>
#include <iostream>
#include <new>

namespace {
namespace q = org::ttldtor::qdcxx;
bool failing = false;
std::size_t remaining = 0;
constexpr q::core::SchemaLimits SCHEMA{4, 128, 256, 128, 32768};
constexpr q::events::MappingLimits MAPPING{128, 256, 32, 64, 256, 128, 32768, 1000000};
constexpr q::events::EventValueLimits VALUES{128, 8, 136};

/** Failure is armed only around one production call, never around test-framework reporting. */
struct AllocationFault {
    explicit AllocationFault(std::size_t count) {
        remaining = count;
        failing = true;
    }

    ~AllocationFault() {
        failing = false;
    }
};

/** Sweep each reached ordinary allocation until a complete candidate succeeds.
 * @tparam Operation Production operation returning expected<T,MappingError>.
 * @param operation Pure preparation/projection attempt with immutable source values.
 * @return Number of reached, atomically refused allocation sites.
 */
template <class Operation> std::size_t sweep(Operation operation) {
    constexpr std::size_t MAXIMUM_ALLOCATION_SITES = 128;

    for (std::size_t site = 0; site < MAXIMUM_ALLOCATION_SITES; ++site) {
        bool succeeded = false;
        std::optional<q::events::MappingError> error;
        {
            AllocationFault fault(site);
            auto result = operation();
            succeeded = bool(result);

            if (!result) {
                error = result.error();
            }
        }

        if (succeeded) {
            CHECK(site > 0);

            return site;
        }

        REQUIRE(error);
        CHECK(*error == q::events::MappingError::ALLOCATION_FAILURE);
    }

    FAIL("Production operation exceeded finite allocation-site sweep");

    return 0;
}
} // namespace

/** @param bytes Ordinary requested bytes. @return Owned allocation or explicit injected failure. */
void *operator new(std::size_t bytes) {
    if (failing) {
        if (remaining == 0) {
            throw std::bad_alloc{};
        }

        --remaining;
    }

    if (auto p = std::malloc(bytes ? bytes : 1)) {
        return p;
    }

    throw std::bad_alloc{};
}

void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
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

TEST_CASE("MarketMaker and derived Order atomically refuse each actual preparation/output allocation") {
    auto schema = q::events::defaultMarketMakerSchema(SCHEMA);
    REQUIRE(schema);
    auto failures = sweep([&] {
        return q::events::PreparedMarketMaker::prepare(*schema, 0, MAPPING);
    });
    failures += sweep([&] {
        return q::events::PreparedDerivedOrder::prepare(*schema, 0, q::events::DerivedOrderKind::MARKET_MAKER, MAPPING);
    });
    auto maker = q::events::PreparedMarketMaker::prepare(*schema, 0, MAPPING);
    REQUIRE(maker);
    auto derived =
        q::events::PreparedDerivedOrder::prepare(*schema, 0, q::events::DerivedOrderKind::MARKET_MAKER, MAPPING);
    REQUIRE(derived);
    std::vector<q::core::FieldValue> fields;

    for (const auto &field : schema->records()[0].fields) {
        fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
    }

    fields[1].raw = -1;
    const std::u16string symbol(128, u'A');
    failures += sweep([&] {
        return maker->map(symbol, {schema->recordContext(0), fields}, VALUES);
    });
    failures += sweep([&] {
        return derived->map(symbol, {schema->recordContext(0), fields}, q::events::DerivedOrderRole::BID, VALUES);
    });
    auto event = derived->map(symbol, {schema->recordContext(0), fields}, q::events::DerivedOrderRole::BID, VALUES);
    REQUIRE(event);
    CHECK(fields[1].raw == -1);
    CHECK(event->marketMaker == std::u16string(8, u'\u00ff'));
    fields.clear();
    schema = std::unexpected(q::core::SchemaError::INVALID_SCHEMA);
    CHECK(event->symbol == symbol);
    CHECK(event->marketMaker->size() == 8);
    std::cout << "DERIVED_ALLOCATION_REFUSALS " << failures << " MM=" << sizeof(q::events::MarketMaker)
              << " Order=" << sizeof(q::events::Order) << '\n';
}
