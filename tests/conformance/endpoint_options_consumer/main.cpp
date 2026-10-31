// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Native installed-package example: explicit immutable configuration only, without runtime/JSON or I/O activation.
#include "qd/client/endpoint_options.hpp"
#include "qd/events/quote.hpp"

#include <iostream>

/** Construct two independent immutable snapshots without any runtime or ambient configuration.
 * @return Zero for preserved independent effective values; nonzero for preparation/output failure.
 */
int main() {
    namespace qdcxx = org::ttldtor::qdcxx;
    auto schema = qdcxx::events::defaultQuoteSchema({16, 128, 256, 128, 16384});
    auto input = qdcxx::client::controlledStreamSettings(std::chrono::milliseconds{2000});

    if (!schema || !input) {
        return 1;
    }

    input->host = "127.0.0.1";
    input->port = 7301;
    auto first = qdcxx::client::EndpointOptions::create(*schema, *input);
    input->port = 7302;
    input->timeouts.read = std::chrono::milliseconds{750};
    auto second = qdcxx::client::EndpointOptions::create(*schema, *input);

    if (!first || !second || !first->requirePlainStream() || !second->requirePlainStream() ||
        first->settings().port != 7301 || second->settings().port != 7302 ||
        first->settings().timeouts.read != std::chrono::milliseconds{2000}) {
        return 1;
    }

    std::cout << "Independent endpoint snapshots: " << first->settings().port << ", " << second->settings().port
              << '\n';
    std::cout.flush();

    return std::cout ? 0 : 1;
}
