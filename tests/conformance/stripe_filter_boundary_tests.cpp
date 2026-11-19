// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../src/api/publisher_delivery.hpp"
#include "qd/core/symbol_striper.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace core = q::core;
namespace api = q::api;

/** @return Explicit finite classification grants for the fixed policy integration scenarios. */
core::SymbolStriperLimits grants() {
    return {1024, 1024, 1024, 64 * 1024};
}
} // namespace

TEST_CASE("stripe subset optimization preserves the actual publisher physical record predicate") {
    api::EventRegistryWork registryWork;
    auto registry = api::EventRegistry::builtIn({}, registryWork);
    REQUIRE(registry);
    const auto &schema = registry->schema();
    core::RecordContext quote;

    for (const auto &record : schema.records()) {
        if (record.name == u"Quote") {
            quote = schema.recordContext(record.id);
        }
    }

    REQUIRE(schema.recordId(quote));
    const std::array symbols{u"A", u"AAPL", u"IBM", u"MSFT", u"SPY", u"ZZ", u"=2*MSFT", u"/ES"};

    for (const auto definition : {u"byhash4", u"byrange-A-M-Z-"}) {
        auto striper = core::SymbolStriper::parse(definition, grants());
        REQUIRE(striper);

        for (const auto expression : {u"hash1of2", u"range-D-Z-", u"A*", u":Other"}) {
            const auto complete = u":Quote&(A*,MSFT)&(" + std::u16string(expression) + u")";
            const auto policy = api::FeedFilter::Builder(*registry, api::DeliveryContract::STREAM)
                                    .withCategory(u"real policy", complete)
                                    .build();
            const auto stripeLeaf = core::SymbolStripeFilter::parse(expression, grants());
            std::optional<core::StripeIntersection> overlap;

            if (stripeLeaf) {
                const auto intersected = striper->intersectingStripes(*stripeLeaf);
                REQUIRE(intersected);
                overlap = *intersected;
            } else {
                REQUIRE(stripeLeaf.error() == core::SymbolStripeError::UNKNOWN_SPECIFICATION);
            }

            for (const auto symbol : symbols) {
                const auto expected = api::detail::publisherRecordAllowed(policy, schema, quote, symbol);
                std::size_t accepted = 0;

                for (std::size_t stripe = 0; stripe < striper->stripeCount(); ++stripe) {
                    if (overlap && (stripe < overlap->first || stripe - overlap->first >= overlap->count)) {
                        continue;
                    }

                    const auto leaf = striper->stripeFilter(stripe);
                    REQUIRE(leaf);
                    const auto combined =
                        api::FeedFilter::Builder(*registry, api::DeliveryContract::STREAM)
                            .withCategory(u"real policy and stripe",
                                          u"(" + complete + u")&(" + std::u16string(leaf->name()) + u")")
                            .build();

                    if (api::detail::publisherRecordAllowed(combined, schema, quote, symbol)) {
                        ++accepted;
                    }
                }

                CHECK(accepted == (expected ? 1 : 0));
            }
        }
    }

    CHECK(api::detail::publisherRecordAllowed({}, schema, quote, u"IBM"));
}
