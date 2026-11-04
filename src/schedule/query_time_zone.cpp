// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "schedule_private.hpp"
#include "time_zone_private.hpp"
#include <chrono>

namespace org::ttldtor::qdcxx::schedule::detail {
ScheduleError timeZoneError(time::TimeZoneError error) noexcept {
    switch (error) {
    case time::TimeZoneError::INVALID_ID:
        return ScheduleError::UNKNOWN_TIME_ZONE;
    case time::TimeZoneError::INVALID_ARGUMENT:
        return ScheduleError::TIME_ZONE_DATA;
    case time::TimeZoneError::NOT_READY:
        return ScheduleError::NOT_READY;
    case time::TimeZoneError::OUT_OF_RANGE:
        return ScheduleError::OUT_OF_RANGE;
    case time::TimeZoneError::RESOURCE_LIMIT:
        return ScheduleError::RESOURCE_LIMIT;
    case time::TimeZoneError::ALLOCATION:
        return ScheduleError::ALLOCATION;
    case time::TimeZoneError::DATA:
        return ScheduleError::TIME_ZONE_DATA;
    }

    return ScheduleError::TIME_ZONE_DATA;
}

std::int64_t rawReference(const Context &context) noexcept {
    if (context.queryOptions.rawOffsetReferenceTime) {
        return *context.queryOptions.rawOffsetReferenceTime;
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void stampTimeZone(Rules &rules, const Context &context, std::int64_t reference, Construction &construction) {
    construction.inspect(rules.zone.size());
    std::size_t bytes;
    try {
        bytes = time::detail::bindingLogicalBytes(rules.zone.size());
    } catch (const time::detail::Failure &failure) {
        throw Failure{timeZoneError(failure.error)};
    }
    construction.allocate(bytes);

    if (bytes > std::numeric_limits<std::size_t>::max() - rules.modelBytes) {
        throw Failure{ScheduleError::RESOURCE_LIMIT};
    }

    auto binding = context.timeZones->bind(rules.zone, {reference, context.queryOptions.maxIdUnits});

    if (!binding) {
        throw Failure{timeZoneError(binding.error())};
    }

    rules.rawOffsetSeconds = binding->getRawOffsetSeconds();
    rules.rawOffsetReferenceTime = reference;
    rules.timeZoneBinding = std::move(*binding);
    rules.modelBytes += bytes;
}
} // namespace org::ttldtor::qdcxx::schedule::detail
