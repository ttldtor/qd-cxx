// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace org::ttldtor::qdcxx::glossary {
/** Finite per-value model bounds. Zero admits only empty input. */
struct AdditionalUnderlyingLimits {
    std::size_t textUnits = 1048576; ///< Maximum input/output UTF-16 code units.
    std::size_t entries = 16384;     ///< Maximum parsed entries, including duplicates, or map entries.
};

/** Owned additional-underlying text and symbol/SPC values, matching the production Java grammar.
 * Exact text defines equality; text parsing preserves order/spelling and the first duplicate wins.
 * Independent values may be used concurrently; callers synchronize assignment/destruction with readers.
 */
class AdditionalUnderlyings {
    std::u16string text_;
    std::map<std::u16string, double, std::less<>> map_;

    public:
    using Map = std::map<std::u16string, double, std::less<>>; ///< Owned symbols ordered by exact UTF-16 units.
    /** Construct an independent empty value. */
    AdditionalUnderlyings() = default;
    /** Copy an independent value; allocation failures propagate and no incomplete value is published. */
    AdditionalUnderlyings(const AdditionalUnderlyings &) = default;
    /** Move ownership; the source becomes an empty reusable value.
     * @param other Source value, modified to empty on success.
     * @throws std::bad_alloc Empty map sentinel allocation fails on implementations that allocate it; source is
     * unchanged.
     */
    AdditionalUnderlyings(AdditionalUnderlyings &&other) noexcept(
        std::is_nothrow_default_constructible_v<std::u16string> && std::is_nothrow_default_constructible_v<Map>);
    /** Replace atomically after a successful copy/move; failure during argument copying preserves this value.
     * @param other Independent staged value.
     * @return This value after replacement.
     */
    AdditionalUnderlyings &operator=(AdditionalUnderlyings other) noexcept;
    /** @return Process-lifetime empty value, safely initialized on first use (also before main). */
    [[nodiscard]] static const AdditionalUnderlyings &empty();
    /** Parse text without normalization; null/empty means empty.
     * @param text Exact UTF-16 input borrowed only for this call.
     * @param limits Finite input and entry limits.
     * @return Independent owned value.
     * @throws std::invalid_argument Invalid separators or SPC syntax; Java-compatible message.
     * @throws std::length_error Configured text/entry limit exceeded.
     * @throws std::bad_alloc Allocation fails, propagated without conversion to a syntax error.
     */
    [[nodiscard]] static AdditionalUnderlyings valueOf(std::optional<std::u16string_view> text,
                                                       AdditionalUnderlyingLimits limits = {});
    /** Render sorted symbols with the production 15-significant-digit policy, then parse the rendered text.
     * @param map Owned-key map borrowed only for this call; null means empty.
     * @param limits Finite output and entry limits.
     * @return Independent value; its SPC values are the parsed rounded representation.
     * @throws std::invalid_argument Empty/prohibited symbol or nonfinite SPC.
     * @throws std::length_error Configured text/entry limit exceeded.
     * @throws std::bad_alloc Allocation fails.
     */
    [[nodiscard]] static AdditionalUnderlyings valueOf(const Map *map, AdditionalUnderlyingLimits limits = {});

    /** @return Owned original text, borrowed until this value is modified or destroyed. */
    [[nodiscard]] const std::u16string &getText() const noexcept {
        return text_;
    }

    /** @return Immutable owned map, borrowed until this value is modified or destroyed. */
    [[nodiscard]] const Map &getMap() const noexcept {
        return map_;
    }

    /** @param symbol Exact symbol borrowed for this call; null means absent.
     * @return SPC, including signed zero or infinity admitted from text; zero for absent symbols.
     */
    [[nodiscard]] double getSpc(std::optional<std::u16string_view> symbol) const noexcept;
    /** Search without validating the complete grammar, scanning from the right like Java's static getSPC.
     * @param text Exact UTF-16 text borrowed for this call; null means empty.
     * @param symbol Exact symbol borrowed for this call; null/empty returns zero.
     * @return First numeric match scanning from the right, or zero; malformed surrounding text is ignored.
     */
    [[nodiscard]] static double getSpc(std::optional<std::u16string_view> text,
                                       std::optional<std::u16string_view> symbol) noexcept;

    /** @return Independent copy of the original text. */
    [[nodiscard]] std::u16string toString() const {
        return text_;
    }

    /** @param other Value to compare by exact original text.
     * @return Whether the original text is equal, independently of numeric-map equality.
     */
    [[nodiscard]] bool operator==(const AdditionalUnderlyings &other) const noexcept {
        return text_ == other.text_;
    }
};
} // namespace org::ttldtor::qdcxx::glossary
