// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"
#include "qd/events/event_flags.hpp"
#include "session.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>

namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "Usage: qdcxx-feed-candle HOST:PORT CANDLE_SYMBOL FROM_MS\n"
                     "Receive QTP HISTORY candles for ten seconds. Use a canonical symbol such as SPY{=m}.\n"
                     "FROM_MS is the inclusive Unix millisecond bound, 0..2147483647999. Prints time and OHLC.\n"
                     "Exit 0 after a candle and clean drain, 1 on failure or no candles, 2 for invalid input.\n";
        std::cout.flush();

        return std::cout ? 0 : 1;
    }

    if (argc != 4) {
        std::cerr << "Expected an address, Candle symbol and lower time bound. Use --help.\n";

        return 2;
    }

    constexpr std::int64_t MAX_FROM_TIME = std::int64_t{std::numeric_limits<std::int32_t>::max()} * 1000 + 999;
    const std::string_view fromArgument{argv[3]};
    std::int64_t fromTime{};
    const auto end = fromArgument.data() + fromArgument.size();
    const auto parsed = std::from_chars(fromArgument.data(), end, fromTime);

    if (parsed.ec != std::errc{} || parsed.ptr != end || fromTime < 0 || fromTime > MAX_FROM_TIME) {
        std::cerr << "Expected a Unix millisecond time in 0..2147483647999.\n";

        return 2;
    }

    try {
        auto received = std::make_shared<std::atomic<bool>>(false);
        q::api::EndpointOptions options;
        options.contract = q::api::DeliveryContract::HISTORY;
        auto endpoint = q::api::Endpoint::create(options);
        auto candles = endpoint->getFeed()->createTimeSeriesSubscription<q::events::Candle>();

        candles->setFromTime(fromTime);
        candles->addEventListener([received](std::span<const q::events::Candle> batch) {
            for (const auto &candle : batch) {
                if (candle.eventFlags & q::events::indexed_event_flags::REMOVE_EVENT) {
                    continue;
                }

                std::cout << candle.getTime() << " open=" << candle.getOpen() << " high=" << candle.getHigh()
                          << " low=" << candle.getLow() << " close=" << candle.getClose() << '\n';
                received->store(true);
            }
        });
        candles->addSymbols(argv[2]);
        endpoint->connect(argv[1]);

        std::this_thread::sleep_for(10s);
        const auto result = q::examples::finish(*endpoint);
        std::cout.flush();

        if (!received->load() || !result.drained || result.deadlineExpired || result.hadGap || !std::cout ||
            candles->getDiagnostic().code != q::api::ErrorCode::NONE) {
            std::cerr << "Candle subscription failed or received no candles.\n";

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
