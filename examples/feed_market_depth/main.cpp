// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/api/feed.hpp>
#include <qd/api/order_book_model.hpp>
#include <qd/events/order_source.hpp>

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "Usage: qdcxx-feed-market-depth --run [--address HOST:PORT] [--source NAME] [--duration-ms N]\n"
                     "Print committed order books for AAPL and MSFT over TCP.\n"
                     "Defaults: demo.dxfeed.com:7300, source NTV, 10000 ms. Duration: 1..600000 ms.\n"
                     "One connection attempt; source data may be delayed or unavailable.\n"
                     "Help opens no connection. Exit 0: both books received; 1: failure or missing data;\n"
                     "2: invalid arguments. Running starts an SDK worker and a network connection.\n";
        std::cout.flush();

        return std::cout ? 0 : 1;
    }

    std::string address = "demo.dxfeed.com:7300";
    std::string sourceName = "NTV";
    auto duration = 10000ms;
    bool run = false;
    bool addressSeen = false;
    bool durationSeen = false;
    bool sourceSeen = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view option = argv[i];

        if (option == "--run" && !run) {
            run = true;
        } else if (i + 1 < argc && option == "--address" && !std::exchange(addressSeen, true)) {
            address = argv[++i];
        } else if (i + 1 < argc && option == "--source" && !std::exchange(sourceSeen, true)) {
            sourceName = argv[++i];
        } else if (i + 1 < argc && option == "--duration-ms" && !std::exchange(durationSeen, true)) {
            const std::string_view value = argv[++i];
            int milliseconds = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), milliseconds);

            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || milliseconds <= 0 ||
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

    if (!run || address.empty() || address.size() > 4096 || sourceName.empty() || sourceName.size() > 16) {
        std::cerr << "Invalid arguments; use --help.\n";

        return 2;
    }

    try {
        q::api::OrderBookModel book;
        const std::u16string sourceUnits(sourceName.begin(), sourceName.end());
        const auto source = q::events::OrderSource::valueOf(sourceUnits).indexedSource();
        std::atomic<unsigned> books{0};
        q::api::RuntimeOptions options;

        if (address.starts_with("127.0.0.1:"))
            options.dnsServers.push_back({{127, 0, 0, 1}});

        q::api::ApiRuntime runtime(std::move(options));
        q::api::EndpointOptions endpointOptions;
        endpointOptions.contract = q::api::DeliveryContract::MIXED;
        endpointOptions.maximumAttempts = 1;
        auto endpoint = runtime.createEndpoint(endpointOptions);
        auto subscription = endpoint->getFeed()->createSubscription<q::events::Order>();
        subscription->addEventListener([&](std::span<const q::events::Order> orders) {
            for (const auto &order : orders) {
                const auto updated = book.processEvents(order.symbol, order.getSourceId(), std::span(&order, 1));

                if (!updated)
                    throw std::runtime_error("order book update refused");

                const auto snapshot = book.getSnapshot(order.symbol);

                if (!snapshot)
                    throw std::runtime_error("order book snapshot refused");

                if (order.symbol != u"AAPL" && order.symbol != u"MSFT")
                    throw std::runtime_error("unexpected symbol");

                const char *symbol = order.symbol == u"AAPL" ? "AAPL" : "MSFT";
                std::cout << symbol << " bids:";

                for (const auto &bid : snapshot->buyOrders)
                    std::cout << ' ' << bid.getPrice() << '@' << bid.getSizeAsDouble();

                std::cout << " asks:";

                for (const auto &ask : snapshot->sellOrders)
                    std::cout << ' ' << ask.getPrice() << '@' << ask.getSizeAsDouble();

                std::cout << '\n' << std::flush;

                if (!std::cout)
                    throw std::runtime_error("output failed");

                if (!snapshot->buyOrders.empty() || !snapshot->sellOrders.empty())
                    books.fetch_or(order.symbol == u"AAPL" ? 1u : 2u);
            }
        });
        subscription->setSelectors(std::array{q::api::SubscriptionSelector::indexed(u"AAPL", source),
                                              q::api::SubscriptionSelector::indexed(u"MSFT", source)});
        endpoint->connect(address);
        const auto wait =
            endpoint->awaitState(q::api::EndpointState::CLOSED, std::chrono::steady_clock::now() + duration);
        const auto connected = endpoint->getState() == q::api::EndpointState::CONNECTED;
        const auto beforeClose = endpoint->getDiagnostic();
        const auto result = endpoint->closeAndAwaitTermination();
        runtime.closeAndAwaitTermination();

        if (result.diagnostic.code != q::api::ErrorCode::NONE)
            std::cerr << "Retained endpoint diagnostic: " << result.diagnostic.stage << '\n';

        if (wait.status != q::api::WaitStatus::DEADLINE_EXPIRED || !connected ||
            beforeClose.code != q::api::ErrorCode::NONE || !result.drained || result.deadlineExpired || result.hadGap ||
            subscription->getDiagnostic().code != q::api::ErrorCode::NONE || books != 3 || !std::cout) {
            std::cerr << "Order subscription failed: endpoint=" << static_cast<int>(result.diagnostic.code)
                      << " stage=" << result.diagnostic.stage << " drained=" << result.drained
                      << " deadline=" << result.deadlineExpired << " gap=" << result.hadGap << " books=" << books
                      << " subscription=" << static_cast<int>(subscription->getDiagnostic().code) << '\n';

            return 1;
        }

        return 0;
    } catch (const q::api::Error &error) {
        std::cerr << error.what() << '\n';

        return error.diagnostic().code == q::api::ErrorCode::INVALID_ARGUMENT ? 2 : 1;
    } catch (const std::invalid_argument &error) {
        std::cerr << error.what() << '\n';

        return 2;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
