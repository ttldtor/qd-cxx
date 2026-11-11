// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/endpoint_configuration.hpp"
#include "qd/events/order_source.hpp"

#include "signal_stop.hpp"

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace {
namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;
constexpr auto DEFAULT_DURATION = 10s;
constexpr auto MAXIMUM_DURATION = 600000ms;
constexpr std::size_t MAXIMUM_ADDRESS_BYTES = 4096;
constexpr char16_t MAXIMUM_ASCII_UNIT = 0x7f;

class RuntimeShutdown final {
    q::api::ApiRuntime &runtime_;

    public:
    explicit RuntimeShutdown(q::api::ApiRuntime &runtime) noexcept : runtime_(runtime) {
    }

    ~RuntimeShutdown() {
        runtime_.closeAndAwaitTermination();
    }
};

void printSymbol(std::u16string_view symbol) {
    for (auto unit : symbol) {
        if (unit > MAXIMUM_ASCII_UNIT) {
            throw std::runtime_error("non-ASCII symbol");
        }

        std::cout << static_cast<char>(unit);
    }
}

void printEvent(const q::api::RegisteredEvent &event) {
    std::visit(
        [](const auto &value) {
            using Event = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Event, q::events::Quote>) {
                std::cout << "Quote ";
                printSymbol(value.symbol);
                std::cout << " bid=" << value.getBidPrice() << " ask=" << value.getAskPrice();
            } else if constexpr (std::is_same_v<Event, q::events::Trade>) {
                std::cout << "Trade ";
                printSymbol(value.symbol);
                std::cout << " price=" << value.getPrice() << " size=" << value.getSizeAsDouble();
            } else if constexpr (std::is_same_v<Event, q::events::Order>) {
                std::cout << "Order ";
                printSymbol(value.symbol);
                std::cout << " source=" << value.getSourceId() << " index=" << value.getIndex()
                          << " price=" << value.getPrice() << " size=" << value.getSizeAsDouble();
            } else {
                throw std::runtime_error("unexpected event type");
            }

            std::cout << '\n';
        },
        event);
}

