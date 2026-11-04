// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/candle_symbol.hpp"
#include "qd/events/market_event_symbols.hpp"

#include <array>
#include <bit>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
using Symbol = std::optional<std::u16string_view>;
using OwnedSymbol = std::optional<std::u16string>;
constexpr std::size_t EXCHANGE_ATTRIBUTE = 0;
constexpr std::size_t PRICE_ATTRIBUTE = 1;
constexpr std::size_t SESSION_ATTRIBUTE = 2;
constexpr std::size_t PERIOD_ATTRIBUTE = 3;
constexpr std::size_t ALIGNMENT_ATTRIBUTE = 4;
constexpr std::size_t PRICE_LEVEL_ATTRIBUTE = 5;
constexpr std::size_t DATA_TYPE_ATTRIBUTE = 6;

/** @param text Owned nullable text borrowed for this call.
 * @return Optional view valid while text remains unchanged.
 */
Symbol view(const OwnedSymbol &text) noexcept {
    return text ? Symbol(*text) : std::nullopt;
}

/** @param symbol Borrowed nullable text, not retained.
 * @return Independent copy preserving null.
 */
OwnedSymbol copy(Symbol symbol) {
    return symbol ? OwnedSymbol(std::in_place, *symbol) : std::nullopt;
}

/** @param symbol Borrowed nullable symbol.
 * @param value Independent period to set.
 * @return Owned changed symbol; the default removes the period key.
 */
OwnedSymbol changePeriod(Symbol symbol, CandlePeriod value) {
    return value == CandlePeriod{} ? market_symbols::removeAttributeStringByKey(symbol, u"")
                                   : market_symbols::changeAttributeStringByKey(symbol, u"", value.toString());
}

/** @param symbol Borrowed nullable symbol.
 * @return Independent period-normalized symbol, preserving the discarded default-removal result.
 */
OwnedSymbol normalizePeriod(Symbol symbol) {
    auto text = market_symbols::getAttributeStringByKey(symbol, u"");

    if (!text) {
        return copy(symbol);
    }

    try {
        auto value = CandlePeriod::parse(*text);

        if (value == CandlePeriod{}) {
            (void)market_symbols::removeAttributeStringByKey(symbol, u"");
        }

        auto canonical = value.toString();

        return *text == canonical ? copy(symbol) : market_symbols::changeAttributeStringByKey(symbol, u"", canonical);
    } catch (const std::invalid_argument &) {
        return copy(symbol);
    }
}

/** @param symbol Borrowed nullable input.
 * @return Owned result after the six reference normalization passes in their actual order.
 */
OwnedSymbol normalize(Symbol symbol) {
    auto result = normalizeCandlePrice(symbol);
    result = normalizeCandleSession(view(result));
    result = normalizePeriod(view(result));
    result = normalizeCandleAlignment(view(result));
    result = CandlePriceLevel::normalizeAttributeForSymbol(view(result));

    return CandleDataType::normalizeAttributeForSymbol(view(result));
}

/** @param symbol Borrowed nullable symbol.
 * @param attribute Independent attribute value.
 * @return Owned result after applying exactly this attribute.
 */
OwnedSymbol change(Symbol symbol, const CandleAttribute &attribute) {
    return std::visit(
        [&](const auto &value) -> OwnedSymbol {
            using Value = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Value, CandlePrice>) {
                return changeCandlePrice(symbol, value);
            } else if constexpr (std::is_same_v<Value, CandleSession>) {
                return changeCandleSession(symbol, value);
            } else if constexpr (std::is_same_v<Value, CandleAlignment>) {
                return changeCandleAlignment(symbol, value);
            } else if constexpr (std::is_same_v<Value, CandlePeriod>) {
                return changePeriod(symbol, value);
            } else {
                return value.changeAttributeForSymbol(symbol);
            }
        },
        attribute);
}
} // namespace

struct CandleSymbol::State {
    OwnedSymbol symbol;
    OwnedSymbol base;
    CandleExchange exchange;
    CandlePrice price = DEFAULT_CANDLE_PRICE;
    CandleSession session = DEFAULT_CANDLE_SESSION;
    CandlePeriod period;
    CandleAlignment alignment = DEFAULT_CANDLE_ALIGNMENT;
    CandlePriceLevel level;
    CandleDataType dataType;

