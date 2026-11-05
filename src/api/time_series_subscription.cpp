// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/time_series_subscription.hpp"
#include "qd/api/event_registry.hpp"

#include <algorithm>
#include <mutex>
#include <new>
#include <stdexcept>

namespace org::ttldtor::qdcxx::api {
namespace {
/** Validate only explicit business-marker metadata, never a record layout or spelling.
 * @param capability Caller-supplied marker value.
 * @return Whether it is one of the supported declaration categories.
 */
bool validCapability(EventSubscriptionCapability capability) noexcept {
    switch (capability) {
    case EventSubscriptionCapability::NON_INDEXED:
    case EventSubscriptionCapability::INDEXED:
    case EventSubscriptionCapability::TIME_SERIES:
        return true;
    default:
        return false;
    }
}

/** Compare effective sets without treating input order as a new demand.
 * @param first Existing accepted owning set.
 * @param second Prepared candidate set, already unique by typed identity.
 * @return Whether both identity sets and retained demand attributes are equal.
 */
bool sameSet(const std::vector<SubscriptionSelector> &first, const std::vector<SubscriptionSelector> &second) noexcept {
    if (first.size() != second.size())
        return false;

    for (const auto &value : first) {
        const auto found = std::ranges::find(second, value);

        if (found == second.end() || !value.sameDemand(*found))
            return false;
    }

    return true;
}
} // namespace

TimeSeriesEventDescriptor::TimeSeriesEventDescriptor(std::u16string identity,
                                                     EventSubscriptionCapability capability) noexcept
    : identity_(std::move(identity)), capability_(capability) {
}

std::expected<TimeSeriesEventDescriptor, TimeSeriesModelError>
TimeSeriesEventDescriptor::declared(std::u16string stableIdentity, EventSubscriptionCapability capability) {
    if (stableIdentity.empty() || !validCapability(capability))
        return std::unexpected(TimeSeriesModelError::INVALID_CAPABILITY);

    return TimeSeriesEventDescriptor(std::move(stableIdentity), capability);
}

std::u16string_view TimeSeriesEventDescriptor::stableIdentity() const noexcept {
    return identity_;
}

EventSubscriptionCapability TimeSeriesEventDescriptor::capability() const noexcept {
    return capability_;
}

bool TimeSeriesEventDescriptor::operator==(const TimeSeriesEventDescriptor &other) const noexcept {
    return identity_ == other.identity_;
}

bool isTimeSeriesEventType(EventType type) noexcept {
    switch (type) {
    case EventType::QUOTE:
    case EventType::PROFILE:
    case EventType::TRADE:
    case EventType::TRADE_ETH:
    case EventType::SUMMARY:
    case EventType::TEXT_MESSAGE:
    case EventType::TEXT_CONFIGURATION:
    case EventType::MESSAGE:
    case EventType::CONFIGURATION:
        return false;
    default:
        return false;
    }
}

bool hasTimeSeriesFeedReceiver(const TimeSeriesEventDescriptor &descriptor) noexcept {
    static_cast<void>(descriptor);

    return false;
}

/** Pure instance-local state; all mutation and snapshot copying shares this mutex. */
struct TimeSeriesSubscriptionModel::Impl {
    mutable std::mutex mutex;                          ///< Never enters callbacks, transport, schemas or another model.
    TimeSeriesModelLimits limits;                      ///< Immutable finite count/unit limits.
    std::vector<TimeSeriesEventDescriptor> eventTypes; ///< Normalized owning business declarations.
    std::vector<SubscriptionSelector> symbols;         ///< Current raw typed identities and retained attributes.
    std::int64_t fromTime = TIME_SERIES_FROM_TIME_DISABLED; ///< One inclusive lower-bound property for the set.
    std::uint64_t revision = 0; ///< Checked model-local state version, unrelated to transport/source epochs.

