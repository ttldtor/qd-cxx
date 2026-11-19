// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/core/symbol_striper.hpp"
#include "shared_record_block.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::core {
namespace detail {
/** Closed static partition kinds; ANY is the single-stripe identity. */
enum class StripeKind { ANY, HASH, RANGE };

/** Complete immutable leaf; no caller input or protocol state is borrowed. */
struct SymbolStripeRule {
    StripeKind kind = StripeKind::ANY;
    SymbolStriperLimits limits{};
    std::size_t storageBytes = 0;
    std::u16string name;
    std::u16string lower;
    std::u16string upper;
    std::uint32_t index = 0;
    std::uint32_t count = 1;
};

/** Complete immutable striper; hash count requires constant retained metadata. */
struct SymbolStriperState {
    StripeKind kind = StripeKind::ANY;
    SymbolStriperLimits limits{};
    std::size_t storageBytes = 0;
    std::u16string name;
    std::vector<std::u16string> boundaries;
    std::vector<std::uint64_t> codes;
    std::uint32_t count = 1;
    std::size_t codeLength = 0;
};
} // namespace detail

namespace {
using Error = SymbolStripeError;
using Kind = detail::StripeKind;
constexpr std::uint32_t HASH_MULTIPLIER = 0xB46394CDU;
constexpr std::uint32_t JAVA_INT_MAX = 0x7FFFFFFFU;
constexpr std::size_t RANGE_CODE_UNITS = 8;
constexpr char16_t MAX_RANGE_CODE_UNIT = 0x7F;
constexpr unsigned BYTE_BITS = 8;
constexpr unsigned LONG_BITS = 64;

/** Add bounded storage counts before allocating or publishing.
 * @param left First count.
 * @param right Additional count.
 * @return Exact sum, throwing RESOURCE_LIMIT for arithmetic overflow.
 */
std::size_t add(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw Error::RESOURCE_LIMIT;
    }

    return left + right;
}

/** Count array backing without overflowing.
 * @param count Number of stored items.
 * @param size Bytes per item.
 * @return Requested array bytes, throwing RESOURCE_LIMIT on overflow.
 */
std::size_t multiply(std::size_t count, std::size_t size) {
    if (size && count > std::numeric_limits<std::size_t>::max() / size) {
        throw Error::RESOURCE_LIMIT;
    }

    return count * size;
}

/** Enforce a real zero-capable finite grant.
 * @param value Actual count.
 * @param maximum Available grant.
 */
void requireCapacity(std::size_t value, std::size_t maximum) {
    if (value > maximum) {
        throw Error::RESOURCE_LIMIT;
    }
}

/** Recognize the exact ASCII range-boundary alphabet.
 * @param value Borrowed code unit.
 * @return Whether it is an ASCII letter or decimal digit.
 */
bool dictionary(char16_t value) noexcept {
    return (value >= u'A' && value <= u'Z') || (value >= u'a' && value <= u'z') || (value >= u'0' && value <= u'9');
}

/** Recognize one ASCII decimal digit.
 * @param value Borrowed code unit.
 * @return Whether value is in 0..9.
 */
bool digit(char16_t value) noexcept {
    return value >= u'0' && value <= u'9';
}

/** Decode a nonempty unsigned spelling in the positive Java-int domain.
 * @param text Borrowed complete decimal spelling; signs and whitespace are excluded.
 * @return Exact value, throwing INVALID_SPECIFICATION for malformed/overflowing input.
 */
std::uint32_t decimal(std::u16string_view text) {
    if (text.empty()) {
        throw Error::INVALID_SPECIFICATION;
    }

    std::uint32_t result = 0;

    for (const auto unit : text) {
        if (!digit(unit)) {
            throw Error::INVALID_SPECIFICATION;
        }

        const auto next = static_cast<std::uint32_t>(unit - u'0');

        if (result > (JAVA_INT_MAX - next) / 10U) {
            throw Error::INVALID_SPECIFICATION;
        }

        result = result * 10U + next;
    }

    return result;
}

/** Validate hash count grammar, separately from explicit materialization grants.
 * @param count Parsed Java-int count.
 */
void validateHashCount(std::uint32_t count) {
    if (count < 2 || !std::has_single_bit(count)) {
        throw Error::INVALID_SPECIFICATION;
    }
}

/** Validate an optionally open range boundary.
 * @param boundary Borrowed boundary text.
 * @param allowEmpty Whether an open boundary is admitted.
 */
void validateBoundary(std::u16string_view boundary, bool allowEmpty) {
    if ((!allowEmpty && boundary.empty()) || boundary.size() > RANGE_CODE_UNITS ||
        !std::ranges::all_of(boundary, dictionary)) {
        throw Error::INVALID_SPECIFICATION;
    }
}

