// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/encoding/penta_symbol.hpp"

#include <bit>

namespace org::ttldtor::qdcxx::encoding {
namespace {
/** Fixed source PentaCodec ASCII table: zero means unsupported, not an empty code. */
constexpr auto PENTAS = [] {
    std::array<std::uint16_t, 128> table{};

    for (std::size_t c = 'A'; c <= 'Z'; ++c) {
        table[c] = static_cast<std::uint16_t>(c - 'A' + 1);
    }

    table['.'] = 27;
    table['/'] = 28;
    table['$'] = 29;
    std::uint16_t extended = 0x03c0;

    for (std::size_t c = 32; c <= 126; ++c) {
        if (table[c] == 0 && c != '\'' && c != '`') {
            table[c] = extended++;
        }
    }

    return table;
}();
constexpr std::size_t MAX_SYMBOL_UNITS = 7;    ///< Source encode(String) maximum.
constexpr unsigned MAX_PENTA_BITS = 35;        ///< Source cipher domain, including slash/dollar high forms.
constexpr std::uint32_t LOW_BITS = 0x3fffffff; ///< Thirty low cipher/penta bits.
} // namespace

std::optional<PentaSymbolEncoding> encodePentaSymbol(std::u16string_view symbol) noexcept {
    if (symbol.size() > MAX_SYMBOL_UNITS) {
        return std::nullopt;
    }

    std::uint64_t penta = 0;
    unsigned bits = 0;

    for (const auto unit : symbol) {
        if (unit >= PENTAS.size() || PENTAS[unit] == 0) {
            return std::nullopt;
        }

        const auto code = PENTAS[unit];
        const unsigned width = code < 32 ? 5 : 10;

        if (width > MAX_PENTA_BITS - bits) {
            return std::nullopt;
        }

        penta = (penta << width) | code;
        bits += width;
    }

    std::uint32_t cipher = 0;

    if (bits <= 30) {
        cipher = static_cast<std::uint32_t>(penta) | 0x40000000u;
    } else {
        const auto high = penta >> 30;

        if (high == PENTAS['/']) {
            cipher = (static_cast<std::uint32_t>(penta) & LOW_BITS) | 0x80000000u;
        } else if (high == PENTAS['$']) {
            cipher = (static_cast<std::uint32_t>(penta) & LOW_BITS) | 0xc0000000u;
        } else {
            return std::nullopt;
        }
    }

    PentaSymbolEncoding result{std::bit_cast<std::int32_t>(cipher), {}, 0};
    const auto append = [&](std::uint64_t value) {
        result.bytes[result.size++] = static_cast<std::byte>(value & 0xffu);
    };

    if (penta == 0) {
        append(0xfe); // Canonical zero-bit symbol, distinct from a raw empty string.
    } else if (penta < 0x8000u) {
        append(penta >> 8);
        append(penta);
    } else if (penta < 0x100000u) {
        append(0xe0u | (penta >> 16));
        append(penta >> 8);
        append(penta);
    } else if (penta < 0x40000000u) {
        append(0x80u | (penta >> 24));
        append(penta >> 16);
        append(penta >> 8);
        append(penta);
    } else {
        append(0xf0u | (penta >> 32));
        append(penta >> 24);
        append(penta >> 16);
        append(penta >> 8);
        append(penta);
    }

    return result;
}

bool isCanonicalPentaSymbol(std::u16string_view symbol, std::int32_t cipher) noexcept {
    const auto encoded = encodePentaSymbol(symbol);

    return cipher == (encoded ? encoded->cipher : 0);
}
} // namespace org::ttldtor::qdcxx::encoding
