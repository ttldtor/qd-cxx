// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/encoding/symbol_reader.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::encoding {
namespace {
/// Wire discriminator boundaries and standalone prefixes from the pinned PentaCodec grammar.
constexpr unsigned PENTA_30 = 0x80, RESERVED_FIRST = 0xc0, PENTA_20 = 0xe0, PENTA_35 = 0xf0;
constexpr unsigned MRU_FLAGS = 0xf8, NEW_FLAGS = 0xf9, UTF_BYTES = 0xfc, CESU_UNITS = 0xfd;
constexpr unsigned REPEAT = 0xff;
/// Penta groups have five bits; 30 and 31 introduce an extended ten-bit character.
constexpr unsigned GROUP_BITS = 5, GROUP_MASK = 31, EXTENDED_FIRST = 30, EXTENDED_MASK = 1023;
constexpr unsigned EXTENDED_BASE = 960, SLASH_CODE = 28, DOLLAR_CODE = 29;
/// Cipher encoding reserves high bits for ordinary, slash-prefixed and dollar-prefixed pentas.
constexpr unsigned CIPHER_BITS = 30;
constexpr std::uint32_t CIPHER_BASE = 0x40000000, CIPHER_LOW_MASK = 0x3fffffff;
/// Symbol string lengths use a stricter nonnegative int domain than UTFString.
constexpr std::int64_t MAX_SYMBOL_LENGTH = 2147483647;
/// FD characters must fit one UTF-16 unit, even when their encoding uses four bytes.
constexpr unsigned BMP_MAX = 0xffff;
/// Every fixed penta tail is big endian in eight-bit octets.
constexpr unsigned OCTET_BITS = 8;

/** Construct the small immutable penta-to-ASCII lookup, with zero for undefined entries.
 * @return Exact pinned character mapping; this table is not a mutable symbol cache.
 */
constexpr auto characterTable() {
    std::array<char16_t, EXTENDED_MASK + 1> table{};

    for (unsigned c = 'A'; c <= 'Z'; ++c) {
        table[c - 'A' + 1] = static_cast<char16_t>(c);
    }

    table[27] = u'.'; // The three punctuation codes follow A..Z.
    table[SLASH_CODE] = u'/';
    table[DOLLAR_CODE] = u'$';
    unsigned code = EXTENDED_BASE;

    for (unsigned c = 32; c <= 126; ++c) { // Printable ASCII, excluding already coded and ineligible characters.
        if ((c >= 'A' && c <= 'Z') || c == '.' || c == '/' || c == '$' || c == '\'' || c == '`') {
            continue;
        }

        table[code++] = static_cast<char16_t>(c);
    }

    return table;
}

/// Constant lookup shared across readers; no runtime cache or resolver.
constexpr auto CHARACTERS = characterTable();

/** Map an existing bounded primitive outcome to the symbol API.
 * @param status Primitive outcome.
 * @return Corresponding symbol classification; output-only failures map to invalid encoding.
 */
SymbolStatus symbolStatus(Status status) noexcept {
    switch (status) {
    case Status::OK:
        return SymbolStatus::OK;
    case Status::INSUFFICIENT_INPUT:
        return SymbolStatus::TRUNCATED_INPUT;
    case Status::INVALID_LENGTH:
        return SymbolStatus::INVALID_LENGTH;
    case Status::RESOURCE_LIMIT:
        return SymbolStatus::RESOURCE_LIMIT;
    default:
        return SymbolStatus::INVALID_ENCODING;
    }
}

/** Read one FD character with Java's full-tail-before-validation consumption.
 * @param input Bounded complete-message remainder.
 * @param position Current position, advanced over examined bytes.
 * @param point Receives a BMP value only on success.
 * @return OK, truncation or invalid encoding, including decoded values above BMP.
 */
SymbolStatus readCesuUnit(std::span<const std::byte> input, std::size_t &position, char16_t &point) noexcept {
    const auto character = readUtfChar(input.subspan(position));
    position += character.consumed;

    if (character.status != Status::OK) {
        return symbolStatus(character.status);
    }

    if (static_cast<std::uint32_t>(character.value) > BMP_MAX) {
        return SymbolStatus::INVALID_ENCODING;
    }

    point = static_cast<char16_t>(character.value);

    return SymbolStatus::OK;
}
} // namespace

SymbolReader::SymbolReader(SymbolLimits limits, std::uint32_t optionMask) noexcept : limits_(limits) {
    (void)beginMessage(optionMask);
}

