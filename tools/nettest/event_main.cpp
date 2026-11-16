// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/api/publisher.hpp"
#include "qd/events/short_string.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {
namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

constexpr std::u16string_view SYMBOL = u"IBM";
constexpr auto IDLE_CADENCE = 1ms;
constexpr auto DRAIN_TIMEOUT = 2s;
constexpr auto MAX_DURATION = 300u;
constexpr auto MAX_STARTUP = 120u;
constexpr auto MAX_RATE = 1000000u;
constexpr auto SEQUENCE_BITS = 22u;
constexpr auto SEQUENCE_LIMIT = std::uint64_t{1} << SEQUENCE_BITS;
constexpr std::uint64_t NANOSECONDS_PER_SECOND = 1000000000;
constexpr std::size_t MAX_STREAM_BATCH = 8;

constexpr std::string_view HELP = R"(Usage: qdcxx-event-nettest p --port PORT [OPTIONS]
Publish synthetic IBM TimeAndSale/STREAM or Quote/TICKER events through the public API.
Serves 1..8 clients over plain TCP.

  --port PORT        Required listener port; 0 selects a free port
  --address ADDRESS  Numeric listener address, default 127.0.0.1
  --clients N        Number of connected clients to await (1..8), default 1
  --duration SECONDS Publication window (1..300), default 10
  --startup SECONDS  Wait for matching demand (1..120), default 10
  --rate EVENTS      Offered events per second (1..1000000), default 1000
  --pacing MODE      sleep (default) or yield for capacity tests with extra CPU use
  --batch N          Maximum events per call (STREAM 1..8, TICKER 1), default 1
  --record NAME      TimeAndSale (default) or Quote
  --symbol IBM       Fixed symbol
  --contract NAME    stream (default) for TimeAndSale, ticker for Quote
  --start-signal stdin  Accept PROBE commands and then GO after READY
  --help, --help-ru  Print help without starting the runtime

Prints PORT and RESULT with generated and admitted events, admission retries,
and fully sent DATA frames. The configured offer must be at least two events
and fit the 22-bit sequence range. After the window the tool waits up to two
seconds for local admission and drain. Verify receipt on every peer. In stdin
mode, PROBE publishes a sequence-0 event, followed by PROBE_SENT after local
drain. The controller must verify receipt by every peer before sending GO.
Exit: 0 success or help, 1 runtime or output error, 2 invalid arguments.
)";

constexpr std::string_view HELP_RU = R"(Использование: qdcxx-event-nettest p --port ПОРТ [ПАРАМЕТРЫ]
Публикует синтетические TimeAndSale/STREAM или Quote/TICKER для IBM через публичный API.
Обслуживает от 1 до 8 клиентов по обычному TCP.

  --port ПОРТ        Обязательный порт слушателя; 0 выбирает свободный
  --address АДРЕС    Числовой адрес слушателя, по умолчанию 127.0.0.1
  --clients N        Число ожидаемых клиентов (1..8), по умолчанию 1
  --duration СЕКУНДЫ Окно публикации (1..300), по умолчанию 10
  --startup СЕКУНДЫ  Ожидание подписки (1..120), по умолчанию 10
  --rate СОБЫТИЯ     Предложено событий в секунду (1..1000000), по умолчанию 1000
  --pacing РЕЖИМ     sleep (по умолчанию) или yield для замера с повышенным расходом CPU
  --batch N          Предел событий на вызов (STREAM 1..8, TICKER 1), по умолчанию 1
  --record ИМЯ       TimeAndSale (по умолчанию) или Quote
  --symbol IBM       Фиксированный символ
  --contract ИМЯ     stream для TimeAndSale, ticker для Quote
  --start-signal stdin  Принимать команды PROBE, затем GO после READY
  --help, --help-ru  Справка без запуска сетевой подсистемы

