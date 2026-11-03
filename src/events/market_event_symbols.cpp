// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/market_event_symbols.hpp"

namespace org::ttldtor::qdcxx::events::market_symbols {
namespace {
/** Locate a nonempty trailing brace block without interpreting its contents.
 * @param symbol Borrowed exact UTF-16 input.
 * @return Block offset, or the complete input length when no block is recognized.
 */
std::size_t lengthWithoutAttributes(std::u16string_view symbol) noexcept {
    if (symbol.size() >= 3 && symbol.back() == u'}') {
        const auto open = symbol.rfind(u'{', symbol.size() - 2);

        if (open != std::u16string_view::npos && open < symbol.size() - 2) {
            return open;
        }
    }

    return symbol.size();
}

/** Inspect the regional suffix within an already located prefix.
 * @param symbol Borrowed exact input.
 * @param length Prefix length within symbol, excluding any recognized block.
 * @return True when the prefix ends with an ampersand and one code unit.
 */
bool hasCode(std::u16string_view symbol, std::size_t length) noexcept {
    return length >= 2 && symbol[length - 2] == u'&';
}

/** Find the base prefix without allocating.
 * @param symbol Borrowed exact input.
 * @param length Prefix length within symbol, excluding any recognized block.
 * @return Prefix length without its regional suffix when present.
 */
std::size_t baseLength(std::u16string_view symbol, std::size_t length) noexcept {
    return hasCode(symbol, length) ? length - 2 : length;
}
} // namespace

bool hasExchangeCode(std::optional<std::u16string_view> symbol) noexcept {
    return symbol && hasCode(*symbol, lengthWithoutAttributes(*symbol));
}

char16_t getExchangeCode(std::optional<std::u16string_view> symbol) noexcept {
    return hasExchangeCode(symbol) ? (*symbol)[lengthWithoutAttributes(*symbol) - 1] : 0;
}

std::optional<std::u16string> getBaseSymbol(std::optional<std::u16string_view> symbol) {
    if (!symbol) {
        return std::nullopt;
    }

    return std::u16string(symbol->substr(0, baseLength(*symbol, lengthWithoutAttributes(*symbol))));
}

std::optional<std::u16string> changeExchangeCode(std::optional<std::u16string_view> symbol, char16_t exchangeCode) {
    if (!symbol && exchangeCode == 0) {
        return std::nullopt;
    }

    const auto input = symbol.value_or(u"");
    const auto length = lengthWithoutAttributes(input);
    std::u16string result(input.substr(0, baseLength(input, length)));

    if (exchangeCode != 0) {
        result.push_back(u'&');
        result.push_back(exchangeCode);
    }

    result.append(input.substr(length));

    return result;
}

std::optional<std::u16string> changeBaseSymbol(std::optional<std::u16string_view> symbol,
                                               std::optional<std::u16string_view> baseSymbol) {
    const auto input = symbol.value_or(u"");
    const auto length = lengthWithoutAttributes(input);
    const auto suffix = input.substr(baseLength(input, length));

    if (suffix.empty()) {
        return baseSymbol ? std::optional<std::u16string>(*baseSymbol) : std::nullopt;
    }

    std::u16string result(baseSymbol.value_or(u"null"));
    result.append(suffix);

    return result;
}
} // namespace org::ttldtor::qdcxx::events::market_symbols
