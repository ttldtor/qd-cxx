// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/events/candle_period.hpp>
#ifdef QDCXX_CONSUMER_FEED
#    include <qd/api/feed.hpp>
#endif
#include <limits>
#include <stdexcept>

/** Public installed consumer; input-free CTest fixture, returns zero on exact value checks.
 * @return Zero for success; nonzero for a value, exception, or inactive feed-lifetime failure.
 */
int main() {
    using namespace org::ttldtor::qdcxx::events;

    try {
        auto period = CandlePeriod::parse(u"2.5Minutes");

        if (period.toString() != u"2.5m" || period.getPeriodIntervalMillis() != 150000 ||
            CandlePeriod::parse(u"Mo").getType() != CandleType::MONTH ||
            CandlePeriod(0x1p63).toString() != u"9223372036854775807t" ||
            CandlePeriod(std::numeric_limits<double>::infinity(), CandleType::TICK).getPeriodIntervalMillis() != 0 ||
            CandlePeriod(0.0) == CandlePeriod(-0.0)) {
            return 1;
        }

        bool rejected = false;

        try {
            (void)CandlePeriod::parse(u"NaNt");
        } catch (const std::invalid_argument &) {
            rejected = true;
        }

        if (!rejected) {
            return 2;
        }

#ifdef QDCXX_CONSUMER_FEED
        org::ttldtor::qdcxx::api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint();
        (void)endpoint->closeAndAwaitTermination();
#endif

        return 0;
    } catch (...) {
        return 3;
    }
}
