// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/order_book_model.hpp"

#include "qd/events/event_flags.hpp"
#include "qd/events/order_index.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <new>
#include <optional>
#include <set>
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

constexpr unsigned COMPOSITE_SCOPE = 1U << std::to_underlying(events::Scope::COMPOSITE);
constexpr unsigned REGIONAL_SCOPE = 1U << std::to_underlying(events::Scope::REGIONAL);
constexpr unsigned AGGREGATE_SCOPE = 1U << std::to_underlying(events::Scope::AGGREGATE);
constexpr unsigned ORDER_SCOPE = 1U << std::to_underlying(events::Scope::ORDER);
constexpr unsigned ALL_SCOPES = COMPOSITE_SCOPE | REGIONAL_SCOPE | AGGREGATE_SCOPE | ORDER_SCOPE;

/** Map one exact filter to the order scopes it allows.
 * @param filter Requested view filter.
 * @return Allowed scope bits, or no value for an unrecognized policy.
 */
std::optional<unsigned> scopeMask(OrderBookModelFilter filter) noexcept {
    switch (filter) {
    case OrderBookModelFilter::UNFILTERED:
    case OrderBookModelFilter::ALL:
        return ALL_SCOPES;
    case OrderBookModelFilter::COMPOSITE:
        return COMPOSITE_SCOPE;
    case OrderBookModelFilter::REGIONAL:
        return REGIONAL_SCOPE;
    case OrderBookModelFilter::AGGREGATE:
        return AGGREGATE_SCOPE;
    case OrderBookModelFilter::ORDER:
        return ORDER_SCOPE;
    case OrderBookModelFilter::COMPOSITE_REGIONAL:
        return COMPOSITE_SCOPE | REGIONAL_SCOPE;
    case OrderBookModelFilter::COMPOSITE_REGIONAL_AGGREGATE:
        return COMPOSITE_SCOPE | REGIONAL_SCOPE | AGGREGATE_SCOPE;
    }

    return std::nullopt;
}

/** Borrow one exchange/market-maker slot during a single read-only pass.
 * @param order An entry in the unchanged side vector.
 * @return Exchange and nullable maker, whose text is valid until that entry moves.
 */
auto marketSlot(const events::Order &order) noexcept {
    const auto maker = order.marketMaker ? std::optional<std::u16string_view>(*order.marketMaker) : std::nullopt;

    return std::pair{order.getExchangeCode(), maker};
}

/** Remove lower-scope entries covered by the eligible, same-side detailed entries.
 * @param side Owning detached orders whose source and scope filters already passed.
 */
void applyScopeFallback(std::vector<events::Order> &side) {
    constexpr std::size_t EXCHANGE_CODE_COUNT = 1U << 7;
    std::array<bool, EXCHANGE_CODE_COUNT> detailed{};
    std::set<std::pair<char16_t, std::optional<std::u16string_view>>> individuals;
    bool hasNonComposite = false;

    for (const auto &order : side) {
        const auto scope = order.getScope();
        hasNonComposite |= scope != events::Scope::COMPOSITE;

        if (scope == events::Scope::ORDER) {
            individuals.insert(marketSlot(order));
        }

        if (scope == events::Scope::ORDER || scope == events::Scope::AGGREGATE) {
            detailed[order.getExchangeCode()] = true;
        }
    }

    std::vector<bool> hidden;
    hidden.reserve(side.size());

    for (const auto &order : side) {
        const auto scope = order.getScope();
        hidden.push_back((scope == events::Scope::COMPOSITE && hasNonComposite) ||
                         (scope == events::Scope::REGIONAL && detailed[order.getExchangeCode()]) ||
                         (scope == events::Scope::AGGREGATE && individuals.contains(marketSlot(order))));
    }

    individuals.clear();
    std::size_t index = 0;
    std::erase_if(side, [&hidden, &index](const auto &) {
        return hidden[index++];
    });
}

/** Project the display size from the raw retained business size.
 * @param order Detached output; the source decimal encoding remains intact.
 * @param lotSize Positive multiplier. Individual orders keep their original size.
 */
void scaleLot(events::Order &order, std::int32_t lotSize) {
    if (lotSize == 1 || order.getScope() == events::Scope::ORDER) {
        return;
    }

    constexpr std::size_t ORDER_SIZE_SNAPSHOT = 1;
    const auto scaled = order.getSizeAsDouble() * lotSize;

    if (!order.businessNumericSnapshot) {
        order.businessNumericSnapshot.emplace();
    }

    (*order.businessNumericSnapshot)[ORDER_SIZE_SNAPSHOT] = {order.size.raw, scaled};
}

