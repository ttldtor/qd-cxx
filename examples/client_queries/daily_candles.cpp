// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {

/** Parse an inclusive Unix millisecond boundary.
 * @param text Borrowed decimal argument.
 * @param time Output integer on success.
 * @return True if the whole argument represents a signed millisecond.
 */
bool parseTime(std::string_view text, std::int64_t &time) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), time);

    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

} // namespace

/** Query and print a finite range of daily candles.
 * @param argc Number of command-line arguments.
 * @param argv Borrowed command-line arguments.
 * @return Zero for help or complete coverage, one for failure, two for usage.
 */
int main(int argc, char **argv) {
    namespace q = org::ttldtor::qdcxx;
    using namespace std::chrono_literals;

    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "Usage: qdcxx-daily-candles ADDRESS SYMBOL FROM_MS TO_MS\n"
                     "Read daily Candle events over QTP/TCP. Use a base ASCII symbol and inclusive Unix milliseconds.\n"
                     "The request waits up to ten seconds. Only complete coverage exits zero.\n"
                     "Help opens no connection. A query opens one connection and closes it after the result.\n"
                     "Exit codes: 0 complete or help, 1 query/timeout/output failure, 2 invalid arguments.\n";

        return std::cout ? 0 : 1;
    }

    std::int64_t fromTime = 0;
    std::int64_t toTime = 0;

    if (argc != 5 || !parseTime(argv[3], fromTime) || !parseTime(argv[4], toTime) || fromTime > toTime) {
        std::cerr << "Expected an address, a base symbol and an inclusive millisecond range.\n";

        return 2;
    }

    const std::string_view symbolArgument(argv[2]);
    constexpr std::size_t MAX_SYMBOL_BYTES = 128;
    constexpr unsigned char ASCII_DELETE = 127;

    if (symbolArgument.empty() || symbolArgument.size() > MAX_SYMBOL_BYTES) {
        std::cerr << "Invalid base symbol length.\n";

        return 2;
    }

    std::u16string baseSymbol;

    for (const unsigned char unit : symbolArgument) {
        if (unit <= ' ' || unit >= ASCII_DELETE || unit == '{' || unit == '}') {
            std::cerr << "Invalid base symbol.\n";

            return 2;
        }

        baseSymbol.push_back(static_cast<char16_t>(unit));
    }

    try {
        q::api::RuntimeOptions options;

        if (std::string_view(argv[1]).starts_with("127.0.0.1:"))
            options.dnsServers.push_back({{127, 0, 0, 1}});

        q::api::ApiRuntime runtime(std::move(options));
        q::api::EndpointOptions endpointOptions;
        endpointOptions.contract = q::api::DeliveryContract::HISTORY;
        auto endpoint = runtime.createEndpoint(endpointOptions);
        const auto symbol = q::events::CandleSymbol::valueOf(baseSymbol, q::events::CandlePeriod::DAY);
        const auto text = symbol.getSymbol();

        if (!text) {
            std::cerr << "Invalid daily candle symbol.\n";

            return 2;
        }

        const auto deadline = std::chrono::steady_clock::now() + 10s;
        auto pending = endpoint->getFeed()->requestTimeSeries<q::events::Candle>(*text, fromTime, toTime, deadline);
        endpoint->connect(argv[1]);

        const auto snapshot = pending.get();
        const auto close = endpoint->closeAndAwaitTermination();

        if (close.diagnostic.code != q::api::ErrorCode::NONE)
            std::cerr << "Endpoint close: " << close.diagnostic.stage << '\n';

        if (snapshot.state != q::api::HistorySnapshotState::COMPLETE) {
            std::cerr << "The source did not establish complete coverage for the requested range.\n";

            return 1;
        }

        for (const auto &candle : snapshot.events)
            std::cout << candle.getTime() << " close=" << candle.getClose() << '\n';

        return !close.deadlineExpired && std::cout ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