    /** Prepare a whole replacement or additive batch before publishing any accepted field.
     * @param input Borrowed raw values, valid through this synchronous call.
     * @param additive Keep existing identities before applying the supplied batch.
     * @return One committed effective change or typed refusal preserving every old field.
     */
    std::expected<TimeSeriesModelChange, TimeSeriesModelError> mutate(std::span<const SubscriptionSelector> input,
                                                                      bool additive) {
        std::lock_guard lock(mutex);

        if (input.size() > limits.inputSymbols)
            return std::unexpected(TimeSeriesModelError::RESOURCE_LIMIT);

        try {
            std::vector<SubscriptionSelector> candidate;

            if (additive)
                candidate = symbols;

            for (const auto &value : input) {
                if (value.eventSymbol().size() > limits.symbolUnits)
                    return std::unexpected(TimeSeriesModelError::RESOURCE_LIMIT);

                const auto found = std::ranges::find(candidate, value);

                if (found == candidate.end()) {
                    if (candidate.size() == limits.symbols)
                        return std::unexpected(TimeSeriesModelError::RESOURCE_LIMIT);

                    candidate.push_back(value);
                } else {
                    *found = value;
                }
            }

            std::size_t units = 0;

            for (const auto &value : candidate) {
                if (value.eventSymbol().size() > limits.totalSymbolUnits - units)
                    return std::unexpected(TimeSeriesModelError::RESOURCE_LIMIT);

                units += value.eventSymbol().size();
            }

            if (sameSet(symbols, candidate))
                return TimeSeriesModelChange{revision, false, false};

            if (revision == std::numeric_limits<std::uint64_t>::max())
                return std::unexpected(TimeSeriesModelError::REVISION_EXHAUSTED);

            symbols.swap(candidate);
            ++revision;

            return TimeSeriesModelChange{revision, true, false};
        } catch (const std::bad_alloc &) {
            return std::unexpected(TimeSeriesModelError::ALLOCATION_FAILURE);
        } catch (const std::length_error &) {
            return std::unexpected(TimeSeriesModelError::RESOURCE_LIMIT);
        }
    }
};

TimeSeriesSubscriptionModel::TimeSeriesSubscriptionModel(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

TimeSeriesSubscriptionModel::TimeSeriesSubscriptionModel(TimeSeriesSubscriptionModel &&other) noexcept = default;

TimeSeriesSubscriptionModel &TimeSeriesSubscriptionModel::operator=(TimeSeriesSubscriptionModel &&other) noexcept {
    if (this != &other)
        impl_ = std::move(other.impl_);

    return *this;
}

TimeSeriesSubscriptionModel::~TimeSeriesSubscriptionModel() = default;

std::expected<TimeSeriesSubscriptionModel, TimeSeriesModelError>
TimeSeriesSubscriptionModel::create(std::span<const TimeSeriesEventDescriptor> eventTypes,
                                    TimeSeriesModelLimits limits) {
    if (!limits.eventTypes || !limits.typeIdentityUnits || !limits.symbols || !limits.inputSymbols ||
        !limits.symbolUnits || !limits.totalSymbolUnits ||
        limits.symbols > std::numeric_limits<std::size_t>::max() / sizeof(SubscriptionSelector) ||
        limits.eventTypes > std::numeric_limits<std::size_t>::max() / sizeof(TimeSeriesEventDescriptor)) {
        return std::unexpected(TimeSeriesModelError::INVALID_LIMITS);
    }

    if (eventTypes.empty())
        return std::unexpected(TimeSeriesModelError::UNSUPPORTED_EVENT_TYPE);

    if (eventTypes.size() > limits.eventTypes)
        return std::unexpected(TimeSeriesModelError::RESOURCE_LIMIT);

    for (std::size_t index = 0; index < eventTypes.size(); ++index) {
        const auto &type = eventTypes[index];

        if (type.stableIdentity().empty() || !validCapability(type.capability()))
            return std::unexpected(TimeSeriesModelError::INVALID_CAPABILITY);

        if (type.stableIdentity().size() > limits.typeIdentityUnits)
            return std::unexpected(TimeSeriesModelError::RESOURCE_LIMIT);

        for (std::size_t previous = 0; previous < index; ++previous) {
            if (eventTypes[previous] == type && eventTypes[previous].capability() != type.capability())
                return std::unexpected(TimeSeriesModelError::CONFLICTING_EVENT_TYPE);
        }
    }

    for (const auto &type : eventTypes) {
        if (type.capability() != EventSubscriptionCapability::TIME_SERIES)
            return std::unexpected(TimeSeriesModelError::UNSUPPORTED_EVENT_TYPE);
    }

    try {
        auto state = std::make_unique<Impl>();
        state->limits = limits;
        state->eventTypes.reserve(eventTypes.size());

        for (const auto &type : eventTypes) {
            if (std::ranges::find(state->eventTypes, type) == state->eventTypes.end())
                state->eventTypes.push_back(type);
        }

        return TimeSeriesSubscriptionModel(std::move(state));
    } catch (const std::bad_alloc &) {
        return std::unexpected(TimeSeriesModelError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(TimeSeriesModelError::RESOURCE_LIMIT);
    }
}

bool TimeSeriesSubscriptionModel::valid() const noexcept {
    return static_cast<bool>(impl_);
}

std::expected<std::int64_t, TimeSeriesModelError> TimeSeriesSubscriptionModel::getFromTime() const {
    if (!impl_)
        return std::unexpected(TimeSeriesModelError::INVALID_STATE);

    std::lock_guard lock(impl_->mutex);

    return impl_->fromTime;
}

std::expected<TimeSeriesModelChange, TimeSeriesModelError>
TimeSeriesSubscriptionModel::setFromTime(std::int64_t fromTime) {
    if (!impl_)
        return std::unexpected(TimeSeriesModelError::INVALID_STATE);

    std::lock_guard lock(impl_->mutex);

    if (impl_->fromTime == fromTime)
        return TimeSeriesModelChange{impl_->revision, false, false};

    if (impl_->revision == std::numeric_limits<std::uint64_t>::max())
        return std::unexpected(TimeSeriesModelError::REVISION_EXHAUSTED);

    impl_->fromTime = fromTime;
    ++impl_->revision;

    return TimeSeriesModelChange{impl_->revision, false, true};
}

std::expected<TimeSeriesModelChange, TimeSeriesModelError>
TimeSeriesSubscriptionModel::setSymbols(std::span<const SubscriptionSelector> symbols) {
    if (!impl_)
        return std::unexpected(TimeSeriesModelError::INVALID_STATE);

    return impl_->mutate(symbols, false);
}

std::expected<TimeSeriesModelChange, TimeSeriesModelError>
TimeSeriesSubscriptionModel::addSymbols(std::span<const SubscriptionSelector> symbols) {
    if (!impl_)
        return std::unexpected(TimeSeriesModelError::INVALID_STATE);

    return impl_->mutate(symbols, true);
}

std::expected<TimeSeriesModelChange, TimeSeriesModelError>
TimeSeriesSubscriptionModel::removeSymbol(const SubscriptionSelector &symbol) {
    if (!impl_)
        return std::unexpected(TimeSeriesModelError::INVALID_STATE);

    std::lock_guard lock(impl_->mutex);
    const auto found = std::ranges::find(impl_->symbols, symbol);

    if (found == impl_->symbols.end())
        return TimeSeriesModelChange{impl_->revision, false, false};

    if (impl_->revision == std::numeric_limits<std::uint64_t>::max())
        return std::unexpected(TimeSeriesModelError::REVISION_EXHAUSTED);

    impl_->symbols.erase(found);
    ++impl_->revision;

    return TimeSeriesModelChange{impl_->revision, true, false};
}

std::expected<TimeSeriesModelChange, TimeSeriesModelError> TimeSeriesSubscriptionModel::clearSymbols() {
    return setSymbols({});
}

std::expected<TimeSeriesSubscriptionSnapshot, TimeSeriesModelError> TimeSeriesSubscriptionModel::snapshot() const {
    if (!impl_)
        return std::unexpected(TimeSeriesModelError::INVALID_STATE);

    std::lock_guard lock(impl_->mutex);
    try {
        TimeSeriesSubscriptionSnapshot result;
        result.revision = impl_->revision;
        result.fromTime = impl_->fromTime;
        result.eventTypes = impl_->eventTypes;
        result.symbols = impl_->symbols;
        result.decoratedSymbols.reserve(result.symbols.size());

        for (const auto &symbol : result.symbols)
            result.decoratedSymbols.emplace_back(symbol, result.fromTime);

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(TimeSeriesModelError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(TimeSeriesModelError::RESOURCE_LIMIT);
    }
}

} // namespace org::ttldtor::qdcxx::api
