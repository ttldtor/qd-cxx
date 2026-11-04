// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/event_value.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Encode the existing byte-valued SHORT_STRING business representation.
 * @param text Borrowed nullable UTF-16 input; at most eight units, each at most 0xFF, including zero units.
 * No input storage is retained. Null, empty and all-zero input encode to zero; embedded zero units are omitted.
 * @return Signed64 packed nonzero bytes in source order, or SHORT_STRING_TOO_LONG/INVALID_SHORT_STRING_CHARACTER.
 */
[[nodiscard]] std::expected<std::int64_t, EventValueError>
encodeShortString(std::optional<std::u16string_view> text) noexcept;
/** Decode every raw signed64 SHORT_STRING pattern without a cache or encoding conversion.
 * @param code Packed bytes; all zero bytes are omitted, including internal ones.
 * @return Null for zero, otherwise independently owned one-to-eight byte-valued UTF-16 units.
 * @throws std::bad_alloc Output allocation fails; no result escapes.
 * @throws std::length_error Output exceeds string capacity.
 */
[[nodiscard]] std::optional<std::u16string> decodeShortString(std::int64_t code);
} // namespace org::ttldtor::qdcxx::events
