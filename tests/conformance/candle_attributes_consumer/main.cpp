// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/events/candle_attributes.hpp>
#include <stdexcept>
#ifdef ORG_TTLDTOR_QDCXX_CONSUMER_FEED
#    include <qd/api/feed.hpp>
#endif

/** Installed API check; no input, network connection or global configuration.
 * @return Zero for exact values and an inactive feed lifetime; nonzero on failure.
 */
int main() {
    using namespace org::ttldtor::qdcxx::events;
    try {
        auto symbol = changeCandlePrice(u"AAPL{=m}", CandlePrice::BID);
        auto grouped = CandlePriceLevel::parse(u"0.25").changeAttributeForSymbol(*symbol);
        auto text = std::u16string(u"custom");
        auto custom = CandleDataType(text);
        text.assign(u"changed");

        if (getCandlePrice(*grouped) != CandlePrice::BID ||
            CandlePriceLevel::getAttributeForSymbol(*grouped).getValue() != 0.25 || custom.getValue() != u"custom" ||
            getCandleSession(u"A{tho=t}") != CandleSession::ANY || parseCandleSession(u"t") != CandleSession::REGULAR ||
            normalizeCandlePrice(u"A{price=LAST}") != u"A{price=last}" ||
            CandleDataType::normalizeAttributeForSymbol(u"A{dt=}") != u"A" ||
            CandleExchange(u'Q').changeAttributeForSymbol(u"A{a=s}") != u"A&Q{a=s}") {
            return 1;
        }

        bool rejected = false;
        try {
            (void)CandlePriceLevel(-0.0);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }

        if (!rejected) {
            return 2;
        }

#ifdef ORG_TTLDTOR_QDCXX_CONSUMER_FEED
        org::ttldtor::qdcxx::api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint();
        (void)endpoint->closeAndAwaitTermination();
#endif

        return 0;
    } catch (...) {
        return 3;
    }
}
