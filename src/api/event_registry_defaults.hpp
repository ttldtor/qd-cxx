// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/api/event_registry.hpp"

namespace org::ttldtor::qdcxx::api {
enum class DeliveryContract;

namespace detail {
inline constexpr char16_t FIRST_TIME_AND_SALE_EXCHANGE = u'A'; ///< First default Java market exchange.
inline constexpr char16_t LAST_TIME_AND_SALE_EXCHANGE = u'Z';  ///< Last default Java market exchange.
inline constexpr std::size_t TIME_AND_SALE_DEFAULT_RECORDS =
    2 + LAST_TIME_AND_SALE_EXCHANGE - FIRST_TIME_AND_SALE_EXCHANGE; ///< Composite plus all regional records.

/** Prepare the contract-aware SDK default with its explicit finite construction seed.
 * @param contract Actual receiving family; unsupported values refuse.
 * @return Complete independent owning registry or original atomic failure.
 */
[[nodiscard]] std::expected<EventRegistry, EventRegistryFailure> defaultReceivingRegistry(DeliveryContract contract);
/** Prepare SDK-owned STREAM defaults, including composite and A..Z TimeAndSale.
 * @param limits Complete finite schema, route and storage bounds.
 * @param work Remaining preparation credits; failures consume attempted work.
 * @return Complete independent registry or atomic original refusal.
 */
[[nodiscard]] std::expected<EventRegistry, EventRegistryFailure> defaultStreamEventRegistry(EventRegistryLimits limits,
                                                                                            EventRegistryWork &work);
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
