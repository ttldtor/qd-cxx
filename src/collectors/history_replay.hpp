// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/collectors/history.hpp"

namespace org::ttldtor::qdcxx::collectors::detail {
/** Owner-only recovery of an obsolete selected delivery, using the collector's existing fixed candidate bank. */
struct HistoryReplayAccess {
    /** Replace one view's desired cut and restart all its current initial cursors atomically.
     * @param owner Actual serialized collector; no sibling view or source epoch is reset.
     * @param view Current validated view identity.
     * @param requests Complete borrowed desired replacement, copied before publication.
     * @param coreWork Remaining whole-turn desired credits.
     * @param work Remaining whole-turn source clone/replay/disposal credits; no refill.
     * @return Original desired barrier or typed failure; failure before commit leaves the view unchanged.
     */
    static std::expected<core::SubscriptionChange, HistoryFailure>
    replace(HistoryCollector &owner, const HistoryViewToken &view, std::span<const core::SubscriptionRequest> requests,
            core::SubscriptionWork &coreWork, HistoryWork &work);
};
} // namespace org::ttldtor::qdcxx::collectors::detail
