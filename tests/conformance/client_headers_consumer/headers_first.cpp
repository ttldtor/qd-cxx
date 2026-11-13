// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/api/feed.hpp>
#include <qd/api/independent_subscription.hpp>
#include <qd/api/indexed_event_model.hpp>
#include <qd/api/statistics.hpp>
#include <qd/api/time_series_event_model.hpp>
#include <qd/api/time_series_subscription.hpp>
#include <qd/client/stream_dispatcher.hpp>
#include <qd/core/statistics.hpp>
#include <qd/mux/queue.hpp>

namespace q = org::ttldtor::qdcxx;

static_assert(q::client::ProgressKind::CALLBACK == q::client::ProgressKind::CALLBACK_COMPLETED);
static_assert(q::mux::QueueProgressKind::CALLBACK == q::mux::QueueProgressKind::CALLBACK_COMPLETED);
static_assert(static_cast<int>(q::client::ProgressKind::CALLBACK_COMPLETED) == 4);
static_assert(static_cast<int>(q::client::ProgressKind::CLOSING) == 5);
static_assert(static_cast<int>(q::mux::QueueProgressKind::CALLBACK_COMPLETED) == 6);
static_assert(static_cast<int>(q::mux::QueueProgressKind::CONTROL) == 7);

#ifdef _WIN32
#    include <Windows.h>
#    if !defined(min) || !defined(max) || !defined(CALLBACK)
#        error Windows SDK must define its macros after including qdcxx headers.
#    endif
bool windowsFirst();
#endif

int main() {
#ifdef _WIN32

    if (!windowsFirst()) {
        return 1;
    }

#endif

    q::api::RuntimeOptions options;
    options.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(options);
    const auto endpoint = runtime.createEndpoint();
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();

    return closed.drained && !closed.deadlineExpired && !closed.hadGap && endpoint->getStatistics().final ? 0 : 1;
}