Печатает PORT и RESULT со счётчиками созданных и принятых событий, повторных
попыток приёма и отправленных DATA-кадров. Предложенная нагрузка должна включать
как минимум два события и укладываться в 22-битную последовательность. После окна
инструмент ждёт до двух секунд завершения локального приёма и отправки.
Доставку проверяет каждый получатель. В режиме stdin PROBE публикует событие
с sequence=0 и печатает PROBE_SENT после завершения локальной отправки.
Управляющая программа ждёт подтверждения каждого получателя перед отправкой GO.
Код выхода: 0 успех или справка, 1 ошибка исполнения или вывода, 2 ошибка аргументов.
)";

struct Options {
    std::string address = "127.0.0.1";
    std::uint16_t port = 0;
    std::chrono::seconds duration{10};
    std::chrono::seconds startup{10};
    std::uint32_t rate = 1000;
    std::size_t clients = 1;
    std::size_t batch = 1;
    bool ticker = false;
    bool startSignal = false;
    bool yieldPacing = false;
};

unsigned number(std::string_view key, std::string_view value, unsigned minimum, unsigned maximum) {
    unsigned result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);

    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result < minimum || result > maximum)
        throw std::invalid_argument("Invalid value for " + std::string(key));

    return result;
}

Options parse(std::span<char *> arguments) {
    constexpr std::array<std::string_view, 12> KEYS{"--port",    "--duration", "--startup",      "--rate",
                                                    "--batch",   "--record",   "--symbol",       "--contract",
                                                    "--clients", "--address",  "--start-signal", "--pacing"};

    if (std::string_view(arguments.front()) != "p" || arguments.size() % 2 == 0)
        throw std::invalid_argument("Expected p followed by option/value pairs");

    Options options;
    std::string_view record = "TimeAndSale";
    std::string_view contract = "stream";
    std::array<bool, KEYS.size()> seen{};

    for (std::size_t index = 1; index < arguments.size(); index += 2) {
        const std::string_view key = arguments[index];
        const std::string_view value = arguments[index + 1];
        const auto found = std::ranges::find(KEYS, key);

        if (found == KEYS.end())
            throw std::invalid_argument("Unknown option " + std::string(key));

        const auto slot = static_cast<std::size_t>(found - KEYS.begin());

        if (std::exchange(seen[slot], true))
            throw std::invalid_argument("Duplicate option " + std::string(key));

        if (key == "--port")
            options.port = static_cast<std::uint16_t>(number(key, value, 0, std::numeric_limits<std::uint16_t>::max()));
        else if (key == "--duration")
            options.duration = std::chrono::seconds(number(key, value, 1, MAX_DURATION));
        else if (key == "--startup")
            options.startup = std::chrono::seconds(number(key, value, 1, MAX_STARTUP));
        else if (key == "--rate")
            options.rate = number(key, value, 1, MAX_RATE);
        else if (key == "--clients")
            options.clients = number(key, value, 1, 8);
        else if (key == "--batch")
            options.batch = number(key, value, 1, MAX_STREAM_BATCH);
        else if (key == "--address")
            options.address = value;
        else if (key == "--record")
            record = value;
        else if (key == "--contract")
            contract = value;
        else if (key == "--start-signal" && value == "stdin")
            options.startSignal = true;
        else if (key == "--pacing" && (value == "sleep" || value == "yield"))
            options.yieldPacing = value == "yield";
        else if ((key == "--symbol" && value != "IBM") || key == "--start-signal" || key == "--pacing")
            throw std::invalid_argument("Unsupported value for " + std::string(key));
    }

    if (!seen.front())
        throw std::invalid_argument("--port is required");

    if (record == "Quote" && contract == "ticker")
        options.ticker = true;
    else if (record != "TimeAndSale" || contract != "stream")
        throw std::invalid_argument("Unsupported record and contract combination");

    if (options.ticker && options.batch != 1)
        throw std::invalid_argument("TICKER requires --batch 1");

    const auto offered = static_cast<std::uint64_t>(options.duration.count()) * options.rate;

    if (offered < 2)
        throw std::invalid_argument("Configured offer must include at least two events");

    if (offered >= SEQUENCE_LIMIT)
        throw std::invalid_argument("Offered load exceeds the 22-bit sequence range");

    return options;
}

