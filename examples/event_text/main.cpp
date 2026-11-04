// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/event_text.hpp"
#include "qd/events/quote.hpp"

#include <iostream>
#include <string_view>

namespace {
/** @param russian Select the complete translated input-free help. */
void help(bool russian) {
    if (russian) {
        std::cout
            << "Вывести событие Quote как Java-совместимый UTF-8 текст.\n"
               "Вызов: qdcxx-event-text-example --run | --help | --help-ru\n"
               "Без аргументов/--help: справка EN; --help-ru: справка RU; рабочие входы не нужны.\n"
               "--run: явный UTC-контекст, EventTextLimits{1024,4096,16384,65536}, строгий UTF-16.\n"
               "Результат: одна строка UTF-8 в stdout. Только собственные объекты в памяти.\n"
               "Справка не создаёт контекст и не запускает работу. Файлы, сеть и провайдер timezone не используются.\n"
               "Коды: 0 успех/справка, 1 ошибка форматирования/вывода, 2 неверные аргументы.\n";
    } else {
        std::cout << "Print one Quote event as Java-compatible UTF-8 text.\n"
                     "Usage: qdcxx-event-text-example --run | --help | --help-ru\n"
                     "No arguments/--help: English help; --help-ru: Russian help; no working inputs required.\n"
                     "--run: explicit UTC context, EventTextLimits{1024,4096,16384,65536}, strict UTF-16.\n"
                     "Output: one UTF-8 line on stdout. Side effects: owned in-memory objects only.\n"
                     "Help creates no context and starts no work. No files, network or timezone provider are used.\n"
                     "Exit: 0 success/help, 1 formatting/output failure, 2 invalid arguments.\n";
    }
}

/** @return Zero after the complete bounded public API example; nonzero on formatting/output failure. */
int run() {
    // event-text-usage-begin
    using namespace org::ttldtor::qdcxx::events;

    Quote quote{};
    quote.symbol = u"DEMO/\u20ac";
    quote.eventTime = 1;

    const auto context = EventTextContext::utc();
    const EventTextLimits limits{1024, 4096, 16384, 65536};
    const auto text = toStringUtf8(quote, context, limits, MalformedUtf16::REJECT);

    if (!text) {
        std::cerr << "Event formatting failed: " << static_cast<int>(text.error()) << '\n';

        return 1;
    }

    std::cout << *text << '\n';
    // event-text-usage-end

    return std::cout.good() ? 0 : 1;
}
} // namespace

/** @param argc Argument count.
 * @param argv Input-free help selector or explicit finite --run.
 * @return Documented success/help, formatting/output or usage code.
 */
int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        help(false);

        return std::cout.good() ? 0 : 1;
    }

    if (argc == 2 && std::string_view(argv[1]) == "--help-ru") {
        help(true);

        return std::cout.good() ? 0 : 1;
    }

    if (argc == 2 && std::string_view(argv[1]) == "--run") {
        return run();
    }

    std::cerr << "Invalid arguments; use --help.\n";

    return 2;
}
