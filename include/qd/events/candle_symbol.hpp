// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/events/candle_attributes.hpp"
#include "qd/events/candle_period.hpp"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace org::ttldtor::qdcxx::events {
/** One of the seven supported Candle attribute values; custom data-type strings remain open-ended. */
using CandleAttribute = std::variant<CandleExchange, CandlePrice, CandleSession, CandlePeriod, CandleAlignment,
                                     CandlePriceLevel, CandleDataType>;

/** Immutable owning Candle identity and parsed attributes, without aggregation or receiving work.
 * Copies/moves share immutable owned storage; moves preserve the source value. Input text is never
 * retained. Const inspection of independent copies is concurrent; assignment to one object requires
 * external synchronization. Borrowed getter views survive until this object's replacement/destruction.
 * No ambient properties, runtime, callbacks or caches are used.
 */
class CandleSymbol final {
    struct State;
    std::shared_ptr<const State> state_;
    /** @param state Fully initialized immutable owned candidate. */
    explicit CandleSymbol(std::shared_ptr<const State> state) noexcept;
    /** @param symbol Borrowed input; null represents Java null.
     * @param attributes Borrowed complete attribute list; inputs are not retained.
     * @return Complete independent value, or an exception before publication.
     */
    [[nodiscard]] static CandleSymbol create(std::optional<std::u16string_view> symbol,
                                             std::span<const CandleAttribute> attributes);

    public:
    /** @param other Immutable value to share without allocating. */
    CandleSymbol(const CandleSymbol &other) noexcept = default;
    /** @param other Value to share; its original value remains valid. */
    CandleSymbol(CandleSymbol &&other) noexcept;
    /** @param other Value to share without allocating.
     * @return This object after replacement; previous borrowed views can be invalidated.
     */
    CandleSymbol &operator=(const CandleSymbol &other) noexcept = default;
    /** @param other Value to share; its original value remains valid.
     * @return This object after replacement; previous borrowed views can be invalidated.
     */
    CandleSymbol &operator=(CandleSymbol &&other) noexcept;
    /** Normalize and parse a symbol using the actual reference attribute order and refusals.
     * @param symbol Borrowed exact UTF-16; null is preserved and uses default getters.
     * @return Immutable owning value.
     * @throws std::invalid_argument A recognized attribute is invalid.
     * @throws std::out_of_range Malformed input produces an invalid substring range.
     * @throws std::bad_alloc Candidate allocation fails.
     * @throws std::length_error Candidate exceeds string capacity.
     */
    [[nodiscard]] static CandleSymbol valueOf(std::optional<std::u16string_view> symbol);
    /** Apply all attributes in order, normalize, then retain explicit getter values.
     * Repeated attribute kinds are invalid even when equal. Explicit nonroundtripping periods
     * retain their value instead of reparsing the canonical text.
     * @param symbol Borrowed exact UTF-16; null is accepted.
     * @param attributes Complete borrowed list; custom strings are already owned values.
     * @return Immutable owning value; failure exposes no partially changed object.
     * @throws std::invalid_argument Invalid enum, duplicate kind or invalid remaining attribute.
     * @throws std::out_of_range Malformed input produces an invalid substring range.
     * @throws std::bad_alloc Candidate allocation fails.
     * @throws std::length_error Candidate exceeds string capacity.
     */
    [[nodiscard]] static CandleSymbol valueOf(std::optional<std::u16string_view> symbol,
                                              std::initializer_list<CandleAttribute> attributes);

    /** Convenience factory for the same seven attribute types.
     * @tparam Attributes One or more values constructible as CandleAttribute.
     * @param symbol Borrowed exact UTF-16; null is accepted.
     * @param attributes Values borrowed for this call, copied into the complete candidate.
     * @return Immutable owning value, with the same refusals as the attribute-list overload.
     */
    template <class... Attributes>
        requires(sizeof...(Attributes) > 0 && (std::is_constructible_v<CandleAttribute, const Attributes &> && ...))
    [[nodiscard]] static CandleSymbol valueOf(std::optional<std::u16string_view> symbol,
                                              const Attributes &...attributes) {
        return valueOf(symbol, {CandleAttribute(attributes)...});
    }

    /** @return Borrowed nullable canonical text; no global key sorting is implied. */
    [[nodiscard]] std::optional<std::u16string_view> getSymbol() const noexcept;
    /** @return Independent nullable canonical UTF-16 copy.
     * @throws std::bad_alloc Copy allocation fails.
     * @throws std::length_error Output exceeds string capacity.
     */
    [[nodiscard]] std::optional<std::u16string> toString() const;
    /** @return Borrowed nullable base market symbol without regional suffix/recognized attributes. */
    [[nodiscard]] std::optional<std::u16string_view> getBaseSymbol() const noexcept;
    /** @return Independent exchange value, including composite zero. */
    [[nodiscard]] CandleExchange getExchange() const noexcept;
    /** @return Parsed or explicitly supplied input-price selection. */
    [[nodiscard]] CandlePrice getPrice() const noexcept;
    /** @return Parsed or explicitly supplied input-session selection. */
    [[nodiscard]] CandleSession getSession() const noexcept;
    /** @return Independent parsed or explicitly supplied aggregation period. */
    [[nodiscard]] CandlePeriod getPeriod() const noexcept;
    /** @return Parsed or explicitly supplied boundary alignment. */
    [[nodiscard]] CandleAlignment getAlignment() const noexcept;
    /** @return Independent parsed or explicitly supplied price interval. */
    [[nodiscard]] CandlePriceLevel getPriceLevel() const noexcept;
    /** @return Borrowed immutable data type; valid until this object's replacement/destruction. */
    [[nodiscard]] const CandleDataType &getDataType() const noexcept;
    /** Java String hash over the canonical UTF-16 units with signed32 wraparound.
     * @return Exact reference hash code.
     * @throws std::invalid_argument The canonical symbol is null (Java null-domain failure).
     */
    [[nodiscard]] std::int32_t hashCode() const;
    /** Java-like equality, including its nullable-domain error; self comparison is always true.
     * @param other Immutable value to compare.
     * @return Exact canonical string equality for nonnull left operands.
     * @throws std::invalid_argument The left symbol is null and other is a different object.
     */
    [[nodiscard]] bool equals(const CandleSymbol &other) const;
    /** Total symmetric C++ convenience equality, distinct from Java equals on null values.
     * @param other Immutable value to compare.
     * @return Exact optional canonical text equality; two null symbols compare equal.
     */
    [[nodiscard]] bool operator==(const CandleSymbol &other) const noexcept;
};
} // namespace org::ttldtor::qdcxx::events