/** Generate the physical TimeAndSale pattern used by the Record NetTest verifier. */
q::events::TimeAndSale makeSale(std::uint64_t sequence) {
    constexpr auto TIME_WORD_BITS = 32u;
    constexpr auto FIELD_SPREAD = 120u;
    constexpr auto WIDE_DECIMAL_UNIT = 256;
    constexpr auto WIDE_DECIMAL_INTEGER_RANK = 128;
    const auto nowNanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());
    const auto now = std::chrono::floor<std::chrono::milliseconds>(nowNanos);
    const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
    const auto millis = static_cast<std::uint64_t>((now - seconds).count());
    const auto index =
        (static_cast<std::uint64_t>(seconds.count()) << TIME_WORD_BITS) | (millis << SEQUENCE_BITS) | sequence;
    const auto value = [sequence](std::uint64_t field) {
        return static_cast<std::int32_t>((sequence + field) % FIELD_SPREAD) + 1;
    };
    const auto decimal = [&value](std::uint64_t field) {
        return q::core::WideDecimal{value(field) * WIDE_DECIMAL_UNIT + WIDE_DECIMAL_INTEGER_RANK};
    };

    q::events::TimeAndSale sale;
    sale.symbol = SYMBOL;
    sale.index = std::bit_cast<std::int64_t>(index);
    sale.timeNanoPart = static_cast<std::int32_t>((nowNanos - now).count());
    sale.exchangeCode = static_cast<char16_t>(u'A' + value(2) % 26);
    sale.tradeId = value(3);
    sale.price = decimal(4);
    sale.size = decimal(5);
    sale.bidPrice = decimal(6);
    sale.askPrice = decimal(7);
    sale.exchangeSaleConditions = q::events::decodeShortString(value(8));
    sale.flags = value(9);

    return sale;
}

/** Generate the Quote/TICKER sequence and timestamp used by the verifier. */
q::events::Quote makeQuote(std::uint64_t sequence) {
    constexpr auto FIELD_SPREAD = 120u;
    constexpr auto WIDE_DECIMAL_UNIT = 256;
    constexpr auto WIDE_DECIMAL_INTEGER_RANK = 128;
    const auto nowNanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());
    const auto now = std::chrono::floor<std::chrono::milliseconds>(nowNanos);
    const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
    const auto value = [sequence](std::uint64_t field) {
        return static_cast<std::int32_t>((sequence + field) % FIELD_SPREAD) + 1;
    };
    const auto decimal = [&value](std::uint64_t field) {
        return q::core::WideDecimal{value(field) * WIDE_DECIMAL_UNIT + WIDE_DECIMAL_INTEGER_RANK};
    };

    q::events::Quote quote{};
    quote.symbol = SYMBOL;
    quote.bidTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(seconds).count();
    quote.askTimeMs = quote.bidTimeMs;
    quote.timeMs = now.count();
    quote.sequence = static_cast<std::int32_t>(sequence);
    quote.timeNanoPart = static_cast<std::int32_t>((nowNanos - now).count());
    quote.bidExchangeCode = static_cast<char16_t>(u'A' + value(1) % 26);
    quote.bidPrice = decimal(2);
    quote.bidSize = decimal(3);
    quote.askExchangeCode = static_cast<char16_t>(u'A' + value(5) % 26);
    quote.askPrice = decimal(6);
    quote.askSize = decimal(7);

    return quote;
}

/** Wait before the next scheduled publication or admission attempt.
 * @param yieldPacing Yield to a runnable thread, or sleep on the normal benchmark cadence.
 */
void pauseProducer(bool yieldPacing) {
    if (yieldPacing)
        std::this_thread::yield();
    else
        std::this_thread::sleep_for(IDLE_CADENCE);
}

