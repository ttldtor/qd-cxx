// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/time/time_zone_data.hpp"
#include "timezone/timezone_provider.hpp"
#include <array>
#include <limits>
#include <string>

namespace org::ttldtor::qdcxx::time::detail {
inline constexpr std::size_t NO_METADATA = std::numeric_limits<std::size_t>::max();

/** Internal typed failure, translated before a public model/scalar escapes. */
struct Failure {
    TimeZoneError error;
};

/** Resolved finite native/legacy identity; no date-library type crosses this private boundary. */
struct ZoneDescriptor {
    provider::Zone native;
    std::int64_t fixedSeconds = 0;
    std::size_t legacyIndex = NO_METADATA;
    bool fixed = false;
    std::int64_t cutoffOffsetSeconds = 0;
    std::int64_t finalRawSeconds = 0;
};

/** Immutable process-wide fixed provider profile, independent of calendars and event values. */
struct TimeZoneProfile {
    std::array<ZoneDescriptor, 631> identities;
    provider::Stats nativeStats{};
};

/** One bounded caller-owned immutable binding model; no global binding registry is retained. */
struct ZoneBindingModel {
    std::shared_ptr<const TimeZoneProfile> profile;
    ZoneDescriptor zone;
    std::u16string id;
    std::int64_t reference;
    std::int32_t referenceRawSeconds;
};

/** Source-internal bridge; installed consumers use only the public header. */
struct Access {
    /** @param data Ready data capability.
     * @return Immutable private profile pin.
     */
    static const std::shared_ptr<const TimeZoneProfile> &profile(const TimeZoneData &data) noexcept;
    /** @param binding Owning immutable binding.
     * @return Its private model pin.
     */
    static const std::shared_ptr<const ZoneBindingModel> &model(const ZoneBinding &binding) noexcept;
};

/** @param idUnits Validated identity units.
 * @return Checked distinct owned logical model bytes, before allocation; excludes shared backing/allocator overhead.
 */
std::size_t bindingLogicalBytes(std::size_t idUnits);
/** @param binding Owning immutable identity/reference binding.
 * @param localMillis Physical local-day/clock fields on the epoch axis.
 * @return Calendar-compatible gap-forward/later-overlap UTC milliseconds or typed refusal; no clock/file work.
 */
std::expected<std::int64_t, TimeZoneError> localToUtc(const ZoneBinding &binding, std::int64_t localMillis) noexcept;
} // namespace org::ttldtor::qdcxx::time::detail
