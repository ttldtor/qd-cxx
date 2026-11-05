// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/api/event_registry.hpp"

namespace org::ttldtor::qdcxx::api::detail {
/** Prepare SDK-owned HISTORY defaults without changing the ordinary built-in catalog.
 * @param limits Complete finite schema, route and storage bounds.
 * @param work Caller preparation credits, consumed on attempted construction/refusal.
 * @return Owning standard Order registry, or original atomic schema/mapping/registry failure.
 */
[[nodiscard]] std::expected<EventRegistry, EventRegistryFailure> defaultHistoryEventRegistry(EventRegistryLimits limits,
                                                                                             EventRegistryWork &work);
} // namespace org::ttldtor::qdcxx::api::detail
