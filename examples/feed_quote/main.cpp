// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;
constexpr auto DEFAULT_DURATION = 10s;      ///< Finite admission interval for this console application.
constexpr auto MAXIMUM_DURATION = 600000ms; ///< Maximum caller-selected interval.
constexpr auto CONTROL_CADENCE = 20ms;      ///< Signal/control observation only; the library owns all feed progress.
constexpr std::size_t MAXIMUM_ADDRESS_BYTES = 4096; ///< Finite command-line endpoint spelling.
constexpr std::size_t MAXIMUM_SYMBOL_BYTES = 128;   ///< Default public selector bound.
constexpr char16_t MAXIMUM_ASCII_UNIT = 0x7f;       ///< Console profile admits exact ASCII selectors only.
volatile std::sig_atomic_t interrupted = 0;

/** Write the selected event symbol without an implicit narrowing conversion.
 * @param symbol Borrowed owning-event text; non-ASCII data fails this console profile.
 */
void printSymbol(std::u16string_view symbol) {
    for (const auto unit : symbol) {
        if (unit > MAXIMUM_ASCII_UNIT) {
            throw std::runtime_error("quote-symbol-profile");
        }

        std::cout << static_cast<char>(unit);
    }
}

/** Publish a signal flag without calling streams, allocation or endpoint methods.
 * @param signal Unused process signal identity.
 */
void onInterrupt(int signal) noexcept {
    static_cast<void>(signal);
    interrupted = 1;
}

/** Print usage before constructing any SDK/schema/runtime owner.
 * @param russian Select the Russian description.
 * @return Zero if help output succeeds, one for output failure.
 */
int help(bool russian) {
    if (russian) {
        std::cout << "Использование: qdcxx-feed-demo --run [параметры]\n"
                     "Публичный API: подключение binary TCP, подписка Quote и вывод цен.\n"
                     "  --address HOST:PORT   Источник, по умолчанию demo.dxfeed.com:7300.\n"
                     "  --symbol SYMBOL       Точный ASCII-символ, по умолчанию AAPL.\n"
                     "  --duration-ms N       Интервал получения 1..600000 мс, по умолчанию 10000.\n"
                     "  --help / --help-ru    Справка без SDK, DNS или подключения.\n"
                     "Используется системный IPv4 DNS; TCP без TLS и credentials.\n"
                     "Вывод: symbol, bid, bidSize, ask, askSize; данные demo могут быть задержанными.\n"
                     "Коды: 0 получены Quote и выполнен local drain; 1 ошибка/нет данных;\n"
                     "2 неверные аргументы; 130 прерывание. Создаются SDK worker и сетевые соединения;\n"
                     "входные файлы и глобальные настройки не изменяются. Ctrl+C запрашивает close.\n";
    } else {
        std::cout << "Usage: qdcxx-feed-demo --run [options]\n"
                     "Public API example: binary TCP connection, Quote subscription and prices.\n"
                     "  --address HOST:PORT   Source; default demo.dxfeed.com:7300.\n"
                     "  --symbol SYMBOL       Exact ASCII selector; default AAPL.\n"
                     "  --duration-ms N       Admission interval 1..600000 ms; default 10000.\n"
                     "  --help / --help-ru    Help without SDK, DNS or connection work.\n"
                     "Uses system IPv4 DNS; plain TCP without TLS or credentials.\n"
                     "Output: symbol, bid, bidSize, ask, askSize; demo data can be delayed.\n"
                     "Exits: 0 Quote received and local drain completed; 1 failure/no data;\n"
                     "2 invalid arguments; 130 interruption. Creates an SDK worker and sockets;\n"
                     "no input files or global settings are changed. Ctrl+C requests close.\n";
    }

    std::cout.flush();

    return std::cout ? 0 : 1;
}
} // namespace

/** Run the finite standalone example through installed public API only.
 * @param argc Process argument count.
 * @param argv Borrowed arguments, retained until return.
 * @return Documented help/usage/data/retirement result.
 */
