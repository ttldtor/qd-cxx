// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "formatter.hpp"
#include <array>
#include <bit>
#include <charconv>

namespace org::ttldtor::qdcxx::inspect::detail {
/** Counting/writing cursor: every append checks subtraction before any expansion. */
class Row {
    std::size_t remaining_; ///< Bytes still permitted in this pass.
    std::string *output_;   ///< Null during counting; exact reserved destination during writing.

    public:
    /** Create one bounded pass.
     * @param budget Maximum logical bytes.
     * @param output Null to count without storing, otherwise destination.
     */
    Row(std::size_t budget, std::string *output) : remaining_(budget), output_(output) {
    }

    /** Append a bounded fragment.
     * @param text Borrowed already encoded bytes.
     */
    void add(std::string_view text) {
        if (text.size() > remaining_) {
            throw OutputLimit{};
        }

        remaining_ -= text.size();

        if (output_) {
            output_->append(text);
        }
    }

    /** Append a locale-independent number using fixed temporary storage.
     * @tparam T Integral type.
     * @param value Exact value.
     */
    template <typename T> void number(T value) {
        std::array<char, 32> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        add(std::string_view(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())));
    }

    /** Return unconsumed budget.
     * @return Remaining logical bytes.
     */
    std::size_t remaining() const {
        return remaining_;
    }

    /** Append four hex digits for one UTF-16 unit.
     * @param unit Exact unit.
     * @param upper Whether escape-style uppercase digits are required.
     */
    void hex(char16_t unit, bool upper) {
        const std::string_view digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
        std::array<char, 4> buffer{};

        // A UTF-16 unit has four nibbles, most significant first.
        for (std::size_t i = 0; i < buffer.size(); ++i) {
            buffer[i] = digits[(unit >> ((3 - i) * 4)) & 0xf];
        }

        add(std::string_view(buffer.data(), buffer.size()));
    }

    /** Append exact legacy unit hex or the empty sentinel.
     * @param text Exact UTF-16 units.
     */
    void units(std::u16string_view text) {
        if (text.empty()) {
            add("-");
        }

        for (auto unit : text) {
            hex(unit, false);
        }
    }

    /** Append exact lowercase opaque byte hex without executing its contents. */
    void bytes(std::span<const std::byte> value) {
        constexpr std::string_view DIGITS = "0123456789abcdef";

        for (const auto byte : value) {
            const auto raw = std::to_integer<unsigned>(byte);
            const std::array<char, 2> pair{DIGITS[raw >> 4], DIGITS[raw & 15]};
            add({pair.data(), pair.size()});
        }
    }

    /** Append exact binary64 bits, most significant nibble first.
     * @param value WideDecimal numeric view, whose NaN representation is already canonical.
     */
    void doubleBits(double value) {
        const auto bits = std::bit_cast<std::uint64_t>(value);

        // Four UTF-16-sized groups cover all 64 bits without temporary allocation.
        for (int shift = 48; shift >= 0; shift -= 16) {
            hex(static_cast<char16_t>(bits >> shift), false);
        }
    }

    /** Append quoted safe UTF-8 according to OFFLINE-INSPECTION, without normalization.
     * @param text Exact UTF-16 units; lone surrogates are escaped losslessly.
     */
    void quote(std::u16string_view text) {
        constexpr char16_t HIGH_FIRST = 0xd800, HIGH_LAST = 0xdbff;
        constexpr char16_t LOW_FIRST = 0xdc00, LOW_LAST = 0xdfff;
        constexpr std::uint32_t SUPPLEMENTARY_BASE = 0x10000;
        add("\"");

        for (std::size_t i = 0; i < text.size(); ++i) {
            const auto unit = text[i];

            switch (unit) {
            case u'"':
                add("\\\"");
                continue;
            case u'\\':
                add("\\\\");
                continue;
            case u'\t':
                add("\\t");
                continue;
            case u'\r':
                add("\\r");
                continue;
            case u'\n':
                add("\\n");
                continue;
            default:
                break;
            }

            std::uint32_t scalar = unit;

            if (unit >= HIGH_FIRST && unit <= HIGH_LAST && i + 1 < text.size() && text[i + 1] >= LOW_FIRST &&
                text[i + 1] <= LOW_LAST) {
                // Surrogates carry ten payload bits each.
                scalar = SUPPLEMENTARY_BASE + ((unit - HIGH_FIRST) << 10) + (text[++i] - LOW_FIRST);
            } else if (unit < 0x20 || (unit >= 0x7f && unit <= 0x9f) || unit == 0x061c ||
                       (unit >= 0x200e && unit <= 0x200f) || (unit >= 0x2028 && unit <= 0x202e) ||
                       (unit >= 0x2066 && unit <= 0x206f) || (unit >= HIGH_FIRST && unit <= LOW_LAST)) {
                // C0/C1/DEL, line separators, bidi controls and lone surrogates cannot reach the terminal raw.
                add("\\u");
                hex(unit, true);
                continue;
            }

            std::array<char, 4> buffer{};
            std::size_t count = 0;

            // UTF-8 scalar ranges and lead/continuation payload masks (surrogates excluded above).
            if (scalar <= 0x7f) {
                buffer[count++] = static_cast<char>(scalar);
            } else if (scalar <= 0x7ff) {
                buffer[count++] = static_cast<char>(0xc0 | (scalar >> 6));
                buffer[count++] = static_cast<char>(0x80 | (scalar & 0x3f));
            } else if (scalar <= 0xffff) {
                buffer[count++] = static_cast<char>(0xe0 | (scalar >> 12));
                buffer[count++] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3f));
                buffer[count++] = static_cast<char>(0x80 | (scalar & 0x3f));
            } else {
                buffer[count++] = static_cast<char>(0xf0 | (scalar >> 18));
                buffer[count++] = static_cast<char>(0x80 | ((scalar >> 12) & 0x3f));
                buffer[count++] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3f));
                buffer[count++] = static_cast<char>(0x80 | (scalar & 0x3f));
            }

            add(std::string_view(buffer.data(), count));
        }

        add("\"");
    }
};

/** Count before allocating one exact logical row, then repeat the same deterministic composition.
 * @tparam Compose Row composition callable.
 * @param budget Remaining encoded stdout bytes.
 * @param compose Callable accepting a counting or writing cursor.
 * @return Owned complete row; OutputLimit on excess, allocation exceptions propagate.
 */
template <typename Compose> std::string build(std::size_t budget, Compose compose) {
    Row counter(budget, nullptr);
    compose(counter);
    const auto size = budget - counter.remaining();
    std::string result;
    result.reserve(size);
    Row writer(size, &result);
    compose(writer);

    return result;
}
} // namespace org::ttldtor::qdcxx::inspect::detail