/** Retry the same event only when the typed publisher's input FIFO is full. */
template <typename Event>
void admit(const q::api::Publisher &publisher, const Event &event, std::uint64_t sequence, Clock::time_point deadline,
           std::uint64_t &admissionRetries, bool yieldPacing = false) {
    do {
        const auto accepted = publisher.publish(event);

        if (accepted)
            return;

        const auto &error = accepted.error();

        if (error.code != q::api::ErrorCode::RESOURCE_LIMIT || error.stage != "publisher-pending-quotes")
            throw std::runtime_error("Failed to admit event " + std::to_string(sequence) + ": " +
                                     std::string(error.stage));

        ++admissionRetries;
        pauseProducer(yieldPacing);
    } while (Clock::now() < deadline);

    throw std::runtime_error("Timed out admitting event " + std::to_string(sequence));
}

/** Wait for the admitted events to leave the observed publisher queues. */
q::api::PublisherFamilyStatistics drain(const q::api::PublisherEndpoint &endpoint, const Options &options,
                                        std::uint64_t admitted, Clock::time_point deadline) {
    do {
        const auto diagnostic = endpoint.getDiagnostic();

        if (diagnostic.code != q::api::ErrorCode::NONE)
            throw std::runtime_error(std::string(diagnostic.stage));

        const auto stats = endpoint.getStatistics();
        const auto &family = stats.families[options.ticker ? 0 : 1];

        if (stats.retiredPeers.value != 0)
            throw std::runtime_error("Peer retired before the publication drained");

        if (stats.activePeers == options.clients && stats.pendingPublications == 0 &&
            family.pendingSourceRecords == 0 && family.queuedDeliveryRecords == 0 &&
            family.retainedOutputRecords == 0 && family.ingressRecords.value == admitted &&
            family.sentFrames.value != 0)
            return family;

        std::this_thread::sleep_for(IDLE_CADENCE);
    } while (Clock::now() < deadline);

    const auto stats = endpoint.getStatistics();
    const auto &family = stats.families[options.ticker ? 0 : 1];
    throw std::runtime_error("Publisher drain timed out: admitted=" + std::to_string(admitted) +
                             " ingress=" + std::to_string(family.ingressRecords.value) +
                             " pending=" + std::to_string(stats.pendingPublications) +
                             " source=" + std::to_string(family.pendingSourceRecords) +
                             " queued=" + std::to_string(family.queuedDeliveryRecords) +
                             " retained=" + std::to_string(family.retainedOutputRecords) +
                             " sent=" + std::to_string(family.sentFrames.value));
}

