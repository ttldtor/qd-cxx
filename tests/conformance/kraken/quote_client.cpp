// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"
#include "qd/events/quote.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <syncstream>

namespace {

namespace q = org::ttldtor::qdcxx;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr auto MAX_LISTEN = 120s;
constexpr auto CONNECT_BUDGET = 12s;
constexpr auto SYMBOL = u"BTC/USD";

/** Parse a positive bounded observation interval in milliseconds.
 * @param token Decimal argument with no suffix.
 * @return Finite observation interval.
 */
std::chrono::milliseconds duration(std::string_view token) {
    int count = 0;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), count);

    if (error != std::errc{} || end != token.data() + token.size() || count <= 0 ||
        std::chrono::milliseconds{count} > MAX_LISTEN) {
        throw std::invalid_argument("invalid observation duration");
    }

    return std::chrono::milliseconds{count};
}

/** Write the received raw fields and their independent decimal projections as one line.
 * @param quote Event borrowed for the duration of its callback.
 */
void emitQuote(const q::events::Quote &quote) {
    if (quote.symbol != SYMBOL) {
        throw std::runtime_error("unexpected Quote symbol");
    }

    std::osyncstream(std::cout) << "QUOTE BTC/USD " << quote.bidPrice.raw << ' ' << quote.askPrice.raw << ' '
                                << quote.bidSize.raw << ' ' << quote.askSize.raw << ' ' << quote.bidPrice.toString()
                                << ' ' << quote.askPrice.toString() << ' ' << quote.bidSize.toString() << ' '
                                << quote.askSize.toString() << ' ' << quote.bidTimeMs << ' ' << quote.askTimeMs << ' '
                                << quote.timeMs << ' ' << quote.eventTime << ' ' << quote.sequence << ' '
                                << quote.timeNanoPart << ' ' << static_cast<unsigned>(quote.bidExchangeCode) << ' '
                                << static_cast<unsigned>(quote.askExchangeCode) << '\n'
                                << std::flush_emit;
}

/** Use an independent public Feed endpoint with its own lifetime and demand.
 * @param address Gateway QTP endpoint.
 * @param listenFor Finite observation interval or zero for supervisor commands.
 * @return Zero after at least one Quote and a local drain.
 */
int run(std::string_view address, std::chrono::milliseconds listenFor) {
    std::atomic<unsigned> received{0};
    q::api::ApiRuntime runtime;
    auto endpoint = runtime.createEndpoint();
    auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addEventListener([&received](std::span<const q::events::Quote> quotes) {
        for (const auto &quote : quotes) {
            emitQuote(quote);
            received.fetch_add(1, std::memory_order_relaxed);
        }
    });
    subscription->addSymbols("BTC/USD");
    endpoint->connect(address);

    const auto connection = endpoint->awaitState(q::api::EndpointState::CONNECTED, Clock::now() + CONNECT_BUDGET);
    const bool ready = connection.status == q::api::WaitStatus::REACHED;
    bool stopRequested = false;

    if (ready) {
        std::osyncstream(std::cout) << "CLIENT_READY\n" << std::flush_emit;

        if (listenFor > 0ms) {
            const auto observation = endpoint->awaitState(q::api::EndpointState::CLOSED, Clock::now() + listenFor);
            stopRequested = observation.status == q::api::WaitStatus::DEADLINE_EXPIRED;
        } else {
            std::string command;
            stopRequested = static_cast<bool>(std::getline(std::cin, command)) && command == "STOP";
        }
    }

    const auto result = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();
    const auto count = received.load(std::memory_order_relaxed);
    const auto failure = subscription->getDiagnostic();
    std::cout << "CLIENT_STOP " << count << std::endl;

    if (!ready || !stopRequested || count == 0 || result.deadlineExpired || result.hadGap || !result.drained ||
        failure.code != q::api::ErrorCode::NONE) {
        std::cerr << "Client result: connected=" << ready << " stop=" << stopRequested << " drained=" << result.drained
                  << " timeout=" << result.deadlineExpired << " gap=" << result.hadGap
                  << " endpoint=" << static_cast<int>(result.diagnostic.code)
                  << " subscription=" << static_cast<int>(failure.code) << '\n';

        return 1;
    }

    return std::cout ? 0 : 1;
}

} // namespace

/** Receive BTC/USD in a separate QTP process until STOP or an explicit finite interval. */
int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "Usage: qdcxx_kraken_quote_client ADDRESS [DURATION_MS]\n";

        return 2;
    }

    try {
        return run(argv[1], argc == 3 ? duration(argv[2]) : 0ms);
    } catch (const std::exception &error) {
        std::cerr << "Quote client: " << error.what() << '\n';

        return 1;
    }
}
