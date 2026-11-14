// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/api/publisher.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/order_source.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <syncstream>
#include <thread>
#include <utility>

namespace {
namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

constexpr std::string_view SYMBOL = "IBM";
constexpr std::u16string_view SYMBOL16 = u"IBM";
constexpr std::string_view USAGE = R"(Usage: qdcxx-distribution source [PORT]
       qdcxx-distribution server PORT SOURCE_PORT
       qdcxx-distribution client PORT [SYMBOL...]

Run a plain loopback QTP source, relay, or receiving application.
Source and server accept port 0 and print the selected PORT. The source defaults to port 0.
The client defaults to IBM and prints Quote, TimeAndSale and Order on one mixed session.

Commands on stdin:
  source  QUOTE, EVENTS, REMOVE, STATS, STOP
  server  STATS, STOP
  client  CLEAR, STATS, STOP
EOF stops the selected role. The source publishes synthetic data and reports IBM demand.
Exit codes: 0 stopped or help, 1 runtime/I/O failure, 2 invalid arguments or command.
)";
constexpr std::string_view USAGE_RU = R"(Использование: qdcxx-distribution source [PORT]
               qdcxx-distribution server PORT SOURCE_PORT
               qdcxx-distribution client PORT [SYMBOL...]

Запуск источника, сервера или клиента QTP на loopback.
Источник и сервер принимают порт 0 и печатают выбранный PORT. У источника порт 0 по умолчанию.
Клиент по умолчанию подписывается на IBM и печатает Quote, TimeAndSale и Order из одной сессии.

Команды stdin:
  source  QUOTE, EVENTS, REMOVE, STATS, STOP
  server  STATS, STOP
  client  CLEAR, STATS, STOP
EOF останавливает выбранную роль. Источник публикует синтетические данные и показывает спрос IBM.
Коды выхода: 0 остановка или справка, 1 ошибка выполнения или ввода-вывода, 2 ошибочные аргументы или команда.
)";

template <class... T> void emit(T &&...parts) {
    std::osyncstream line(std::cout);
    (line << ... << std::forward<T>(parts)) << '\n' << std::flush_emit;
}

std::optional<std::uint16_t> port(std::string_view input, bool zeroAllowed) {
    unsigned value{};
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);

    if (error != std::errc{} || end != input.data() + input.size() || value > 65535 || (!value && !zeroAllowed))
        return {};

    return static_cast<std::uint16_t>(value);
}

bool report(std::expected<void, q::api::Diagnostic> value) {
    if (value)
        return true;

    std::cerr << value.error().stage << '\n';

    return false;
}

q::core::WideDecimal decimal(std::int64_t significand, unsigned digits = 0) {
    constexpr std::int64_t SIGNIFICAND_UNIT = 256;
    constexpr std::int64_t RANK_INTEGER = 128;

    return {significand * SIGNIFICAND_UNIT + RANK_INTEGER + static_cast<std::int64_t>(digits)};
}

bool publish(const q::api::Publisher &publisher, std::string_view command) {
    if (command == "QUOTE") {
        q::events::Quote quote{};
        quote.symbol = SYMBOL16;
        quote.bidPrice = decimal(101);
        quote.askPrice = decimal(102);
        quote.bidSize = decimal(3);
        quote.askSize = decimal(3);

        return report(publisher.publish(quote));
    }

    namespace flags = q::events::indexed_event_flags;
    q::events::Order order;
    order.symbol = SYMBOL16;
    order.index = 7;

    if (command == "REMOVE") {
        order.eventFlags = flags::REMOVE_EVENT;

        return report(publisher.publish(order));
    }

    q::events::TimeAndSale sale;
    sale.symbol = SYMBOL16;
    sale.index = std::int64_t{1700000000} << 32;
    sale.price = decimal(102);
    sale.size = decimal(4);
    order.price = decimal(10375, 2);
    order.size = decimal(5);
    order.flags = 4; // BUY in the native Order wire flag word.
    order.eventFlags = flags::SNAPSHOT_BEGIN | flags::TX_PENDING;

    if (!report(publisher.publish(sale)) || !report(publisher.publish(sale)) || !report(publisher.publish(order)))
        return false;

    order.index = 0;
    order.eventFlags = flags::SNAPSHOT_END;
    order.price = decimal(1035, 1);
    order.size = decimal(2);

    return report(publisher.publish(order));
}

