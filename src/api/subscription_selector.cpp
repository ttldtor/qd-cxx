// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/subscription_selector.hpp"

#include "qd/events/time_helpers.hpp"

#include <bit>
#include <limits>
#include <utility>

namespace org::ttldtor::qdcxx::api {
SubscriptionSelector::SubscriptionSelector(SubscriptionSymbolKind kind, std::u16string symbol, std::int32_t sourceId,
                                           std::int64_t fromTime) noexcept
    : kind_(kind), symbol_(std::move(symbol)), sourceId_(sourceId), fromTime_(fromTime) {
}

SubscriptionSelector &SubscriptionSelector::operator=(const SubscriptionSelector &other) {
    if (this == &other) {
        return *this;
    }

    auto replacement = other;
    std::swap(kind_, replacement.kind_);
    symbol_.swap(replacement.symbol_);
    std::swap(sourceId_, replacement.sourceId_);
    std::swap(fromTime_, replacement.fromTime_);

    return *this;
}

SubscriptionSelector SubscriptionSelector::plain(std::u16string symbol) {
    return {SubscriptionSymbolKind::PLAIN, std::move(symbol), 0, 0};
}

SubscriptionSelector SubscriptionSelector::indexed(std::u16string symbol, const events::IndexedEventSource &source) {
    return {SubscriptionSymbolKind::INDEXED, std::move(symbol), source.id(), 0};
}

SubscriptionSelector SubscriptionSelector::timeSeries(std::u16string symbol, std::int64_t fromTime) {
    return {SubscriptionSymbolKind::TIME_SERIES, std::move(symbol), 0, fromTime};
}

SubscriptionSelector SubscriptionSelector::all() noexcept {
    return {SubscriptionSymbolKind::ALL, {}, 0, 0};
}

SubscriptionSymbolKind SubscriptionSelector::kind() const noexcept {
    return kind_;
}

std::u16string_view SubscriptionSelector::eventSymbol() const noexcept {
    return symbol_;
}

std::optional<std::int32_t> SubscriptionSelector::sourceId() const noexcept {
    if (kind_ == SubscriptionSymbolKind::INDEXED || kind_ == SubscriptionSymbolKind::TIME_SERIES) {
        return sourceId_;
    }

    return std::nullopt;
}

std::optional<std::int64_t> SubscriptionSelector::fromTime() const noexcept {
    if (kind_ == SubscriptionSymbolKind::TIME_SERIES) {
        return fromTime_;
    }

    return std::nullopt;
}

std::optional<std::int64_t> SubscriptionSelector::qdTime() const noexcept {
    if (kind_ != SubscriptionSymbolKind::TIME_SERIES) {
        return std::nullopt;
    }

    if (fromTime_ == std::numeric_limits<std::int64_t>::max()) {
        return fromTime_;
    }

    return events::withTimeSequenceTime(0, fromTime_);
}

bool SubscriptionSelector::operator==(const SubscriptionSelector &other) const noexcept {
    return kind_ == other.kind_ && symbol_ == other.symbol_ && sourceId_ == other.sourceId_;
}

bool SubscriptionSelector::sameDemand(const SubscriptionSelector &other) const noexcept {
    return *this == other && fromTime_ == other.fromTime_;
}

std::int32_t SubscriptionSelector::hashCode() const noexcept {
    constexpr std::uint32_t JAVA_HASH_MULTIPLIER = 31;
    constexpr std::int32_t WILDCARD_HASH = 42; // WildcardSymbol's exact star text.

    if (kind_ == SubscriptionSymbolKind::ALL) {
        return WILDCARD_HASH;
    }

    std::uint32_t result = 0;

    for (const auto unit : symbol_) {
        result = result * JAVA_HASH_MULTIPLIER + static_cast<std::uint32_t>(unit);
    }

    if (kind_ == SubscriptionSymbolKind::INDEXED) {
        result += std::bit_cast<std::uint32_t>(sourceId_) * JAVA_HASH_MULTIPLIER;
    }

    return std::bit_cast<std::int32_t>(result);
}

std::expected<std::optional<core::SubscriptionRequest>, SelectorRequestError>
indexedSelectorRequest(const SubscriptionSelector &selector, const events::PreparedOrderSource &binding) noexcept {
    if (selector.kind() != SubscriptionSymbolKind::PLAIN && selector.kind() != SubscriptionSymbolKind::INDEXED) {
        return std::unexpected(SelectorRequestError::UNSUPPORTED_SELECTOR);
    }

    if (!binding.context().valid()) {
        return std::unexpected(SelectorRequestError::INVALID_CONTEXT);
    }

    if (selector.sourceId() && *selector.sourceId() >= 0 && *selector.sourceId() != binding.source().id()) {
        return std::optional<core::SubscriptionRequest>{};
    }

    return std::optional<core::SubscriptionRequest>{{{binding.context(), selector.eventSymbol()}, {}}};
}

} // namespace org::ttldtor::qdcxx::api
