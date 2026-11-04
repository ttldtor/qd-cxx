// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/ipf/instrument_profile.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace org::ttldtor::qdcxx::ipf {
/** Option-chain admission or strike-selection failure. */
enum class OptionChainError {
    LIMIT_EXCEEDED, ///< A configured aggregate, input-count or cumulative-work limit was exceeded.
    PROFILE_ACCESS, ///< The profile's own limits prevent reading a required standard numeric/date field.
    INVALID_COUNT   ///< A negative requested strike count is invalid.
};

/** Finite per-builder limits; zero disables admission of the corresponding resource. */
struct OptionChainLimits {
    std::size_t profiles = 100000; ///< Maximum successfully inspected input profiles, including ignored profiles.
    std::size_t chains = 4096;     ///< Maximum product/underlying chains retained together.
    std::size_t series = 16384;    ///< Maximum series summed across chains.
    std::size_t options = 200000;  ///< Maximum call/put strike slots summed across chains.
    std::size_t retainedUnits = 16777216; ///< Maximum logical retained UTF-16 units, charging shared profiles per slot.
    std::size_t stagingUnits = 50331648;  ///< Maximum conservative old/new/profile UTF-16 staging charge per admission.
    std::size_t workUnits = 1073741824; ///< Maximum cumulative logical admission work; see option-chain specification.
};

/** Attributes identifying a series; expiration has highest ordering priority.
 * Text uses exact UTF-16 order. Multiplier and SPC use Java Double.compare order, including signed zero and NaN.
 */
struct OptionSeriesMetadata {
    std::int32_t expiration = 0;          ///< Expiration day ID since 1970-01-01 GMT; zero is inadmissible.
    std::int32_t lastTrade = 0;           ///< Last trading day ID; zero denotes missing data.
    double multiplier = 0;                ///< Market-value multiplier, including nonfinite values.
    double spc = 0;                       ///< Shares per contract, including nonfinite values.
    std::u16string additionalUnderlyings; ///< Exact additional-underlying text, without parsing.
    std::u16string mmy;                   ///< Exact maturity month-year text.
    std::u16string optionType;            ///< Exact option-type text.
    std::u16string expirationStyle;       ///< Exact expiration-cycle text.
    std::u16string settlementStyle;       ///< Exact settlement-determination text.
    std::u16string cfi; ///< CFI with its second UTF-16 unit replaced by X; no full CFI validation is performed.
    /** Compare all identity attributes in their specified priority order.
     * @param other Metadata borrowed for this comparison.
     * @return Negative, zero or positive ordering; all NaN representations compare equal.
     */
    [[nodiscard]] int compareTo(const OptionSeriesMetadata &other) const noexcept;
};

/** Total binary64 strike ordering, with -0.0 before +0.0 and all NaNs after positive infinity. */
struct OptionStrikeLess {
    /** Compare two strike values using Java Double.compare ordering.
     * @param left First value.
     * @param right Second value.
     * @return Whether left precedes right; NaNs compare equivalent.
     */
    [[nodiscard]] bool operator()(double left, double right) const noexcept;
};

/** Owned immutable profile reference. Copies never retain a reference to the caller's input profile. */
using OptionProfile = std::shared_ptr<const InstrumentProfile>;
/** Ordered strike slots owning their immutable profiles. */
using OptionMap = std::map<double, OptionProfile, OptionStrikeLess>;

/** One immutable series of calls and puts; ordinary copies share only immutable owned profiles.
 * Containers and metadata are owned values. Concurrent read-only use is allowed after publication.
 */
class OptionSeries {
    OptionSeriesMetadata metadata_;
    OptionMap calls_;
    OptionMap puts_;
    friend class OptionChainsBuilder;

    public:
    /** Read series identity.
     * @return Borrowed metadata valid for this series object's lifetime.
     */
    [[nodiscard]] const OptionSeriesMetadata &getMetadata() const noexcept;
    /** Read calls in total binary64 strike order.
     * @return Borrowed immutable slots valid for this series object's lifetime.
     */
    [[nodiscard]] const OptionMap &getCalls() const noexcept;
    /** Read puts in total binary64 strike order.
     * @return Borrowed immutable slots valid for this series object's lifetime.
     */
    [[nodiscard]] const OptionMap &getPuts() const noexcept;
    /** Merge call and put strikes without duplicates under total binary64 ordering.
     * @return Independent owned ascending strikes; allocation failure propagates.
     */
    [[nodiscard]] std::vector<double> getStrikes() const;
    /** Select strikes around the insertion point of a center value, retaining Java tail truncation.
     * @param count Maximum number of strikes; negative values are rejected.
     * @param strike Center value, including signed zero and nonfinite values.
     * @return Independent selected strikes, or INVALID_COUNT; allocation failure propagates.
     */
    [[nodiscard]] std::expected<std::vector<double>, OptionChainError> getNStrikesAround(std::int32_t count,
                                                                                         double strike) const;
};