std::jthread observeDemand(q::api::PublisherEndpoint &endpoint) {
    return std::jthread([&endpoint](std::stop_token stop) {
        constexpr std::array families{q::core::SubscriptionFamily::TICKER, q::core::SubscriptionFamily::STREAM,
                                      q::core::SubscriptionFamily::HISTORY};
        constexpr std::array names{"Quote", "TimeAndSale", "Order"};
        std::array<bool, families.size()> present{};

        while (!stop.stop_requested()) {
            for (std::size_t i = 0; i < families.size(); ++i) {
                if (endpoint.awaitDemand(families[i], SYMBOL16, !present[i], Clock::now() + 25ms)) {
                    present[i] = !present[i];
                    emit(present[i] ? "ADD " : "REMOVE ", names[i]);
                }
            }
        }
    });
}

void printStatistics(const q::api::PublisherEndpoint &endpoint) {
    const auto stats = endpoint.getStatistics();
    const auto source = endpoint.getSourceStatus();
    constexpr std::array names{"TICKER", "STREAM", "HISTORY"};
    emit("STATS peers=", stats.activePeers, " retiredPeers=", stats.retiredPeers.value,
         " publications=", stats.pendingPublications, " sourceGeneration=", source.generation,
         " sourceConnected=", source.connected, " sourceStage=", source.diagnostic.stage);

    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto &family = stats.families[i];
        const auto value = [](q::core::StatisticCounter counter) {
            switch (counter.availability) {
            case q::core::StatisticAvailability::AVAILABLE:
                return std::to_string(counter.value);
            case q::core::StatisticAvailability::SATURATED:
                return std::string("saturated");
            default:
                return std::string("unavailable");
            }
        };
        emit("STATS ", names[i], " ingressFrames=", value(family.ingressFrames),
             " ingressRecords=", value(family.ingressRecords), " sentFrames=", value(family.sentFrames),
             " desiredKeys=", family.desiredKeys, " pendingSourceRecords=", family.pendingSourceRecords,
             " queuedDeliveryRecords=", family.queuedDeliveryRecords,
             " retainedOutputRecords=", family.retainedOutputRecords,
             " knownDiscardedRecords=", value(family.knownDiscardedRecords));
    }
}

int source(q::transport::RuntimeBootstrap &bootstrap, std::uint16_t listenPort) {
    q::api::PublisherOptions options;
    options.listener.port = listenPort;
    options.mixed = true;
    auto created = q::api::PublisherEndpoint::create(bootstrap, options);

    if (!created) {
        std::cerr << created.error().stage << '\n';

        return 1;
    }

    auto &endpoint = **created;
    emit("PORT ", endpoint.port());
    auto demand = observeDemand(endpoint);
    const auto publisher = endpoint.getPublisher();
    int status = 0;

    for (std::string command; std::getline(std::cin, command) && command != "STOP";) {
        if (command == "STATS") {
            printStatistics(endpoint);

            continue;
        }

        if (command != "QUOTE" && command != "EVENTS" && command != "REMOVE") {
            std::cerr << "Invalid source command\n";
            status = 2;

            break;
        }

        if (!publish(publisher, command)) {
            status = 1;

            break;
        }

        emit("SENT ", command);
    }

    demand.request_stop();
    demand.join();
    endpoint.closeAndAwaitTermination();

    const auto diagnostic = endpoint.getDiagnostic();

    if (!status && diagnostic.code != q::api::ErrorCode::NONE) {
        std::cerr << diagnostic.stage << '\n';
        status = 1;
    }

    return status;
}

int server(q::transport::RuntimeBootstrap &bootstrap, std::uint16_t listenPort, std::uint16_t sourcePort) {
    q::api::PublisherOptions options;
    options.listener.port = listenPort;
    options.mixed = true;
    options.source = q::api::PublisherSourceOptions{"127.0.0.1", sourcePort};
    auto created = q::api::PublisherEndpoint::create(bootstrap, options);

    if (!created) {
        std::cerr << created.error().stage << '\n';

        return 1;
    }

    auto &endpoint = **created;
    emit("PORT ", endpoint.port());
    std::jthread generation([&](std::stop_token stop) {
        std::uint64_t current = 0;

        while (!stop.stop_requested()) {
            if (endpoint.awaitSourceGeneration(current, Clock::now() + 100ms)) {
                current = endpoint.getSourceStatus().generation;
                emit("SOURCE ", current);
            }
        }
    });
    bool valid = true;

    for (std::string command; std::getline(std::cin, command) && command != "STOP";) {
        if (command != "STATS") {
            valid = false;

            break;
        }

        printStatistics(endpoint);
    }

    generation.request_stop();
    generation.join();
    endpoint.closeAndAwaitTermination();

    if (!valid) {
        std::cerr << "Invalid server command\n";

        return 2;
    }

    const auto diagnostic = endpoint.getDiagnostic();

    if (diagnostic.code != q::api::ErrorCode::NONE) {
        std::cerr << diagnostic.stage << " (detail " << diagnostic.detailCode << ")\n";

        return 1;
    }

    return 0;
}

