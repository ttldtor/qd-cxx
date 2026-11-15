// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout
            << "Usage: qdcxx-feed-demo --run [--address HOST:PORT] [--symbol SYMBOL] [--duration-ms N]\n"
               "Subscribe to Quote over TCP and print bid and ask prices and sizes.\n"
               "Defaults: demo.dxfeed.com:7300, AAPL, 10000 ms. Duration: 1..600000 ms.\n"
               "Help opens no connection. Exit 0: quotes received, 1: failure or no data, 2: invalid arguments.\n";

        std::cout.flush();

        return std::cout ? 0 : 1;
    }

    std::string address = "demo.dxfeed.com:7300";
    std::string symbol = "AAPL";
    auto duration = 10000ms;
    bool run = false;
    bool addressSeen = false;
    bool symbolSeen = false;
    bool durationSeen = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view option = argv[i];

        if (option == "--run" && !run) {
            run = true;
        } else if (i + 1 < argc && option == "--address" && !std::exchange(addressSeen, true)) {
            address = argv[++i];
        } else if (i + 1 < argc && option == "--symbol" && !std::exchange(symbolSeen, true)) {
            symbol = argv[++i];
        } else if (i + 1 < argc && option == "--duration-ms" && !std::exchange(durationSeen, true)) {
            const std::string_view value = argv[++i];
            int milliseconds = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), milliseconds);

            if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || milliseconds <= 0 ||
                milliseconds > 600000) {
                std::cerr << "Invalid duration; use --help.\n";

                return 2;
            }

            duration = std::chrono::milliseconds(milliseconds);
        } else {
            std::cerr << "Invalid arguments; use --help.\n";

            return 2;
        }
    }

    if (!run || address.empty() || address.size() > 4096 || symbol.empty() || symbol.size() > 128) {
        std::cerr << "Invalid arguments; use --help.\n";

        return 2;
    }

    try {
        std::atomic<std::size_t> received{0};
        q::api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint();
        auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();

        subscription->addEventListener([&](std::span<const q::events::Quote> quotes) {
            for (const auto &quote : quotes) {
                std::cout << std::setprecision(std::numeric_limits<double>::max_digits10) << symbol
                          << " bid=" << quote.getBidPrice() << " bidSize=" << quote.getBidSizeAsDouble()
                          << " ask=" << quote.getAskPrice() << " askSize=" << quote.getAskSizeAsDouble() << '\n'
                          << std::flush;
                ++received;
            }
        });
        subscription->addSymbols(symbol);
        endpoint->connect(address);

        const auto deadline = std::chrono::steady_clock::now() + duration;
        static_cast<void>(endpoint->awaitState(q::api::EndpointState::CLOSED, deadline));
        const auto result = endpoint->closeAndAwaitTermination();
        runtime.closeAndAwaitTermination();

        std::cout.flush();

        if (result.diagnostic.code != q::api::ErrorCode::NONE) {
            std::cerr << "Endpoint diagnostic: " << static_cast<int>(result.diagnostic.code) << " "
                      << result.diagnostic.stage << '\n';
        }

        if (!result.drained || result.deadlineExpired || result.hadGap || received == 0 ||
            subscription->getDiagnostic().code != q::api::ErrorCode::NONE || !std::cout || !std::cerr) {
            std::cerr << "Quote subscription failed or received no data.\n";

            return 1;
        }

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
