// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/api/event_registry.hpp"

namespace org::ttldtor::qdcxx::api {
enum class DeliveryContract;

namespace detail {
/** Prepare the contract-aware SDK default with its explicit finite construction seed.
 * @param contract Actual receiving family; unsupported values refuse.
 * @return Complete independent owning registry or original atomic failure.
 */
[[nodiscard]] std::expected<EventRegistry, EventRegistryFailure> defaultReceivingRegistry(DeliveryContract contract);
/** Prepare SDK-owned TICKER defaults, including real A..Z Quote-derived Order routes.
 * @param limits Complete finite schema, route and storage bounds.
 * @param work Remaining preparation credits; failures consume attempted work.
 * @return Complete independent registry or atomic original refusal.
 */
[[nodiscard]] std::expected<EventRegistry, EventRegistryFailure> defaultTickerEventRegistry(EventRegistryLimits limits,
                                                                                            EventRegistryWork &work);
/** Prepare SDK-owned HISTORY defaults without changing the ordinary built-in catalog.
 * @param limits Complete finite schema, route and storage bounds.
 * @param work Caller preparation credits, consumed on attempted construction/refusal.
 * @return Owning standard Order registry, or original atomic schema/mapping/registry failure.
 */
[[nodiscard]] std::expected<EventRegistry, EventRegistryFailure> defaultHistoryEventRegistry(EventRegistryLimits limits,
                                                                                             EventRegistryWork &work);
} // namespace detail
} // namespace org::ttldtor::qdcxx::api
