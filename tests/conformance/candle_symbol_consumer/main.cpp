// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/events/candle_symbol.hpp>
#ifdef ORG_TTLDTOR_QDCXX_CONSUMER_FEED
#    include <qd/api/feed.hpp>
#endif
#include <stdexcept>

/** Installed guide/API check with no input or network connection.
 * @return Zero for exact owning values, object desired-map identity and inactive endpoint retirement.
 */
int main() {
    using namespace org::ttldtor::qdcxx::events;
    try {
        auto symbol = CandleSymbol::valueOf(u"AAPL&Q", CandlePeriod(15, CandleType::MINUTE), CandlePrice::BID);
        auto equivalent = CandleSymbol::valueOf(u"AAPL&Q{=15Minutes,price=BID}");

        if (!(symbol == equivalent) || !symbol.equals(equivalent) || symbol.hashCode() != equivalent.hashCode() ||
            symbol.toString() != u"AAPL&Q{=15m,price=bid}" || symbol.getBaseSymbol() != u"AAPL") {
            return 1;
        }

        auto nullSymbol = CandleSymbol::valueOf(std::nullopt);

        if (nullSymbol.toString() || !(nullSymbol == CandleSymbol::valueOf(std::nullopt))) {
            return 2;
        }

#ifdef ORG_TTLDTOR_QDCXX_CONSUMER_FEED
        namespace api = org::ttldtor::qdcxx::api;
        api::RuntimeOptions options;
        options.maximumSymbolsPerSubscription = 1;
        api::ApiRuntime runtime(options);
        auto endpoint = runtime.createEndpoint();
        auto subscription = endpoint->getFeed()->createSubscription<Quote>();
        subscription->setSymbols({symbol, equivalent});
        bool capacityRejected = false;
        try {
            subscription->addSymbols("OTHER");
        } catch (const api::Error &error) {
            capacityRejected = error.diagnostic().code == api::ErrorCode::RESOURCE_LIMIT;
        }

        if (!capacityRejected) {
            return 3;
        }

        subscription->removeSymbols(equivalent);
        subscription->addSymbols("OTHER");
        subscription->setSymbols({});
        subscription->addSymbols(symbol);

        if (!endpoint->closeAndAwaitTermination().drained) {
            return 4;
        }

#endif

        return 0;
    } catch (...) {
        return 5;
    }
}
