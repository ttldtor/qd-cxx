// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/live_session.hpp"

namespace org::ttldtor::qdcxx::protocol::detail {
/** Required independent slice grants; zero is zero and source/epoch admission also remains active. */
struct StripeEventLimits {
    std::size_t metadataBytes;        ///< State plus complete view arrays, control fields and arena alignment.
    std::size_t copiedPayloadBytes;   ///< Legacy owned symbol/text/opaque values and separately copied legacy ints.
    std::size_t retainedPayloadBytes; ///< Unique complete immutable backing retained by selected shared rows.
};

/** Pre-publication slice cost; physical aliases are counted once per distinct backing in this slice. */
struct StripeEventAccounting {
    std::size_t records = 0;              ///< Exact selected original row occurrences.
    std::size_t fields = 0;               ///< Exact selected tagged field descriptors.
    std::size_t copiedTextUnits = 0;      ///< Exact legacy symbol and nullable text units copied contiguously.
    std::size_t copiedOpaqueBytes = 0;    ///< Exact legacy opaque bytes copied contiguously.
    std::size_t legacyValues = 0;         ///< Exact selected legacy compact-only values.
    std::size_t metadataBytes = 0;        ///< State and descriptor/control capacity.
    std::size_t copiedPayloadBytes = 0;   ///< Actual owned payload bytes copied into the new operational arena.
    std::size_t retainedPayloadBytes = 0; ///< Unique full shared backings retained by this selected operation.
    bool sharedRows = false;              ///< At least one selected row has a pure immutable alias.
};

/** Private collector composition seam; source and result access/release remain serialized.
 * Shared payloads retain only pure bytes/catalog metadata. Legacy input is copied into an independent
 * counted eager arena, and detached clients must continue through the existing pure-copy fallback.
 * No payload alias retains a parent event, operational snapshot or LiveIdentity.
 */
struct StripeEventAccess {
    /** Preflight original row identities and complete finite costs without allocating or changing source.
     * @param source Existing admitted DATA owner, borrowed only through this synchronous call.
     * @param rows Strictly increasing original row indices, preserving source order without duplicates.
     * @param limits Independent metadata/copy/retention grants for one slice.
     * @param work Explicit continuing work; all successful charges are retained after refusal.
     * @return Exact costs or typed invalid input, work, resource or arithmetic refusal.
     */
    [[nodiscard]] static std::expected<StripeEventAccounting, LiveError>
    measure(const LiveEvent &source, std::span<const std::size_t> rows, StripeEventLimits limits, LiveWork &work);
    /** Build one independently owned eager operational event, preserving original source metadata and epoch.
     * @param source Borrowed original; success and failure leave its complete ownership and rows unchanged.
     * @param rows Strictly increasing original row selection; empty creates an empty DATA event.
     * @param limits Explicit complete per-slice grants; source epoch capacities remain independently active.
     * @param work Mutable complete validation/copy/allocation work; no refunds.
     * @return Complete slice or failure with no output and no mutation of already completed child work.
     */
    [[nodiscard]] static std::expected<LiveEvent, LiveError>
    select(const LiveEvent &source, std::span<const std::size_t> rows, StripeEventLimits limits, LiveWork &work);
};
} // namespace org::ttldtor::qdcxx::protocol::detail
