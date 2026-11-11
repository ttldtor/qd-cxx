// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/candle_routes.hpp"

#include "qd/events/market_event_symbols.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
using Symbol = std::optional<std::u16string_view>;
using OwnedSymbol = std::optional<std::u16string>;
constexpr char16_t FIRST_PRINTABLE = 0x20;
constexpr char16_t LAST_PRINTABLE = 0x7e;
const CandlePeriod MINUTE{1, CandleType::MINUTE};

/** @param text Optional owning text, borrowed during the call.
 * @return Optional view valid while text remains unchanged.
 */
Symbol view(const OwnedSymbol &text) noexcept {
    return text ? Symbol(*text) : std::nullopt;
}

/** Validate a printable exact selector using the Feed's accepted spelling domain.
 * @param text Borrowed non-null symbol.
 * @return True for a nonempty non-wildcard printable ASCII selector.
 */
bool validSymbol(std::u16string_view text) noexcept {
    return !text.empty() && text.front() != u'*' && std::ranges::all_of(text, [](char16_t unit) {
        return unit >= FIRST_PRINTABLE && unit <= LAST_PRINTABLE;
    });
}

/** Resolve an exact schema lookup name.
 * @param record Immutable local record.
 * @param name Exact field key.
 * @return Field ID, or no value if absent.
 */
std::optional<std::size_t> fieldId(const core::LocalRecord &record, std::u16string_view name) noexcept {
    for (const auto &lookup : record.lookups)
        if (lookup.name == name)
            return lookup.fieldId;

    return std::nullopt;
}

/** Require source index words to be the mapper's Time and Sequence fields.
 * @param record Complete validated candidate record.
 * @return True only for the actual indexed Time/Sequence pair.
 */
bool indexedLayout(const core::LocalRecord &record) noexcept {
    if (!record.hasTime || !record.indexedTimeLayout)
        return false;

    const auto *pair = std::get_if<core::IndexedTimePairInt32>(&*record.indexedTimeLayout);
    const auto first = fieldId(record, u"Time");
    const auto second = fieldId(record, u"Sequence");

    return pair && first && second && pair->firstField == *first && pair->secondField == *second;
}
} // namespace

PreparedCandleRoutes::PreparedCandleRoutes(std::vector<Entry> entries, std::size_t symbolUnits) noexcept
    : entries_(std::move(entries)), symbolUnits_(symbolUnits) {
}

std::expected<PreparedCandleRoutes, CandleRouteFailure>
PreparedCandleRoutes::prepare(const core::LocalSchema &schema, std::span<const std::int32_t> recordIds,
                              CandleRouteLimits limits) {
    if (recordIds.size() > limits.records)
        return std::unexpected(CandleRouteFailure{CandleRouteError::RESOURCE_LIMIT, {}, {}});

    std::optional<std::int32_t> preparing;

    try {
        std::vector<Entry> entries;
        entries.reserve(recordIds.size());

        for (const auto id : recordIds) {
            preparing = id;
            const auto context = schema.recordContext(id);

            if (!context.valid())
                return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, id, {}});

            const auto &record = schema.records()[static_cast<std::size_t>(id)];
            const std::u16string_view name(record.name);

            if (name.size() > limits.recordNameUnits)
                return std::unexpected(CandleRouteFailure{CandleRouteError::RESOURCE_LIMIT, id, {}});

            std::optional<CandlePeriod> period;
            std::optional<CandlePrice> price;
            CandleRecordKind kind;

            if (name == u"TradeHistory") {
                kind = CandleRecordKind::TRADE_HISTORY;
                period = CandlePeriod::TICK;
            } else if (name.starts_with(u"Trade.")) {
                if (market_symbols::hasAttributes(name) || name.size() == 6)
                    return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, id, {}});

                kind = CandleRecordKind::CANDLE;
                period = CandlePeriod::parse(name.substr(6));
            } else if (name.starts_with(u"Candle")) {
                kind = CandleRecordKind::CANDLE;
                auto stripped = market_symbols::removeAttributeStringByKey(name, u"");
                stripped = changeCandlePrice(view(stripped), DEFAULT_CANDLE_PRICE);

                if (!stripped || *stripped != u"Candle")
                    return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, id, {}});

                const auto periodText = market_symbols::getAttributeStringByKey(name, u"");

                if (periodText) {
                    const auto parsed = CandlePeriod::parse(*periodText);

                    if (parsed != CandlePeriod::DEFAULT)
                        period = parsed;
                }

                const auto selectedPrice = getCandlePrice(name);

                if (selectedPrice != DEFAULT_CANDLE_PRICE)
                    price = selectedPrice;

                if (price && (!period || *period != MINUTE))
                    return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, id, {}});
            } else {
                return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, id, {}});
            }

            if (record.hasTime && !indexedLayout(record))
                return std::unexpected(
                    CandleRouteFailure{CandleRouteError::INVALID_MAPPING, id, MappingError::INVALID_INPUT});

            const auto mapping = PreparedCandle::prepare(schema, id, kind, limits.mapping);

            if (!mapping)
                return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_MAPPING, id, mapping.error()});

            for (const auto &previous : entries) {
                if (previous.recordId == id)
                    return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, id, {}});

                if (previous.period == period && previous.price == price)
                    return std::unexpected(CandleRouteFailure{CandleRouteError::DUPLICATE_DESCRIPTOR, id, {}});
            }

            entries.push_back({id, context, kind, period, price});
        }

        return PreparedCandleRoutes(std::move(entries), limits.symbolUnits);
    } catch (const std::bad_alloc &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::ALLOCATION_FAILURE, preparing, {}});
    } catch (const std::length_error &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::ALLOCATION_FAILURE, preparing, {}});
    } catch (const std::invalid_argument &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, preparing, {}});
    } catch (const std::out_of_range &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, preparing, {}});
    }
}

