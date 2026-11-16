// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/api/publisher.hpp"
#include "qd/events/quote.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace core = org::ttldtor::qdcxx::core;
namespace api = org::ttldtor::qdcxx::api;
namespace events = org::ttldtor::qdcxx::events;
namespace transport = org::ttldtor::qdcxx::transport;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr auto SYMBOL = u"IBM";
constexpr std::uint64_t MAX_COUNT = 10000000;
constexpr std::uint64_t MAX_RATE = 10000000;
constexpr std::size_t MAX_BATCH = 8;
constexpr auto STARTUP_TIMEOUT = 60s;
constexpr auto OPERATION_TIMEOUT = 60s;
constexpr auto POLL_INTERVAL = 1ms;
constexpr std::int64_t DECIMAL_UNIT = 256;
constexpr std::int64_t DECIMAL_RANK = 128;

constexpr std::string_view HELP =
    "Usage: qdcxx-quote-stream-nettest PORT COUNT RATE BATCH [--stream-bytes TARGET HARD]\n"
    "Publish synthetic IBM Quote/STREAM records over TCP to one QD consumer.\n"
    "PORT is 0..65535 (0 chooses an ephemeral port), COUNT is 1..10000000,\n"
    "RATE is 0..10000000 records/s (0 is unpaced), BATCH is 1..8.\n"
    "--stream-bytes enables byte limits for STREAM output; TARGET is the\n"
    "payload threshold and HARD is the maximum complete frame size in bytes.\n"
    "Without this option the existing batch-based mode is used.\n"
    "Prints PORT, waits for matching subscription, publishes a sequence-0 probe,\n"
    "prints READY and waits for GO on stdin. Verify probe receipt before GO.\n"
    "Prints RESULT with admitted records, time, retries and completed STREAM frames.\n"
    "Frame count includes the startup probe; byte mode also reports failed frames.\n"
    "Bid.Time and Bid.Size carry a monotonic timestamp for same-host measurement.\n"
    "The producer opens a TCP listener and creates a runtime worker.\n"
    "Exit 0 on success, 1 on runtime failure, 2 on invalid arguments.\n";

constexpr std::string_view HELP_RU =
    "Использование: qdcxx-quote-stream-nettest ПОРТ КОЛИЧЕСТВО СКОРОСТЬ ПАКЕТ [--stream-bytes ЦЕЛЬ ПРЕДЕЛ]\n"
    "Публикует записи IBM Quote/STREAM по TCP одному приёмнику QD.\n"
    "ПОРТ 0..65535 (0 выбирает свободный), КОЛИЧЕСТВО 1..10000000,\n"
    "СКОРОСТЬ 0..10000000 записей/с (0 без ограничителя), ПАКЕТ 1..8.\n"
    "--stream-bytes включает байтовые лимиты STREAM: ЦЕЛЬ для полезных данных\n"
    "и ПРЕДЕЛ размера полного кадра. Без параметра используется прежний режим.\n"
    "Печатает PORT, ждёт подписку и отправляет пробную запись с номером 0,\n"
    "печатает READY и ждёт GO в stdin. Перед GO проверьте приём пробной записи.\n"
    "RESULT содержит число записей, время в нс, повторы и число завершённых кадров STREAM.\n"
    "Число кадров включает пробную запись; байтовый режим также сообщает об ошибках кадров.\n"
    "Bid.Time и Bid.Size содержат монотонную метку времени для измерения на одной машине.\n"
    "Процесс открывает TCP-слушатель и создаёт поток runtime.\n"
    "Код 0 означает успех, 1 ошибку исполнения, 2 неверные параметры.\n";

std::uint64_t parseNumber(std::string_view input, std::uint64_t minimum, std::uint64_t maximum) {
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);

    if (error != std::errc{} || end != input.data() + input.size() || value < minimum || value > maximum)
        throw std::invalid_argument("Invalid numeric argument");

    return value;
}

std::int64_t decimal(std::int64_t value) {
    return value * DECIMAL_UNIT + DECIMAL_RANK;
}

