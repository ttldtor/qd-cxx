// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/events/summary.hpp"
#include "qd/events/trade.hpp"
#include "qd/events/trade_eth.hpp"
#include "qd/mux/queue.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <variant>

namespace org::ttldtor::qdcxx::client {
namespace detail {
class SubscriptionReceiver;
struct ReceiverPlan;
} // namespace detail

/** Existing prepared business projections; other records remain fully generic. */
enum class PreparedEventKind {
    QUOTE,     ///< Existing schema-bound Quote projection.
    PROFILE,   ///< Existing schema-bound Profile projection.
    TRADE,     ///< Complete owning Trade projection.
    TRADE_ETH, ///< Complete distinct owning extended-hours projection.
    SUMMARY    ///< Complete owning session-summary projection.
};

/** Explicit source for optional event metadata; indexed identity is never a timestamp substitute. */
enum class ReceiverEventTimePolicy {
    UNSPECIFIED,   ///< No event timestamp is asserted; the legacy event member remains zero.
    FIELD_METADATA ///< Use actual original FIELD metadata when supplied, otherwise remain unspecified.
};

/** One exact local record route, prepared before a registration can become visible. */
struct PreparedSubscriptionBinding {
    std::int32_t localId;                 ///< Local record ID in the factory's exact schema instance.
    PreparedEventKind kind;               ///< Selected existing mapper, without event-name inference.
    events::MappingLimits mappingLimits;  ///< Finite preparation allowances copied into each attempt.
    events::EventValueLimits valueLimits; ///< Owned result bounds; stringUnits applies only to Profile.
    ReceiverEventTimePolicy eventTimePolicy =
        ReceiverEventTimePolicy::UNSPECIFIED; ///< Explicit source of optional event timestamp metadata.
};

/** Aggregate immutable plan bounds, separate from queue reservations and application captures. */
struct ReceiverLimits {
    std::size_t bindings;    ///< Maximum prepared routes, including zero for a generic-only receiver.
    std::size_t shapeFields; ///< Maximum sum of complete bound-record field counts, including extra fields.
    std::size_t selections;  ///< Maximum sum of declared per-route MappingLimits::selections allowances.
};

/** Caller-owned preparation credits; failed attempts may consume work and never replenish it. */
struct ReceiverWork {
    std::size_t visits;          ///< Input, duplicate-route and aggregate-plan inspections.
    std::size_t preparationWork; ///< Conservative sum of each mapper's declared count/unit/comparison allowances.
};

/** Caller-owned projection credits, retained and debited across repeated project calls. */
struct ReceiverProjectionWork {
    std::size_t visits;    ///< Index, route, complete shape, declared selection allowance and timestamp inspection.
    std::size_t textUnits; ///< Conservative per-route owned text allowance, not measured output length.
    std::size_t bytes;     ///< Conservative logical result/text copy work, excluding allocator overhead and RSS.
};

/** Local receiver failure; mapper and queue failures retain their existing distinct categories. */
enum class ReceiverError {
    INVALID_INPUT,     ///< Null consumer, duplicate/invalid route, enum or projection index.
    RESOURCE_LIMIT,    ///< Finite plan/output allowance or checked-size arithmetic exceeded.
    WORK_LIMIT,        ///< Caller-supplied preparation/projection work was exhausted.
    ALLOCATION_FAILURE ///< Candidate adapter/plan storage could not be allocated.
};

/** Atomic factory refusal with optional position in the caller's binding list. */
struct ReceiverFailure {
    std::variant<ReceiverError, events::MappingError, mux::QueueFailure> cause; ///< Original typed cause.
    std::optional<std::size_t> bindingIndex; ///< Failing route, absent for aggregate/queue/consumer failures.
};

/** Fully owning optional projection; generic records are represented by monostate. */
struct SubscriptionProjection {
    std::variant<std::monostate, events::Quote, events::Profile, events::Trade, events::TradeETH, events::Summary>
        value;                               ///< Owned event or explicit generic route.
    std::optional<std::int64_t> eventTimeMs; ///< Actual selected FIELD metadata only; rawTime stays independent.
};

/** Projection refusal, independent of queue delivery/acknowledgement. */
using ReceiverProjectionFailure = std::variant<ReceiverError, events::MappingError>;

/** Callback-only generic batch and lazy prepared projection service.
 * The wrapper and its plan expire on callback return even when retain() preserves generic records.
 * All methods are serialized with this callback. No method accesses a public recovery owner.
 */
class SubscriptionBatch {
    const mux::BorrowedMuxBatch *batch_; ///< Existing callback-only immutable record backing.
    const detail::ReceiverPlan *plan_;   ///< Executing adapter's immutable mapping plan, never retained by this view.
    friend class detail::SubscriptionReceiver;

