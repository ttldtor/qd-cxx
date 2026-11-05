// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/api/time_series_subscription.hpp>

#include <type_traits>

namespace api = org::ttldtor::qdcxx::api;
static_assert(std::is_nothrow_move_constructible_v<api::TimeSeriesSubscriptionModel>);
static_assert(!std::is_copy_constructible_v<api::TimeSeriesSubscriptionModel>);
static_assert(api::TimeSeriesSymbolValue<api::TimeSeriesSubscriptionSymbol<>>);
