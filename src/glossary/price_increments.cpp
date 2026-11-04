// Copyright (C) 2002 - 2024 Devexperts LLC.
// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, see https://mozilla.org/MPL/2.0/.
#include "qd/glossary/price_increments.hpp"
#include "numeric_text.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::glossary {
namespace {
constexpr int MAXIMUM_PRECISION = 18;
constexpr double MINIMUM_INCREMENT = 1e-18;
constexpr double RELATIVE_EPS = 1e-6;

/** @param text Exact nonempty input.
 * @return Whether the entire input is a signed, optional-point decimal zero spelling.
 */
bool isEmptyText(std::u16string_view text) noexcept {
    bool point = false;
    bool zero = false;

    for (std::size_t i = text.front() == u'-' || text.front() == u'+' ? 1 : 0; i < text.size(); ++i) {
        if (text[i] == u'0') {
            zero = true;
        } else if (text[i] == u'.' && !point) {
            point = true;
        } else {
            return false;
        }
    }

    return zero;
}

/** @param units Requested text length.
 * @param limits Explicit finite value bounds.
 * @throws std::length_error Input exceeds configured or Java signed32 text bounds.
 */
void checkText(std::size_t units, PriceIncrementLimits limits) {
    if (units > limits.textUnits || units > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::length_error("price increments text limit exceeded");
    }
}

/** @param count Requested number of ranges.
 * @param limits Explicit finite value bounds.
 * @throws std::length_error Range table exceeds its configured bound.
 */
void checkRanges(std::size_t count, PriceIncrementLimits limits) {
    if (count > limits.ranges) {
        throw std::length_error("price increments range limit exceeded");
    }
}

/** @param value Nonnegative increment.
 * @return Required fractional decimal digits, from zero through eighteen.
 */
int computePrecision(double value) noexcept {
    for (int i = 0; i < MAXIMUM_PRECISION; ++i) {
        auto rounded = std::floor(value + 0.5);
        auto eps = std::abs(value * RELATIVE_EPS);

        if (rounded >= value - eps && rounded <= value + eps) {
            return i;
        }

        value *= 10;
    }

    return MAXIMUM_PRECISION;
}

/** @param price Input price.
 * @param increment Positive increment selected for that price and policy.
 * @param mode Resolved policy (UP/DOWN already converted).
 * @return Reference rounded price after decimal cleanup.
 * @throws std::domain_error UNNECESSARY requires rounding.
 * @throws std::invalid_argument Unknown policy.
 * @throws std::bad_alloc Decimal conversion allocation fails.
 */
double roundWithIncrement(double price, double increment, PriceRoundingMode mode) {
    auto scaled = price / increment;
    double rounded;

    switch (mode) {
    case PriceRoundingMode::CEILING:
        rounded = std::ceil(scaled - RELATIVE_EPS);
        break;
    case PriceRoundingMode::FLOOR:
        rounded = std::floor(scaled + RELATIVE_EPS);
        break;
    case PriceRoundingMode::HALF_UP:
        rounded = price >= 0 ? std::floor(scaled + 0.5 + RELATIVE_EPS) : std::ceil(scaled - 0.5 - RELATIVE_EPS);
        break;
    case PriceRoundingMode::HALF_DOWN:
        rounded = price >= 0 ? std::ceil(scaled - 0.5 - RELATIVE_EPS) : std::floor(scaled + 0.5 + RELATIVE_EPS);
        break;
    case PriceRoundingMode::HALF_EVEN:
    case PriceRoundingMode::UNNECESSARY: {
        rounded = std::floor(scaled);
        auto remainder = scaled - rounded;

        if (remainder >= 0.5 + RELATIVE_EPS ||
            (remainder > 0.5 - RELATIVE_EPS && std::floor(rounded / 2) * 2 != rounded)) {
            ++rounded;
        }

        auto result = detail::roundPriceDecimal(rounded * increment);

        if (mode == PriceRoundingMode::UNNECESSARY) {
            if (result != price) {
                throw std::domain_error("price is not round");
            }

            return price;
        }

        return result;
    }
    default:
        throw std::invalid_argument("unknown price rounding mode");
    }

    return detail::roundPriceDecimal(rounded * increment);
}
} // namespace

/** Fully parsed owned value; constructed before it is shared as const. */
struct PriceIncrements::State {
    std::u16string text;            ///< Exact original identity.
    std::vector<double> increments; ///< Alternating finite increments and upper limits.
    std::vector<double> precisions; ///< Range precisions interleaved with cleaned breakpoints.
};

PriceIncrements::PriceIncrements(std::shared_ptr<const State> state) noexcept : state_(std::move(state)) {
}

