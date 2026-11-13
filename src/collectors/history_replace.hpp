// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../core/subscription_state_access.hpp"
#include "qd/collectors/history.hpp"

namespace org::ttldtor::qdcxx::collectors::detail {
/** Owner-only staged replacement. Keep the candidate inside one uninterrupted owner turn. */
struct HistoryReplaceAccess {
    class Candidate {
        friend struct HistoryReplaceAccess;
        HistoryCollector *owner_;
        HistoryViewToken view_;
        core::detail::SubscriptionStateAccess::PreparedReplace desired_;
        bool publishBank_;

        Candidate(HistoryCollector &owner, const HistoryViewToken &view,
                  core::detail::SubscriptionStateAccess::PreparedReplace desired, bool publishBank) noexcept;

        public:
        Candidate(Candidate &&other) noexcept;
        ~Candidate();
    };

    /** Prepare the complete desired and history metadata before any participating owner publishes. */
    [[nodiscard]] static std::expected<Candidate, HistoryFailure>
    prepare(HistoryCollector &owner, const HistoryViewToken &view, std::span<const core::SubscriptionRequest> requests,
            core::SubscriptionWork &coreWork, HistoryWork &work, bool restartInitial);

    /** Borrow the fully reconciled desired entries from the prepared replacement. */
    [[nodiscard]] static std::span<const core::SubscriptionEntry> entries(const Candidate &candidate) noexcept;

    /** Return the desired revision that the prepared mappings will reference. */
    [[nodiscard]] static std::uint64_t revision(const Candidate &candidate) noexcept;

    /** Capture the unchanged source epoch for all mappings in the prepared replacement. */
    [[nodiscard]] static HistoryEpochToken epoch(const Candidate &candidate) noexcept;

    /** Publish a candidate from the same unmodified collector, consuming its desired reservation. */
    [[nodiscard]] static core::SubscriptionChange commit(HistoryCollector &owner, Candidate candidate) noexcept;
};
} // namespace org::ttldtor::qdcxx::collectors::detail
