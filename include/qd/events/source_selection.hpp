// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/indexed_event_source.hpp"

#include <optional>

namespace org::ttldtor::qdcxx::events {
/** Owned pure selector for all indexed sources or one ID.
 * ALL is distinct from explicit DEFAULT (ID zero). Specific selection accepts any
 * base identity, including custom/negative IDs and null names, without capability checks.
 * This value performs no subscription, source enumeration or transport work.
 */
class SourceSelection final {
    std::optional<IndexedEventSource> source_;
    /** @param source Owned specific identity, or nullopt for ALL. */
    explicit SourceSelection(std::optional<IndexedEventSource> source);

    public:
    /** @return ALL selection without allocation. */
    [[nodiscard]] static SourceSelection all();
    /** @param source Identity transferred into this selector; names are owned.
     * @return Specific selection; no capability or range validation is applied.
     */
    [[nodiscard]] static SourceSelection specific(IndexedEventSource source);
    /** @return True only for ALL, including when no finite source catalog is known. */
    [[nodiscard]] bool isAll() const noexcept;
    /** @return Null for ALL, otherwise borrowed identity until assignment, move-from or destruction. */
    [[nodiscard]] const IndexedEventSource *source() const noexcept;
    /** @param candidate Identity to match; only its ID is inspected.
     * @return True for ALL or an equal selected ID.
     */
    [[nodiscard]] bool matches(const IndexedEventSource &candidate) const noexcept;
    /** @param other Selector to compare.
     * @return True for two ALL selections or specific selections with equal IDs.
     */
    [[nodiscard]] bool operator==(const SourceSelection &other) const noexcept;
};
} // namespace org::ttldtor::qdcxx::events
