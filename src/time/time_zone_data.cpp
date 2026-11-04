// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "time_zone_private.hpp"
#include "timezone_identity_data.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace org::ttldtor::qdcxx::time {
namespace detail {
namespace {
constexpr std::int64_t MILLIS_PER_SECOND = 1000;
constexpr std::int64_t HOUR_SECONDS = 3600;
constexpr std::int64_t LEGACY_TABLE_BOUNDARY = -2208988800000LL;

/** @param result Native operation result.
 * @param query Whether INVALID_ARGUMENT denotes the private query envelope rather than setup arguments.
 */
void requireProvider(provider::Error result, bool query = false) {
    if (result == provider::Error::NONE) {
        return;
    }

    if (result == provider::Error::ALLOCATION) {
        throw Failure{TimeZoneError::ALLOCATION};
    }

    if (result == provider::Error::NOT_READY) {
        throw Failure{TimeZoneError::NOT_READY};
    }

    if (result == provider::Error::INVALID_ARGUMENT) {
        throw Failure{query ? TimeZoneError::OUT_OF_RANGE : TimeZoneError::INVALID_ARGUMENT};
    }

    if (result == provider::Error::LIMIT) {
        throw Failure{TimeZoneError::RESOURCE_LIMIT};
    }

    throw Failure{TimeZoneError::DATA};
}

/** @param text Exact accepted identity.
 * @return Its immutable metadata index, or NO_METADATA; comparisons are bounded by fixed profile names.
 */
std::size_t metadataIndex(std::u16string_view text) noexcept {
    std::size_t first = 0;
    auto count = zone_data::IDENTITIES.size();

    while (count != 0) {
        const auto step = count / 2;

        if (zone_data::IDENTITIES[first + step].id < text) {
            first += step + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }

    return first < zone_data::IDENTITIES.size() && zone_data::IDENTITIES[first].id == text ? first : NO_METADATA;
}

/** @param id Exact requested spelling.
 * @return Numeric seconds for normalized Java custom GMT identities, or no match.
 */
std::optional<std::int64_t> customOffset(std::u16string_view id) noexcept {
    if (id.size() != 9 || id.substr(0, 3) != u"GMT" || (id[3] != u'+' && id[3] != u'-') || id[6] != u':') {
        return {};
    }

    for (const auto position : {4, 5, 7, 8}) {
        if (id[position] < u'0' || id[position] > u'9') {
            return {};
        }
    }

    const auto hours = (id[4] - u'0') * 10 + id[5] - u'0';
    const auto minutes = (id[7] - u'0') * 10 + id[8] - u'0';

    if (hours > 23 || minutes > 59) {
        return {};
    }

    return (hours * HOUR_SECONDS + minutes * 60) * (id[3] == u'-' ? -1 : 1);
}

/** @param metadata Actual immutable Java identity facts.
 * @return Whether its fixed semantics require no provider; ordinary named zones with past changes are not inferred
 * fixed.
 */
bool fixedIdentity(const zone_data::Identity &metadata) noexcept {
    const auto id = metadata.id;
    const std::string_view mapped(metadata.mapped);

    return (metadata.systemV && !metadata.legacyDst) || mapped.starts_with("+") || mapped.starts_with("-") ||
           id.starts_with(u"Etc/") || id == u"UTC" || id == u"GMT" || id == u"UCT" || id == u"Universal" ||
           id == u"Zulu" || id == u"Greenwich" || id == u"GMT0" || id == u"GMT+0" || id == u"GMT-0";
}

/** @param descriptor Ready immutable native/legacy identity.
 * @param reference Constructor/reference milliseconds, independent from query dates.
 * @return Raw metadata following the selected data's standard history and explicit rearguard conventions.
 */
std::int64_t rawAtReference(const ZoneDescriptor &descriptor, std::int64_t reference) {
    if (descriptor.fixed || descriptor.legacyIndex != NO_METADATA) {
        return descriptor.fixedSeconds;
    }

    const auto maximum = provider::maximumUtcMillis();
    const auto effective = reference < LEGACY_TABLE_BOUNDARY || reference > maximum ? maximum : reference;
    std::int64_t offset = 0, save = 0;
    requireProvider(provider::offsets(descriptor.native, effective, offset, save), true);
    auto raw = offset - save;
    const std::string_view canonical(provider::canonicalName(descriptor.native));

    for (const auto &convention : zone_data::RAW_CONVENTIONS) {
        if (canonical == convention.canonical && effective >= convention.begin && effective < convention.end) {
            raw += convention.adjustment;
        }
    }

    return raw;
}

/** @param descriptor Ready legacy identity.
 * @param utc Exact queried UTC milliseconds.
 * @return Actual legacy transitions/recurrence offset, preserving source SystemV rules.
 */
std::int64_t legacyOffset(const ZoneDescriptor &descriptor, std::int64_t utc) {
    if (utc < LEGACY_TABLE_BOUNDARY) {
        return descriptor.finalRawSeconds;
    }

    if (utc > provider::maximumUtcMillis()) {
        throw Failure{TimeZoneError::OUT_OF_RANGE};
    }

    const auto &metadata = zone_data::IDENTITIES[descriptor.legacyIndex];
    const auto first = zone_data::TRANSITIONS.begin() + static_cast<std::ptrdiff_t>(metadata.first);
    const auto last = first + static_cast<std::ptrdiff_t>(metadata.count);
    const auto seconds = utc / 1000 - (utc % 1000 < 0 ? 1 : 0);

    if (seconds <= (last - 1)->seconds) {
        auto low = first;
        auto count = metadata.count;

        while (count != 0) {
            const auto step = count / 2;

            if ((low + static_cast<std::ptrdiff_t>(step))->seconds <= seconds) {
                low += static_cast<std::ptrdiff_t>(step + 1);
                count -= step + 1;
            } else {
                count = step;
            }
        }

        return low == first ? descriptor.fixedSeconds : (low - 1)->after;
    }

    std::int64_t spring = 0, autumn = 0;

    if (!provider::legacyBoundaries(utc, descriptor.fixedSeconds, spring, autumn)) {
        throw Failure{TimeZoneError::OUT_OF_RANGE};
    }

    return descriptor.fixedSeconds + (utc >= spring && utc < autumn ? HOUR_SECONDS : 0);
}

/** @param descriptor Ready immutable identity.
 * @param utc Exact scalar query.
 * @return Actual offset; pre1900 uses FINAL raw base, not constructor/reference raw metadata.
 */
std::int64_t actualOffset(const ZoneDescriptor &descriptor, std::int64_t utc) {
    if (descriptor.fixed) {
        return descriptor.fixedSeconds;
    }

    if (utc < LEGACY_TABLE_BOUNDARY) {
        return descriptor.finalRawSeconds;
    }

    if (descriptor.legacyIndex != NO_METADATA) {
        return legacyOffset(descriptor, utc);
    }

    std::int64_t offset = 0, save = 0;
    requireProvider(provider::offsets(descriptor.native, utc, offset, save), true);

    return offset;
}

/** @param local Local scalar fields.
 * @param seconds Offset to subtract.
 * @return Checked candidate instant without signed overflow.
 */
std::int64_t candidateUtc(std::int64_t local, std::int64_t seconds) {
    const auto delta = seconds * MILLIS_PER_SECOND;

    if ((delta > 0 && local < std::numeric_limits<std::int64_t>::min() + delta) ||
        (delta < 0 && local > std::numeric_limits<std::int64_t>::max() + delta)) {
        throw Failure{TimeZoneError::OUT_OF_RANGE};
    }

    return local - delta;
}

/** @param descriptor Ready immutable identity.
 * @param local Requested local physical-day/clock scalar.
 * @return Gap-forward/later-overlap UTC instant, including the synthetic UTC1900 transition-table edge.
 */
std::int64_t convertLocal(const ZoneDescriptor &descriptor, std::int64_t local) {
    if (descriptor.fixed) {
        return candidateUtc(local, descriptor.fixedSeconds);
    }

    const auto raw = descriptor.finalRawSeconds;
    const auto rawCandidate = candidateUtc(local, raw);
    const auto cutoffLocal = LEGACY_TABLE_BOUNDARY + std::min(raw, descriptor.cutoffOffsetSeconds) * MILLIS_PER_SECOND;

    if (local < cutoffLocal) {
        return rawCandidate;
    }

    if (descriptor.legacyIndex != NO_METADATA) {
        const auto daylightCandidate = candidateUtc(local, descriptor.fixedSeconds + HOUR_SECONDS);
        const auto standardValid = legacyOffset(descriptor, rawCandidate) == descriptor.fixedSeconds;
        const auto daylightValid =
            legacyOffset(descriptor, daylightCandidate) == descriptor.fixedSeconds + HOUR_SECONDS;

        return standardValid || !daylightValid ? rawCandidate : daylightCandidate;
    }

    std::int64_t first = 0, second = 0;
    int kind = 0;
    requireProvider(provider::localOffsets(descriptor.native, local, first, second, kind), true);
    std::optional<std::int64_t> best;

    for (const auto seconds : {raw, first, second}) {
        const auto candidate = candidateUtc(local, seconds);

        if (actualOffset(descriptor, candidate) == seconds && (!best || candidate > *best)) {
            best = candidate;
        }
    }

    if (best) {
        return *best;
    }

    if (rawCandidate >= LEGACY_TABLE_BOUNDARY &&
        candidateUtc(local, descriptor.cutoffOffsetSeconds) < LEGACY_TABLE_BOUNDARY) {
        return rawCandidate;
    }

    if (kind == 1) {
        return candidateUtc(local, first);
    }

    throw Failure{TimeZoneError::DATA};
}

/** @param value Provider-computed scalar seconds.
 * @return Checked public offset representation.
 */
std::int32_t offsetSeconds(std::int64_t value) {
    if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max()) {
        throw Failure{TimeZoneError::DATA};
    }

    return static_cast<std::int32_t>(value);
}
} // namespace

const std::shared_ptr<const TimeZoneProfile> &Access::profile(const TimeZoneData &data) noexcept {
    return data.profile_;
}

const std::shared_ptr<const ZoneBindingModel> &Access::model(const ZoneBinding &binding) noexcept {
    return binding.model_;
}

std::size_t bindingLogicalBytes(std::size_t units) {
    if (units > (std::numeric_limits<std::size_t>::max() - sizeof(ZoneBindingModel)) / sizeof(char16_t)) {
        throw Failure{TimeZoneError::RESOURCE_LIMIT};
    }

    return sizeof(ZoneBindingModel) + units * sizeof(char16_t);
}

std::expected<std::int64_t, TimeZoneError> localToUtc(const ZoneBinding &binding, std::int64_t local) noexcept {
    try {
        return convertLocal(Access::model(binding)->zone, local);
    } catch (const Failure &failure) {
        return std::unexpected(failure.error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TimeZoneError::ALLOCATION);
    } catch (...) {
        return std::unexpected(TimeZoneError::DATA);
    }
}
} // namespace detail