int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        return help(false);
    }

    if (argc == 2 && std::string_view(argv[1]) == "--help-ru") {
        return help(true);
    }

    std::string address = "demo.dxfeed.com:7300";
    std::string symbol = "AAPL";
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(DEFAULT_DURATION);
    bool run = false;
    bool addressSeen = false;
    bool symbolSeen = false;
    bool durationSeen = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view option = argv[index];

        if (option == "--run" && !run) {
            run = true;
            continue;
        }

        if (index + 1 >= argc) {
            std::cerr << "Invalid arguments; use --help.\n";

            return 2;
        }

        const std::string_view value = argv[++index];

        if (option == "--address" && !addressSeen && !value.empty() && value.size() <= MAXIMUM_ADDRESS_BYTES) {
            address = value;
            addressSeen = true;
        } else if (option == "--symbol" && !symbolSeen && !value.empty() && value.size() <= MAXIMUM_SYMBOL_BYTES) {
            symbol = value;
            symbolSeen = true;
        } else if (option == "--duration-ms" && !durationSeen) {
            std::int64_t milliseconds = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), milliseconds);

            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || milliseconds <= 0 ||
                milliseconds > MAXIMUM_DURATION.count()) {
                std::cerr << "Invalid duration; use --help.\n";

                return 2;
            }

            duration = std::chrono::milliseconds(milliseconds);
            durationSeen = true;
        } else {
            std::cerr << "Invalid arguments; use --help.\n";

            return 2;
        }
    }

    if (!run) {
        std::cerr << "Use --run to connect; use --help for usage.\n";

        return 2;
    }

    if (std::signal(SIGINT, onInterrupt) == SIG_ERR || std::signal(SIGTERM, onInterrupt) == SIG_ERR) {
        return 1;
    }

#ifdef SIGPIPE

    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return 1;
    }

#endif
    try {
        std::atomic<std::size_t> received{0};
        q::api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint();
        auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        subscription->addEventListener([&](std::span<const q::events::Quote> quotes) {
            for (const auto &quote : quotes) {
                printSymbol(quote.symbol);
                std::cout << " bid=" << quote.bidPrice.toString() << " bidSize=" << quote.bidSize.toString()
                          << " ask=" << quote.askPrice.toString() << " askSize=" << quote.askSize.toString() << '\n';
                std::cout.flush();

                if (!std::cout) {
                    throw std::runtime_error("quote-output");
                }

                received.fetch_add(1, std::memory_order_relaxed);
            }
        });
        subscription->addSymbols(symbol);
        endpoint->connect(address);

        const auto cutoff = std::chrono::steady_clock::now() + duration;

        while (!interrupted && std::chrono::steady_clock::now() < cutoff &&
               endpoint->getState() != q::api::EndpointState::FAILED &&
               subscription->getState() != q::api::SubscriptionState::FAILED) {
            std::this_thread::sleep_for(CONTROL_CADENCE);
        }

        const auto closed = endpoint->closeAndAwaitTermination();
        runtime.closeAndAwaitTermination();

        if (interrupted) {
            return 130;
        }

        const auto failure = subscription->getDiagnostic();

        const bool deliveredAndDrained = closed.drained && !closed.deadlineExpired && !closed.hadGap &&
                                         received.load(std::memory_order_relaxed) != 0 &&
                                         failure.code == q::api::ErrorCode::NONE;

        if (!deliveredAndDrained || closed.diagnostic.code != q::api::ErrorCode::NONE) {
            std::cerr << "Feed result: endpoint=" << static_cast<int>(closed.diagnostic.code)
                      << " drained=" << closed.drained << " deadlineExpired=" << closed.deadlineExpired
                      << " hadGap=" << closed.hadGap << " endpointStage=" << closed.diagnostic.stage
                      << " endpointNative=" << closed.diagnostic.nativeCode
                      << " endpointDetail=" << closed.diagnostic.detailCode
                      << " subscription=" << static_cast<int>(failure.code) << " subscriptionStage=" << failure.stage
                      << " records=" << received.load(std::memory_order_relaxed) << '\n';
        }

        return deliveredAndDrained && std::cerr ? 0 : 1;
    } catch (const q::api::Error &error) {
        const auto diagnostic = error.diagnostic();
        std::cerr << "Feed error=" << static_cast<int>(diagnostic.code) << " stage=" << diagnostic.stage
                  << " native=" << diagnostic.nativeCode << " detail=" << diagnostic.detailCode << '\n';

        return 1;
    } catch (...) {
        std::cerr << "Application preparation failed.\n";

        return 1;
    }
}
