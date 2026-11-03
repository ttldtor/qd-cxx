// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/events/order_source.hpp>
#include <qd/events/source_selection.hpp>

#include <stdexcept>

namespace events = org::ttldtor::qdcxx::events;

int main() {
    const auto all = events::SourceSelection::all();
    const auto defaultOnly = events::SourceSelection::specific(events::IndexedEventSource::DEFAULT);
    const auto custom = events::SourceSelection::specific(events::IndexedEventSource(-1, std::nullopt));

    if (all == defaultOnly || !all.matches(*custom.source()) || defaultOnly.matches(*custom.source()) ||
        !events::OrderSource::NTV.isPublishable(events::OrderEventType::ORDER_IMBALANCE) ||
        !events::OrderSource::NUAM.isFullOrderBook() || events::OrderSource::fullOrderBook().size() != 3 ||
        events::OrderSource::publishable(events::OrderEventType::NUAM_ORDER).size() != 3 ||
        events::OrderSource::valueOf(u"TEST").isPublishable(events::OrderEventType::ORDER)) {
        return 1;
    }

    try {
        static_cast<void>(events::OrderSource::publishable(static_cast<events::OrderEventType>(6)));

        return 2;
    } catch (const std::invalid_argument &) {
    }

    return 0;
}
