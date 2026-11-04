// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed_filter.hpp"

namespace org::ttldtor::qdcxx::api::detail {
/** One already selected immutable QD coordinate; schema storage is retained by the caller registry. */
struct FilterCoordinate {
    std::u16string_view record;
    std::u16string symbol;
    bool wildcard = false;
};

/** Resolve only existing registered receiving capabilities, without activating a subscription. */
[[nodiscard]] std::vector<FilterCoordinate> filterSubscriptionCoordinates(const EventRegistry &registry,
                                                                          DeliveryContract contract, EventType type,
                                                                          const SubscriptionSelector &selector,
                                                                          const FeedFilterLimits &limits);

/** Resolve existing publisher projection independently from subscription reserved-prefix admission. */
[[nodiscard]] std::vector<FilterCoordinate> filterEventCoordinates(const EventRegistry &registry,
                                                                   DeliveryContract contract,
                                                                   const RegisteredEvent &event,
                                                                   const FeedFilterLimits &limits);
} // namespace org::ttldtor::qdcxx::api::detail