using Fields = std::array<core::FieldValueView, events::QUOTE_FIELD_TYPES.size()>;

Fields makeFields(std::uint64_t ordinal) {
    const auto now = std::chrono::floor<std::chrono::microseconds>(Clock::now().time_since_epoch());
    const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now - seconds).count();
    const std::array<std::int64_t, 8> values{
        seconds.count(), u'B', decimal(static_cast<std::int64_t>(ordinal)),     decimal(micros + 1),
        seconds.count(), u'A', decimal(static_cast<std::int64_t>(ordinal + 1)), decimal(2)};
    Fields result;

    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = {events::QUOTE_FIELD_TYPES[i], values[i], {}, {}};

    return result;
}

void run(std::uint16_t port, std::uint64_t count, std::uint64_t rate, std::size_t batch,
         std::optional<api::PublisherStreamByteLimits> streamBytes) {
    core::LocalRecord record{0, u"Quote", false, {}, {}};
    std::vector<std::u16string> names;
    names.reserve(events::QUOTE_FIELD_NAMES.size());

    for (std::size_t i = 0; i < events::QUOTE_FIELD_NAMES.size(); ++i) {
        const auto name = events::QUOTE_FIELD_NAMES[i];
        names.emplace_back(name.begin(), name.end());
        record.fields.push_back({std::to_underlying(events::QUOTE_FIELD_TYPES[i])});
        record.lookups.push_back({names.back(), i});
    }

    const auto schema =
        core::LocalSchema::create(std::span(&record, 1), {1, 8, 8, 64, 2048}, core::LocalProfile::BINARY_FIELDS_V1);

    if (!schema)
        throw std::runtime_error("Cannot create Quote catalog");

    std::array<std::vector<std::u16string>, 1> wireNames{std::move(names)};
    auto bootstrap = transport::RuntimeBootstrap::create();

    if (!bootstrap)
        throw std::runtime_error(std::string(bootstrap.error().stage));

    api::PublisherOptions options;
    options.listener.port = port;
    options.listener.maxConnections = 1;
    options.pendingQuotes = 1024;
    options.recordLimits.pendingBytes = api::PublisherRecordLimits::MAX_PENDING_BYTES;
    options.streamBatchRecords = batch;
    options.streamBytes = streamBytes;
    options.catalog.emplace(*schema, wireNames);
    auto endpoint = api::PublisherEndpoint::create(*bootstrap, options);

    if (!endpoint)
        throw std::runtime_error(std::string(endpoint.error().stage));

    std::cout << "PORT " << (*endpoint)->port() << std::endl;

    if (!std::cout)
        throw std::runtime_error("Cannot report listener port");

    if (!(*endpoint)->awaitDemand(core::SubscriptionFamily::STREAM, u"Quote", SYMBOL, true,
                                  Clock::now() + STARTUP_TIMEOUT))
        throw std::runtime_error("Timed out waiting for Quote/STREAM subscription");

    const auto publisher = (*endpoint)->getPublisher();
    const auto context = schema->recordContext(0);
    const auto probeFields = makeFields(0);
    const api::PublisherStreamRecordView probe{context, SYMBOL, probeFields};
    const auto probeResult = publisher.publishStreamRecords(std::span(&probe, 1));

    if (!probeResult)
        throw std::runtime_error(std::string(probeResult.error().stage));

    std::cout << "READY" << std::endl;
    std::string command;

    if (!std::cout)
        throw std::runtime_error("Cannot report readiness");

    if (!std::getline(std::cin, command) || command != "GO")
        throw std::invalid_argument("Expected GO on stdin");

    std::array<Fields, MAX_BATCH> fields;
    std::array<api::PublisherStreamRecordView, MAX_BATCH> rows;
    std::uint64_t retries = 0;
    const auto start = Clock::now();
    auto lastProgress = start;

    for (std::uint64_t done = 0; done < count;) {
        const auto size = static_cast<std::size_t>(std::min<std::uint64_t>(batch, count - done));

        if (rate != 0) {
            const auto due = std::chrono::nanoseconds((done * 1000000000ULL) / rate);

            while (Clock::now() - start < due)
                std::this_thread::yield();
        }

        for (std::size_t i = 0; i < size; ++i) {
            fields[i] = makeFields(done + i + 1);
            rows[i] = {context, SYMBOL, fields[i]};
        }

        for (;;) {
            auto accepted = publisher.publishStreamRecords(std::span(rows).first(size));

            if (accepted)
                break;

            if (accepted.error().code != api::ErrorCode::RESOURCE_LIMIT ||
                accepted.error().stage != "publisher-pending-records")
                throw std::runtime_error(std::string(accepted.error().stage));

            if (Clock::now() - lastProgress >= OPERATION_TIMEOUT)
                throw std::runtime_error("Timed out waiting for publisher capacity");

            ++retries;
            std::this_thread::yield();
        }

        done += size;
        lastProgress = Clock::now();
    }

    const auto publishNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
    const auto deadline = Clock::now() + OPERATION_TIMEOUT;
    bool drained = false;

    while (Clock::now() < deadline) {
        const auto stats = (*endpoint)->getStatistics();
        const auto &stream = stats.families[1];

        if (stats.retiredPeers.value != 0)
            throw std::runtime_error("Receiver disconnected before drain");

        if (stats.activePeers == 1 && stats.pendingPublications == 0 && stream.pendingSourceRecords == 0 &&
            stream.queuedDeliveryRecords == 0 && stream.retainedOutputRecords == 0 &&
            stream.ingressRecords.value == count + 1 && stream.sentFrames.value != 0) {
            drained = true;
            break;
        }

        std::this_thread::sleep_for(POLL_INTERVAL);
    }

    if (!drained)
        throw std::runtime_error("Timed out draining publisher");

    const auto stream = (*endpoint)->getStatistics().families[1];
    (*endpoint)->closeAndAwaitTermination();

    if ((*endpoint)->getDiagnostic().code != api::ErrorCode::NONE)
        throw std::runtime_error(std::string((*endpoint)->getDiagnostic().stage));

    std::cout << "RESULT count=" << count << " publishNs=" << publishNs << " retries=" << retries
              << " streamFrames=" << stream.sentFrames.value;

    if (streamBytes)
        std::cout << " failedFrames=" << stream.failedFrames.value;

    std::cout << std::endl;

    if (!std::cout)
        throw std::runtime_error("Cannot report publication result");
}

} // namespace

