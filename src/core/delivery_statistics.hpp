// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/statistics.hpp"
#include <mutex>
#include <utility>

namespace org::ttldtor::qdcxx::core::detail {

/** Pure bounded accounting owner shared by actual delivery producers; never retains a callback or runtime. */
struct DeliveryStatisticsState {
    mutable std::mutex mutex;     ///< Lock order: facade port before statistics; never call user code under either.
    SubscriptionStatistics value; ///< Every mutation and public cut uses this one mutex.
    std::size_t submittedSelectors = 0; ///< One facade command's logical selector count until actual application.

    std::uint64_t submittedDesiredRevision = 0; ///< Logical revision belonging to the queued facade command.

    /** Apply one complete accounting transition without allocation or user code.
     * @param operation Library-only bounded mutation, required not to throw.
     */
    template <class Operation> void update(Operation &&operation) noexcept {
        std::lock_guard lock(mutex);
        operation(value);
        value.observedAt = std::chrono::steady_clock::now();
    }

    /** Copy the coherent retained value without delivery progress or callback waiting.
     * @return Detached owning cut, valid after this accounting owner retires.
     */
    [[nodiscard]] SubscriptionStatistics snapshot() const noexcept {
        std::lock_guard lock(mutex);

        return value;
    }
};

/** Apply a transition only when the facade elected finite delivery accounting.
 * @param state Optional detached accounting owner.
 * @param operation Bounded nonthrowing library mutation.
 */
template <class Operation>
void updateDelivery(const std::shared_ptr<DeliveryStatisticsState> &state, Operation &&operation) noexcept {
    if (state)
        state->update(std::forward<Operation>(operation));
}
} // namespace org::ttldtor::qdcxx::core::detail
