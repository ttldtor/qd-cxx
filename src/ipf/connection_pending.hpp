// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "profile_access.hpp"
#include <vector>

namespace org::ttldtor::qdcxx::ipf::connection_detail {
/** Copy into pre-reserved pending slots only after whole requested-backing admission.
 * The caller separately admits logical profile/unit/work limits and keeps the input immutable.
 * On allocation failure both the existing vector prefix and used-byte reservation are unchanged.
 * @param values Worker-owned vector; this function never permits capacity growth.
 * @param profile Stable source borrowed through copy construction.
 * @param used Aggregate backing of all retained pending copies, released after their destruction.
 * @param maximum Available batch-copy bytes within the original resident allowance.
 * @return True after one whole copy, false before allocation/growth on capacity/backing refusal.
 * @throws std::bad_alloc Copy allocation fails after admission; all partial storage is retired.
 * @throws std::length_error Supported string/container copy cannot represent its source.
 */
inline bool appendPendingProfile(std::vector<InstrumentProfile> &values, const InstrumentProfile &profile,
                                 std::size_t &used, std::size_t maximum) {
    if (values.size() == values.capacity()) {
        return false;
    }

    const auto bytes = collector_detail::CollectorProfileAccess::copyBacking(profile);

    if (!bytes || used > maximum || *bytes > maximum - used) {
        return false;
    }

    used += *bytes;
    try {
        values.push_back(profile);
    } catch (...) {
        used -= *bytes;
        throw;
    }

    return true;
}
} // namespace org::ttldtor::qdcxx::ipf::connection_detail
