// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>

namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "Usage: qdcxx-feed-reconnect --run [HOST:PORT [SYMBOL]]\n"
                     "Subscribe to Quote, reconnect once, and keep the same subscription.\n"
                     "Defaults: demo.dxfeed.com:7300, AAPL. Waits 10 seconds before and after reconnect.\n"
                     "Uses plain TCP and starts a network worker. Help does not connect.\n"
                     "Exit 0: connected at shutdown, 1: failure, 2: invalid arguments.\n";

        return std::cout ? 0 : 1;
    }

    if (argc > 4 || std::string_view(argv[1]) != "--run" || (argc > 2 && std::string_view(argv[2]).empty()) ||
        (argc > 3 && std::string_view(argv[3]).empty())) {
        std::cerr << "Invalid arguments; use --help.\n";

        return 2;
    }

    const std::string_view address = argc > 2 ? argv[2] : "demo.dxfeed.com:7300";
    const std::string_view symbol = argc > 3 ? argv[3] : "AAPL";

    try {
        q::api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint();
        auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();

        subscription->addEventListener([symbol](std::span<const q::events::Quote> quotes) {
            for (const auto &quote : quotes) {
                std::cout << symbol << " bid=" << quote.getBidPrice() << " ask=" << quote.getAskPrice() << '\n';
            }

            std::cout.flush();
        });
        subscription->addSymbols(symbol);
        endpoint->connect(address);

        static_cast<void>(endpoint->awaitState(q::api::EndpointState::CLOSED, std::chrono::steady_clock::now() + 10s));
        endpoint->reconnect();
        static_cast<void>(endpoint->awaitState(q::api::EndpointState::CLOSED, std::chrono::steady_clock::now() + 10s));

        const auto state = endpoint->getState();
        const auto result = endpoint->closeAndAwaitTermination();
        runtime.closeAndAwaitTermination();

        if (result.diagnostic.code != q::api::ErrorCode::NONE) {
            std::cerr << "Endpoint diagnostic: " << static_cast<int>(result.diagnostic.code) << ' '
                      << result.diagnostic.stage << '\n';
        }

        if (result.deadlineExpired || (!result.drained && !result.hadGap) ||
            state != q::api::EndpointState::CONNECTED ||
            subscription->getDiagnostic().code != q::api::ErrorCode::NONE || !std::cout || !std::cerr) {
            std::cerr << "Connection or subscription failed.\n";

            return 1;
        }

        return 0;
    } catch (const q::api::Error &error) {
        std::cerr << error.what() << '\n';

        return error.diagnostic().code == q::api::ErrorCode::INVALID_ARGUMENT ? 2 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