const PriceIncrements &PriceIncrements::empty() noexcept {
    static const PriceIncrements value;

    return value;
}

PriceIncrements PriceIncrements::valueOf(std::optional<std::u16string_view> input, PriceIncrementLimits limits) {
    if (!input || input->empty()) {
        return {};
    }

    auto text = *input;
    checkText(text.size(), limits);

    if (isEmptyText(text)) {
        return {};
    }

    auto count = static_cast<std::size_t>(std::count(text.begin(), text.end(), u';')) + 1;
    checkRanges(count, limits);

    if (count > (std::numeric_limits<std::size_t>::max() - 1) / 2) {
        throw std::length_error("price increments range size overflow");
    }

    auto state = std::make_shared<State>();
    state->increments.reserve(count * 2 - 1);
    std::size_t start = 0;

    for (;;) {
        auto separator = text.find(u';', start);

        if (separator == text.npos) {
            if (start >= text.size() || text[start] == u' ' || text.back() == u' ') {
                throw std::invalid_argument("inappropriate use of separators");
            }

            auto inc = detail::parseAdditionalNumber(text.substr(start));

            if (!std::isfinite(inc) || inc < 0 || (!state->increments.empty() && inc < MINIMUM_INCREMENT)) {
                throw std::invalid_argument("increment is not a finite positive number");
            }

            state->increments.push_back(inc);
            break;
        }

        if (separator + 2 >= text.size() || text[separator + 1] != u' ') {
            throw std::invalid_argument("inappropriate use of separators");
        }

        auto space = text.find(u' ', start);

        if (space == text.npos || space <= start || space >= separator - 1 || space < text.rfind(u' ', separator - 1)) {
            throw std::invalid_argument("inappropriate use of separators");
        }

        auto inc = detail::parseAdditionalNumber(text.substr(start, space - start));

        if (!std::isfinite(inc) || inc < MINIMUM_INCREMENT) {
            throw std::invalid_argument("increment is not a finite positive number");
        }

        auto limit = detail::parseAdditionalNumber(text.substr(space + 1, separator - space - 1));

        if (!std::isfinite(limit) || limit < MINIMUM_INCREMENT) {
            throw std::invalid_argument("limit is not a finite positive number");
        }

        if (state->increments.size() > 1 && limit <= state->increments.back() + 2 * MINIMUM_INCREMENT) {
            throw std::invalid_argument("increments are not ordered properly");
        }

        state->increments.push_back(inc);
        state->increments.push_back(limit);
        start = separator + 2;
    }

    state->precisions.resize(state->increments.size());

    for (std::size_t i = 0; i < state->increments.size(); i += 2) {
        state->precisions[i] = computePrecision(state->increments[i]);
    }

    for (std::size_t i = 1; i < state->increments.size(); i += 2) {
        state->precisions[i] = detail::roundPriceDecimal(
            state->increments[i] + std::min(state->increments[i - 1], state->increments[i + 1]) * RELATIVE_EPS);
    }

    state->text = text;

    return PriceIncrements(std::move(state));
}

PriceIncrements PriceIncrements::valueOf(double increment, PriceIncrementLimits limits) {
    if (!std::isfinite(increment) || increment < 0) {
        throw std::invalid_argument("increment is not a finite positive number");
    }

    if (increment == 0) {
        return {};
    }

    checkRanges(1, limits);

    return valueOf(std::u16string_view(detail::formatAdditionalNumber(increment)), limits);
}

PriceIncrements PriceIncrements::valueOf(std::span<const double> increments, PriceIncrementLimits limits) {
    if (increments.empty() || (increments.size() == 1 && increments.front() == 0)) {
        return {};
    }

    if (increments.size() % 2 == 0) {
        throw std::invalid_argument("Increments length is even");
    }

    checkRanges(increments.size() / 2 + 1, limits);
    std::u16string text; ///< Exact original identity.

    for (std::size_t i = 0; i < increments.size(); ++i) {
        auto value = increments[i];

        if (!std::isfinite(value) || value < MINIMUM_INCREMENT) {
            throw std::invalid_argument(i % 2 == 0 ? "Increment is not a finite positive number"
                                                   : "Limit is not a finite positive number");
        }

        if (i % 2 != 0 && i > 1 && value <= increments[i - 2] + 2 * MINIMUM_INCREMENT) {
            throw std::invalid_argument("Increments are not ordered properly");
        }

        auto number = detail::formatAdditionalNumber(value);
        auto separator = i == 0       ? std::u16string_view{}
                         : i % 2 != 0 ? std::u16string_view(u" ")
                                      : std::u16string_view(u"; ");

        if (number.size() > limits.textUnits || separator.size() > limits.textUnits - number.size() ||
            text.size() > limits.textUnits - number.size() - separator.size()) {
            throw std::length_error("price increments text limit exceeded");
        }

        text += separator;
        text += number;
    }

    return valueOf(std::u16string_view(text), limits);
}

