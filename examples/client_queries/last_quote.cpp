// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>
#include <utility>

/** Request one owning Quote with a finite deadline.
 * @param argc Number of command-line arguments.
 * @param argv Borrowed command-line arguments.
 * @return Zero for help or a completed query, two for usage, one for failure.
 */
int main(int argc, char **argv) {
    namespace q = org::ttldtor::qdcxx;
    using namespace std::chrono_literals;

    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "Usage: qdcxx-last-quote ADDRESS SYMBOL\n"
                     "Request one Quote over QTP/TCP within ten seconds and print its bid and ask.\n"
                     "Help opens no connection. The query opens one connection and closes it after the result.\n"
                     "Exit codes: 0 help or completed query, 1 query/timeout/output failure, 2 invalid arguments.\n";

        return std::cout ? 0 : 1;
    }

    if (argc == 2 && std::string_view(argv[1]) == "--help-ru") {
        std::cout
            << "Использование: qdcxx-last-quote АДРЕС СИМВОЛ\n"
               "Запрашивает одну котировку Quote по QTP/TCP за десять секунд и выводит bid и ask.\n"
               "Справка не открывает соединение. Запрос открывает одно соединение и закрывает его после ответа.\n"
               "Коды: 0 справка или готовый запрос, 1 ошибка запроса, таймаута или вывода, 2 неверные аргументы.\n";

        return std::cout ? 0 : 1;
    }

    if (argc != 3) {
        std::cerr << "Expected an address and a symbol.\n";

        return 2;
    }

    try {
        q::api::RuntimeOptions options;

        if (std::string_view(argv[1]).starts_with("127.0.0.1:"))
            options.dnsServers.push_back({{127, 0, 0, 1}});

        q::api::ApiRuntime runtime(std::move(options));
        auto endpoint = runtime.createEndpoint();
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        auto pending = endpoint->getFeed()->requestLastEvent<q::events::Quote>(argv[2], deadline);
        endpoint->connect(argv[1]);

        const auto quote = pending.await();
        std::cout << argv[2] << " bid=" << quote.getBidPrice() << " ask=" << quote.getAskPrice() << '\n';
        const auto close = endpoint->closeAndAwaitTermination();

        if (close.diagnostic.code != q::api::ErrorCode::NONE) {
            std::cerr << "Endpoint close: " << close.diagnostic.stage << " native=" << close.diagnostic.nativeCode
                      << '\n';
        }

        return !close.deadlineExpired && std::cout ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