    /** Borrow existing immutable batch and executing adapter plan.
     * @param batch Underlying callback borrow.
     * @param plan Immutable adapter plan, kept alive by the queue's executing callback guard.
     */
    SubscriptionBatch(const mux::BorrowedMuxBatch &batch, const detail::ReceiverPlan &plan) noexcept;

    public:
    /** Prevent escape by value; references still obey the callback-only lifetime.
     * @param other Source callback borrow.
     */
    SubscriptionBatch(const SubscriptionBatch &other) = delete;
    /** Prevent assignment of a callback borrow.
     * @param other Source borrow.
     * @return Not callable.
     */
    SubscriptionBatch &operator=(const SubscriptionBatch &other) = delete;
    /** Prevent moving a callback-only service beyond its invocation.
     * @param other Source callback borrow.
     */
    SubscriptionBatch(SubscriptionBatch &&other) = delete;
    /** Prevent move assignment of a callback borrow.
     * @param other Source borrow.
     * @return Not callable.
     */
    SubscriptionBatch &operator=(SubscriptionBatch &&other) = delete;

    /** Inspect every exact generic value and existing membership/source/index metadata.
     * @return Direct immutable record span, valid during this callback or an independently retained batch lease.
     */
    [[nodiscard]] std::span<const mux::MuxRecordView> records() const noexcept;
    /** Retain only detached generic record backing without allocation.
     * @return Existing owning batch lease; it retains no receiver plan, callable, queue or transport.
     */
    [[nodiscard]] mux::OwnedMuxBatch retain() const noexcept;
    /** Synchronously project one record without hidden input copies or result caching.
     * @param index Position in records(); out-of-range is an explicit error.
     * @param work Remaining caller credits; repeated calls debit them again, including failed attempts.
     * @return Independent owned Quote/Profile/Trade, generic monostate or typed failure. No callback or state mutation
     * occurs; result ownership is separate from the queue ledger. The wrapper cannot be used after callback return.
     */
    [[nodiscard]] std::expected<SubscriptionProjection, ReceiverProjectionFailure>
    project(std::size_t index, ReceiverProjectionWork &work) const;
};

/** Application-created callable, held strongly through its one whole-batch invocation. */
class SubscriptionConsumer {
    public:
    /** Release application callback state; arbitrary application work is outside receiver accounting. */
    virtual ~SubscriptionConsumer() = default;
    /** Receive exactly one callback per underlying queue batch, without internal per-record callbacks.
     * @param batch Callback-only generic records and explicit prepared projection service.
     * @return Existing whole-batch acknowledgement/pause action. Exceptions retain existing queue isolation.
     * Public recovery destruction/stop inside this callback keeps its existing supported semantics.
     */
    virtual mux::BatchAction onBatch(const SubscriptionBatch &batch) = 0;
};

/** Prepare all typed routes before opening one initially empty logical subscription.
 * Owner access is serialized and cannot be reentered from a queue callback. Existing source admission/reset
 * is allowed; immutable schema/family/policy must match. Refusal submits no desired command and opens no
 * registration. Success returns the existing handle: add/remove/replace/clear and their command tickets
 * retain their current semantics, without claiming wire emission or upstream acknowledgement.
 * @param queue Existing queue owner, borrowed only during this call.
 * @param schema Exact immutable schema origin; equal-shaped independent schemas do not match.
 * @param family Required queue delivery contract.
 * @param policy Required immutable selector profile, never changed implicitly.
 * @param bindings Borrowed unique local typed routes; unlisted records remain generic.
 * @param consumer Application callback, preserved on refusal; arbitrary captures are application-owned.
 * @param receiverLimits Finite separate immutable plan bounds.
 * @param clientLimits Existing complete per-registration queue reservations.
 * @param receiverWork Remaining preparation credits; attempted work is not refunded on failure.
 * @param queueWork Existing queue inspection/open/retirement credits.
 * @return Existing empty handle or atomic typed refusal. No callback, native factory or network is started.
 */
[[nodiscard]] std::expected<mux::MuxHandle, ReceiverFailure>
openSubscriptionReceiver(mux::MuxQueue &queue, const core::LocalSchema &schema, core::SubscriptionFamily family,
                         core::SubscriptionSelectorPolicy policy, std::span<const PreparedSubscriptionBinding> bindings,
                         const std::shared_ptr<SubscriptionConsumer> &consumer, ReceiverLimits receiverLimits,
                         mux::ClientLimits clientLimits, ReceiverWork &receiverWork, mux::QueueWork &queueWork);

} // namespace org::ttldtor::qdcxx::client
