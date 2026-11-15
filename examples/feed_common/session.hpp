// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

#include <iostream>

namespace org::ttldtor::qdcxx::examples {
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
