// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/candle.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace org::ttldtor::qdcxx::events {

/** Refusal while preparing a candidate catalog or resolving one Candle symbol. */
enum class CandleRouteError {
    INVALID_INPUT,        ///< Invalid record identity, malformed record name or unsupported selector.
    DUPLICATE_DESCRIPTOR, ///< Two selected records claim the same Java delegate descriptor.
    MISSING_ROUTE,        ///< No matching fixed or generic record exists.
    INVALID_MAPPING,      ///< Candidate record cannot support its actual Candle delegate.
    RESOURCE_LIMIT,       ///< An explicit catalog or symbol bound was exceeded.
    ALLOCATION_FAILURE    ///< An owned candidate or result could not be allocated.
};

/** Detached failure with the selected schema ID and any mapper preparation failure. */
struct CandleRouteFailure {
    CandleRouteError error;
    std::optional<std::int32_t> recordId;
    std::optional<MappingError> mappingError;
};

/** Finite caller limits for one immutable physical Candle catalog. */
struct CandleRouteLimits {
    std::size_t records;         ///< Maximum explicit candidate IDs, including duplicates.
    std::size_t recordNameUnits; ///< Maximum UTF-16 units in one physical record name.
    std::size_t symbolUnits;     ///< Maximum UTF-16 units in one resolved physical symbol.
    MappingLimits mapping;       ///< Mapper preparation work and storage limits for each candidate.
};

/** Independently owned physical demand; it retains no schema declarations or caller symbol storage. */
struct CandlePhysicalRoute {
    std::int32_t recordId;       ///< Exact selected schema record ID.
    core::RecordContext context; ///< Owning selected schema provenance.
    CandleRecordKind kind;       ///< Prepared physical Candle or TradeHistory delegate kind.
    std::u16string symbol;       ///< Exact source symbol for the selected physical record.
};

/** Immutable bounded selector for an explicitly supplied set of eligible physical records.
 * Construction validates the complete catalog before publishing a resolver. Entries are explicit
 * candidate record IDs for one delivery contract; the caller determines which are eligible.
 * A candidate declaring indexed time must map it to Time and Sequence. The caller selects indexed
 * candidates when the intended source contract requires indexed history.
 * Independent const calls may run concurrently. No schema declarations or caller strings are retained.
 */
class PreparedCandleRoutes {
    struct Entry {
        std::int32_t recordId;
        core::RecordContext context;
        CandleRecordKind kind;
        std::optional<CandlePeriod> period;
        std::optional<CandlePrice> price;
    };

    std::vector<Entry> entries_;
    std::size_t symbolUnits_;

    /** @param entries Fully validated and independently owned descriptor catalog.
     * @param symbolUnits Finite source symbol bound.
     */
    PreparedCandleRoutes(std::vector<Entry> entries, std::size_t symbolUnits) noexcept;

    public:
    /** Validate the caller's complete physical record list and create an immutable descriptor catalog.
     * @param schema Validated immutable schema, borrowed through this call.
     * @param recordIds Exact candidate IDs eligible for one endpoint contract; duplicates are rejected.
     * @param limits Explicit finite catalog, name, symbol and mapper bounds.
     * @return Independent resolver or structured failure; no partial catalog is returned.
     */
    [[nodiscard]] static std::expected<PreparedCandleRoutes, CandleRouteFailure>
    prepare(const core::LocalSchema &schema, std::span<const std::int32_t> recordIds, CandleRouteLimits limits);

    /** Choose one eligible Java-compatible descriptor and form an independently owned physical demand.
     * @param symbol Caller-owned immutable Candle identity, borrowed through return.
     * @return Exact record/context/kind and physical symbol, or a structured refusal without demand.
     */
    [[nodiscard]] std::expected<CandlePhysicalRoute, CandleRouteFailure> resolve(const CandleSymbol &symbol) const;

    /** Recover an owning logical candle identity from one selected physical record and source symbol.
     * @param recordId Exact record ID previously admitted to this catalog.
     * @param physicalSymbol Borrowed nonempty exact physical source symbol.
     * @return Independent canonical CandleSymbol or structured refusal.
     */
    [[nodiscard]] std::expected<CandleSymbol, CandleRouteFailure> restore(std::int32_t recordId,
                                                                          std::u16string_view physicalSymbol) const;
};

} // namespace org::ttldtor::qdcxx::events