const std::u16string &PriceIncrements::getText() const noexcept {
    if (state_) {
        return state_->text;
    }

    static const std::u16string text;

    return text;
}

std::vector<double> PriceIncrements::getPriceIncrements() const {
    return state_ ? state_->increments : std::vector<double>{0};
}

double PriceIncrements::getPriceIncrement() const noexcept {
    return state_ ? state_->increments.front() : 0;
}

double PriceIncrements::getPriceIncrement(double price, std::int32_t direction) const noexcept {
    if (!state_) {
        return 0;
    }

    if (price < 0) {
        price = -price;
        // Java signed32 negation wraps for MIN_VALUE; avoid C++ signed overflow.
        direction = std::bit_cast<std::int32_t>(0u - static_cast<std::uint32_t>(direction));
    }

    if (std::isnan(price)) {
        return getPriceIncrement();
    }

    auto &increments = state_->increments;
    std::size_t i = 1;

    while (i < increments.size() &&
           price > increments[i] + std::min(increments[i - 1], increments[i + 1]) * RELATIVE_EPS) {
        i += 2;
    }

    if (direction < 0 || i >= increments.size() ||
        price < increments[i] - std::min(increments[i - 1], increments[i + 1]) * RELATIVE_EPS) {
        return increments[i - 1];
    }

    return direction > 0 ? increments[i + 1] : std::min(increments[i - 1], increments[i + 1]);
}

int PriceIncrements::getPricePrecision() const noexcept {
    return state_ ? static_cast<int>(state_->precisions.front()) : 0;
}

int PriceIncrements::getPricePrecision(double price) const noexcept {
    if (!state_) {
        return 0;
    }

    if (price < 0) {
        price = -price;
    }

    if (std::isnan(price)) {
        return getPricePrecision();
    }

    std::size_t i = 1;

    while (i < state_->precisions.size() && price > state_->precisions[i]) {
        i += 2;
    }

    return static_cast<int>(state_->precisions[i - 1]);
}

double PriceIncrements::roundPrice(double price, std::int32_t direction) const {
    if (std::isnan(price)) {
        return price;
    }

    auto increment = getPriceIncrement(price, direction);

    if (increment == 0) {
        return price;
    }

    auto mode = direction < 0   ? PriceRoundingMode::FLOOR
                : direction > 0 ? PriceRoundingMode::CEILING
                                : PriceRoundingMode::HALF_EVEN;

    return roundWithIncrement(price, increment, mode);
}

double PriceIncrements::roundPrice(double price, PriceRoundingMode mode) const {
    if (std::isnan(price)) {
        return price;
    }

    if (mode == PriceRoundingMode::UP) {
        mode = price >= 0 ? PriceRoundingMode::CEILING : PriceRoundingMode::FLOOR;
    } else if (mode == PriceRoundingMode::DOWN) {
        mode = price >= 0 ? PriceRoundingMode::FLOOR : PriceRoundingMode::CEILING;
    }

    auto direction = mode == PriceRoundingMode::CEILING ? 1 : mode == PriceRoundingMode::FLOOR ? -1 : 0;
    auto increment = getPriceIncrement(price, direction);

    return increment == 0 ? price : roundWithIncrement(price, increment, mode);
}

double PriceIncrements::incrementPrice(double price, std::int32_t direction, double step) const {
    if (direction == 0) {
        throw std::invalid_argument("direction is 0");
    }

    if (!std::isfinite(step) || step < 0) {
        throw std::invalid_argument("step is not a finite positive number");
    }

    auto increment = getPriceIncrement(price, direction);
    auto delta = std::max(increment, step);

    // Math.max gives positive zero precedence even when its first operand is negative zero.
    if (delta == 0 && (!std::signbit(increment) || !std::signbit(step))) {
        delta = 0.0;
    }

    return roundPrice(direction > 0 ? price + delta : price - delta);
}

std::int32_t PriceIncrements::hashCode() const noexcept {
    std::uint32_t hash = 0;

    for (auto ch : getText()) {
        hash = hash * 31 + ch;
    }

    return std::bit_cast<std::int32_t>(hash);
}

std::u16string PriceIncrements::toString() const {
    return getText();
}

bool PriceIncrements::operator==(const PriceIncrements &other) const noexcept {
    return getText() == other.getText();
}
} // namespace org::ttldtor::qdcxx::glossary
