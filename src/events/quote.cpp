// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/quote.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
std::expected<core::LocalSchema, core::SchemaError> defaultQuoteSchema(core::SchemaLimits limits) {
    constexpr std::array<std::u16string_view, 8> PROPERTY_NAMES{u"BidTime",  u"BidExchangeCode", u"BidPrice",
                                                                u"BidSize",  u"AskTime",         u"AskExchangeCode",
                                                                u"AskPrice", u"AskSize"};
    try {
        core::LocalRecord record{0, u"Quote", false, {}, {}};

        for (std::size_t i = 0; i < QUOTE_FIELD_TYPES.size(); ++i) {
            const std::u16string localName(QUOTE_FIELD_NAMES[i].begin(), QUOTE_FIELD_NAMES[i].end());
            record.fields.push_back({std::to_underlying(QUOTE_FIELD_TYPES[i])});
            record.lookups.push_back({localName, i});
            record.lookups.push_back({u"Quote." + localName, i});
            record.lookups.push_back({std::u16string(PROPERTY_NAMES[i]), i});
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::QUOTE_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

std::expected<Quote, QuoteError> mapQuote(std::u16string_view symbol, std::span<const core::FieldValue> fields,
                                          std::size_t symbolUnits) {
    constexpr std::int64_t MILLISECONDS_PER_SECOND = 1000;
    constexpr std::uint64_t UTF16_UNIT_MASK = 0xffff;

    if (fields.size() != QUOTE_FIELD_TYPES.size()) {
        return std::unexpected(QuoteError::INVALID_SHAPE);
    }

    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto &value = fields[i];

        if (value.type != QUOTE_FIELD_TYPES[i] || !core::validFieldValue(value)) {
            return std::unexpected(QuoteError::INVALID_SHAPE);
        }
    }

    if (symbol.size() > symbolUnits) {
        return std::unexpected(QuoteError::RESOURCE_LIMIT);
    }

    try {
        const auto bid = fields[0].raw * MILLISECONDS_PER_SECOND;
        const auto ask = fields[4].raw * MILLISECONDS_PER_SECOND;

        return Quote{std::u16string(symbol),
                     bid,
                     static_cast<char16_t>(static_cast<std::uint64_t>(fields[1].raw) & UTF16_UNIT_MASK),
                     {fields[2].raw},
                     {fields[3].raw},
                     ask,
                     static_cast<char16_t>(static_cast<std::uint64_t>(fields[5].raw) & UTF16_UNIT_MASK),
                     {fields[6].raw},
                     {fields[7].raw},
                     std::max(bid, ask)};
    } catch (const std::bad_alloc &) {
        return std::unexpected(QuoteError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(QuoteError::ALLOCATION_FAILURE);
    }
}
} // namespace org::ttldtor::qdcxx::events
