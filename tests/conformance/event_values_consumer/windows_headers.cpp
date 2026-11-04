// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <Windows.h>

#include <qd/events/event_flags.hpp>
#include <qd/events/event_value.hpp>
#include <qd/events/market_enums.hpp>
#include <qd/events/short_string.hpp>
#include <qd/events/time_helpers.hpp>

// Including the value surface must neither collide with nor erase the platform DELETE macro.
static_assert(DELETE == 0x00010000L);
static_assert(org::ttldtor::qdcxx::events::marketEnumCode(org::ttldtor::qdcxx::events::OrderAction::DELETE_ORDER) == 4);