/** Immutable chain for one product or underlying symbol. */
class OptionChain {
    std::u16string symbol_;
    std::vector<OptionSeries> series_;
    friend class OptionChainsBuilder;

    public:
    /** Read the exact product or underlying symbol.
     * @return Borrowed symbol valid for this chain object's lifetime.
     */
    [[nodiscard]] const std::u16string &getSymbol() const noexcept;
    /** Read all series in metadata order.
     * @return Borrowed immutable series valid for this chain object's lifetime.
     */
    [[nodiscard]] const std::vector<OptionSeries> &getSeries() const noexcept;
};

/** Owned chain snapshot; symbols use deterministic UTF-16 order (Java's outer HashMap has no order). */
using OptionChains = std::map<std::u16string, std::shared_ptr<const OptionChain>, std::less<>>;

/** Profile-based option-chain builder with atomic admission and owned immutable snapshots.
 * No ambient settings, callbacks or network work. Mutating a builder requires external synchronization.
 * Limit/profile errors and allocation exceptions leave the entire builder unchanged. Copies share immutable
 * chains until mutation. A copied getChains() map survives subsequent changes and builder/input destruction.
 */
class OptionChainsBuilder {
    OptionChainLimits limits_;
    OptionChains chains_;
    std::size_t profiles_ = 0;
    std::size_t series_ = 0;
    std::size_t options_ = 0;
    std::size_t retainedUnits_ = 0;
    std::size_t workUnits_ = 0;
    /** Exchange coherent state without allocation.
     * @param other Builder receiving the previous state.
     */
    void swap(OptionChainsBuilder &other);

    public:
    /** Create an empty independent builder.
     * @param limits Explicit per-instance finite limits, sampled and copied once.
     */
    explicit OptionChainsBuilder(OptionChainLimits limits = {});
    /** Copy a builder, sharing immutable chains while keeping independent admission counters.
     * @param other Source borrowed for construction; allocation failure leaves it unchanged.
     */
    OptionChainsBuilder(const OptionChainsBuilder &other) = default;
    /** Transfer all state, leaving the source empty and reusable with its original limits.
     * @param other Source whose chain ownership and counters are transferred on success.
     * Allocation failure before transfer leaves the source unchanged.
     */
    OptionChainsBuilder(OptionChainsBuilder &&other);
    /** Replace all state atomically; argument construction may allocate and propagate an exception.
     * @param other Owned replacement, copied or moved before entering this function.
     * @return This builder; failed argument construction leaves it unchanged.
     */
    OptionChainsBuilder &operator=(OptionChainsBuilder other);
    /** Build from profiles in input order, replacing duplicate slots with the last admitted profile.
     * @param profiles Input borrowed only until return; admitted profiles are copied.
     * @param limits Explicit per-builder limits; no caller-owned configuration is retained.
     * @return Owned builder or the first admission error; allocation failure propagates.
     */
    [[nodiscard]] static std::expected<OptionChainsBuilder, OptionChainError>
    build(std::span<const InstrumentProfile> profiles, OptionChainLimits limits = {});
    /** Inspect one profile and atomically update both product and underlying chains.
     * Only exact TYPE=OPTION with OC/OP CFI, nonzero expiration, finite strike and a nonempty chain symbol is added.
     * @param profile Borrowed for the call; retained data is an immutable owned copy.
     * @return True if added/replaced, false if ignored, or an error without any builder change.
     */
    [[nodiscard]] std::expected<bool, OptionChainError> addProfile(const InstrumentProfile &profile);
    /** Read current chains; copy the map to retain a stable owning snapshot.
     * @return Borrowed map invalidated by the next successful mutation or builder destruction.
     */
    [[nodiscard]] const OptionChains &getChains() const noexcept;
    /** Read the limits sampled at construction.
     * @return Independent limit values.
     */
    [[nodiscard]] OptionChainLimits getLimits() const noexcept;
    /** Read the count of successfully inspected profiles, including ignored inputs.
     * @return Input count; failed admissions do not increment it.
     */
    [[nodiscard]] std::size_t getProfileCount() const noexcept;
    /** Read cumulative logical work charged to successful inspections.
     * @return Logical work units; failed admissions do not consume them.
     */
    [[nodiscard]] std::size_t getWorkUnits() const noexcept;
};
} // namespace org::ttldtor::qdcxx::ipf