/** Check eligibility for positive aggregate correction stamps.
 * @param order Completed order, with original indexed flags on updates.
 * @return True for a live finite-price aggregate with positive size and a known side.
 */
bool eligibleForCorrection(const events::Order &order) noexcept {
    const auto side = order.getOrderSide();

    return (order.eventFlags & events::indexed_event_flags::REMOVE_EVENT) == 0 &&
           order.getScope() == events::Scope::AGGREGATE && order.getSizeAsDouble() > 0 &&
           std::isfinite(order.getPrice()) && (side == events::Side::BUY || side == events::Side::SELL);
}

/** Obtain an exact nonnegative interval even across signed epoch extremes.
 * @param newer Time in milliseconds, at least older.
 * @param older Time in milliseconds.
 * @return Unsigned distance in milliseconds.
 */
std::uint64_t elapsed(std::int64_t newer, std::int64_t older) noexcept {
    return static_cast<std::uint64_t>(newer) - static_cast<std::uint64_t>(older);
}

/** Test whether the two finite prices on opposite sides cross.
 * @param leftSide First order side.
 * @param leftPrice First order price.
 * @param rightSide Second order side.
 * @param rightPrice Second order price.
 * @return True when one side buys at or above the other side's ask.
 */
bool crossed(events::Side leftSide, double leftPrice, events::Side rightSide, double rightPrice) noexcept {
    return leftSide != rightSide && (leftSide == events::Side::BUY ? leftPrice >= rightPrice : rightPrice >= leftPrice);
}

} // namespace

OrderBookModel::OrderBookModel(IndexedEventModelLimits limits) : OrderBookModel(limits, OrderBookCorrectionOptions{}) {
}

OrderBookModel::OrderBookModel(IndexedEventModelLimits limits, OrderBookCorrectionOptions corrections)
    : limits_(limits), orders_(limits), correctionOptions_(corrections) {
    if ((corrections.policy != OrderBookCorrectionPolicy::DISABLED &&
         corrections.policy != OrderBookCorrectionPolicy::SAME_SYMBOL) ||
        corrections.keepAge.count() < 0 || corrections.flipAge.count() < 0) {
        throw std::invalid_argument("Invalid order-book correction options");
    }
}

OrderBookModel::OrderBookModel(OrderBookModel &&other) : OrderBookModel(other.limits_, other.correctionOptions_) {
    orders_ = std::move(other.orders_);
    corrections_.swap(other.corrections_);
    source_ = std::move(other.source_);
}

OrderBookModel &OrderBookModel::operator=(OrderBookModel &&other) noexcept {
    if (this != &other) {
        orders_ = std::move(other.orders_);
        limits_ = other.limits_;
        correctionOptions_ = other.correctionOptions_;
        corrections_.clear();
        corrections_.swap(other.corrections_);
        source_ = std::move(other.source_);
    }

    return *this;
}

void OrderBookModel::correct(CorrectionEntries &entries, const events::Order &order) const {
    const auto index = order.getIndex();
    CorrectionStamp incoming{order.getTime(), order.getPrice(), order.getOrderSide(), false};
    const auto keep = static_cast<std::uint64_t>(correctionOptions_.keepAge.count());
    const auto flip = static_cast<std::uint64_t>(correctionOptions_.flipAge.count());
    const auto sameSlot = [&incoming](const auto &other) {
        return incoming.side == other.side && incoming.price == other.price;
    };
    const auto crosses = [&incoming](const auto &other) {
        return crossed(incoming.side, incoming.price, other.side, other.price);
    };

    for (const auto &[otherIndex, other] : entries) {
        if (otherIndex == index || other.suppressed || other.time <= incoming.time) {
            continue;
        }

        const auto age = elapsed(other.time, incoming.time);

        if (sameSlot(other) || age > keep || (crosses(other) && age > flip)) {
            incoming.suppressed = true;
            entries.insert_or_assign(index, incoming);

            return;
        }
    }

    const auto previous = entries.find(index);

    if (previous != entries.end() && previous->second.suppressed && sameSlot(previous->second) &&
        previous->second.time >= incoming.time) {
        return;
    }

    for (auto &[otherIndex, other] : entries) {
        if (otherIndex == index || other.suppressed || other.time > incoming.time) {
            continue;
        }

        const auto age = elapsed(incoming.time, other.time);

        if (sameSlot(other) || age >= keep || (crosses(other) && age >= flip)) {
            other.suppressed = true;
        }
    }

    entries.insert_or_assign(index, incoming);
}

