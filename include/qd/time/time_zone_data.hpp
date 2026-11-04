// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

namespace org::ttldtor::qdcxx::time {
namespace detail {
struct TimeZoneProfile;
struct ZoneBindingModel;
struct Access;
} // namespace detail

/** Typed failures; no incomplete provider or binding is returned. */
enum class TimeZoneError {
    INVALID_ID,       ///< The exact UTF-16 identity is outside the pinned Java compatibility policy.
    INVALID_ARGUMENT, ///< Invalid initialization/path option before admission.
    NOT_READY,        ///< A valid named identity requires an explicitly ready data capability.
    OUT_OF_RANGE,     ///< This non-fixed binding cannot query the supplied instant in its native envelope.
    RESOURCE_LIMIT,   ///< A finite admission/model/identity bound would be exceeded, or initialization was already
                      ///< admitted.
    ALLOCATION,       ///< An ordinary owning allocation failed.
    DATA              ///< Missing/corrupt/unusable controlled data or an unexpected provider failure.
};

/** Finite fixed-snapshot admission bounds; ordinary allocation overhead is not a hard heap/RSS quota. */
struct TimeZoneLimits {
    std::size_t files = 11;           ///< Maximum fixed-profile files.
    std::size_t totalBytes = 1100000; ///< Maximum cumulative admitted source bytes.
    std::size_t lineBytes = 1024;     ///< Maximum bytes in one source line.
    std::size_t lines = 35000;        ///< Maximum cumulative source lines.
    std::size_t records = 12000;      ///< Maximum charged source records.
    std::size_t zones = 600;          ///< Maximum native zone records.
    std::size_t links = 400;          ///< Maximum native link records.
    std::size_t rules = 10000;        ///< Maximum adjusted native rule records.
    std::size_t modelUnits = 32768;   ///< Cumulative native insertions; discarded work is not refunded.
};

/** Explicit immutable binding-construction options, separate from subsequent query instants. */
struct TimeZoneBindingOptions {
    std::optional<std::int64_t> rawOffsetReferenceTime;
    ///< Constructor-clock replacement; absent captures current signed Unix milliseconds once at binding creation.
    std::size_t maxIdUnits = 128; ///< Finite accepted UTF-16 identity length before an owning copy.
};

/** Copied scalar result; constructor/reference raw metadata is distinct from historical actual offset. */
struct TimeZoneOffset {
    std::int32_t totalSeconds;        ///< Actual UTC-to-local offset at the requested instant, in whole seconds.
    std::int32_t referenceRawSeconds; ///< Captured constructor/reference raw offset; not inferred DST at query time.
};

/** Inclusive scalar-query envelope; a fixed binding supports the complete signed millisecond domain. */
struct TimeZoneQueryBounds {
    std::int64_t minimumUtcMillis; ///< Inclusive lower instant; pre1900 named queries use final raw-offset semantics.
    std::int64_t
        maximumUtcMillis; ///< Inclusive upper instant; named native profiles leave following-year search headroom.
};

class ZoneBinding;

/** Owning capability for one immutable private provider/data generation per process.
 * initialize reads only eleven explicitly named regular files below an absolute root, compares
 * exact pinned bytes, and finishes all native/identity initialization before publication. No home,
 * OS-zone discovery, network, automatic updates or reload occurs. One admitted attempt is permitted
 * per process, including terminal failures. Copies may serve independent calendar/text contexts.
 */
class TimeZoneData final {
    std::shared_ptr<const detail::TimeZoneProfile> profile_;
    explicit TimeZoneData(std::shared_ptr<const detail::TimeZoneProfile> profile) noexcept;
    friend struct detail::Access;
    friend class ZoneBinding;

