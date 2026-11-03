// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Owned source identity. Every signed ID and exact nullable UTF-16 name is accepted.
 * Equality and hashCode use only the ID, including comparisons with order sources.
 * Distinct values require no shared storage or object-address identity.
 */
class IndexedEventSource final {
    std::int32_t id_;
    std::optional<std::u16string> name_;

    public:
    /** Copy an exact source name without interpreting exchange codes or routes.
     * @param id Any signed 32-bit source ID.
     * @param name Owned exact UTF-16 name; nullopt represents Java null.
     * @throws std::bad_alloc Allocation fails.
     * @throws std::length_error The owned name exceeds string capacity.
     */
    IndexedEventSource(std::int32_t id, std::optional<std::u16string> name);

    /** @return Signed source ID, without range normalization. */
    [[nodiscard]] std::int32_t id() const noexcept;
    /** @return Nullable borrowed exact name; valid until this value is assigned, moved from or destroyed. */
    [[nodiscard]] std::optional<std::u16string_view> name() const noexcept;
    /** @return Signed Java-compatible hash, exactly equal to id(). */
    [[nodiscard]] std::int32_t hashCode() const noexcept;
    /** Compare IDs, ignoring names.
     * @param other Source identity to compare.
     * @return True when the IDs match.
     */
    [[nodiscard]] bool operator==(const IndexedEventSource &other) const noexcept;

    /** Default identity with ID zero and name DEFAULT. */
    static const IndexedEventSource DEFAULT;
};
} // namespace org::ttldtor::qdcxx::events

namespace std {
/** Standard hash adapter; signed hashCode is converted to size_t. */
template <> struct hash<org::ttldtor::qdcxx::events::IndexedEventSource> {
    /** @param source Owned identity to hash.
     * @return ID converted to size_t, consistent with equality.
     */
    size_t operator()(const org::ttldtor::qdcxx::events::IndexedEventSource &source) const noexcept {
        return static_cast<size_t>(source.hashCode());
    }
};
} // namespace std
