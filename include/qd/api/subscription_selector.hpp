// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/subscription.hpp"
#include "qd/events/indexed_event_source.hpp"
#include "qd/events/order_index.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::api {
/** Explicit logical identity; textual punctuation never creates a decorator. */
enum class SubscriptionSymbolKind {
    PLAIN,       ///< Exact event symbol, with no explicit indexed source.
    INDEXED,     ///< Event symbol and source ID; source names are not identity or retained metadata.
    TIME_SERIES, ///< Event symbol identity with a separate mutable-effective fromTime attribute.
    ALL          ///< Explicit wildcard, distinct from every textual symbol including "*".
};

/** Independently owned selector value. No schema, source name, callback or input view is retained.
 * Equality is symmetric and includes kind. TIME_SERIES equality excludes fromTime, as in Java.
 * Factories accept exact UTF-16, including empty text; activation separately validates its domain.
 */
class SubscriptionSelector final {
    SubscriptionSymbolKind kind_;
    std::u16string symbol_;
    std::int32_t sourceId_ = 0;
    std::int64_t fromTime_ = 0;

    /** Adopt complete owned value storage.
     * @param kind Explicit logical category.
     * @param symbol Owned exact event symbol; empty for ALL.
     * @param sourceId Source ID; zero outside INDEXED.
     * @param fromTime Signed milliseconds; meaningful only for TIME_SERIES.
     */
    SubscriptionSelector(SubscriptionSymbolKind kind, std::u16string symbol, std::int32_t sourceId,
                         std::int64_t fromTime) noexcept;

    public:
    /** Copy exact owned state; input text is never borrowed after construction.
     * @param other Independent source value; allocation exceptions propagate before construction completes.
     */
    SubscriptionSelector(const SubscriptionSelector &other) = default;
    /** Transfer owned text without throwing.
     * @param other Source remains valid with unspecified text after the move.
     */
    SubscriptionSelector(SubscriptionSelector &&other) noexcept = default;
    /** Replace complete typed state with the strong allocation-failure guarantee.
     * @param other Borrowed source value; self-copy is unchanged and does not allocate.
     * @return This value; a failed replacement copy leaves its kind, text, source and time unchanged.
     */
    SubscriptionSelector &operator=(const SubscriptionSelector &other);
    /** Transfer complete state without throwing; moved-from and self-moved text is valid but unspecified.
     * @param other Source value whose owned text is transferred.
     * @return This valid value; previous contents are not guaranteed after self-move.
     */
    SubscriptionSelector &operator=(SubscriptionSelector &&other) noexcept = default;
    /** @param symbol Exact event symbol, transferred by value.
     * @return Plain independently owned identity; allocation exceptions propagate.
     */
    [[nodiscard]] static SubscriptionSelector plain(std::u16string symbol);
    /** @param symbol Exact event symbol, transferred by value.
     * @param source Source identity; only its ID is copied, never its nullable name.
     * @return Independently owned source-specific identity; allocation exceptions propagate.
     */
    [[nodiscard]] static SubscriptionSelector indexed(std::u16string symbol, const events::IndexedEventSource &source);
    /** @param symbol Exact event symbol, transferred by value.
     * @param fromTime Signed Unix milliseconds; all signed64 values, including MAX sentinel, are preserved.
     * @return Owned time-series selector, whose equality excludes fromTime.
     */
    [[nodiscard]] static SubscriptionSelector timeSeries(std::u16string symbol, std::int64_t fromTime);
    /** @return Explicit wildcard with no desired text; grants no activation permission. */
    [[nodiscard]] static SubscriptionSelector all() noexcept;
    /** @return Explicit category, independent of textual spelling. */
    [[nodiscard]] SubscriptionSymbolKind kind() const noexcept;
    /** @return Borrowed exact event symbol; empty for ALL, valid until mutation or destruction of this value. */
    [[nodiscard]] std::u16string_view eventSymbol() const noexcept;
    /** @return Explicit source ID for INDEXED, DEFAULT zero for TIME_SERIES, otherwise nullopt. */
    [[nodiscard]] std::optional<std::int32_t> sourceId() const noexcept;
    /** @return Signed milliseconds for TIME_SERIES, otherwise nullopt. */
    [[nodiscard]] std::optional<std::int64_t> fromTime() const noexcept;
    /** Convert the retained fromTime value without asserting any record or event-family capability.
     * @return Production generic QD time-sequence value for TIME_SERIES, preserving MAX sentinel;
     * nullopt for other kinds. This does not create or authorize a HISTORY or typed Feed subscription.
     */
    [[nodiscard]] std::optional<std::int64_t> qdTime() const noexcept;
    /** @param other Independent selector to compare.
     * @return Symmetric typed logical identity; TIME_SERIES fromTime is excluded.
     */
    [[nodiscard]] bool operator==(const SubscriptionSelector &other) const noexcept;
    /** @param other Independent desired value to compare.
     * @return Identity plus effective fromTime equality, suitable for deciding desired revisions.
     */
    [[nodiscard]] bool sameDemand(const SubscriptionSelector &other) const noexcept;
    /** @return Java-compatible signed32 hash for the corresponding kind; collisions across kinds are permitted. */
    [[nodiscard]] std::int32_t hashCode() const noexcept;
};

/** Atomic adapter refusal; no desired state or caller storage is changed. */
enum class SelectorRequestError {
    INVALID_CONTEXT,     ///< Prepared source context is invalid.
    UNSUPPORTED_SELECTOR ///< The adapter does not implement this selector kind.
};

/** Resolve ordinary/source-specific demand against an existing validated source binding.
 * This is prepared source/index metadata, not a typed Order or source-catalog implementation.
 * @param selector Owning PLAIN or INDEXED value, retained by the caller through synchronous request copying.
 * @param binding Existing PreparedOrderSource, whose source and full record shape were validated at preparation.
 * @return Borrowed exact request for matching source (all prepared sources for PLAIN), nullopt for a different
 * nonnegative source, or typed unsupported error. Negative explicit IDs select the available source set as in
 * the production source delegate set; their logical identities remain distinct. The request has no history bound.
 */
[[nodiscard]] std::expected<std::optional<core::SubscriptionRequest>, SelectorRequestError>
indexedSelectorRequest(const SubscriptionSelector &selector, const events::PreparedOrderSource &binding) noexcept;

} // namespace org::ttldtor::qdcxx::api

namespace std {
/** Standard hash consistent with symmetric selector equality. */
template <> struct hash<org::ttldtor::qdcxx::api::SubscriptionSelector> {
    /** @param selector Independently owned logical value.
     * @return Signed32 hash converted to size_t, as for existing source identities.
     */
    size_t operator()(const org::ttldtor::qdcxx::api::SubscriptionSelector &selector) const noexcept {
        return static_cast<size_t>(selector.hashCode());
    }
};
} // namespace std
