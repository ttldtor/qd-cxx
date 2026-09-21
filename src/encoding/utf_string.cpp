#include "qd/encoding/utf_string.hpp"

#include <array>
#include <limits>
#include <utility>

namespace qd::encoding {
namespace {
/// Maximum encoded body length allowed by the locked Java length check.
constexpr std::uint64_t MAX_BODY_BYTES = 8589934588ULL;

/** Read one writer character, combining only a valid adjacent surrogate pair.
 * @param value Borrowed input units.
 * @param index Valid unit index, advanced over one unit or a pair.
 * @return Code point or unchanged isolated surrogate unit.
 */
std::uint32_t nextCharacter(std::u16string_view value, std::size_t &index) noexcept {
    std::uint32_t point = value[index++];

    if (point >= 0xd800 && point <= 0xdbff && index < value.size() && value[index] >= 0xdc00 &&
        value[index] <= 0xdfff) {
        point = 0x10000 + ((point - 0xd800) << 10) + (value[index++] - 0xdc00);
    }

    return point;
}

/** Count canonical body bytes for a code point or isolated surrogate.
 * @param point Value returned by nextCharacter.
 * @return One through four encoded bytes.
 */
std::size_t characterSize(std::uint32_t point) noexcept {
    return point < 0x80 ? 1 : point < 0x800 ? 2 : point < 0x10000 ? 3 : 4;
}
} // namespace

UtfStringResult readUtfString(std::span<const std::byte> input, UtfStringLimits limits) {
    const auto prefix = readCompactLong(input);
    auto position = prefix.consumed;

    if (prefix.status != Status::OK) {
        return {std::nullopt, position, prefix.status};
    }

    if (prefix.value < -1 || prefix.value > static_cast<std::int64_t>(MAX_BODY_BYTES)) {
        return {std::nullopt, position, Status::INVALID_LENGTH};
    }

    if (prefix.value == -1) {
        return {std::nullopt, position, Status::OK};
    }

    if (static_cast<std::uint64_t>(prefix.value) > limits.bodyBytes) {
        return {std::nullopt, position, Status::RESOURCE_LIMIT};
    }

    auto remaining = prefix.value;
    std::u16string value;

    while (remaining > 0) {
        if (position == input.size()) {
            return {std::nullopt, position, Status::INSUFFICIENT_INPUT};
        }

        const auto first = std::to_integer<unsigned int>(input[position++]);
        const std::size_t size = first < 0x80             ? 1
                                 : (first & 0xe0) == 0xc0 ? 2
                                 : (first & 0xf0) == 0xe0 ? 3
                                 : (first & 0xf8) == 0xf0 ? 4
                                                          : 0;

        if (size == 0) {
            return {std::nullopt, position, Status::INVALID_ENCODING};
        }

        // Java reads the entire tail before checking any continuation bits.
        if (input.size() - position < size - 1) {
            return {std::nullopt, input.size(), Status::INSUFFICIENT_INPUT};
        }

        std::uint32_t point = first & (size == 1 ? 0x7f : 0x7f >> size);
        bool valid = true;

        for (std::size_t index = 1; index < size; ++index) {
            const auto tail = std::to_integer<unsigned int>(input[position++]);
            valid = valid && (tail & 0xc0) == 0x80;
            point = (point << 6) | (tail & 0x3f);
        }

        if (!valid || point > 0x10ffff) {
            return {std::nullopt, position, Status::INVALID_ENCODING};
        }

        const std::size_t units = point >= 0x10000 ? 2 : 1;

        if (units > limits.codeUnits - value.size()) {
            return {std::nullopt, position, Status::RESOURCE_LIMIT};
        }

        if (units == 2) {
            value.push_back(static_cast<char16_t>(0xd800 + ((point - 0x10000) >> 10)));
            value.push_back(static_cast<char16_t>(0xdc00 + ((point - 0x10000) & 0x3ff)));
        } else {
            value.push_back(static_cast<char16_t>(point));
        }

        remaining -= static_cast<std::int64_t>(size);
    }

    if (remaining < 0) {
        return {std::nullopt, position, Status::INVALID_ENCODING};
    }

    return {std::move(value), position, Status::OK};
}

WriteResult writeUtfString(std::optional<std::u16string_view> value, std::span<std::byte> output,
                           UtfStringLimits limits) noexcept {
    std::uint64_t bodySize = 0;

    if (value) {
        if (value->size() > MAX_BODY_BYTES) {
            return {0, Status::INVALID_LENGTH};
        }

        if (value->size() > limits.codeUnits) {
            return {0, Status::RESOURCE_LIMIT};
        }

        for (std::size_t index = 0; index < value->size();) {
            const auto size = characterSize(nextCharacter(*value, index));

            if (size > MAX_BODY_BYTES - bodySize) {
                return {0, Status::INVALID_LENGTH};
            }

            bodySize += size;
        }

        if (bodySize > limits.bodyBytes) {
            return {0, Status::RESOURCE_LIMIT};
        }
    }

    std::array<std::byte, 9> prefix{};
    const auto encoded = writeCompactLong(value ? static_cast<std::int64_t>(bodySize) : -1, prefix);

    if (bodySize > std::numeric_limits<std::size_t>::max() - encoded.written || output.size() < encoded.written ||
        bodySize > output.size() - encoded.written) {
        return {0, Status::INSUFFICIENT_OUTPUT};
    }

    std::size_t position = 0;

    for (; position < encoded.written; ++position) {
        output[position] = prefix[position];
    }

    if (value) {
        for (std::size_t index = 0; index < value->size();) {
            const auto point = nextCharacter(*value, index);
            const auto size = characterSize(point);

            if (size == 1) {
                output[position++] = static_cast<std::byte>(point);
            } else {
                output[position++] = static_cast<std::byte>((0xffU << (8 - size)) | (point >> (6 * (size - 1))));

                for (std::size_t tail = size - 1; tail > 0; --tail) {
                    output[position++] = static_cast<std::byte>(0x80 | ((point >> (6 * (tail - 1))) & 0x3f));
                }
            }
        }
    }

    return {position, Status::OK};
}
} // namespace qd::encoding
