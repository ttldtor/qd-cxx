// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/order_book_model.hpp"

#include "qd/events/event_flags.hpp"
#include "qd/events/order_index.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace org::ttldtor::qdcxx::api {
namespace {

/** Test the legacy end marker without changing exact encoded identity.
 * @param order Input order.
 * @return True when no explicit end marker exists and the old index convention applies.
 */
bool needsLegacySnapshotEnd(const events::Order &order) noexcept {
    using events::indexed_event_flags::SNAPSHOT_END;
    using events::indexed_event_flags::SNAPSHOT_SNIP;

    constexpr unsigned SPECIAL_SOURCE_SHIFT = 48;
    constexpr unsigned EXCHANGE_SHIFT = 32;
    const auto bits = static_cast<std::uint64_t>(order.getIndex());
    const auto specialSource = static_cast<std::int32_t>(bits >> SPECIAL_SOURCE_SHIFT);
    const auto exchange = static_cast<char16_t>(bits >> EXCHANGE_SHIFT);

    return (order.eventFlags & (SNAPSHOT_END | SNAPSHOT_SNIP)) == 0 && static_cast<std::uint32_t>(bits) == 0 &&
           (!events::isSpecialOrderSourceId(specialSource) || exchange == u'\0');
}

/** Compare sizes using Java Double.compare, including NaN and signed zero.
 * @param left Left business size.
 * @param right Right business size.
 * @return Ascending size relation.
 */
std::strong_ordering compareSize(double left, double right) noexcept {
    if (std::isnan(left)) {
        return std::isnan(right) ? std::strong_ordering::equal : std::strong_ordering::greater;
    }

    if (std::isnan(right) || left < right) {
        return std::strong_ordering::less;
    }

    if (left > right) {
        return std::strong_ordering::greater;
    }

    if (left == 0 && right == 0) {
        return std::signbit(right) <=> std::signbit(left);
    }

    return std::strong_ordering::equal;
}

/** Compare the reference secondary order for equal-price entries on one side.
 * @param left Left order.
 * @param right Right order.
 * @return Whether left precedes right.
 */
bool tieBreak(const events::Order &left, const events::Order &right) noexcept {
    const bool leftIndividual = left.getScope() == events::Scope::ORDER;
    const bool rightIndividual = right.getScope() == events::Scope::ORDER;

    if (leftIndividual != rightIndividual) {
        return !leftIndividual;
    }

    if (leftIndividual) {
        return std::tie(left.timeSequence, left.index) < std::tie(right.timeSequence, right.index);
    }

    const auto size = compareSize(left.getSizeAsDouble(), right.getSizeAsDouble());

    if (size != 0) {
        return size > 0;
    }

    using Key = std::tuple<std::int64_t, events::Scope, char16_t, const std::optional<std::u16string> &, std::int64_t>;

    return Key{left.timeSequence, left.getScope(), left.getExchangeCode(), left.marketMaker, left.index} <
           Key{right.timeSequence, right.getScope(), right.getExchangeCode(), right.marketMaker, right.index};
}

/** Sort one side with numeric prices before NaNs and the market order tie breaks.
 * @param side Owning entries on one side of the detached book.
 * @param buy True for descending numeric price, false for ascending.
 */
void sortSide(std::vector<events::Order> &side, bool buy) {
    std::sort(side.begin(), side.end(), [buy](const auto &left, const auto &right) {
        const auto leftPrice = left.getPrice();
        const auto rightPrice = right.getPrice();

        if (std::isnan(leftPrice) != std::isnan(rightPrice)) {
            return !std::isnan(leftPrice);
        }

        if (leftPrice < rightPrice) {
            return !buy;
        }

        if (leftPrice > rightPrice) {
            return buy;
        }

        return tieBreak(left, right);
    });
}

} // namespace

OrderBookModel::OrderBookModel(IndexedEventModelLimits limits) : limits_(limits), orders_(limits) {
}

std::expected<void, IndexedEventModelError>
OrderBookModel::processEvents(std::u16string_view symbol, std::int32_t sourceId, std::span<const events::Order> input) {
    if (symbol.size() > limits_.symbolUnits || input.size() > limits_.inputEvents) {
        return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
    }

    try {
        std::vector<events::Order> normalized;

        if (std::ranges::any_of(input, needsLegacySnapshotEnd)) {
            if (std::ranges::any_of(input, [symbol, sourceId](const auto &order) {
                    return order.symbol != symbol || order.getSourceId() != sourceId;
                })) {
                return std::unexpected(IndexedEventModelError::WRONG_KEY);
            }

            normalized.assign(input.begin(), input.end());

            for (auto &order : normalized) {
                if (needsLegacySnapshotEnd(order)) {
                    order.eventFlags |= events::indexed_event_flags::SNAPSHOT_END;
                }
            }

            input = normalized;
        }

        if (const auto applied = orders_.processEvents(symbol, sourceId, input); !applied) {
            return std::unexpected(applied.error());
        }

        return {};
    } catch (const std::length_error &) {
        return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(IndexedEventModelError::ALLOCATION_FAILURE);
    }
}

std::expected<OrderBookSnapshot, IndexedEventModelError> OrderBookModel::getSnapshot(std::u16string_view symbol) const {
    auto orders = orders_.getSnapshotForSymbol(symbol);

    if (!orders) {
        return std::unexpected(orders.error());
    }

    try {
        OrderBookSnapshot book;

        for (auto &order : *orders) {
            if (order.hasSize() || order.getScope() == events::Scope::COMPOSITE) {
                auto &side = order.getOrderSide() == events::Side::BUY ? book.buyOrders : book.sellOrders;
                side.push_back(std::move(order));
            }
        }

        sortSide(book.buyOrders, true);
        sortSide(book.sellOrders, false);

        return book;
    } catch (const std::length_error &) {
        return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(IndexedEventModelError::ALLOCATION_FAILURE);
    }
}

void OrderBookModel::clear() noexcept {
    orders_.clear();
}

} // namespace org::ttldtor::qdcxx::api