TimeZoneData::TimeZoneData(std::shared_ptr<const detail::TimeZoneProfile> profile) noexcept
    : profile_(std::move(profile)) {
}

TimeZoneData::TimeZoneData(TimeZoneData &&other) noexcept : profile_(other.profile_) {
}

TimeZoneData &TimeZoneData::operator=(TimeZoneData &&other) noexcept {
    profile_ = other.profile_;

    return *this;
}

std::string_view TimeZoneData::getVersion() const noexcept {
    return "2026f";
}

std::expected<TimeZoneData, TimeZoneError> TimeZoneData::initialize(const std::filesystem::path &root,
                                                                    TimeZoneLimits limits) {
#if !ORG_TTLDTOR_QDCXX_TIME_ZONE_BACKEND
    (void)root;
    (void)limits;

    return std::unexpected(TimeZoneError::NOT_READY);
#else
    try {
        auto profile = std::make_shared<detail::TimeZoneProfile>();
        detail::provider::Limits nativeLimits;
        nativeLimits.files = limits.files;
        nativeLimits.totalBytes = limits.totalBytes;
        nativeLimits.lineBytes = limits.lineBytes;
        nativeLimits.lines = limits.lines;
        nativeLimits.records = limits.records;
        nativeLimits.zones = limits.zones;
        nativeLimits.links = limits.links;
        nativeLimits.rules = limits.rules;
        nativeLimits.modelUnits = limits.modelUnits;
        detail::requireProvider(detail::provider::initialize(root, nativeLimits, profile->nativeStats));

        for (std::size_t index = 0; index < detail::zone_data::IDENTITIES.size(); ++index) {
            const auto &metadata = detail::zone_data::IDENTITIES[index];
            auto &descriptor = profile->identities[index];

            if (detail::fixedIdentity(metadata)) {
                descriptor.fixed = true;
                descriptor.fixedSeconds = metadata.referenceRawMillis / 1000;
                descriptor.finalRawSeconds = descriptor.fixedSeconds;
                descriptor.cutoffOffsetSeconds = descriptor.fixedSeconds;
            } else if (metadata.systemV) {
                descriptor.legacyIndex = index;
                descriptor.fixedSeconds = metadata.referenceRawMillis / 1000;
                descriptor.finalRawSeconds = descriptor.fixedSeconds;
                descriptor.cutoffOffsetSeconds = descriptor.fixedSeconds;
            } else {
                detail::requireProvider(detail::provider::resolve(metadata.mapped, descriptor.native));
                std::int64_t save = 0;
                detail::requireProvider(detail::provider::offsets(descriptor.native, detail::LEGACY_TABLE_BOUNDARY,
                                                                  descriptor.cutoffOffsetSeconds, save));
                descriptor.finalRawSeconds = detail::rawAtReference(descriptor, detail::provider::maximumUtcMillis());
            }
        }

        return TimeZoneData(std::move(profile));
    } catch (const detail::Failure &failure) {
        return std::unexpected(failure.error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TimeZoneError::ALLOCATION);
    } catch (const std::filesystem::filesystem_error &) {
        return std::unexpected(TimeZoneError::DATA);
    } catch (const std::length_error &) {
        return std::unexpected(TimeZoneError::RESOURCE_LIMIT);
    }
#endif
}