/** Find the dictionary symbol after the optional spread multiplier and unused prefix.
 * @param symbol Exact physical UTF-16 symbol.
 * @return First significant code-unit offset, possibly symbol.size().
 */
std::size_t rangeStart(std::u16string_view symbol) noexcept {
    std::size_t start = 0;

    if (symbol.starts_with(u'=')) {
        start = 1;

        if (start < symbol.size() && (symbol[start] == u'+' || symbol[start] == u'-')) {
            ++start;
        }

        auto end = start;
        bool seenDigit = false;
        bool seenDot = false;
        bool validNumber = true;

        while (end < symbol.size()) {
            if (digit(symbol[end])) {
                seenDigit = true;
                ++end;
            } else if (symbol[end] == u'.') {
                if (seenDot || !seenDigit) {
                    validNumber = false;
                    break;
                }

                seenDot = true;
                ++end;
            } else {
                break;
            }
        }

        if (validNumber && seenDigit && end < symbol.size() && symbol[end] == u'*') {
            start = end + 1;
        }
    }

    while (start < symbol.size() && !dictionary(symbol[start])) {
        ++start;
    }

    return start;
}

/** Encode at most eight clamped ASCII units into high bytes of an unsigned Java-long code.
 * @param symbol Borrowed significant symbol suffix.
 * @param units Maximum significant units, at most RANGE_CODE_UNITS.
 * @return Zero-padded comparable code; the high sign bit remains clear.
 */
std::uint64_t rangeCode(std::u16string_view symbol, std::size_t units = RANGE_CODE_UNITS) noexcept {
    std::uint64_t result = 0;
    const auto count = std::min({symbol.size(), units, RANGE_CODE_UNITS});

    for (std::size_t index = 0; index < count; ++index) {
        const auto unit = std::min(symbol[index], MAX_RANGE_CODE_UNIT);
        result |= static_cast<std::uint64_t>(unit) << (LONG_BITS - BYTE_BITS * static_cast<unsigned>(index + 1));
    }

    return result;
}

/** Compute the production high-bit hash partition with defined unsigned overflow.
 * @param symbol Exact UTF-16 units.
 * @param count Valid power-of-two stripe count, at least two.
 * @return Zero-based stripe index.
 */
std::uint32_t hashIndex(std::u16string_view symbol, std::uint32_t count) noexcept {
    std::uint32_t hash = 0;

    for (const auto unit : symbol) {
        hash = 31U * hash + unit;
    }

    return (hash * HASH_MULTIPLIER) >> (32U - std::countr_zero(count));
}

/** Format unsigned decimal metadata as exact ASCII UTF-16.
 * @param value Unsigned value.
 * @return Owned decimal spelling.
 */
std::u16string number(std::size_t value) {
    const auto ascii = std::to_string(value);

    return {ascii.begin(), ascii.end()};
}

/** Measure a string's complete allocated character capacity, conservatively including its terminator.
 * @param text Owned immutable candidate string.
 * @return Conservative byte count.
 */
std::size_t textBytes(const std::u16string &text) {
    return multiply(add(text.capacity(), 1), sizeof(char16_t));
}

/** Enforce the complete leaf backing grant after candidate construction.
 * @param rule Unpublished candidate.
 */
void validateStorage(detail::SymbolStripeRule &rule) {
    auto bytes = add(detail::recordControlBytes<detail::SymbolStripeRule>(), textBytes(rule.name));
    bytes = add(bytes, add(textBytes(rule.lower), textBytes(rule.upper)));
    requireCapacity(bytes, rule.limits.storageBytes);
    rule.storageBytes = bytes;
}

/** Enforce complete striper object, strings and vector capacities.
 * @param state Unpublished immutable candidate.
 */
void validateStorage(detail::SymbolStriperState &state) {
    auto bytes = add(detail::recordControlBytes<detail::SymbolStriperState>(), textBytes(state.name));
    bytes = add(bytes, multiply(state.boundaries.capacity(), sizeof(std::u16string)));
    bytes = add(bytes, multiply(state.codes.capacity(), sizeof(std::uint64_t)));

    for (const auto &boundary : state.boundaries) {
        bytes = add(bytes, textBytes(boundary));
    }

    requireCapacity(bytes, state.limits.storageBytes);
    state.storageBytes = bytes;
}
} // namespace

SymbolStripeFilter::SymbolStripeFilter(std::shared_ptr<const detail::SymbolStripeRule> rule) noexcept
    : rule_(std::move(rule)) {
}

