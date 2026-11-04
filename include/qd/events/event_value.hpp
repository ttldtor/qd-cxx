// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

namespace org::ttldtor::qdcxx::events {
/** Allocation-free input errors for the event value conversions. */
enum class EventValueError {
    INVALID_ENUM_CODE,             ///< Code lies outside the corresponding Java bit-pattern table.
    INVALID_SEQUENCE,              ///< Sequence is outside the unsigned 22-bit domain.
    INVALID_MONTH,                 ///< Gregorian month is outside 1 through 12.
    SHORT_STRING_TOO_LONG,         ///< Input contains more than eight UTF-16 units, including zero units.
    INVALID_SHORT_STRING_CHARACTER ///< A UTF-16 unit exceeds the byte-valued SHORT_STRING domain.
};
} // namespace org::ttldtor::qdcxx::events