std::expected<ZoneBinding, TimeZoneError> TimeZoneData::bind(std::u16string_view id,
                                                             TimeZoneBindingOptions options) const {
    return ZoneBinding::create(id, this, options);
}

ZoneBinding::ZoneBinding(std::shared_ptr<const detail::ZoneBindingModel> model) noexcept : model_(std::move(model)) {
}

ZoneBinding::ZoneBinding(ZoneBinding &&other) noexcept : model_(other.model_) {
}

ZoneBinding &ZoneBinding::operator=(ZoneBinding &&other) noexcept {
    model_ = other.model_;

    return *this;
}

std::expected<ZoneBinding, TimeZoneError> ZoneBinding::create(std::u16string_view id, const TimeZoneData *data,
                                                              TimeZoneBindingOptions options) {
    try {
        if (id.size() > options.maxIdUnits) {
            return std::unexpected(TimeZoneError::RESOURCE_LIMIT);
        }

        detail::ZoneDescriptor descriptor;

        if (const auto fixed = detail::customOffset(id)) {
            descriptor.fixed = true;
            descriptor.fixedSeconds = *fixed;
            descriptor.finalRawSeconds = *fixed;
            descriptor.cutoffOffsetSeconds = *fixed;
        } else {
            const auto index = detail::metadataIndex(id);

            if (index == detail::NO_METADATA) {
                return std::unexpected(TimeZoneError::INVALID_ID);
            }

            const auto &metadata = detail::zone_data::IDENTITIES[index];

            if (detail::fixedIdentity(metadata)) {
                descriptor.fixed = true;
                descriptor.fixedSeconds = metadata.referenceRawMillis / 1000;
                descriptor.finalRawSeconds = descriptor.fixedSeconds;
                descriptor.cutoffOffsetSeconds = descriptor.fixedSeconds;
            } else {
                if (data == nullptr) {
                    return std::unexpected(TimeZoneError::NOT_READY);
                }

                descriptor = data->profile_->identities[index];
            }
        }

        (void)detail::bindingLogicalBytes(id.size());
        auto model = std::make_shared<detail::ZoneBindingModel>();

        if (data != nullptr) {
            model->profile = data->profile_;
        }

        model->zone = descriptor;
        model->id = id;
        model->reference = options.rawOffsetReferenceTime ? *options.rawOffsetReferenceTime
                                                          : std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                std::chrono::system_clock::now().time_since_epoch())
                                                                .count();
        model->referenceRawSeconds = detail::offsetSeconds(detail::rawAtReference(descriptor, model->reference));

        return ZoneBinding(std::move(model));
    } catch (const detail::Failure &failure) {
        return std::unexpected(failure.error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TimeZoneError::ALLOCATION);
    } catch (const std::length_error &) {
        return std::unexpected(TimeZoneError::RESOURCE_LIMIT);
    }
}

