// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "feed_internal.hpp"

namespace org::ttldtor::qdcxx::api::detail {
/** Lock root-turn and target admission without lock-order inversion between concurrent runtimes. */
class SubscriptionObserverAdmission final {
    std::shared_ptr<SubscriptionChangeDomain> target_;
    std::shared_ptr<SubscriptionChangeDomain> root_;
    std::shared_ptr<SubscriptionChangeDomain>
        driver_; ///< Physical dispatcher resumes foreign causal roots iteratively.
    std::unique_lock<std::mutex> targetLock_;
    std::unique_lock<std::mutex> rootLock_;
    std::unique_lock<std::mutex> driverLock_;

    public:
    /** @param target Detached target domain; root is the current callback turn when present. */
    explicit SubscriptionObserverAdmission(std::shared_ptr<SubscriptionChangeDomain> target);
    SubscriptionObserverAdmission(const SubscriptionObserverAdmission &) = delete;
    SubscriptionObserverAdmission &operator=(const SubscriptionObserverAdmission &) = delete;
};

/** Prepare detached finite observer storage before a subscription is published.
 * @param port Fully configured unpublished subscription.
 * @param limits Immutable validated runtime capacities.
 */
void initializeSubscriptionObservers(const std::shared_ptr<SubscriptionPort> &port, const RuntimeOptions &limits);
/** Copy an accepted observation while the subscription mutex is held.
 * @param port Detached logical state protected by its mutex.
 * @return Independent complete owning value; allocation exceptions propagate before publication.
 */
[[nodiscard]] SubscriptionSnapshot subscriptionSnapshotLocked(const SubscriptionPort &port);
/** Reserve and publish complete notification backing before the nonthrowing desired-state swap.
 * @param port Owner protected by both its change-domain mutex and subscription mutex, in that order.
 * @param symbols Complete validated candidate, borrowed until the synchronous copy finishes.
 * @param revision Checked next desired identity.
 */
void prepareSubscriptionChangeLocked(SubscriptionPort &port, const std::vector<SubscriptionSelector> &symbols,
                                     std::uint64_t revision);
/** Drain admitted local changes without native/network progress or waiting on another callback owner.
 * @param domain Detached shared owner; reentry/concurrent calls leave delivery to its existing claimant.
 */
void dispatchSubscriptionChanges(const std::shared_ptr<SubscriptionChangeDomain> &domain) noexcept;
/** Admit a finite owning observer and optional initial phase.
 * @param port Logical subscription; close fences admission.
 * @param listener Nonempty callback, moved into one independently charged box.
 * @return Non-reused listener identity; Error or allocation exceptions preserve old accepted state.
 */
[[nodiscard]] std::uint64_t addSubscriptionObserver(const std::shared_ptr<SubscriptionPort> &port,
                                                    SubscriptionChangeListener listener);
/** Withdraw future selection and reserve an allocation-free terminal callback.
 * @param port Detached subscription.
 * @param id Observer identity; absent values do nothing.
 */
void removeSubscriptionObserver(const std::shared_ptr<SubscriptionPort> &port, std::uint64_t id);
/** Fence observers and reserve all terminal selections; ordinary notification capacity cannot refuse close.
 * @param port Detached subscription, retained through outside-lock dispatch.
 * @param dispatch False preserves nonblocking public close; true is actual owner retirement.
 */
void closeSubscriptionObservers(const std::shared_ptr<SubscriptionPort> &port, bool dispatch = true) noexcept;
/** Detect advisory callbacks so blocking owner waits cannot await their own retirement.
 * @return True during a selected observer or its final capture release on this thread.
 */
[[nodiscard]] bool subscriptionObserverCallbackActive() noexcept;

/** Defer symbol-observer dispatch through an existing complete data callback selection. */
class SubscriptionDataCallbackScope final {
    std::shared_ptr<SubscriptionChangeDomain> domain_;
    std::shared_ptr<SubscriptionChangeDomain> previousDriver_; ///< Existing same-thread continuation dispatcher.
    std::shared_ptr<SubscriptionChangeDomain> previousRoot_;   ///< Existing same-thread bounded continuation root.
    std::shared_ptr<SubscriptionChangeDomain> previousOwner_;  ///< Existing same-thread selected callback owner.
    std::size_t previousChannel_ = 0;                          ///< Existing selected callback fence channel.

    public:
    /** @param domain Runtime-local detached observer owner; no native state is retained. */
    explicit SubscriptionDataCallbackScope(std::shared_ptr<SubscriptionChangeDomain> domain) noexcept;
    /** Leave the data domain, then perform finite admitted deferred notifications outside all locks. */
    ~SubscriptionDataCallbackScope();
    SubscriptionDataCallbackScope(const SubscriptionDataCallbackScope &) = delete;
    SubscriptionDataCallbackScope &operator=(const SubscriptionDataCallbackScope &) = delete;
};
} // namespace org::ttldtor::qdcxx::api::detail