std::expected<SymbolStripeFilter, SymbolStripeError> SymbolStripeFilter::parse(std::u16string_view specification,
                                                                               SymbolStriperLimits limits) {
    try {
        requireCapacity(specification.size(), limits.specificationUnits);
        requireCapacity(detail::recordControlBytes<detail::SymbolStripeRule>(), limits.storageBytes);
        auto rule =
            std::allocate_shared<detail::SymbolStripeRule>(detail::RecordBlockAllocator<detail::SymbolStripeRule>{});
        rule->limits = limits;
        rule->name = specification;

        if (specification == u"*") {
            rule->kind = Kind::ANY;
        } else if (specification.starts_with(u"hash")) {
            const auto separator = specification.find(u"of", 4);

            if (separator == specification.npos) {
                throw Error::INVALID_SPECIFICATION;
            }

            rule->index = decimal(specification.substr(4, separator - 4));
            rule->count = decimal(specification.substr(separator + 2));
            validateHashCount(rule->count);
            requireCapacity(rule->count, limits.stripes);

            if (rule->index >= rule->count) {
                throw Error::INVALID_SPECIFICATION;
            }

            rule->kind = Kind::HASH;
        } else if (specification.starts_with(u"range")) {
            if (!specification.starts_with(u"range-") || !specification.ends_with(u'-')) {
                throw Error::INVALID_SPECIFICATION;
            }

            const auto separator = specification.find(u'-', 6);

            if (separator == specification.npos || separator == specification.size() - 1) {
                throw Error::INVALID_SPECIFICATION;
            }

            rule->lower = specification.substr(6, separator - 6);
            rule->upper = specification.substr(separator + 1, specification.size() - separator - 2);
            validateBoundary(rule->lower, true);
            validateBoundary(rule->upper, true);

            if (!rule->lower.empty() && !rule->upper.empty() && rule->lower >= rule->upper) {
                throw Error::INVALID_SPECIFICATION;
            }

            rule->kind = rule->lower.empty() && rule->upper.empty() ? Kind::ANY : Kind::RANGE;

            if (rule->kind == Kind::ANY) {
                rule->name = u"*";
            }
        } else {
            throw Error::UNKNOWN_SPECIFICATION;
        }

        requireCapacity(1, limits.stripes);
        validateStorage(*rule);

        return SymbolStripeFilter(std::move(rule));
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    }
}

bool SymbolStripeFilter::valid() const noexcept {
    return bool(rule_);
}

std::u16string_view SymbolStripeFilter::name() const noexcept {
    return rule_ ? std::u16string_view(rule_->name) : std::u16string_view{};
}

std::size_t SymbolStripeFilter::storageBytes() const noexcept {
    return rule_ ? rule_->storageBytes : 0;
}

std::expected<bool, SymbolStripeError> SymbolStripeFilter::accepts(std::u16string_view symbol,
                                                                   bool wildcard) const noexcept {
    if (!rule_) {
        return std::unexpected(Error::INVALID_STATE);
    }

    if (symbol.size() > rule_->limits.symbolUnits) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    }

    if (wildcard || rule_->kind == Kind::ANY) {
        return true;
    }

    if (rule_->kind == Kind::HASH) {
        return hashIndex(symbol, rule_->count) == rule_->index;
    }

    const auto tail = symbol.substr(rangeStart(symbol));

    return (rule_->lower.empty() || std::u16string_view(rule_->lower) <= tail) &&
           (rule_->upper.empty() || tail < std::u16string_view(rule_->upper));
}

SymbolStriper::SymbolStriper(std::shared_ptr<const detail::SymbolStriperState> state) noexcept
    : state_(std::move(state)) {
}

std::expected<SymbolStriper, SymbolStripeError> SymbolStriper::parse(std::u16string_view specification,
                                                                     SymbolStriperLimits limits) {
    try {
        requireCapacity(specification.size(), limits.specificationUnits);
        requireCapacity(detail::recordControlBytes<detail::SymbolStriperState>(), limits.storageBytes);
        auto state = std::allocate_shared<detail::SymbolStriperState>(
            detail::RecordBlockAllocator<detail::SymbolStriperState>{});
        state->limits = limits;

        if (specification.empty() || specification == u"by1") {
            state->name = u"by1";
        } else if (specification.starts_with(u"byhash")) {
            state->count = decimal(specification.substr(6));
            validateHashCount(state->count);
            requireCapacity(state->count, limits.stripes);
            state->kind = Kind::HASH;
            state->name = u"byhash" + number(state->count);
        } else if (specification.starts_with(u"byrange")) {
            if (!specification.starts_with(u"byrange-") || !specification.ends_with(u'-') ||
                specification.size() < 10) {
                throw Error::INVALID_SPECIFICATION;
            }

            const auto count = std::ranges::count(specification.substr(8), u'-');
            requireCapacity(add(static_cast<std::size_t>(count), 1), limits.stripes);
            state->boundaries.reserve(static_cast<std::size_t>(count));
            state->codes.reserve(static_cast<std::size_t>(count));
            std::size_t begin = 8;

            while (begin < specification.size()) {
                const auto end = specification.find(u'-', begin);

                if (end == specification.npos) {
                    throw Error::INVALID_SPECIFICATION;
                }

                const auto boundary = specification.substr(begin, end - begin);
                validateBoundary(boundary, false);

                if (!state->boundaries.empty() && std::u16string_view(state->boundaries.back()) >= boundary) {
                    throw Error::INVALID_SPECIFICATION;
                }

                state->boundaries.emplace_back(boundary);
                state->codes.push_back(rangeCode(boundary));
                state->codeLength = std::max(state->codeLength, boundary.size());
                begin = end + 1;
            }

            if (state->boundaries.size() >= JAVA_INT_MAX) {
                throw Error::RESOURCE_LIMIT;
            }

            state->count = static_cast<std::uint32_t>(state->boundaries.size() + 1);
            state->kind = Kind::RANGE;
            state->name = specification;
        } else {
            throw Error::UNKNOWN_SPECIFICATION;
        }

        requireCapacity(state->count, limits.stripes);
        validateStorage(*state);

        return SymbolStriper(std::move(state));
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    }
}