void produce(const Options &options) {
    auto bootstrap = q::transport::RuntimeBootstrap::create();

    if (!bootstrap)
        throw std::runtime_error(std::string(bootstrap.error().stage));

    q::api::PublisherOptions publisherOptions;
    publisherOptions.listener.address = options.address;
    publisherOptions.listener.port = options.port;
    publisherOptions.listener.maxConnections = options.clients;
    publisherOptions.streamBatchRecords = options.batch;
    publisherOptions.mixed = true;
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, publisherOptions);

    if (!endpoint)
        throw std::runtime_error(std::string(endpoint.error().stage));

    std::cout << "PORT " << (*endpoint)->port() << std::endl;

    if (!std::cout)
        throw std::runtime_error("Cannot report producer port");

    const auto family = options.ticker ? q::core::SubscriptionFamily::TICKER : q::core::SubscriptionFamily::STREAM;
    const auto record = options.ticker ? u"Quote" : u"TimeAndSale";
    const auto startupDeadline = Clock::now() + options.startup;

    if (!(*endpoint)->awaitDemand(family, record, SYMBOL, true, startupDeadline))
        throw std::runtime_error("Timed out waiting for matching demand");

    while ((*endpoint)->getStatistics().activePeers != options.clients && Clock::now() < startupDeadline)
        std::this_thread::sleep_for(IDLE_CADENCE);

    if ((*endpoint)->getStatistics().activePeers != options.clients)
        throw std::runtime_error("Timed out waiting for connected clients");

    const auto publisher = (*endpoint)->getPublisher();
    std::uint64_t probeAdmissions = 0;
    std::uint64_t probeFrames = 0;
    std::uint64_t admissionRetries = 0;

    if (options.startSignal) {
        std::cout << "READY" << std::endl;
        std::string command;

        while (std::getline(std::cin, command) && command == "PROBE") {
            if (options.ticker)
                admit(publisher, makeQuote(0), 0, Clock::now() + DRAIN_TIMEOUT, admissionRetries);
            else
                admit(publisher, makeSale(0), 0, Clock::now() + DRAIN_TIMEOUT, admissionRetries);

            const auto probe = drain(**endpoint, options, ++probeAdmissions, Clock::now() + DRAIN_TIMEOUT);
            probeFrames = probe.sentFrames.value;
            std::cout << "PROBE_SENT" << std::endl;

            if (!std::cout)
                throw std::runtime_error("Cannot report probe completion");
        }

        if (command != "GO" || probeAdmissions == 0 || !std::cin)
            throw std::runtime_error("Expected PROBE followed by GO on standard input");

        admissionRetries = 0;
    }

    const auto start = Clock::now();
    const auto end = start + options.duration;
    const auto drainDeadline = end + DRAIN_TIMEOUT;
    std::uint64_t generated = 0;
    std::uint64_t admitted = 0;
    std::array<q::events::TimeAndSale, MAX_STREAM_BATCH> sales;

    for (auto now = start; now < end; now = Clock::now()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
        const auto offered = static_cast<std::uint64_t>(elapsed) * options.rate / NANOSECONDS_PER_SECOND;

        if (generated == offered) {
            pauseProducer(options.yieldPacing);
            continue;
        }

        const auto count = std::min<std::uint64_t>(options.batch, offered - generated);

        if (options.ticker) {
            const auto sequence = generated + 1;
            admit(publisher, makeQuote(sequence), sequence, drainDeadline, admissionRetries, options.yieldPacing);
        } else if (options.batch == 1) {
            const auto sequence = generated + 1;
            admit(publisher, makeSale(sequence), sequence, drainDeadline, admissionRetries, options.yieldPacing);
        } else {
            for (std::size_t i = 0; i < count; ++i)
                sales[i] = makeSale(generated + i + 1);

            admit(publisher, std::span<const q::events::TimeAndSale>(sales).first(count), generated + 1, drainDeadline,
                  admissionRetries, options.yieldPacing);
        }

        generated += count;
        admitted += count;
    }

    if (admitted == 0)
        throw std::runtime_error("No event was admitted during the publication window");

    const auto drainStart = Clock::now();
    const auto stats = drain(**endpoint, options, admitted + probeAdmissions, drainDeadline);
    const auto drainMillis = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - drainStart).count();
    (*endpoint)->closeAndAwaitTermination();

    if (const auto diagnostic = (*endpoint)->getDiagnostic(); diagnostic.code != q::api::ErrorCode::NONE)
        throw std::runtime_error(std::string(diagnostic.stage));

    std::cout << "RESULT seconds=" << options.duration.count() << " generated=" << generated << " admitted=" << admitted
              << " connections=" << options.clients
              << " ingressRecords=" << stats.ingressRecords.value - probeAdmissions
              << " sentFrames=" << stats.sentFrames.value - probeFrames << " admissionRetries=" << admissionRetries
              << " drainMs=" << drainMillis << std::endl;

    if (!std::cout)
        throw std::runtime_error("Cannot report producer result");
}
} // namespace

int main(int argc, char **argv) {
    const auto arguments = std::span(argv + 1, static_cast<std::size_t>(argc - 1));
    const auto first = arguments.empty() ? std::string_view{} : std::string_view(arguments.front());

    if (arguments.empty() || (arguments.size() == 1 && (first == "--help" || first == "--help-ru"))) {
        std::cout << (first == "--help-ru" ? HELP_RU : HELP);

        return std::cout ? 0 : 1;
    }

    try {
        produce(parse(arguments));
    } catch (const std::invalid_argument &error) {
        std::cerr << error.what() << '\n';

        return 2;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }

    return 0;
}
