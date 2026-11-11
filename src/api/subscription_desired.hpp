// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "feed_internal.hpp"

namespace org::ttldtor::qdcxx::api::detail {
/** Commit one complete accepted desired cut after every fallible preparation succeeds.
 * Caller holds the change-domain admission and port mutex, and has validated lifecycle and capacity.
 * @param port Detached logical owner.
 * @param symbols Complete effective candidate, transferred only at successful commit.
 * @param fromTime Candidate range, absent for ordinary subscriptions.
 * @return False for identical demand; Error leaves the old symbols, range, revision and observers unchanged.
 */
bool commitSubscriptionDesiredLocked(SubscriptionPort &port, std::vector<SubscriptionSelector> symbols,
                                     std::optional<std::int64_t> fromTime);
/** Prepare and commit one range replacement using the same transaction as ordinary symbol mutation.
 * Caller holds change-domain admission and port mutex and has checked lifecycle.
 * @param port Time-series-mode detached owner.
 * @param fromTime Signed Unix milliseconds, including MAX disabled sentinel.
 * @return False for unchanged range; refusal preserves the complete old desired cut.
 */
bool replaceSubscriptionFromTimeLocked(SubscriptionPort &port, std::int64_t fromTime);
} // namespace org::ttldtor::qdcxx::api::detail