std::expected<void, IndexedEventModelError>
OrderBookModel::stageCorrections(CorrectionEntries &entries, std::int32_t sourceId,
                                 std::span<const IndexedEventChange<events::Order>> changes,
                                 std::size_t capacity) const {
    std::map<std::int64_t, std::size_t> positions;
    std::vector<const events::Order *> completed;
    bool replacesSource = false;

    for (const auto &change : changes) {
        if (change.kind != IndexedEventChangeKind::UPDATES) {
            replacesSource = true;
            positions.clear();
            completed.clear();
        }

        for (const auto &order : change.events) {
            const auto [position, inserted] = positions.try_emplace(order.getIndex(), completed.size());

            if (inserted) {
                completed.push_back(&order);
            } else {
                completed[position->second] = &order;
            }
        }
    }

    if (replacesSource) {
        std::erase_if(entries, [sourceId](const auto &entry) {
            return events::orderIndexSourceId(entry.first) == sourceId;
        });
    }

    for (const auto *order : completed) {
        if (!eligibleForCorrection(*order)) {
            entries.erase(order->getIndex());
        }
    }

    for (const auto *order : completed) {
        if (!eligibleForCorrection(*order)) {
            continue;
        }

        if (entries.size() == capacity && !entries.contains(order->getIndex())) {
            return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
        }

        correct(entries, *order);
    }

    return {};
}

std::expected<void, IndexedEventModelError>
OrderBookModel::processEvents(std::u16string_view symbol, std::int32_t sourceId, std::span<const events::Order> input) {
    if (!source_.untracked()) {
        return std::unexpected(IndexedEventModelError::SOURCE_EPOCH_REQUIRED);
    }

    return applyEvents(symbol, sourceId, input);
}

std::expected<void, IndexedEventModelError>
OrderBookModel::applyEvents(std::u16string_view symbol, std::int32_t sourceId, std::span<const events::Order> input) {
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

        if (correctionOptions_.policy == OrderBookCorrectionPolicy::DISABLED) {
            if (const auto applied = orders_.processEvents(symbol, sourceId, input); !applied) {
                return std::unexpected(applied.error());
            }

            return {};
        }

        const auto current = corrections_.find(symbol);
        Corrections draft;
        const auto prepare = [&](const auto &changes) -> std::expected<void, IndexedEventModelError> {
            auto capacity = correctionOptions_.entries;

            for (auto entry = corrections_.begin(); entry != corrections_.end(); ++entry) {
                if (entry != current) {
                    capacity -= entry->second.size();
                }
            }

            auto &scratch = draft
                                .try_emplace(std::u16string(symbol),
                                             current == corrections_.end() ? CorrectionEntries{} : current->second)
                                .first->second;

            return stageCorrections(scratch, sourceId, changes, capacity);
        };

        if (const auto applied = orders_.processEventsChecked(symbol, sourceId, input, prepare); !applied) {
            return std::unexpected(applied.error());
        }

        if (!draft.empty()) {
            auto &replacement = draft.begin()->second;

            if (current != corrections_.end()) {
                if (replacement.empty()) {
                    corrections_.erase(current);
                } else {
                    current->second.swap(replacement);
                }
            } else if (!replacement.empty()) {
                corrections_.insert(draft.extract(draft.begin()));
            }
        }

        return {};
    } catch (const std::length_error &) {
        return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(IndexedEventModelError::ALLOCATION_FAILURE);
    }
}

std::expected<OrderBookSnapshot, IndexedEventModelError> OrderBookModel::getSnapshot(std::u16string_view symbol) const {
    return getFilteredSnapshot(symbol, {.filter = OrderBookModelFilter::UNFILTERED});
}

std::expected<OrderBookSnapshot, IndexedEventModelError>
OrderBookModel::getFilteredSnapshot(std::u16string_view symbol, const OrderBookViewOptions &options) const {
    if (!source_.readable()) {
        return std::unexpected(IndexedEventModelError::SOURCE_INACTIVE);
    }

    return buildSnapshot(symbol, options);
}