    /** @param text Fully normalized nullable owned text.
     * @param attributes Complete explicit attribute list, borrowed only during construction.
     * @throws std::invalid_argument Duplicate attribute kinds or invalid remaining attributes.
     */
    State(OwnedSymbol text, std::span<const CandleAttribute> attributes) : symbol(std::move(text)) {
        std::array<bool, std::variant_size_v<CandleAttribute>> supplied{};

        for (const auto &attribute : attributes) {
            auto kind = attribute.index();

            if (supplied[kind]) {
                throw std::invalid_argument("Duplicate candle attribute");
            }

            supplied[kind] = true;
            std::visit(
                [&](const auto &value) {
                    using Value = std::decay_t<decltype(value)>;

                    if constexpr (std::is_same_v<Value, CandleExchange>) {
                        exchange = value;
                    } else if constexpr (std::is_same_v<Value, CandlePrice>) {
                        price = value;
                    } else if constexpr (std::is_same_v<Value, CandleSession>) {
                        session = value;
                    } else if constexpr (std::is_same_v<Value, CandlePeriod>) {
                        period = value;
                    } else if constexpr (std::is_same_v<Value, CandleAlignment>) {
                        alignment = value;
                    } else if constexpr (std::is_same_v<Value, CandlePriceLevel>) {
                        level = value;
                    } else {
                        dataType = value;
                    }
                },
                attribute);
        }

        auto source = view(symbol);
        base = market_symbols::getBaseSymbol(source);

        if (!supplied[EXCHANGE_ATTRIBUTE]) {
            exchange = CandleExchange::getAttributeForSymbol(source);
        }

        if (!supplied[PRICE_ATTRIBUTE]) {
            price = getCandlePrice(source);
        }

        if (!supplied[SESSION_ATTRIBUTE]) {
            session = getCandleSession(source);
        }

        if (!supplied[PERIOD_ATTRIBUTE]) {
            auto textValue = market_symbols::getAttributeStringByKey(source, u"");
            period = textValue ? CandlePeriod::parse(*textValue) : CandlePeriod{};
        }

        if (!supplied[ALIGNMENT_ATTRIBUTE]) {
            alignment = getCandleAlignment(source);
        }

        if (!supplied[PRICE_LEVEL_ATTRIBUTE]) {
            level = CandlePriceLevel::getAttributeForSymbol(source);
        }

        if (!supplied[DATA_TYPE_ATTRIBUTE]) {
            dataType = CandleDataType::getAttributeForSymbol(source);
        }
    }
};

CandleSymbol::CandleSymbol(std::shared_ptr<const State> state) noexcept : state_(std::move(state)) {
}

CandleSymbol::CandleSymbol(CandleSymbol &&other) noexcept : state_(other.state_) {
}

CandleSymbol &CandleSymbol::operator=(CandleSymbol &&other) noexcept {
    state_ = other.state_;

    return *this;
}

CandleSymbol CandleSymbol::create(Symbol symbol, std::span<const CandleAttribute> attributes) {
    auto text = copy(symbol);

    for (const auto &attribute : attributes) {
        if (attribute.valueless_by_exception()) {
            throw std::invalid_argument("Invalid candle attribute");
        }

        text = change(view(text), attribute);
    }

    return CandleSymbol(std::make_shared<const State>(normalize(view(text)), attributes));
}

CandleSymbol CandleSymbol::valueOf(Symbol symbol) {
    return create(symbol, {});
}

CandleSymbol CandleSymbol::valueOf(Symbol symbol, std::initializer_list<CandleAttribute> attributes) {
    return create(symbol, std::span(attributes.begin(), attributes.size()));
}

Symbol CandleSymbol::getSymbol() const noexcept {
    return view(state_->symbol);
}

OwnedSymbol CandleSymbol::toString() const {
    return state_->symbol;
}

Symbol CandleSymbol::getBaseSymbol() const noexcept {
    return view(state_->base);
}

CandleExchange CandleSymbol::getExchange() const noexcept {
    return state_->exchange;
}

CandlePrice CandleSymbol::getPrice() const noexcept {
    return state_->price;
}

CandleSession CandleSymbol::getSession() const noexcept {
    return state_->session;
}

CandlePeriod CandleSymbol::getPeriod() const noexcept {
    return state_->period;
}

CandleAlignment CandleSymbol::getAlignment() const noexcept {
    return state_->alignment;
}

CandlePriceLevel CandleSymbol::getPriceLevel() const noexcept {
    return state_->level;
}

const CandleDataType &CandleSymbol::getDataType() const noexcept {
    return state_->dataType;
}

std::int32_t CandleSymbol::hashCode() const {
    if (!state_->symbol) {
        throw std::invalid_argument("Null candle symbol hash");
    }

    constexpr std::uint32_t JAVA_STRING_HASH_MULTIPLIER = 31;
    std::uint32_t hash = 0;

    for (char16_t unit : *state_->symbol) {
        hash = JAVA_STRING_HASH_MULTIPLIER * hash + unit;
    }

    return std::bit_cast<std::int32_t>(hash);
}

bool CandleSymbol::equals(const CandleSymbol &other) const {
    if (this == &other) {
        return true;
    }

    if (!state_->symbol) {
        throw std::invalid_argument("Null candle symbol equality");
    }

    return state_->symbol == other.state_->symbol;
}

bool CandleSymbol::operator==(const CandleSymbol &other) const noexcept {
    return state_->symbol == other.state_->symbol;
}
} // namespace org::ttldtor::qdcxx::events
