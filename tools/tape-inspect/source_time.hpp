// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/tape_reader.hpp"
#include <cstddef>
#include <optional>

namespace org::ttldtor::qdcxx::tape_inspect {
/** Explicit homogeneous time declaration; ordinary record fields never select it. */
enum class SourceTimePolicy {
    NONE,    ///< Suppress effective time.
    MESSAGE, ///< Timestamp-bearing heartbeat assignment.
    FIELD,   ///< Physical FIELD metadata on every emitted record.
    LONG,    ///< Explicit LONG companion marks.
    TEXT     ///< Explicit TEXT companion marks.
};

/** Provenance of the last external assignment, including an assigned zero. */
enum class ExternalTimeOrigin {
    NONE,    ///< No external timestamp has been assigned.
    MESSAGE, ///< A timestamp-bearing heartbeat assigned the cursor.
    SIDECAR  ///< A reached companion mark assigned the cursor.
};

/** Fixed owning record-time selection; no schema, path, input span or callback is retained. */
struct SourceRecordTiming {
    SourceTimePolicy policy = SourceTimePolicy::NONE; ///< Explicit source declaration.
    std::size_t sourceOrdinal = 0;                    ///< Zero-based source occurrence, independent of FIELD ordinal.
    std::optional<protocol::TapeTiming> external;     ///< Exact assigned timestamp and separately packed cursor.
    ExternalTimeOrigin externalOrigin = ExternalTimeOrigin::NONE; ///< Assignment provenance, even for packed zero.
};
} // namespace org::ttldtor::qdcxx::tape_inspect