    public:
    /** @param other Ready immutable generation to share without allocation. */
    TimeZoneData(const TimeZoneData &other) noexcept = default;
    /** @param other Ready generation; the original handle remains valid. */
    TimeZoneData(TimeZoneData &&other) noexcept;
    /** @param other Ready generation to share.
     * @return This handle after replacement.
     */
    TimeZoneData &operator=(const TimeZoneData &other) noexcept = default;
    /** @param other Ready generation; the original handle remains valid.
     * @return This handle after replacement.
     */
    TimeZoneData &operator=(TimeZoneData &&other) noexcept;
    /** @param root Explicit absolute versioned data directory; named symlink roots/files are refused.
     * @param limits Finite logical profile bounds; restrictive settings may refuse the fixed dataset.
     * @return Ready capability or typed failure; no partially initialized capability escapes.
     */
    [[nodiscard]] static std::expected<TimeZoneData, TimeZoneError> initialize(const std::filesystem::path &root,
                                                                               TimeZoneLimits limits = {});
    /** @param id Exact requested UTF-16 zone identity, copied on success.
     * @param options Finite identity bound and explicit/one-shot constructor raw reference.
     * @return Immutable owning binding retaining this data generation, or typed failure.
     */
    [[nodiscard]] std::expected<ZoneBinding, TimeZoneError> bind(std::u16string_view id,
                                                                 TimeZoneBindingOptions options = {}) const;
    /** @return Immutable transition dataset version, independently of market-calendar defaults. */
    [[nodiscard]] std::string_view getVersion() const noexcept;
};

/** Immutable owning identity/reference binding; copies share a finite owned model.
 * Fixed UTC/normalized-offset/fixed-legacy identities can be created without initialized data.
 * Other accepted named identities require a ready capability. A binding retains all required
 * provider/model backing; destruction of the original data/context handle cannot invalidate it.
 * Queries copy scalars and perform no clock reads, file I/O, callbacks, registry growth or reload.
 * Moves preserve the source. Replacing one handle concurrently requires external synchronization.
 */
class ZoneBinding final {
    std::shared_ptr<const detail::ZoneBindingModel> model_;
    explicit ZoneBinding(std::shared_ptr<const detail::ZoneBindingModel> model) noexcept;
    friend struct detail::Access;

    public:
    /** @param other Immutable model to share without allocation. */
    ZoneBinding(const ZoneBinding &other) noexcept = default;
    /** @param other Immutable model; the original handle remains valid. */
    ZoneBinding(ZoneBinding &&other) noexcept;
    /** @param other Immutable model to share.
     * @return This handle after replacement.
     */
    ZoneBinding &operator=(const ZoneBinding &other) noexcept = default;
    /** @param other Immutable model; the original handle remains valid.
     * @return This handle after replacement.
     */
    ZoneBinding &operator=(ZoneBinding &&other) noexcept;
    /** @param id Exact UTF-16 identity; caller storage is never retained.
     * @param data Optional ready capability; accepted non-fixed names return NOT_READY when absent.
     * @param options Finite copied identity bound and explicit/one-shot raw reference instant.
     * @return Complete immutable binding or typed failure; no global initialization is triggered here.
     */
    [[nodiscard]] static std::expected<ZoneBinding, TimeZoneError>
    create(std::u16string_view id, const TimeZoneData *data = nullptr, TimeZoneBindingOptions options = {});
    /** @return Borrowed exact requested identity, stable while this handle is unchanged/alive. */
    [[nodiscard]] std::u16string_view getId() const noexcept;
    /** @return Captured constructor/reference signed Unix millisecond value. */
    [[nodiscard]] std::int64_t getRawOffsetReferenceTime() const noexcept;
    /** @return Captured raw offset at that reference, distinct from final/historical actual offsets. */
    [[nodiscard]] std::int32_t getRawOffsetSeconds() const noexcept;
    /** @param utcMillis Signed Unix milliseconds without leap-second insertion.
     * @return Copied actual total offset plus constructor/reference raw metadata, or typed refusal.
     */
    [[nodiscard]] std::expected<TimeZoneOffset, TimeZoneError> getOffset(std::int64_t utcMillis) const noexcept;
    /** @return Inclusive scalar-query bounds for this fixed or native binding. */
    [[nodiscard]] TimeZoneQueryBounds getQueryBounds() const noexcept;
    /** @return Distinct owned model fields/ID bytes; shared provider backing and allocator overhead are excluded. */
    [[nodiscard]] std::size_t getLogicalBytes() const noexcept;
};
} // namespace org::ttldtor::qdcxx::time
