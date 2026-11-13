// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/mux/interest.hpp"

namespace org::ttldtor::qdcxx::mux::detail {

/** Serialized owner-only access to a staged fixed-bank replacement. */
struct MuxStateAccess {
    using PreparedReplace = MuxState::PreparedChange; ///< Opaque identity and inactive-bank publication token.

    /** Prepare a replacement without changing the published bank.
     * @param owner Serialized mutable interest owner.
     * @param client Exact local interest client.
     * @param requests Borrowed input, copied before return.
     * @param work Actual remaining turn credits.
     * @return Candidate token, or typed refusal preserving the published bank.
     */
    [[nodiscard]] static std::expected<PreparedReplace, MuxError>
    prepareReplace(MuxState &owner, const MuxClient &client, std::span<const core::SubscriptionRequest> requests,
                   MuxWork &work) {
        return owner.prepare(MuxState::Command::REPLACE, client, requests, {}, work);
    }

    /** Borrow one client's candidate view before publication. */
    [[nodiscard]] static auto entries(const MuxState &owner, const PreparedReplace &candidate,
                                      const MuxClient &client) noexcept {
        return owner.preparedEntries(candidate, client);
    }

    /** Borrow the candidate union used by STREAM. */
    [[nodiscard]] static auto aggregate(const MuxState &owner, const PreparedReplace &candidate) noexcept {
        return owner.preparedAggregate(candidate);
    }

    /** Check original queued projection activations against the prepared replacement. */
    [[nodiscard]] static std::expected<core::DeliveryProjectionMask, MuxError>
    projectionMask(const MuxState &owner, const PreparedReplace &candidate, const MuxClient &client,
                   const core::CapturedDeliveryProjections<MuxMembership> &captured, MuxWork &work) {
        if (!owner.current(candidate))
            return std::unexpected(MuxError::INVALID_STATE);

        return owner.currentProjectionMaskInBank(client, captured, work, candidate.changed);
    }

    /** Publish the candidate only if the owner and inactive bank still match.
     * @param owner Serialized mutable interest owner.
     * @param candidate Exact successful preparation from this owner.
     * @return Local publication outcome, or typed stale candidate refusal.
     */
    [[nodiscard]] static std::expected<MuxChange, MuxError> commitReplace(MuxState &owner,
                                                                          const PreparedReplace &candidate) noexcept {
        return owner.commit(candidate);
    }
};

} // namespace org::ttldtor::qdcxx::mux::detail
