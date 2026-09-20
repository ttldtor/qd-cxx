#include "qd/encoding/compact.hpp"

#include <bit>

namespace qd::encoding {

ReadResult<std::int64_t> readCompactLong(std::span<const std::byte> input) noexcept {
    if (input.empty()) {
        return {0, 0, Status::INSUFFICIENT_INPUT};
    }

    const auto first = std::to_integer<unsigned int>(input[0]);
    unsigned int leading = 0;

    while (leading < 8 && (first & (0x80U >> leading)) != 0) {
        ++leading;
    }

    const auto size = static_cast<std::size_t>(leading + 1);

    if (input.size() < size) {
        return {0, input.size(), Status::INSUFFICIENT_INPUT};
    }

    std::uint64_t bits = leading < 7 ? first & (0x7fU >> leading) : 0;

    for (std::size_t index = 1; index < size; ++index) {
        bits = (bits << 8) | std::to_integer<unsigned int>(input[index]);
    }

    const unsigned int width = leading == 8 ? 64 : 7 * leading + 7;

    if (width < 64 && (bits & (std::uint64_t{1} << (width - 1))) != 0) {
        bits |= (~std::uint64_t{0}) << width;
    }

    return {std::bit_cast<std::int64_t>(bits), size, Status::OK};
}

ReadResult<std::int32_t> readCompactInt(std::span<const std::byte> input) noexcept {
    const auto result = readCompactLong(input);
    const auto lowBits = static_cast<std::uint32_t>(result.value);

    return {std::bit_cast<std::int32_t>(lowBits), result.consumed, result.status};
}

WriteResult writeCompactLong(std::int64_t value, std::span<std::byte> output) noexcept {
    std::size_t size = 1;

    for (; size < 9; ++size) {
        const auto limit = std::int64_t{1} << (7 * size - 1);

        if (value >= -limit && value < limit) {
            break;
        }
    }

    if (output.size() < size) {
        return {0, Status::INSUFFICIENT_OUTPUT};
    }

    auto bits = static_cast<std::uint64_t>(value);

    for (std::size_t index = size - 1; index > 0; --index) {
        output[index] = static_cast<std::byte>(bits & 0xffU);
        bits >>= 8;
    }

    const auto prefix = size == 9 ? 0xffU : (0xffU << (9 - size)) & 0xffU;
    const auto mask = size < 8 ? 0x7fU >> (size - 1) : 0;
    output[0] = static_cast<std::byte>(prefix | (bits & mask));

    return {size, Status::OK};
}

WriteResult writeCompactInt(std::int32_t value, std::span<std::byte> output) noexcept {
    return writeCompactLong(value, output);
}

} // namespace qd::encoding
