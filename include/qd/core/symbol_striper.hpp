// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string_view>

namespace org::ttldtor::qdcxx::core {
namespace detail {
struct SymbolStripeRule;
struct SymbolStriperState;
} // namespace detail

/** Pure construction/query refusal; no source, collector or subscription changes. */
enum class SymbolStripeError {
    INVALID_STATE,         ///< Default or moved-from value has no immutable state.
    UNKNOWN_SPECIFICATION, ///< The requested factory format is not a supported stripe leaf or striper.
    INVALID_SPECIFICATION, ///< A recognized format violates its pinned grammar or numeric domain.
    INVALID_INDEX,         ///< Requested index is outside this striper.
    RESOURCE_LIMIT,        ///< Explicit text, stripe, symbol or complete retained backing grant is exceeded.
    ALLOCATION_FAILURE     ///< Immutable owned state could not be allocated.
};

/** Required finite grants; zero means zero. Construction performs no ambient configuration lookup. */
struct SymbolStriperLimits {
    std::size_t specificationUnits; ///< Maximum UTF-16 units in one input specification.
    std::size_t stripes;            ///< Maximum stripe count; hash metadata does not allocate one row per stripe.
    std::size_t symbolUnits;        ///< Maximum UTF-16 units in one synchronous exact-symbol query.
    std::size_t storageBytes; ///< Complete immutable object/string/vector capacities, excluding allocator overhead.
};

/** Proven contiguous intersecting stripe interval, independent of channels, sources and execution domains. */
struct StripeIntersection {
    std::size_t first; ///< Inclusive first intersecting stripe index.
    std::size_t count; ///< Number of successive intersecting stripes; zero is a known empty result.
};

/** Immutable owning static ANY/hash/range leaf, below the event and physical-record filter boundary.
 * Copies share immutable owned state. Const queries are concurrent; borrowed query input must stay unmodified.
 * This value does not install a FeedFilter or change a subscription. Explicit wildcard admission is separate
 * from the exact textual symbol. Name views require this value or an owning copy to stay alive.
 */
class SymbolStripeFilter final {
    std::shared_ptr<const detail::SymbolStripeRule> rule_;
    friend class SymbolStriper;
    explicit SymbolStripeFilter(std::shared_ptr<const detail::SymbolStripeRule> rule) noexcept;

    public:
    /** Construct an invalid value without allocation. */
    SymbolStripeFilter() noexcept = default;
    /** Parse a static stripe leaf, preserving its accepted name; range--- becomes ANY.
     * @param specification Exact UTF-16 *, hashMofN or range-left-right- syntax, borrowed only during construction.
     * @param limits Explicit finite construction/query grants, copied into the immutable state.
     * @return Owning static leaf or detached error; unrecognized full filter expressions return UNKNOWN_SPECIFICATION.
     */
    [[nodiscard]] static std::expected<SymbolStripeFilter, SymbolStripeError> parse(std::u16string_view specification,
                                                                                    SymbolStriperLimits limits);
    /** @return Whether this value retains an immutable leaf. */
    [[nodiscard]] bool valid() const noexcept;
    /** @return Borrowed leaf name, or empty text for an invalid value. */
    [[nodiscard]] std::u16string_view name() const noexcept;
    /** @return Complete conservative immutable backing model, or zero for an invalid value. */
    [[nodiscard]] std::size_t storageBytes() const noexcept;
    /** Match already resolved physical symbol coordinates without delegate/event-source projection.
     * @param symbol Exact UTF-16 symbol, borrowed only during the call.
     * @param wildcard Explicit QD wildcard admission; true is accepted by every valid leaf.
     * @return Exact leaf predicate or invalid-state/query-bound refusal; no callback or allocation occurs.
     */
    [[nodiscard]] std::expected<bool, SymbolStripeError> accepts(std::u16string_view symbol,
                                                                 bool wildcard = false) const noexcept;
};

/** Immutable deterministic symbol-space partition; every exact string selects one stripe.
 * Copies share independently owned state. No callbacks, source work, wire semantics or executor are introduced.
 * Selection follows the production string route; an explicit ALL subscription must be fanned out separately.
 */
class SymbolStriper final {
    std::shared_ptr<const detail::SymbolStriperState> state_;
    explicit SymbolStriper(std::shared_ptr<const detail::SymbolStriperState> state) noexcept;

    public:
    /** Construct an invalid value without allocation. */
    SymbolStriper() noexcept = default;
    /** Parse MONO, hash or range striping; empty input means by1.
     * @param specification Exact UTF-16 by1, byhashN or byrange-boundaries- syntax, borrowed through this call.
     * @param limits Required finite construction/query grants; unsupported ambient properties are never read.
     * @return Owning immutable partition or detached error, with caller input unchanged.
     */
    [[nodiscard]] static std::expected<SymbolStriper, SymbolStripeError> parse(std::u16string_view specification,
                                                                               SymbolStriperLimits limits);
    /** @return Whether this value retains an immutable partition. */
    [[nodiscard]] bool valid() const noexcept;
    /** @return Borrowed canonical name while this value or an owning copy lives; empty if invalid. */
    [[nodiscard]] std::u16string_view name() const noexcept;
    /** @return Complete conservative immutable backing model, or zero for an invalid value. */
    [[nodiscard]] std::size_t storageBytes() const noexcept;
    /** @return Positive configured stripe count, or zero for an invalid value. */
    [[nodiscard]] std::size_t stripeCount() const noexcept;
    /** @param symbol Exact physical UTF-16 symbol borrowed through the call; no normalization occurs.
     * @return Deterministic zero-based stripe index or invalid-state/query-bound refusal; no allocation occurs.
     */
    [[nodiscard]] std::expected<std::size_t, SymbolStripeError> stripeIndex(std::u16string_view symbol) const noexcept;
    /** Build an independently owned leaf with the same finite query grants.
     * @param index Zero-based stripe index below stripeCount().
     * @return Canonically named immutable static leaf or detached state/index/backing/allocation error.
     */
    [[nodiscard]] std::expected<SymbolStripeFilter, SymbolStripeError> stripeFilter(std::size_t index) const;
    /** Inspect proven overlap without evaluating an arbitrary full filter expression.
     * @param filter Valid immutable static leaf; the caller keeps any additional upper-layer predicate active.
     * @return Exact contiguous interval for compatible hash/range kinds, nullopt when unknown (including ANY/MONO
     * and different kinds), or INVALID_STATE. Unknown requires considering every stripe, never dropping input.
     */
    [[nodiscard]] std::expected<std::optional<StripeIntersection>, SymbolStripeError>
    intersectingStripes(const SymbolStripeFilter &filter) const noexcept;
};
} // namespace org::ttldtor::qdcxx::core
