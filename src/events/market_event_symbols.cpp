// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/market_event_symbols.hpp"
#include <stdexcept>

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

bool hasAttributes(std::optional<std::u16string_view> symbol) noexcept {
    return symbol && lengthWithoutAttributes(*symbol) < symbol->size();
}

namespace {
/** Locate the next attribute after an already found value separator.
 * Attribute scanning deliberately searches past commas for '=' as Java does.
 * @param symbol Borrowed exact UTF-16 input; no storage is retained.
 * @param equals Offset of an existing '=' within symbol, found by the caller.
 * @return Offset after the next comma, or symbol.size() when no comma follows.
 */
std::size_t nextAttribute(std::u16string_view symbol, std::size_t equals) noexcept {
    const auto comma = symbol.find(u',', equals + 1);

    return comma == std::u16string_view::npos ? symbol.size() : comma + 1;
}

/** Erase a scanned attribute and its adjoining separator, or the sole attribute block.
 * @param symbol Caller-owned mutable UTF-16 string; no reference is retained.
 * @param length Recognized opening-brace offset, preserved while removing attributes.
 * @param begin Scanned key offset, at least length + 1 and within symbol.
 * @param next Scanned next-key offset, at most symbol.size() and greater than begin.
 * @throws std::out_of_range A supplied erase offset is outside symbol; valid scanner offsets cannot cause this.
 */
void dropAttribute(std::u16string &symbol, std::size_t length, std::size_t begin, std::size_t next) {
    if (next == symbol.size()) {
        if (begin == length + 1) {
            symbol.resize(length);
        } else {
            symbol.erase(begin - 1, next - begin);
        }
    } else {
        symbol.erase(begin, next - begin);
    }
}

/** Check key presence before inspecting a symbol or allocating its result.
 * @param key Optional borrowed UTF-16 key; empty is valid and nullopt is invalid.
 * @return Borrowed key view with the same lifetime as the caller's input; no storage is retained.
 * @throws std::invalid_argument The key is null.
 */
std::u16string_view requireKey(std::optional<std::u16string_view> key) {
    if (!key) {
        throw std::invalid_argument("Attribute key must not be null");
    }

    return *key;
}
} // namespace

std::optional<std::u16string> getAttributeStringByKey(std::optional<std::u16string_view> symbol,
                                                      std::optional<std::u16string_view> key) {
    const auto requiredKey = requireKey(key);

    if (!hasAttributes(symbol)) {
        return std::nullopt;
    }

    auto position = lengthWithoutAttributes(*symbol) + 1;

    while (position < symbol->size()) {
        const auto equals = symbol->find(u'=', position);

        if (equals == std::u16string_view::npos) {
            break;
        }

        const auto next = nextAttribute(*symbol, equals);

        if (symbol->substr(position, equals - position) == requiredKey) {
            if (next - 1 < equals + 1) {
                throw std::out_of_range("Invalid attribute value substring range");
            }

            return std::u16string(symbol->substr(equals + 1, next - equals - 2));
        }

        position = next;
    }

    return std::nullopt;
}

std::optional<std::u16string> changeAttributeStringByKey(std::optional<std::u16string_view> symbol,
                                                         std::optional<std::u16string_view> key,
                                                         std::optional<std::u16string_view> value) {
    const auto requiredKey = requireKey(key);

    if (!symbol && !value) {
        return std::nullopt;
    }

    std::u16string result(symbol.value_or(u""));
    const auto length = lengthWithoutAttributes(result);

    if (length == result.size()) {
        if (value) {
            result.append(u"{").append(requiredKey).append(u"=").append(*value).append(u"}");
        }

        return result;
    }

    auto position = length + 1;
    bool added = false;

    while (position < result.size()) {
        const auto equals = result.find(u'=', position);

        if (equals == std::u16string::npos) {
            break;
        }

        const auto next = nextAttribute(result, equals);
        const auto comparison = std::u16string_view(result).substr(position, equals - position).compare(requiredKey);

        if (comparison == 0 && (!value || added)) {
            dropAttribute(result, length, position, next);
        } else if (value && (comparison == 0 || (comparison > 0 && !added))) {
            std::u16string attribute(requiredKey);
            attribute.append(u"=").append(*value);
            const auto advance = attribute.size() + 1;

            if (comparison == 0) {
                result.replace(position, next - position - 1, attribute);
            } else {
                attribute.push_back(u',');
                result.insert(position, attribute);
            }

            added = true;
            position += advance;
        } else {
            position = next;
        }
    }

    if (value && !added) {
        std::u16string attribute(u",");
        attribute.append(requiredKey).append(u"=").append(*value);
        result.insert(position - 1, attribute);
    }

    return result;
}

std::optional<std::u16string> removeAttributeStringByKey(std::optional<std::u16string_view> symbol,
                                                         std::optional<std::u16string_view> key) {
    return changeAttributeStringByKey(symbol, key, std::nullopt);
}
} // namespace org::ttldtor::qdcxx::events::market_symbols
