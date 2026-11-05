// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

namespace org::ttldtor::qdcxx::api {
namespace detail {
class CallbackSlotPool;
}
/** Host admission of one event task; acceptance is not callback completion. */
enum class CallbackPostResult {
    ACCEPTED, ///< Host owns the task; completion is separate.
    REJECTED  ///< Host did not accept execution; native subscription records failure.
};
/** Actual claim of one task, independent of host post admission. */
enum class CallbackTaskRunResult {
    COMPLETED, ///< This caller processed the claimed task; inspect statistics for listener outcome.
    DEFERRED,  ///< Existing finite after-return driver owns the nested task.
    CANCELLED  ///< Old, cancelled, moved-from or already claimed task; no listener call.
};

/** Move-only event task. Late, repeated and concurrently attempted runs claim at most once.
 * A retained task keeps fixed detached pool metadata alive, never event payload or native resources.
 * Moving or destroying this object must not race its own member calls. Dropping an accepted unrun task
 * records a dispatch failure. SDK close cancels queued tasks without requiring host executor progress.
 */
class CallbackTask final {
    std::shared_ptr<detail::CallbackSlotPool> pool_;
    std::size_t index_ = 0;
    std::uint64_t generation_ = 0;
    CallbackTask(std::shared_ptr<detail::CallbackSlotPool> pool, std::size_t index, std::uint64_t generation) noexcept;
    friend class detail::CallbackSlotPool;

    public:
    CallbackTask(const CallbackTask &) = delete;
    CallbackTask &operator=(const CallbackTask &) = delete;
    /** Transfer the single host task identity. @param other Existing task, made inert. */
    CallbackTask(CallbackTask &&other) noexcept;
    /** Abandon the old pending identity, then transfer. @param other Existing task. @return This object. */
    CallbackTask &operator=(CallbackTask &&other) noexcept;
    /** Report pending abandonment; never invokes an event listener. */
    ~CallbackTask();
    /** Execute once, outside SDK locks; nested delivery is finitely deferred until outer retirement.
     * Application listener exceptions are contained and recorded on their subscription.
     * @return COMPLETED for an actual run, DEFERRED for an admitted after-return continuation,
     * or CANCELLED for a cancelled, old or already claimed identity.
     */
    [[nodiscard]] CallbackTaskRunResult run() const noexcept;
};

/** Application-owned scheduling policy for event listeners only.
 * post must return promptly. During normal operation accepted tasks must progress or be destroyed.
 * The SDK borrows workers and never stops unrelated application work. It retains this policy object
 * through selected callback/capture retirement. post may run inline; it must not reject after running.
 */
class CallbackExecutor {
    public:
    /** Destroy the policy object; host worker ownership remains with the application. */
    virtual ~CallbackExecutor() = default;
    /** Transfer one task. Throwing or rejecting fails its subscription without retry.
     * @param task Owned task; retaining it retains fixed pure metadata, not callback payload.
     * @return Explicit admission result, independent of actual completion.
     */
    virtual CallbackPostResult post(CallbackTask task) = 0;
};
} // namespace org::ttldtor::qdcxx::api