int help(bool russian) {
    if (russian) {
        std::cout << "Использование: qdcxx-feed-market --run [--address HOST:PORT] [--duration-ms N]\n"
                     "Quote, Trade и производные Order для AAPL, AAPL&Q; затем добавляется MSFT.\n"
                     "Адрес по умолчанию demo.dxfeed.com:7300, длительность 10000 мс (2..600000).\n"
                     "REGIONAL_BID Order зависит от доступности региональных Quote у источника.\n"
                     "Данные demo могут быть задержаны или недоступны из-за прав и расписания.\n"
                     "--help / --help-ru показывают справку без сети; Ctrl+C запрашивает завершение.\n"
                     "TCP без TLS и credentials. Выводятся события и итоговая статистика.\n"
                     "Коды: 0 получены события и выполнен drain; 1 ошибка или нет событий;\n"
                     "2 неверные аргументы; 130 прерывание. Создаются поток SDK и соединение.\n";
    } else {
        std::cout << "Usage: qdcxx-feed-market --run [--address HOST:PORT] [--duration-ms N]\n"
                     "Quote, Trade and derived Order for AAPL, AAPL&Q; then MSFT is added.\n"
                     "Default address demo.dxfeed.com:7300, duration 10000 ms (2..600000).\n"
                     "REGIONAL_BID Order depends on regional Quote availability from the source.\n"
                     "Demo data may be delayed or unavailable due to entitlement and schedule.\n"
                     "--help / --help-ru show offline help; Ctrl+C requests shutdown.\n"
                     "Plain TCP without TLS or credentials. Prints events and final statistics.\n"
                     "Exits: 0 events received and drained; 1 failure or no events;\n"
                     "2 invalid arguments; 130 interruption. Starts an SDK worker and connection.\n";
    }

    std::cout.flush();

    return std::cout ? 0 : 1;
}
} // namespace

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        return help(false);
    }

    if (argc == 2 && std::string_view(argv[1]) == "--help-ru") {
        return help(true);
    }

    std::string address = "demo.dxfeed.com:7300";
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(DEFAULT_DURATION);
    bool run = false;
    bool addressSeen = false;
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
        } else if (option == "--duration-ms" && !durationSeen) {
            std::int64_t milliseconds = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), milliseconds);

            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || milliseconds < 2 ||
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

    bool addressAccepted = false;
    try {
        const std::array properties{q::api::EndpointProperty{"dxfeed.address", address}};
        const auto configuration = q::api::EndpointConfiguration::create({}, {}, properties);

        if (configuration.snapshot().configuredAddress != address) {
            std::cerr << "Invalid address; use plain HOST:PORT and --help.\n";

            return 2;
        }

        addressAccepted = true;
        std::atomic<std::size_t> received{0};
        std::stop_source stop;
        q::api::RuntimeOptions runtimeOptions;

        // Numeric loopback bypasses DNS. Provide the SDK's constructor-time resolver input locally.
        if (address.starts_with("127.0.0.1:")) {
            runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
        }

        q::api::ApiRuntime runtime(std::move(runtimeOptions));
        feed_market::SignalStop signals(stop);
        RuntimeShutdown shutdown(runtime);
        auto endpoint = runtime.createEndpoint(configuration);
        auto subscription = endpoint->getFeed()->createSubscription(
            {q::api::EventType::QUOTE, q::api::EventType::TRADE, q::api::EventType::ORDER});
        subscription->addEventListener([&](std::span<const q::api::RegisteredEvent> events) {
            std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);

            for (const auto &event : events) {
                printEvent(event);
                received.fetch_add(1, std::memory_order_relaxed);
            }

            std::cout.flush();

            if (!std::cout) {
                throw std::runtime_error("event output failed");
            }
        });
        subscription->addStateChangeListener([source = stop](auto, auto state) mutable {
            if (state == q::api::SubscriptionState::FAILED) {
                source.request_stop();
            }
        });
        const auto composite = [](std::u16string symbol) {
            return q::api::SubscriptionSelector::indexed(std::move(symbol),
                                                         q::events::OrderSource::COMPOSITE.indexedSource());
        };
        const auto regionalBid =
            q::api::SubscriptionSelector::indexed(u"AAPL&Q", q::events::OrderSource::REGIONAL_BID.indexedSource());
        subscription->setSelectors(std::array{composite(u"AAPL"), regionalBid});
        endpoint->connect();

        const auto start = std::chrono::steady_clock::now();
        const auto first = endpoint->awaitState(q::api::EndpointState::CLOSED, start + duration / 2, stop.get_token());

        if (first.status == q::api::WaitStatus::DEADLINE_EXPIRED) {
            subscription->addSelectors(std::array{composite(u"MSFT")});
            static_cast<void>(endpoint->awaitState(q::api::EndpointState::CLOSED, start + duration, stop.get_token()));
        }

        const auto closed = endpoint->closeAndAwaitTermination();
        runtime.closeAndAwaitTermination();
        const auto stats = subscription->getStatistics();
        const auto connection = endpoint->getStatistics().connection;
        std::cout << "events=" << received.load(std::memory_order_relaxed)
                  << " delivered=" << stats.deliveredEvents.value
                  << " deliveredAvailability=" << static_cast<int>(stats.deliveredEvents.availability)
                  << " desiredSelectors=" << stats.desiredSelectors << " readBytes=" << connection.readBytes.value
                  << " readBytesAvailability=" << static_cast<int>(connection.readBytes.availability) << '\n';
        std::cout.flush();

        if (signals.interrupted()) {
            return 130;
        }

        const auto failure = subscription->getDiagnostic();
        const bool ok = !signals.failed() && closed.drained && !closed.deadlineExpired && !closed.hadGap &&
                        received.load(std::memory_order_relaxed) != 0 && failure.code == q::api::ErrorCode::NONE;

        if (!ok || closed.diagnostic.code != q::api::ErrorCode::NONE) {
            std::cerr << "Feed result: endpoint=" << static_cast<int>(closed.diagnostic.code)
                      << " endpointStage=" << closed.diagnostic.stage
                      << " endpointDetail=" << closed.diagnostic.detailCode
                      << " endpointNative=" << closed.diagnostic.nativeCode << " drained=" << closed.drained
                      << " hadGap=" << closed.hadGap << " subscription=" << static_cast<int>(failure.code)
                      << " subscriptionStage=" << failure.stage << '\n';
        }

        return ok && std::cout && std::cerr ? 0 : 1;
    } catch (const q::api::Error &error) {
        const auto diagnostic = error.diagnostic();

        if (!addressAccepted && (diagnostic.code == q::api::ErrorCode::INVALID_ARGUMENT ||
                                 diagnostic.code == q::api::ErrorCode::UNSUPPORTED_PROFILE)) {
            std::cerr << "Invalid address; use plain HOST:PORT and --help.\n";

            return 2;
        }

        std::cerr << "Feed error=" << static_cast<int>(diagnostic.code) << " stage=" << diagnostic.stage << '\n';

        return 1;
    } catch (...) {
        std::cerr << "Application preparation failed.\n";

        return 1;
    }
}
