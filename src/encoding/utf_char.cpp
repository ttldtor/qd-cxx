// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/encoding/utf_char.hpp"

#include <array>

namespace org::ttldtor::qdcxx::encoding {
namespace {
/// UTF_CHAR accepts Unicode points including the surrogate range.
constexpr std::uint32_t MAX_CODE_POINT = 0x10ffff;
/// First code points requiring two, three and four bytes respectively.
constexpr std::uint32_t ASCII_LIMIT = 0x80;
constexpr std::uint32_t THREE_BYTE_MIN = 0x800;
constexpr std::uint32_t FOUR_BYTE_MIN = 0x10000;
/// Continuations carry six payload bits after the 10 prefix.
constexpr unsigned int CONTINUATION_BITS = 6;
constexpr unsigned int CONTINUATION_MASK = 0xc0;
constexpr unsigned int CONTINUATION_PREFIX = 0x80;
constexpr unsigned int CONTINUATION_PAYLOAD_MASK = 0x3f;
/// Lead classification by total byte count; index zero is unused.
constexpr std::array<unsigned int, 5> LEAD_MASKS{0, 0x80, 0xe0, 0xf0, 0xf8};
constexpr std::array<unsigned int, 5> LEAD_PREFIXES{0, 0, 0xc0, 0xe0, 0xf0};
constexpr std::array<unsigned int, 5> LEAD_PAYLOAD_MASKS{0, 0x7f, 0x1f, 0x0f, 0x07};
} // namespace

ReadResult<std::int32_t> readUtfChar(std::span<const std::byte> input) noexcept {
    if (input.empty()) {
        return {0, 0, Status::INSUFFICIENT_INPUT};
    }

    const auto lead = std::to_integer<unsigned int>(input[0]);
    std::size_t size = 1;

    while (size < LEAD_MASKS.size() && (lead & LEAD_MASKS[size]) != LEAD_PREFIXES[size]) {
        ++size;
    }

    if (size == LEAD_MASKS.size()) {
        return {0, 1, Status::INVALID_ENCODING};
    }

    if (input.size() < size) {
        return {0, input.size(), Status::INSUFFICIENT_INPUT};
    }

    auto point = lead & LEAD_PAYLOAD_MASKS[size];

    for (std::size_t index = 1; index < size; ++index) {
        const auto tail = std::to_integer<unsigned int>(input[index]);

        if ((tail & CONTINUATION_MASK) != CONTINUATION_PREFIX) {
            return {0, size, Status::INVALID_ENCODING};
        }

        point = (point << CONTINUATION_BITS) | (tail & CONTINUATION_PAYLOAD_MASK);
    }

    if (point > MAX_CODE_POINT) {
        return {0, size, Status::INVALID_ENCODING};
    }

    return {static_cast<std::int32_t>(point), size, Status::OK};
}

WriteResult writeUtfChar(std::int32_t value, std::span<std::byte> output) noexcept {
    if (value < 0 || static_cast<std::uint32_t>(value) > MAX_CODE_POINT) {
        return {0, Status::INVALID_ENCODING};
    }

    auto point = static_cast<std::uint32_t>(value);
    const std::size_t size = point < ASCII_LIMIT ? 1 : point < THREE_BYTE_MIN ? 2 : point < FOUR_BYTE_MIN ? 3 : 4;

    if (output.size() < size) {
        return {0, Status::INSUFFICIENT_OUTPUT};
    }

    for (auto index = size - 1; index > 0; --index) {
        output[index] = static_cast<std::byte>(CONTINUATION_PREFIX | (point & CONTINUATION_PAYLOAD_MASK));
        point >>= CONTINUATION_BITS;
    }

    output[0] = static_cast<std::byte>(LEAD_PREFIXES[size] | point);

    return {size, Status::OK};
}

} // namespace org::ttldtor::qdcxx::encoding
