// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/api/feed.hpp>
#include <qd/api/order_book_model.hpp>
#include <qd/events/candle_symbol.hpp>

#include <charconv>
#include <chrono>
#include <iostream>
#include <string_view>
#include <utility>

namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;

const char *stateName(q::api::HistorySnapshotState state) {
    switch (state) {
    case q::api::HistorySnapshotState::NOT_SUBSCRIBED:
        return "NOT_SUBSCRIBED";
    case q::api::HistorySnapshotState::INCOMPLETE:
        return "INCOMPLETE";
    case q::api::HistorySnapshotState::TRUNCATED:
        return "TRUNCATED";
    case q::api::HistorySnapshotState::COMPLETE:
        return "COMPLETE";
    }

    return "UNKNOWN";
}

int help(bool russian) {
    if (russian)
        std::cout << "Использование: qdcxx-client-queries --run HOST:PORT [HISTORY_HOST:PORT [FROM_MS TO_MS]]\n"
                     "Запрашивает Quote, дневные Candle за последние 24 часа и снимок Order#DEFAULT для AAPL.\n"
                     "Адрес HISTORY по умолчанию совпадает с первым; диапазон задан включительно в миллисекундах.\n"
                     "--help и --help-ru показывают справку без сети. Таймаут 10 секунд, TCP без TLS.\n"
                     "Коды: 0 — результаты и завершение без таймаута; 1 — отказ или ошибка; 2 — аргументы.\n"
                     "Признаки drain, gap и диагностика показываются; потери не исключаются.\n";
    else
        std::cout << "Usage: qdcxx-client-queries --run HOST:PORT [HISTORY_HOST:PORT [FROM_MS TO_MS]]\n"
                     "Requests Quote, daily Candle for the last 24 hours and an Order#DEFAULT snapshot for AAPL.\n"
                     "HISTORY address defaults to the first; optional bounds are inclusive Unix milliseconds.\n"
                     "--help and --help-ru are offline. Timeout is 10 seconds; plain TCP without TLS.\n"
                     "Exit 0: results and retirement without timeout; 1: refusal or failure; 2: invalid arguments.\n"
                     "Drain, gap and diagnostics are reported; lossless streaming is not promised.\n";

    return std::cout ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help"))
        return help(false);

    if (argc == 2 && std::string_view(argv[1]) == "--help-ru")
        return help(true);

    if ((argc != 3 && argc != 4 && argc != 6) || std::string_view(argv[1]) != "--run" ||
        std::string_view(argv[2]).empty() || (argc >= 4 && std::string_view(argv[3]).empty())) {
        std::cerr << "Invalid arguments; use --help.\n";

        return 2;
    }

    const auto now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    std::int64_t fromTime = now - std::chrono::duration_cast<std::chrono::milliseconds>(24h).count();
    std::int64_t toTime = now;

    if (argc == 6) {
        const auto parse = [](std::string_view text, std::int64_t &value) {
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);

            return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        };

        if (!parse(argv[4], fromTime) || !parse(argv[5], toTime) || fromTime > toTime) {
            std::cerr << "Invalid time range; use --help.\n";

            return 2;
        }
    }

    try {
        q::api::RuntimeOptions limits;
        const std::string_view address(argv[2]);
        const std::string_view historyAddress = argc >= 4 ? std::string_view(argv[3]) : address;

        if (address.starts_with("127.0.0.1:") || historyAddress.starts_with("127.0.0.1:"))
            limits.dnsServers.push_back({{127, 0, 0, 1}});

        q::api::ApiRuntime runtime(std::move(limits));
        q::api::EndpointOptions historyOptions;
        historyOptions.contract = q::api::DeliveryContract::HISTORY;
        auto ticker = runtime.createEndpoint();
        auto history = runtime.createEndpoint(historyOptions);
        const auto deadline = std::chrono::steady_clock::now() + 10s;

        const auto candleSymbol = q::events::CandleSymbol::valueOf(u"AAPL{=d}");
        const auto symbol = candleSymbol.getSymbol();

        if (!symbol)
            throw std::runtime_error("missing candle symbol");

        auto last = ticker->getFeed()->requestLastEvent<q::events::Quote>(u"AAPL", deadline);
        auto candles = history->getFeed()->requestTimeSeries<q::events::Candle>(*symbol, fromTime, toTime, deadline);
        const auto &source = q::events::IndexedEventSource::DEFAULT;
        auto orders = history->getFeed()->requestIndexedEvents<q::events::Order>(u"AAPL", source, deadline);
        ticker->connect(address);
        history->connect(historyAddress);
        const auto quote = last.await();
        const auto candleCut = candles.get();
        const auto orderCut = orders.get();
        q::api::OrderBookModel book;
        const auto applied = book.processEvents(u"AAPL", source.id(), orderCut.events);

        if (!applied)
            throw std::runtime_error("order-book input refused");

        const auto bookCut = book.getSnapshot(u"AAPL");

        if (!bookCut)
            throw std::runtime_error("order-book snapshot refused");

        std::cout << "Quote bid=" << quote.getBidPrice() << " ask=" << quote.getAskPrice() << '\n'
                  << "Candle snapshot state=" << stateName(candleCut.state) << " count=" << candleCut.events.size()
                  << '\n';

        for (const auto &candle : candleCut.events)
            std::cout << "Candle time=" << candle.getTime() << " close=" << candle.getClose() << '\n';

        std::cout << "Order snapshot state=" << stateName(orderCut.state) << " bids=" << bookCut->buyOrders.size()
                  << " asks=" << bookCut->sellOrders.size() << '\n';

        if (ticker->getState() != q::api::EndpointState::CONNECTED ||
            history->getState() != q::api::EndpointState::CONNECTED ||
            ticker->getDiagnostic().code != q::api::ErrorCode::NONE ||
            history->getDiagnostic().code != q::api::ErrorCode::NONE)
            throw std::runtime_error("source failed before completed queries were closed");

        const auto tickerClose = ticker->closeAndAwaitTermination();
        const auto historyClose = history->closeAndAwaitTermination();

        for (const auto &closed : {tickerClose, historyClose}) {
            if (!closed.drained || closed.diagnostic.code != q::api::ErrorCode::NONE) {
                std::cerr << "Close: drained=" << closed.drained << " gap=" << closed.hadGap
                          << " deadline=" << closed.deadlineExpired
                          << " code=" << static_cast<int>(closed.diagnostic.code)
                          << " stage=" << closed.diagnostic.stage << " native=" << closed.diagnostic.nativeCode << '\n';
            }
        }

        runtime.closeAndAwaitTermination();
        const auto tickerStats = ticker->getStatistics();
        const auto historyStats = history->getStatistics();
        const auto terminal = [](q::api::EndpointState state) {
            return state == q::api::EndpointState::CLOSED || state == q::api::EndpointState::FAILED;
        };
        std::cout << "Retired ticker final=" << tickerStats.final
                  << " active=" << tickerStats.connection.activeConnections << " history final=" << historyStats.final
                  << " active=" << historyStats.connection.activeConnections << '\n';

        return !tickerClose.deadlineExpired && !historyClose.deadlineExpired && terminal(ticker->getState()) &&
                       terminal(history->getState()) && tickerStats.final && historyStats.final &&
                       tickerStats.connection.activeConnections == 0 &&
                       historyStats.connection.activeConnections == 0 && std::cout
                   ? 0
                   : 1;
    } catch (const q::api::Error &error) {
        std::cerr << "Feed refusal: code=" << static_cast<int>(error.diagnostic().code)
                  << " stage=" << error.diagnostic().stage << '\n';
    } catch (const std::exception &error) {
        std::cerr << "Failure: " << error.what() << '\n';
    }

    return 1;
}
