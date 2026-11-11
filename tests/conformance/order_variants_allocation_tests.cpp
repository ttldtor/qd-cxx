// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest caller: no arguments. Inject each ordinary creator-thread allocation in pure prepare/map;
// require atomic typed failure, unchanged input, and successful reuse. No network, threads or cache writes.
#include "qd/events/analytic_order.hpp"
#include "qd/events/spread_order.hpp"
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
thread_local bool armed = false;
thread_local std::size_t remaining = 0;
namespace q = org::ttldtor::qdcxx;
constexpr q::core::SchemaLimits SCHEMA{4, 128, 256, 128, 32768};
constexpr q::events::MappingLimits MAPPING{128, 256, 32, 64, 256, 128, 65536, 1000000};
constexpr q::events::EventValueLimits VALUES{128, 128, 256};

/** Sweep every ordinary allocation, disarming before result inspection/destruction. */
template <class Action, class Success> bool sweep(Action action, Success success, std::size_t &failures) {
    constexpr std::size_t MAX_POSITIONS = 128;

    for (std::size_t i = 0; i < MAX_POSITIONS; ++i) {
        remaining = i;
        armed = true;
        const auto result = action();
        armed = false;

        if (result) {
            return success(*result);
        }

        if (result.error() != q::events::MappingError::ALLOCATION_FAILURE) {
            return false;
        }

        ++failures;
    }

    return false;
}

/** Create canonical complete zero/null values before injection begins. */
std::vector<q::core::FieldValue> values(const q::core::LocalSchema &schema) {
    std::vector<q::core::FieldValue> result;

    for (const auto &f : schema.records()[0].fields) {
        result.push_back({static_cast<q::core::SerialFieldType>(f.serialTypeId), 0});
    }

    return result;
}
} // namespace

/** Allocate ordinary test storage or inject one selected operation's failure. */
void *operator new(std::size_t size) {
    if (armed) {
        if (remaining == 0) {
            throw std::bad_alloc();
        }

        --remaining;
    }

    if (auto *p = std::malloc(size ? size : 1)) {
        return p;
    }

    throw std::bad_alloc();
}

/** Allocate arrays through the same ordinary injection boundary. */
void *operator new[](std::size_t size) {
    return ::operator new(size);
}

/** Release ordinary matching storage. */
void operator delete(void *p) noexcept {
    std::free(p);
}

/** Release ordinary matching array storage. */
void operator delete[](void *p) noexcept {
    std::free(p);
}

/** Release sized ordinary matching storage. */
void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}

/** Release sized ordinary matching array storage. */
void operator delete[](void *p, std::size_t) noexcept {
    std::free(p);
}

int main() {
    auto spread = q::events::defaultSpreadOrderSchema(SCHEMA);
    auto analytic = q::events::defaultAnalyticOrderSchema(SCHEMA);

    if (!spread || !analytic) {
        return 1;
    }

    std::size_t failures = 0;

    if (!sweep(
            [&] {
                return q::events::PreparedSpreadOrder::prepare(*spread, 0, MAPPING);
            },
            [](const auto &p) {
                return p.sourceId() == 0;
            },
            failures)) {
        return 2;
    }

    if (!sweep(
            [&] {
                return q::events::PreparedAnalyticOrder::prepare(*analytic, 0, MAPPING);
            },
            [](const auto &p) {
                return p.sourceId() == 0;
            },
            failures)) {
        return 3;
    }

    auto ps = q::events::PreparedSpreadOrder::prepare(*spread, 0, MAPPING);
    auto pa = q::events::PreparedAnalyticOrder::prepare(*analytic, 0, MAPPING);

    if (!ps || !pa) {
        return 4;
    }

    auto sv = values(*spread), av = values(*analytic);
    const std::u16string symbol(64, u'X'), text(64, u'Y');
    sv.back().units = text;
    av.back().raw = 0x41424344;
    const auto originalSpread = sv, originalAnalytic = av;

    if (!sweep(
            [&] {
                return ps->map(symbol, {spread->recordContext(0), sv}, VALUES);
            },
            [&](const auto &v) {
                return v.symbol == symbol && v.spreadSymbol == text;
            },
            failures)) {
        return 5;
    }

    if (!sweep(
            [&] {
                return pa->map(symbol, {analytic->recordContext(0), av}, VALUES);
            },
            [&](const auto &v) {
                return v.symbol == symbol && v.marketMaker == u"ABCD";
            },
            failures)) {
        return 6;
    }

    remaining = 0;
    armed = true;
    const auto resource = ps->map(symbol, {spread->recordContext(0), sv}, {64, 64, 127});
    armed = false;

    if (resource || resource.error() != q::events::MappingError::RESOURCE_LIMIT) {
        return 7;
    }

    if (sv != originalSpread || av != originalAnalytic || failures < 4) {
        return 8;
    }

    std::cout << "PASS allocationFailures=" << failures << " SpreadOrderBytes=" << sizeof(q::events::SpreadOrder)
              << " AnalyticOrderBytes=" << sizeof(q::events::AnalyticOrder) << '\n';

    return 0;
}