bool SymbolStriper::valid() const noexcept {
    return bool(state_);
}

std::u16string_view SymbolStriper::name() const noexcept {
    return state_ ? std::u16string_view(state_->name) : std::u16string_view{};
}

std::size_t SymbolStriper::storageBytes() const noexcept {
    return state_ ? state_->storageBytes : 0;
}

std::size_t SymbolStriper::stripeCount() const noexcept {
    return state_ ? state_->count : 0;
}

std::expected<std::size_t, SymbolStripeError> SymbolStriper::stripeIndex(std::u16string_view symbol) const noexcept {
    if (!state_) {
        return std::unexpected(Error::INVALID_STATE);
    }

    if (symbol.size() > state_->limits.symbolUnits) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    }

    if (state_->kind == Kind::ANY) {
        return 0;
    }

    if (state_->kind == Kind::HASH) {
        return hashIndex(symbol, state_->count);
    }

    const auto code = rangeCode(symbol.substr(rangeStart(symbol)), state_->codeLength);
    const auto next = std::upper_bound(state_->codes.begin(), state_->codes.end(), code);

    return static_cast<std::size_t>(next - state_->codes.begin());
}

std::expected<SymbolStripeFilter, SymbolStripeError> SymbolStriper::stripeFilter(std::size_t index) const {
    if (!state_) {
        return std::unexpected(Error::INVALID_STATE);
    }

    if (index >= state_->count) {
        return std::unexpected(Error::INVALID_INDEX);
    }

    try {
        std::u16string expression = u"*";

        if (state_->kind == Kind::HASH) {
            const auto count = number(state_->count);
            auto stripe = number(index);
            stripe.insert(0, count.size() - stripe.size(), u'0');
            expression = u"hash" + stripe + u"of" + count;
        } else if (state_->kind == Kind::RANGE) {
            expression = u"range-";

            if (index) {
                expression += state_->boundaries[index - 1];
            }

            expression += u'-';

            if (index < state_->boundaries.size()) {
                expression += state_->boundaries[index];
            }

            expression += u'-';
        }

        auto limits = state_->limits;
        // The input-text bound applies to caller specifications, not a generated canonical leaf.
        limits.specificationUnits = std::max(limits.specificationUnits, expression.size());

        return SymbolStripeFilter::parse(expression, limits);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    }
}

std::expected<std::optional<StripeIntersection>, SymbolStripeError>
SymbolStriper::intersectingStripes(const SymbolStripeFilter &filter) const noexcept {
    if (!state_ || !filter.rule_) {
        return std::unexpected(Error::INVALID_STATE);
    }

    const auto &rule = *filter.rule_;

    if (state_->kind != rule.kind || state_->kind == Kind::ANY) {
        return std::optional<StripeIntersection>{};
    }

    if (state_->kind == Kind::HASH) {
        if (state_->count <= rule.count) {
            return StripeIntersection{rule.index / (rule.count / state_->count), 1};
        }

        const auto span = state_->count / rule.count;

        return StripeIntersection{static_cast<std::size_t>(rule.index) * span, span};
    }

    const auto first = rule.lower.empty()
                           ? state_->codes.begin()
                           : std::upper_bound(state_->codes.begin(), state_->codes.end(), rangeCode(rule.lower));
    const auto last = rule.upper.empty()
                          ? state_->codes.end()
                          : std::lower_bound(state_->codes.begin(), state_->codes.end(), rangeCode(rule.upper));
    const auto from = static_cast<std::size_t>(first - state_->codes.begin());
    const auto to = rule.upper.empty() ? state_->count : static_cast<std::size_t>(last - state_->codes.begin()) + 1;

    return StripeIntersection{from, to - from};
}
} // namespace org::ttldtor::qdcxx::core
