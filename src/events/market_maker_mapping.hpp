// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "decimal_projection.hpp"
#include <array>

namespace org::ttldtor::qdcxx::events::detail {
/** Actual generated property order shared by raw MarketMaker and derived Order plans. */
inline constexpr std::array<std::array<std::u16string_view, 2>, 10> MARKET_MAKER_KEYS{{{u"MMExchange", u"ExchangeCode"},
                                                                                       {u"MMID", u"MarketMaker"},
                                                                                       {u"MMBid.Time", u"BidTime"},
                                                                                       {u"MMBid.Price", u"BidPrice"},
                                                                                       {u"MMBid.Size", u"BidSize"},
                                                                                       {u"MMBid.Count", u"BidCount"},
                                                                                       {u"MMAsk.Time", u"AskTime"},
                                                                                       {u"MMAsk.Price", u"AskPrice"},
                                                                                       {u"MMAsk.Size", u"AskSize"},
                                                                                       {u"MMAsk.Count", u"AskCount"}}};

/** Prepare the same ten exact properties for either owning business projection.
 * @param schema Complete validated source declarations.
 * @param localId Exact local record identity.
 * @param limits Finite preparation credits.
 * @return Independently owned field plan or atomic refusal.
 */
inline std::expected<EventMapping, MappingError> prepareMarketMakerMapping(const core::LocalSchema &schema,
                                                                           std::int32_t localId, MappingLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array timeTypes{Type::TIME_SECONDS, Type::TIME_MILLIS};
    const std::array<std::span<const Type>, 10> types{RAW_INT_TYPES, RAW_INT_TYPES, timeTypes, NUMERIC_TYPES,
                                                      NUMERIC_TYPES, NUMERIC_TYPES, timeTypes, NUMERIC_TYPES,
                                                      NUMERIC_TYPES, NUMERIC_TYPES};
    constexpr std::array required{true, true, false, true, true, false, false, true, true, false};
    std::array<FieldRequest, 10> requests{};

    for (std::size_t i = 0; i < requests.size(); ++i) {
        requests[i] = {MARKET_MAKER_KEYS[i], types[i], required[i]};
    }

    return EventMapping::prepare(schema, localId, requests, limits);
}
} // namespace org::ttldtor::qdcxx::events::detail