std::expected<CandlePhysicalRoute, CandleRouteFailure> PreparedCandleRoutes::resolve(const CandleSymbol &symbol) const {
    const auto original = symbol.getSymbol();

    if (!original || !validSymbol(*original))
        return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, {}, {}});

    if (original->size() > symbolUnits_)
        return std::unexpected(CandleRouteFailure{CandleRouteError::RESOURCE_LIMIT, {}, {}});

    const auto period = symbol.getPeriod();
    const auto price = symbol.getPrice();
    const Entry *exact = nullptr;
    const Entry *minuteFallback = nullptr;
    const Entry *generic = nullptr;

    for (const auto &entry : entries_) {
        if (!entry.period) {
            generic = &entry;
        } else if (period == MINUTE) {
            if (entry.price && *entry.price == price)
                exact = &entry;
            else if (!entry.price && *entry.period == MINUTE)
                minuteFallback = &entry;
        } else if (!entry.price && *entry.period == period) {
            exact = &entry;
        }
    }

    const auto *chosen = exact ? exact : minuteFallback ? minuteFallback : generic;

    if (!chosen)
        return std::unexpected(CandleRouteFailure{CandleRouteError::MISSING_ROUTE, {}, {}});

    try {
        OwnedSymbol physical;

        if (chosen->period) {
            physical = market_symbols::removeAttributeStringByKey(original, u"");

            if (chosen->price)
                physical = changeCandlePrice(view(physical), DEFAULT_CANDLE_PRICE);
        } else {
            physical.emplace(*original);
        }

        if (!physical || !validSymbol(*physical))
            return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, chosen->recordId, {}});

        if (physical->size() > symbolUnits_)
            return std::unexpected(CandleRouteFailure{CandleRouteError::RESOURCE_LIMIT, chosen->recordId, {}});

        return CandlePhysicalRoute{chosen->recordId, chosen->context, chosen->kind, std::move(*physical)};
    } catch (const std::bad_alloc &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::ALLOCATION_FAILURE, chosen->recordId, {}});
    } catch (const std::length_error &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::ALLOCATION_FAILURE, chosen->recordId, {}});
    } catch (const std::invalid_argument &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, chosen->recordId, {}});
    } catch (const std::out_of_range &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, chosen->recordId, {}});
    }
}

std::expected<CandleSymbol, CandleRouteFailure>
PreparedCandleRoutes::restore(std::int32_t recordId, std::u16string_view physicalSymbol) const {
    const auto found = std::ranges::find_if(entries_, [&](const Entry &entry) {
        return entry.recordId == recordId;
    });

    if (found == entries_.end() || !validSymbol(physicalSymbol))
        return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, recordId, {}});

    if (physicalSymbol.size() > symbolUnits_)
        return std::unexpected(CandleRouteFailure{CandleRouteError::RESOURCE_LIMIT, recordId, {}});

    try {
        const auto logical = [&] {
            if (!found->period)
                return CandleSymbol::valueOf(physicalSymbol);

            if (found->price)
                return CandleSymbol::valueOf(physicalSymbol, *found->period, *found->price);

            return CandleSymbol::valueOf(physicalSymbol, *found->period);
        }();
        const auto text = logical.getSymbol();

        if (!text || !validSymbol(*text))
            return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, recordId, {}});

        if (text->size() > symbolUnits_)
            return std::unexpected(CandleRouteFailure{CandleRouteError::RESOURCE_LIMIT, recordId, {}});

        return logical;
    } catch (const std::bad_alloc &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::ALLOCATION_FAILURE, recordId, {}});
    } catch (const std::length_error &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::ALLOCATION_FAILURE, recordId, {}});
    } catch (const std::invalid_argument &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, recordId, {}});
    } catch (const std::out_of_range &) {
        return std::unexpected(CandleRouteFailure{CandleRouteError::INVALID_INPUT, recordId, {}});
    }
}
} // namespace org::ttldtor::qdcxx::events
