// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "feed_internal.hpp"
#include "qd/transport/tcp_recovery.hpp"

namespace org::ttldtor::qdcxx::api::detail {

/** Actual completed data-callback cut; no callback or final capture is selected by this observation. */
struct HistoryCallbackRetirement {
    bool quiescent;         ///< False while selected user dispatch or its final capture/accounting retirement remains.
    std::uint64_t revision; ///< Exact observed completed data channel, never a future-quiescence promise.
};

/** Sample the existing synchronous data fence; custom execution extends this one adapter.
 * @param port Detached subscription whose data channel is sampled without waiting.
 * @return One checked fence cut; false on an overlapping sample.
 */
[[nodiscard]] HistoryCallbackRetirement historyCallbackRetirement(const SubscriptionPort &port) noexcept;
/** Sample the same cut while the caller already holds the subscription mutex.
 * @param port Locked detached subscription, after any endpoint lock.
 * @return Same nonwaiting data-callback qualification, without recursively locking the port.
 */
[[nodiscard]] HistoryCallbackRetirement historyCallbackRetirementLocked(const SubscriptionPort &port) noexcept;

/** Copy only detached data, under endpoint→subscription order, with no worker or callback waiting.
 * @param port Public subscription's detached state owner.
 * @return Owning AS-OF cut, requalified against current facade/callback fences; copying can throw bad_alloc.
 */
[[nodiscard]] HistorySubscriptionSnapshot copyHistorySnapshot(const std::shared_ptr<SubscriptionPort> &port);

/** Publish actual guarded source evidence after worker progress; no application code executes here.
 * @param recovery Worker-owned native recovery instance.
 * @param handle Exact stable receiver registration in its adopted HISTORY queue.
 * @param port Detached publication target, with a weak endpoint link.
 * @param work Aggregate outer-turn observation credits, consumed without per-key replenishment.
 */
void publishHistorySnapshot(const transport::TcpRecovery &recovery, const mux::MuxHandle &handle,
                            const std::shared_ptr<SubscriptionPort> &port, mux::QueueWork &work) noexcept;

} // namespace org::ttldtor::qdcxx::api::detail