std::u16string_view ZoneBinding::getId() const noexcept {
    return model_->id;
}

std::int64_t ZoneBinding::getRawOffsetReferenceTime() const noexcept {
    return model_->reference;
}

std::int32_t ZoneBinding::getRawOffsetSeconds() const noexcept {
    return model_->referenceRawSeconds;
}

std::size_t ZoneBinding::getLogicalBytes() const noexcept {
    return sizeof(detail::ZoneBindingModel) + model_->id.size() * sizeof(char16_t);
}

TimeZoneQueryBounds ZoneBinding::getQueryBounds() const noexcept {
    return {std::numeric_limits<std::int64_t>::min(),
            model_->zone.fixed ? std::numeric_limits<std::int64_t>::max() : detail::provider::maximumUtcMillis()};
}

std::expected<TimeZoneOffset, TimeZoneError> ZoneBinding::getOffset(std::int64_t utc) const noexcept {
    try {
        return TimeZoneOffset{detail::offsetSeconds(detail::actualOffset(model_->zone, utc)),
                              model_->referenceRawSeconds};
    } catch (const detail::Failure &failure) {
        return std::unexpected(failure.error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TimeZoneError::ALLOCATION);
    } catch (...) {
        return std::unexpected(TimeZoneError::DATA);
    }
}
} // namespace org::ttldtor::qdcxx::time
