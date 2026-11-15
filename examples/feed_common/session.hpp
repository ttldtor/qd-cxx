// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

#include <chrono>
#include <iostream>

namespace org::ttldtor::qdcxx::examples {
/** Allow background delivery until the end of a demonstration window or early endpoint termination.
 * @param endpoint Endpoint receiving in the background.
 * @param duration Maximum time to allow delivery on this call.
 */
inline void receiveFor(const api::Endpoint &endpoint, std::chrono::steady_clock::duration duration) {
    static_cast<void>(endpoint.awaitState(api::EndpointState::CLOSED, std::chrono::steady_clock::now() + duration));
}

/** Close the endpoint, wait for delivery to stop and report retained endpoint diagnostics.
 * @param endpoint Endpoint to close on its owning thread.
 * @return Close outcome for the example's own delivery checks.
 */
inline api::CloseResult finish(api::Endpoint &endpoint) {
    const auto result = endpoint.closeAndAwaitTermination();

    if (result.diagnostic.code != api::ErrorCode::NONE) {
        std::cerr << "Endpoint diagnostic: " << result.diagnostic.stage << '\n';
    }

    return result;
}
} // namespace org::ttldtor::qdcxx::examples
