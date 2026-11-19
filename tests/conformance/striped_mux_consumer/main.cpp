// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/mux/striped.hpp>

#include <array>
#include <memory>

namespace {
namespace q = org::ttldtor::qdcxx;

/** @return Explicit finite initialization/progress turn, independent of global settings. */
q::mux::StripedWork turn() {
    return {100000,
            100000000,
            100000000,
            1000000000,
            {100000,
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
             {100000, 1000000000, 1000000000, 1000000000}},
            {10000, 1000000, 1000000, 10000000},
            {10000, 10000000, 10000000, 100000000}};
}
} // namespace

/** Verify installed core classification, actual factory linkage and complete child closure.
 * @return Zero for the expected public behavior; nonzero identifies failed package/runtime admission.
 */
int main() {
    auto partition = q::core::SymbolStriper::parse(u"byrange-D-M-", {1024, 1024, 1024, 65536});

    if (!partition || partition->stripeIndex(u"IBM") != 1) {
        return 1;
    }

    const q::core::LocalRecord record{0, u"Value", false, {{8}}, {{u"Value", 0}}};
    auto schema = q::core::LocalSchema::create(std::span(&record, 1), {8, 32, 32, 128, 4096},
                                               q::core::LocalProfile::BINARY_FIELDS_V1);

    if (!schema) {
        return 2;
    }

    const q::core::SubscriptionLimits desired{16, 32, 128, 4096, 64, 8192, 64};
    const q::mux::StreamStripeOptions options{
        {16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024},
        desired,
        {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
        {4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024},
        q::core::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1};
    const std::array children{options, options, options};
    auto work = turn();
    auto root = q::mux::StripedCollector::createStream(
        *schema, *partition, children, {16 * 1024 * 1024, 8 * 1024 * 1024, 1024 * 1024, 1024 * 1024, 4 * 1024 * 1024},
        work);

    if (!root || root->stripeCount() != 3) {
        return 3;
    }

    root->requestStop(q::mux::StopMode::GRACEFUL);

    for (std::size_t step = 0; step < 1000; ++step) {
        work = turn();
        auto progress = root->advance(work);

        if (!progress) {
            return 4;
        }

        if (progress->closed) {
            work = turn();
            auto summary = root->summary(work);

            return summary && summary->terminalStripes == 3 && summary->finalCounts ? 0 : 5;
        }
    }

    return 6;
}