SymbolStatus SymbolReader::beginMessage(std::uint32_t optionMask) noexcept {
    previous_.reset();
    mruFlags_ = 1;
    constexpr std::uint32_t HISTORY_SNAPSHOT_OPTION_MASK = 1;
    state_ = (optionMask & ~HISTORY_SNAPSHOT_OPTION_MASK) == 0 ? SymbolStatus::OK : SymbolStatus::UNSUPPORTED_PROFILE;

    return state_;
}

namespace {
/** Decode the shared symbol grammar with allocator-preserving owned value types.
 * @tparam Value Ordinary or explicit-resource symbol owner.
 * @tparam Result Corresponding complete primitive result.
 * @param input Borrowed remaining message payload.
 * @param limits Finite primitive bounds.
 * @param previous Message-local previous symbol, published only on success.
 * @param mruFlags Message-local MRU flags, published only on success.
 * @param state Terminal state updated on failure.
 * @param value Empty symbol carrying the selected allocator.
 * @return Owned complete symbol or exact failure with no partial current value.
 */
template <typename Value, typename Result>
Result readSymbol(std::span<const std::byte> input, SymbolLimits limits, std::optional<Value> &previous,
                  std::int32_t &mruFlags, SymbolStatus &state, Value value) {
    if (state != SymbolStatus::OK) {
        return {std::nullopt, 0, state};
    }

    std::size_t position = 0;
    /** Mark this message terminal without publishing tentative state.
     * @param status Error to report for this call.
     * @return Failure result at the current committed examination position.
     */
    const auto fail = [&](SymbolStatus status) -> Result {
        state = SymbolStatus::FAILED;

        return {std::nullopt, position, status};
    };

    if (input.size() > limits.inputBytes) {
        return fail(SymbolStatus::RESOURCE_LIMIT);
    }

    const auto unitLimit = std::min(limits.codeUnits, limits.stateUnits);
    auto nextMru = mruFlags;
    try {
        if constexpr (std::is_same_v<Value, PmrSymbolValue>) {
            constexpr std::size_t MAX_PENTA_UNITS = 35 / GROUP_BITS;
            const auto previousUnits = previous ? previous->units.size() : 0;
            value.units.reserve(std::min(unitLimit, std::max({input.size(), previousUnits, MAX_PENTA_UNITS})));
        }

        unsigned prefix;

        while (true) {
            if (position == input.size()) {
                return fail(SymbolStatus::TRUNCATED_INPUT);
            }

            prefix = std::to_integer<unsigned>(input[position++]);

            if (prefix != MRU_FLAGS && prefix != NEW_FLAGS) {
                break;
            }

            if (value.eventFlagsBytes != 0) {
                return fail(SymbolStatus::INVALID_ENCODING);
            }

            if (prefix == MRU_FLAGS) {
                value.eventFlags = nextMru;
                value.eventFlagsBytes = 1;
            } else {
                const auto flags = readCompactInt(input.subspan(position));
                position += flags.consumed;

                if (flags.status != Status::OK) {
                    return fail(symbolStatus(flags.status));
                }

                nextMru = flags.value;
                value.eventFlags = nextMru;
                value.eventFlagsBytes = flags.consumed;
            }
        }

        if ((prefix >= RESERVED_FIRST && prefix < PENTA_20) || (prefix > NEW_FLAGS && prefix < UTF_BYTES)) {
            return fail(SymbolStatus::INVALID_ENCODING);
        }

        if (prefix == REPEAT) {
            if (!previous) {
                return fail(SymbolStatus::INVALID_ENCODING);
            }

            if (previous->units.size() > unitLimit) {
                return fail(SymbolStatus::RESOURCE_LIMIT);
            }

            value.units = previous->units;
            value.cipher = previous->cipher;
        } else if (prefix == UTF_BYTES || prefix == CESU_UNITS) {
            const auto lengthStart = position;
            const auto length = readCompactLong(input.subspan(position));
            position += length.consumed;

            if (length.status != Status::OK) {
                return fail(symbolStatus(length.status));
            }

            if (length.value < 0 || length.value > MAX_SYMBOL_LENGTH) {
                return fail(SymbolStatus::INVALID_LENGTH);
            }

            const auto count = static_cast<std::uint64_t>(length.value);

            if (prefix == UTF_BYTES) {
                if (count > limits.inputBytes) {
                    return fail(SymbolStatus::RESOURCE_LIMIT);
                }

                position = lengthStart; // A throwing delegated primitive does not commit its examination.
                auto decoded = [&] {
                    if constexpr (std::is_same_v<Value, SymbolValue>) {
                        return readUtfString(input.subspan(lengthStart), {limits.inputBytes, unitLimit});
                    } else {
                        return readUtfString(input.subspan(lengthStart), {limits.inputBytes, unitLimit},
                                             *value.units.get_allocator().resource());
                    }
                }();
                position += decoded.consumed;

                if (decoded.status != Status::OK) {
                    return fail(symbolStatus(decoded.status));
                }

                value.units = std::move(*decoded.value);
            } else {
                if (count > unitLimit) {
                    return fail(SymbolStatus::RESOURCE_LIMIT);
                }

                for (std::uint64_t i = 0; i < count; ++i) {
                    char16_t point{};
                    const auto status = readCesuUnit(input, position, point);

                    if (status != SymbolStatus::OK) {
                        return fail(status);
                    }

                    value.units.push_back(point);
                }
            }
        } else {
            unsigned tail = 0;
            std::uint64_t penta = 0;

            if (prefix < PENTA_30) {
                penta = prefix;
                tail = 1;
            } else if (prefix < RESERVED_FIRST) {
                penta = prefix & 0x3fU;
                tail = 3; // Three tail bytes after six prefix payload bits.
            } else if (prefix < PENTA_35) {
                penta = prefix & 0x0fU;
                tail = 2; // Two tail bytes after four prefix payload bits.
            } else if (prefix < MRU_FLAGS) {
                penta = prefix & 0x07U;
                tail = 4; // Four tail bytes after three prefix payload bits.
            }

            // FE has no tail and means zero penta.
            if (input.size() - position < tail) {
                position = input.size();

                return fail(SymbolStatus::TRUNCATED_INPUT);
            }

            for (unsigned i = 0; i < tail; ++i) {
                penta = (penta << OCTET_BITS) | std::to_integer<unsigned>(input[position++]);
            }

            unsigned bits = 0;

            while ((penta >> bits) != 0) {
                bits += GROUP_BITS;
            }

            std::uint32_t cipher = 0;

            if (bits <= CIPHER_BITS) {
                cipher = static_cast<std::uint32_t>(penta) + CIPHER_BASE;
            } else if ((penta >> CIPHER_BITS) == SLASH_CODE || (penta >> CIPHER_BITS) == DOLLAR_CODE) {
                const auto kind = (penta >> CIPHER_BITS) == SLASH_CODE ? 2U : 3U;
                cipher = (kind << CIPHER_BITS) | (static_cast<std::uint32_t>(penta) & CIPHER_LOW_MASK);
            }

            value.cipher = std::bit_cast<std::int32_t>(cipher);

            while (bits != 0) {
                bits -= GROUP_BITS;
                auto code = static_cast<unsigned>((penta >> bits) & GROUP_MASK);

                if (code >= EXTENDED_FIRST && bits != 0) {
                    bits -= GROUP_BITS;
                    code = static_cast<unsigned>((penta >> bits) & EXTENDED_MASK);
                }

                if (value.units.size() == unitLimit) {
                    return fail(SymbolStatus::RESOURCE_LIMIT);
                }

                value.units.push_back(CHARACTERS[code]);
            }
        }

        // Copy before publishing: if it throws, previous state remains intact and the message fails.
        Result result{value, position, SymbolStatus::OK};
        previous = std::move(value);
        mruFlags = nextMru;

        return result;
    } catch (const std::bad_alloc &) {
        return fail(SymbolStatus::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return fail(SymbolStatus::ALLOCATION_FAILURE);
    }
}
} // namespace

SymbolResult SymbolReader::read(std::span<const std::byte> input) {
    return readBounded(input, limits_.codeUnits);
}

SymbolResult SymbolReader::readBounded(std::span<const std::byte> input, std::size_t unitLimit) {
    auto limits = limits_;

    limits.codeUnits = std::min(limits.codeUnits, unitLimit);

    return readSymbol<SymbolValue, SymbolResult>(input, limits, previous_, mruFlags_, state_, {{}, 0, 0, 0});
}

PmrSymbolReader::PmrSymbolReader(SymbolLimits limits, std::uint32_t optionMask,
                                 std::pmr::memory_resource &resource) noexcept
    : limits_(limits), resource_(&resource) {
    (void)beginMessage(optionMask);
}

SymbolStatus PmrSymbolReader::beginMessage(std::uint32_t optionMask) noexcept {
    previous_.reset();
    mruFlags_ = 1;
    constexpr std::uint32_t HISTORY_SNAPSHOT_OPTION_MASK = 1;
    state_ = (optionMask & ~HISTORY_SNAPSHOT_OPTION_MASK) == 0 ? SymbolStatus::OK : SymbolStatus::UNSUPPORTED_PROFILE;

    return state_;
}

PmrSymbolResult PmrSymbolReader::read(std::span<const std::byte> input) {
    return readSymbol<PmrSymbolValue, PmrSymbolResult>(input, limits_, previous_, mruFlags_, state_,
                                                       PmrSymbolValue{*resource_});
}
} // namespace org::ttldtor::qdcxx::encoding
