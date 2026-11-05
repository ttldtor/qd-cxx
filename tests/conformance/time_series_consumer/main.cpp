// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/api/time_series_subscription.hpp>

#include <array>
#include <optional>

namespace api = org::ttldtor::qdcxx::api;

/** Exercise installed declarations and compiled symbols without a runtime or private header.
 * @return Zero after detached range/capability checks; otherwise a distinct failing boundary.
 */
int main() {
    auto type =
        api::TimeSeriesEventDescriptor::declared(u"consumer:inherited", api::EventSubscriptionCapability::TIME_SERIES);

    if (!type || api::hasTimeSeriesFeedReceiver(*type))
        return 1;

    std::optional<api::TimeSeriesSubscriptionSnapshot> retained;
    {
        auto owner = api::TimeSeriesSubscriptionModel::create(std::span(&*type, 1));

        if (!owner || *owner->getFromTime() != api::TIME_SERIES_FROM_TIME_DISABLED)
            return 2;

        const std::array values{api::SubscriptionSelector::plain(u"IBM"),
                                api::SubscriptionSelector::timeSeries(u"IBM", 7)};

        if (!owner->setSymbols(values) || !owner->setFromTime(-1))
            return 3;

        auto cut = owner->snapshot();

        if (!cut || cut->symbols.size() != 2 || cut->decoratedSymbols[0].fromTime() != -1)
            return 4;

        retained = std::move(*cut);

        if (!owner->clearSymbols() || retained->symbols.size() != 2)
            return 5;
    }

    if (!retained || retained->fromTime != -1 || retained->decoratedSymbols[1].eventSymbol().fromTime() != 7)
        return 6;

    auto unsupported =
        api::TimeSeriesEventDescriptor::declared(u"consumer:indexed", api::EventSubscriptionCapability::INDEXED);
    const auto rejected = api::TimeSeriesSubscriptionModel::create(std::span(&*unsupported, 1));

    if (rejected || rejected.error() != api::TimeSeriesModelError::UNSUPPORTED_EVENT_TYPE)
        return 7;

    const api::TimeSeriesSubscriptionSymbol<> first(api::SubscriptionSelector::plain(u"IBM"), 0);
    const api::TimeSeriesSubscriptionSymbol<> second(api::SubscriptionSelector::plain(u"IBM"), 1);

    return first == second && first.hashCode() == second.hashCode() && !first.sameDemand(second) ? 0 : 8;
}