std::expected<OrderBookSnapshot, IndexedEventModelError>
OrderBookModel::buildSnapshot(std::u16string_view symbol, const OrderBookViewOptions &options) const {
    const auto mask = scopeMask(options.filter);

    if (!mask || options.lotSize <= 0) {
        return std::unexpected(IndexedEventModelError::INVALID_OPTIONS);
    }

    auto orders =
        options.sourceId ? orders_.getSnapshot(symbol, *options.sourceId) : orders_.getSnapshotForSymbol(symbol);

    if (!orders) {
        return std::unexpected(orders.error());
    }

    try {
        OrderBookSnapshot book;
        const auto corrected = corrections_.find(symbol);

        for (auto &order : *orders) {
            const auto scope = order.getScope();

            if ((*mask & (1U << std::to_underlying(scope))) == 0 ||
                (!order.hasSize() && scope != events::Scope::COMPOSITE)) {
                continue;
            }

            if (corrected != corrections_.end()) {
                const auto stamp = corrected->second.find(order.getIndex());

                if (stamp != corrected->second.end() && stamp->second.suppressed) {
                    continue;
                }
            }

            scaleLot(order, options.lotSize);
            auto &side = order.getOrderSide() == events::Side::BUY ? book.buyOrders : book.sellOrders;
            side.push_back(std::move(order));
        }

        if (options.filter != OrderBookModelFilter::UNFILTERED) {
            applyScopeFallback(book.buyOrders);
            applyScopeFallback(book.sellOrders);
        }

        sortSide(book.buyOrders, true);
        sortSide(book.sellOrders, false);

        if (options.depth) {
            if (book.buyOrders.size() > *options.depth) {
                book.buyOrders.resize(*options.depth);
            }

            if (book.sellOrders.size() > *options.depth) {
                book.sellOrders.resize(*options.depth);
            }
        }

        return book;
    } catch (const std::length_error &) {
        return std::unexpected(IndexedEventModelError::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(IndexedEventModelError::ALLOCATION_FAILURE);
    }
}

ModelSourceState OrderBookModel::getSourceState() const noexcept {
    return source_.state();
}

std::expected<ModelSourceEpoch, ModelSourceError> OrderBookModel::beginSourceEpoch(std::uint64_t ordinal) noexcept {
    auto epoch = source_.begin(ordinal);

    if (epoch) {
        retireData(ModelRetention::DISCARD);
    }

    return epoch;
}

std::expected<void, ModelSourceError> OrderBookModel::markSourceGap(const ModelSourceEpoch &epoch,
                                                                    ModelRetention retention) noexcept {
    const auto result = source_.stop(epoch, retention, false);

    if (result) {
        retireData(retention);
    }

    return result;
}

std::expected<void, ModelSourceError> OrderBookModel::confirmSourceEpoch(const ModelSourceEpoch &epoch) noexcept {
    return source_.confirm(epoch, orders_.hasUnfinished());
}

std::expected<void, ModelSourceError> OrderBookModel::closeSource(const ModelSourceEpoch &epoch,
                                                                  ModelRetention retention) noexcept {
    const auto result = source_.stop(epoch, retention, true);

    if (result) {
        retireData(retention);
    }

    return result;
}

void OrderBookModel::retireData(ModelRetention retention) noexcept {
    if (retention == ModelRetention::RETAIN_COMPLETED) {
        orders_.discardUnfinished();
    } else {
        orders_.clear();
        corrections_.clear();
    }
}

std::expected<void, IndexedEventModelError> OrderBookModel::processSourceEvents(const ModelSourceEpoch &epoch,
                                                                                std::u16string_view symbol,
                                                                                std::int32_t sourceId,
                                                                                std::span<const events::Order> input) {
    if (const auto error = source_.inputError(epoch)) {
        return std::unexpected(*error == ModelSourceError::STALE_EPOCH ? IndexedEventModelError::SOURCE_EPOCH_MISMATCH
                                                                       : IndexedEventModelError::SOURCE_INACTIVE);
    }

    return applyEvents(symbol, sourceId, input);
}

std::expected<ModelSourceSnapshot<OrderBookSnapshot>, IndexedEventModelError>
OrderBookModel::getSourceSnapshot(std::u16string_view symbol, const OrderBookViewOptions &options) const {
    auto snapshot = buildSnapshot(symbol, options);

    if (!snapshot) {
        return std::unexpected(snapshot.error());
    }

    return ModelSourceSnapshot<OrderBookSnapshot>{source_.state(), std::move(*snapshot)};
}

void OrderBookModel::clear() noexcept {
    source_.invalidateInput();
    retireData(ModelRetention::DISCARD);
}

} // namespace org::ttldtor::qdcxx::api
