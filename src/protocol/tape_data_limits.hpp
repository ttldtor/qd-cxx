// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/stream_data.hpp"

#include <array>

namespace org::ttldtor::qdcxx::protocol::detail {
/** Data allowances charged cumulatively across tape envelopes as well as against an optional per-frame limit. */
inline constexpr std::array TAPE_DATA_TOTALS{
    &StreamDataLimits::processedRecords,    &StreamDataLimits::wireFields,
    &StreamDataLimits::emittedRecords,      &StreamDataLimits::emittedValues,
    &StreamDataLimits::ownedSymbolUnits,    &StreamDataLimits::fieldBytes,
    &StreamDataLimits::fieldUnits,          &StreamDataLimits::ownedFieldUnits,
    &StreamDataLimits::payloadEncodedBytes, &StreamDataLimits::payloadWorkBytes,
    &StreamDataLimits::ownedPayloadBytes,   &StreamDataLimits::ownedFieldTimeRecords};
} // namespace org::ttldtor::qdcxx::protocol::detail
