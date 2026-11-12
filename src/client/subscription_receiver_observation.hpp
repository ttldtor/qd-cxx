// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/client/subscription_receiver.hpp"
#include "qd/collectors/ticker.hpp"

namespace org::ttldtor::qdcxx::client::detail {

/** Prepare one receiver and optionally expose its plan while the exact queue registration stays alive.
 * @param queue Existing queue owner.
 * @param schema Exact immutable schema.
 * @param family Existing queue contract.
 * @param policy Existing selector profile.
 * @param bindings Bounded immutable typed routes.
 * @param consumer Callback owner.
 * @param receiverLimits Finite receiver reservations.
 * @param clientLimits Finite queue reservations.
 * @param receiverWork Remaining receiver work.
 * @param queueWork Remaining queue work.
 * @param observationPlan Optional output; borrowed only while the successful registration remains alive.
 * @return Successful handle or original atomic refusal.
 */
[[nodiscard]] std::expected<mux::MuxHandle, ReceiverFailure>
openObservedSubscriptionReceiver(mux::MuxQueue &queue, const core::LocalSchema &schema, core::SubscriptionFamily family,
                                 core::SubscriptionSelectorPolicy policy,
                                 std::span<const PreparedSubscriptionBinding> bindings,
                                 const std::shared_ptr<SubscriptionConsumer> &consumer, ReceiverLimits receiverLimits,
                                 mux::ClientLimits clientLimits, ReceiverWork &receiverWork, mux::QueueWork &queueWork,
                                 const ReceiverPlan **observationPlan);

/** Project one actual owner-borrowed TICKER value through its already prepared native route.
 * @param plan Exact live adapter plan, borrowed only within the validated queue visitor.
 * @param value Current collector value, borrowed until the visitor returns.
 * @param work Finite conversion and prepared projection work; consumed on refusal.
 * @return Owning projection or typed refusal. No source pointer is retained.
 */
[[nodiscard]] std::expected<SubscriptionProjection, ReceiverProjectionFailure>
projectTickerLatest(const ReceiverPlan &plan, const collectors::TickerValue &value, ReceiverProjectionWork &work);

} // namespace org::ttldtor::qdcxx::client::detail
