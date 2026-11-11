// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/core/wide_decimal.hpp"
#include "qd/events/event_mapping.hpp"
#include "qd/events/indexed_event_source.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Owning indexed MarketMaker. Business side times and collector indexed time are independent. */
struct MarketMaker {
    std::u16string symbol;         ///< Exact owning event symbol, without regional rewriting.
    std::int64_t eventTime = 0;    ///< Independent FIELD milliseconds, zero when not selected.
    std::int64_t index = 0;        ///< Public char16 exchange high32 and unsigned MMID low32.
    std::int32_t eventFlags = 0;   ///< Actual indexed transaction/snapshot/removal flags.
    std::int64_t bidTimeMs = 0;    ///< Full business bid milliseconds; absent time is zero.
    core::WideDecimal bidPrice{0}; ///< Generated wide-accessor price; zero represents NaN.
    core::WideDecimal bidSize{0};  ///< Generated wide-accessor size, including fractional values.
    std::int64_t bidCount = 0;     ///< Generated signed32 count widened to signed64.
    std::int64_t askTimeMs = 0;    ///< Full business ask milliseconds; absent time is zero.
    core::WideDecimal askPrice{0}; ///< Generated wide-accessor price, independent of bid.
    core::WideDecimal askSize{0};  ///< Generated wide-accessor size, independent of bid.
    std::int64_t askCount = 0;     ///< Generated signed32 count widened to signed64.
    std::optional<std::array<detail::BusinessNumericSnapshot, 4>> businessNumericSnapshot{};
    ///< Inline direct source doubles; a manual raw edit invalidates only its corresponding cached value.
    /** @return The current public indexed identity. */
    [[nodiscard]] std::int64_t getIndex() const noexcept;
    /** @return DEFAULT source ID zero, independent of index exchange bits. */
    [[nodiscard]] std::int32_t getSourceId() const noexcept;
    /** @return Stable immutable DEFAULT source, retaining no event or operational owner. */
    [[nodiscard]] const IndexedEventSource &getSource() const noexcept;
    /** @return Maximum full bid/ask milliseconds, without sequence packing. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Low char16 exchange unit from the current index high32. */
    [[nodiscard]] char16_t getExchangeCode() const noexcept;
    /** Decode the zero-extended current index low32, independently of derived Order signed decoding.
     * @return Owning nullable byte-valued text, at most four UTF-16 units.
     * @throws std::bad_alloc Result allocation fails.
     * @throws std::length_error The string implementation cannot represent the bounded result.
     */
    [[nodiscard]] std::optional<std::u16string> getMarketMaker() const;
    /** @return Exact direct source bid price while its wide validity key matches. */
    [[nodiscard]] double getBidPrice() const noexcept;
    /** @return Exact direct source bid size while its wide validity key matches. */
    [[nodiscard]] double getBidSize() const noexcept;
    /** @return Exact direct source ask price while its wide validity key matches. */
    [[nodiscard]] double getAskPrice() const noexcept;
    /** @return Exact direct source ask size while its wide validity key matches. */
    [[nodiscard]] double getAskSize() const noexcept;
    /** @return Full owning bid milliseconds. */
    [[nodiscard]] std::int64_t getBidTime() const noexcept;
    /** @return Full owning ask milliseconds. */
    [[nodiscard]] std::int64_t getAskTime() const noexcept;
    /** @return Owning signed count, including negative values. */
    [[nodiscard]] std::int64_t getBidCount() const noexcept;
    /** @return Owning signed count, including negative values. */
    [[nodiscard]] std::int64_t getAskCount() const noexcept;
};

/** Construct the standard indexed MMExchange/MMID physical pair and ten logical fields.
 * @param limits Complete finite schema bounds, including zero.
 * @return Owning default MarketMaker schema or atomic typed refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultMarketMakerSchema(core::SchemaLimits limits);

/** Independently owned exact-field MarketMaker plan, with no source schema or cursor borrow. */
class PreparedMarketMaker {
    EventMapping mapping_;
    std::optional<core::IndexedTimeLayout> indexedTimeLayout_; ///< Actual owning source-time facts only.
    /** @param mapping Validated owning field plan.
     * @param layout Exact optional original physical indexed time layout, owned independently.
     */
    explicit PreparedMarketMaker(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout) noexcept;

    public:
    /** Resolve the required raw/numeric and optional time/count properties.
     * @param schema Explicit validated source declarations.
     * @param localId Exact local record identity.
     * @param limits Finite preparation allowances, never replenished.
     * @return Complete immutable plan or typed context/type/resource refusal.
     */
    [[nodiscard]] static std::expected<PreparedMarketMaker, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Retained exact provenance until this mapping is moved, assigned or destroyed. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Validate the entire local shape and copy only the complete owning output.
     * @param symbol Exact event symbol, borrowed through return.
     * @param values Complete context-tagged local values.
     * @param limits Finite copied symbol/total-unit bounds; no field text is retained.
     * @param eventTime Independent FIELD milliseconds.
     * @param eventFlags Actual collector delivery flags.
     * @param indexedRemovalTime Exact full raw time for explicit REMOVE validation; normal mapping stays strict.
     * @return Independent event or atomic typed refusal.
     */
    [[nodiscard]] std::expected<MarketMaker, MappingError>
    map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits, std::int64_t eventTime = 0,
        std::int32_t eventFlags = 0, std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
    /** Map borrowed fields without constructing an intermediate owning record.
     * @param symbol Exact event symbol, borrowed through return.
     * @param values Complete context-tagged field views, valid through return.
     * @param limits Finite copied symbol/total-unit bounds.
     * @param eventTime Independent FIELD milliseconds.
     * @param eventFlags Actual collector delivery flags.
     * @param indexedRemovalTime Exact full raw time for explicit REMOVE validation; normal mapping stays strict.
     * @return Independent event or atomic typed refusal; inputs are never retained.
     */
    [[nodiscard]] std::expected<MarketMaker, MappingError>
    mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
            std::int64_t eventTime = 0, std::int32_t eventFlags = 0,
            std::optional<std::int64_t> indexedRemovalTime = std::nullopt) const;
};
} // namespace org::ttldtor::qdcxx::events
