// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/schedule/schedule.hpp"

#include <optional>

namespace org::ttldtor::qdcxx::schedule::conformance {
/** Internal caller: schedule-query conformance tests and actual public-value observation consumers.
 * Reads six-cell oracle inputs and projects actual public query values; no schedule algorithm is reimplemented.
 * Returns owning observation text; malformed fixture syntax throws. Product failures remain typed markers.
 */
class QueryProjection {
    ScheduleTimeZoneData data_;
    std::optional<ScheduleDataContext> context_;
    std::int64_t reference_ = 0;
    /** @param reference Explicit model-constructor reference.
     * @return Shared fixture context for this exact reference; no returned query model is retained here.
     */
    const ScheduleDataContext &context(std::int64_t reference);

    public:
    /** @param data Ready explicit pinned provider capability to share. */
    explicit QueryProjection(ScheduleTimeZoneData data);
    /** @param line One exact six-field case TSV row.
     * @return Actual query observation; RAW observes the stored constructor raw component only.
     */
    std::string observe(std::string_view line);
};

/** @param text Borrowed delimiter-separated fixture cells.
 * @param delimiter Literal separator.
 * @return Borrowed cells, including trailing empty cells; caller text must remain alive.
 */
std::vector<std::string_view> splitCells(std::string_view text, char delimiter);
/** @param text Exact UTF-16 hexadecimal fixture input.
 * @return Owning decoded units; malformed fixture syntax throws.
 */
std::u16string decodeHex(std::string_view text);
} // namespace org::ttldtor::qdcxx::schedule::conformance
