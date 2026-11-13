// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <Windows.h>

#if defined(NOMINMAX) || !defined(min) || !defined(max) || !defined(CALLBACK)
#    error This probe requires the active Windows SDK macros.
#endif

#include <qd/api/feed.hpp>
#include <qd/api/independent_subscription.hpp>
#include <qd/api/indexed_event_model.hpp>
#include <qd/api/statistics.hpp>
#include <qd/api/time_series_event_model.hpp>
#include <qd/api/time_series_subscription.hpp>
#include <qd/client/stream_dispatcher.hpp>
#include <qd/core/statistics.hpp>
#include <qd/mux/queue.hpp>

#if !defined(min) || !defined(max) || !defined(CALLBACK)
#    error qdcxx must preserve the Windows SDK macros.
#endif

namespace q = org::ttldtor::qdcxx;

static_assert(static_cast<int>(q::client::ProgressKind::CALLBACK_COMPLETED) == 4);
static_assert(static_cast<int>(q::mux::QueueProgressKind::CALLBACK_COMPLETED) == 6);
static_assert(static_cast<int>(q::client::ProgressKind::CLOSING) == 5);
static_assert(static_cast<int>(q::mux::QueueProgressKind::CONTROL) == 7);

namespace {
int CALLBACK fixture() {
    return min(1, 2) + max(1, 2);
}
} // namespace

bool windowsFirst() {
    int(CALLBACK * callback)() = &fixture;

    return callback() == 3;
}