int client(q::transport::RuntimeBootstrap &bootstrap, std::uint16_t sourcePort, std::span<char *> symbols) {
    q::api::ApiRuntime runtime(bootstrap);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::MIXED;
    options.maximumAttempts = 1;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription(
        {q::api::EventType::QUOTE, q::api::EventType::TIME_AND_SALE, q::api::EventType::ORDER});
    subscription->addEventListener([](std::span<const q::api::RegisteredEvent> batch) {
        for (const auto &event : batch) {
            if (const auto *quote = std::get_if<q::events::Quote>(&event))
                emit("QUOTE ", quote->getBidPrice());
            else if (const auto *sale = std::get_if<q::events::TimeAndSale>(&event))
                emit("SALE ", sale->getPrice());
            else if (const auto *order = std::get_if<q::events::Order>(&event);
                     order && order->getSourceId() == q::events::OrderSource::DEFAULT.id())
                emit("ORDER ", order->getIndex(), ' ', order->eventFlags);
        }
    });

    if (symbols.empty())
        subscription->addSymbols(SYMBOL);

    for (const auto *symbol : symbols)
        subscription->addSymbols(symbol);

    endpoint->connect("127.0.0.1:" + std::to_string(sourcePort));
    emit("STARTED");
    int status = 0;

    for (std::string command; std::getline(std::cin, command) && command != "STOP";) {
        if (command == "STATS") {
            const auto connection = endpoint->getStatistics().connection;
            const auto demand = subscription->getStatistics();
            emit("STATS connections=", connection.establishedConnections.value,
                 " activeSelectors=", demand.activeSelectors, " pendingEvents=", demand.pendingEvents,
                 " discardedEvents=", demand.discardedEvents.value);

            continue;
        }

        if (command != "CLEAR") {
            std::cerr << "Invalid client command\n";
            status = 2;

            break;
        }

        subscription->clearSymbols();
        emit("CLEARED");
    }

    const auto beforeClose = endpoint->getDiagnostic();
    const auto closed = endpoint->closeAndAwaitTermination();
    const auto stage = closed.diagnostic.stage;
    constexpr int WINDOWS_CONNECTION_ABORTED = 1236;
    const bool closingRead = closed.diagnostic.code == q::api::ErrorCode::TRANSPORT_FAILURE &&
                             ((stage == "recovery-peer-end" && closed.diagnostic.nativeCode == 0) ||
                              (stage == "socket-read" && closed.diagnostic.nativeCode == WINDOWS_CONNECTION_ABORTED));

    if (!status && (beforeClose.code != q::api::ErrorCode::NONE || closed.deadlineExpired ||
                    (closed.diagnostic.code != q::api::ErrorCode::NONE && !closingRead))) {
        const auto failure = beforeClose.code != q::api::ErrorCode::NONE ? beforeClose.stage : stage;
        std::cerr << (closed.deadlineExpired ? "Feed close deadline expired" : failure) << '\n';
        status = 1;
    } else if (closingRead) {
        std::cerr << stage << " during local stop (native " << closed.diagnostic.nativeCode << ")\n";
    }

    return status;
}
} // namespace

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << USAGE;

        return !std::cout;
    }

    if (argc == 2 && std::string_view(argv[1]) == "--help-ru") {
        std::cout << USAGE_RU;

        return !std::cout;
    }

    const std::string_view role = argv[1];
    const bool sourceRole = role == "source";
    const bool serverRole = role == "server";
    const bool clientRole = role == "client";
    const auto first = argc >= 3 ? port(argv[2], !clientRole) : std::optional<std::uint16_t>{0};
    const auto upstream = serverRole && argc == 4 ? port(argv[3], false) : std::nullopt;
    const auto symbols = std::span(argv, static_cast<std::size_t>(argc)).subspan(std::min(argc, 3));
    const auto validSymbol = [](std::string_view symbol) {
        return !symbol.empty() && symbol.front() != '*';
    };

    if (!first || !((sourceRole && argc <= 3) || (serverRole && upstream) ||
                    (clientRole && argc >= 3 && std::ranges::all_of(symbols, validSymbol)))) {
        std::cerr << "Invalid arguments. Use --help.\n";

        return 2;
    }

    try {
        auto bootstrap = q::transport::RuntimeBootstrap::create();

        if (!bootstrap) {
            std::cerr << bootstrap.error().stage << '\n';

            return 1;
        }

        if (sourceRole)
            return source(*bootstrap, *first);

        if (serverRole)
            return server(*bootstrap, *first, *upstream);

        return client(*bootstrap, *first, symbols);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