int main(int argc, char **argv) {
    if (argc == 1 ||
        (argc == 2 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "--help-ru"))) {
        std::cout << (argc == 2 && std::string_view(argv[1]) == "--help-ru" ? HELP_RU : HELP);

        return std::cout ? 0 : 1;
    }

    try {
        if (argc != 5 && (argc != 8 || std::string_view(argv[5]) != "--stream-bytes"))
            throw std::invalid_argument("Expected PORT COUNT RATE BATCH [--stream-bytes TARGET HARD]");

        const auto port = parseNumber(argv[1], 0, std::numeric_limits<std::uint16_t>::max());
        const auto count = parseNumber(argv[2], 1, MAX_COUNT);
        const auto rate = parseNumber(argv[3], 0, MAX_RATE);
        const auto batch = parseNumber(argv[4], 1, MAX_BATCH);
        std::optional<api::PublisherStreamByteLimits> streamBytes;

        if (argc == 8) {
            const auto target = parseNumber(argv[6], 1, 64 * 1024 - 1);
            const auto hard = parseNumber(argv[7], target + 1, 64 * 1024);
            streamBytes.emplace(static_cast<std::size_t>(target), static_cast<std::size_t>(hard));
        }

        run(static_cast<std::uint16_t>(port), count, rate, static_cast<std::size_t>(batch), streamBytes);
    } catch (const std::invalid_argument &error) {
        std::cerr << error.what() << '\n';

        return 2;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }

    return 0;
}
