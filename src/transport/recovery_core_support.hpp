// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "recovery_core.hpp"

#include <exception>
#include <limits>

namespace org::ttldtor::qdcxx::transport::detail {

/** Scoped native-poll input gate; only credits deliberately not lent to the callee are restored. */
class RecoveryDecodeGate {
    std::size_t &operations_; ///< Borrowed during the actual inner poll only.
    std::size_t saved_;       ///< Original credits not lent while the one complete-event slot is held.
    bool gated_;              ///< Whether fresh session parsing is paused.
    public:
    /** Gate fresh session parsing without increasing another caller coordinate.
     * @param work Actual caller credits.
     * @param decode Whether the sole complete-event slot is free.
     */
    RecoveryDecodeGate(protocol::LiveWork &work, bool decode) noexcept
        : operations_(work.operations), saved_(work.operations), gated_(!decode) {
        if (gated_)
            operations_ = 0;
    }

    /** Restore only original credits never lent; zero credits cannot have been consumed. */
    ~RecoveryDecodeGate() {
        if (gated_) {
            if (operations_ != 0)
                std::terminate();

            operations_ = saved_;
        }
    }

    /** A borrowed-budget guard has unique scope ownership.
     * @param other Unused source.
     */
    RecoveryDecodeGate(const RecoveryDecodeGate &other) = delete;
    /** Borrowed-budget scope ownership cannot be assigned.
     * @param other Unused source.
     * @return No result; assignment is unavailable.
     */
    RecoveryDecodeGate &operator=(const RecoveryDecodeGate &other) = delete;
};

/** Local bounded action escape; exception bookkeeping is excluded from logical backing. */
struct RecoveryActionRefusal {
    RecoveryDiagnostic diagnostic; ///< Static exact failure/work checkpoint.
};

/** Stop an action without retaining caller or peer text.
 * @param error Typed category.
 * @param stage Static checkpoint.
 */
[[noreturn]] inline void recoveryRefuse(RecoveryError error, std::string_view stage) {
    throw RecoveryActionRefusal{{error, stage}};
}

/** Debit one actual owner coordinate, preserving successful earlier charges.
 * @param amount Required conservative work.
 * @param remaining Actual caller remainder.
 */
inline void recoveryDebit(std::size_t amount, std::size_t &remaining) {
    if (amount > remaining) {
        recoveryRefuse(RecoveryError::WORK_LIMIT, "recovery-action-work");
    }

    remaining -= amount;
}

/** Sum capacities/work in identical units with checked arithmetic.
 * @param left Existing amount.
 * @param right Additional amount.
 * @return Representable sum.
 */
inline std::size_t recoverySum(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        recoveryRefuse(RecoveryError::RESOURCE_LIMIT, "recovery-action-arithmetic");
    }

    return left + right;
}

/** Multiply finite simultaneous work/capacity without wrapping.
 * @param value One owner's amount.
 * @param count Number of equal scopes or actual element width.
 * @return Representable product.
 */
inline std::size_t recoveryProduct(std::size_t value, std::size_t count) {
    if (count != 0 && value > std::numeric_limits<std::size_t>::max() / count) {
        recoveryRefuse(RecoveryError::RESOURCE_LIMIT, "recovery-action-arithmetic");
    }

    return value * count;
}

/** Construct a matching model result without manufacturing any disposal/admission proof.
 * @param ticket Exact current action.
 * @param completion Actual selected result, completed by default.
 * @return Otherwise empty observation.
 */
inline RecoveryObservation recoveryResult(RecoveryTicket ticket,
                                          RecoveryCompletion completion = RecoveryCompletion::COMPLETED) noexcept {
    RecoveryObservation result;
    result.ticket = ticket;
    result.completion = completion;

    return result;
}

/** Recognize only the actual common runtime's sole-output-slot refusal.
 * This matches a library-owned static checkpoint plus typed evidence, never peer/SDK strings.
 * @param failure Actual composition result.
 * @return True only for that exact nonterminal occupancy tuple.
 */
inline bool recoveryOutputBlocked(const RecoveryDiagnostic &failure) noexcept {
    return failure.error == RecoveryError::RESOURCE_LIMIT && failure.transport.error == TcpError::RESOURCE_LIMIT &&
           failure.transport.stage == "output-slot" && failure.transport.nativeCode == 0 &&
           failure.transport.protocolError == protocol::LiveError::NONE;
}

} // namespace org::ttldtor::qdcxx::transport::detail
