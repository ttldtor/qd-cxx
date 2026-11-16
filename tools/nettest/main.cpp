// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "nettest.hpp"

#include "qd/schema/json_schema.hpp"

#include <algorithm>
#include <charconv>
#include <climits>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

#ifdef _WIN32
#    include <windows.h>
#endif

namespace org::ttldtor::qdcxx::nettest {
namespace {
using core::SubscriptionFamily;
constexpr std::size_t MIB = 1024 * 1024;
constexpr std::size_t SCHEMA_BYTES = MIB;
constexpr std::size_t PROVENANCE_BYTES = 4 * MIB;
constexpr core::SchemaLimits SCHEMA_LIMITS{1024, 16384, 32768, 1024, MIB};
constexpr core::SchemaLimits RECORD_LIMITS{1, MAX_FIELDS, 4096, 1024, MIB};
constexpr std::size_t MAX_ADDRESS_UNITS = 45;
constexpr std::size_t MAX_ARGUMENT_UNITS = 4096;

constexpr std::string_view HELP = R"(Usage: qdcxx-nettest p|c --bundle DIRECTORY --port PORT [OPTIONS]

Run a finite physical-record QTP producer (p) or consumer (c) over numeric plain TCP.
The bundle is the directory containing schema.json and provenance.json from one binary-v2
export, for example the installed default or dxlink directory.

  --address IP            Numeric IPv4/IPv6, default 127.0.0.1
  --port PORT             Local producer port (0 chooses one) or remote consumer port
  --record NAME           Exact UTF-8 record name, default TimeAndSale
  --contract NAME         stream (default), ticker, or history for indexed records
  --symbol SYMBOL         Exact UTF-8 symbol, repeat up to 64 times, default IBM
  --instances COUNT       Maximum producer peers / simultaneous consumer connections (1..8), default 1
  --duration SECONDS      Measurement window (1..3600), default 10
  --startup SECONDS       Time to first usable subscription (1..120), default 10
  --rate RECORDS_PER_SEC  Offered producer rate per peer (1..1000000), default 1000
  --batch COUNT           Maximum producer records per DATA frame (1..128), default 100
  --history-bound VALUE   Signed raw HISTORY lower bound for the consumer, default 0
  --verify-stream         Verify deterministic IBM TimeAndSale STREAM until clean source close
  --qd-text               Also print QD-style aggregate data rate (lag unavailable)
  --help / --help-ru      Print help without opening files or sockets

The producer prints PORT, waits for eligible demand, then runs for the measurement
window. The consumer connects, subscribes and counts decoded selected records.
Both print RESULT with boundary-specific counts. Shutdown has a separate finite deadline.
The verification consumer prints ACTIVE at its first record and CHECKSUM after clean EOF.
Its duration is the deadline for source close, with a maximum of 3,600,000,000 records. Periodic consumer heartbeats are disabled in this mode.
Exit status: 0 completed or help, 1 input/network/protocol failure, 2 invalid arguments.
)";

constexpr std::string_view HELP_RU = R"(Использование: qdcxx-nettest p|c --bundle КАТАЛОГ --port ПОРТ [ПАРАМЕТРЫ]

Конечный прогон физических записей QTP по обычному TCP: источник (p) или потребитель (c).
В каталоге должны лежать schema.json и provenance.json одного экспорта binary-v2,
например установленная схема default или dxlink.

  --address IP            Числовой IPv4/IPv6, по умолчанию 127.0.0.1
  --port ПОРТ             Локальный порт источника (0 выбирает свободный) или удалённый порт
  --record ИМЯ            Точное имя записи в UTF-8, по умолчанию TimeAndSale
  --contract ИМЯ          stream (по умолчанию), ticker или history для индексной записи
  --symbol СИМВОЛ         Точный символ UTF-8, до 64 повторов параметра, по умолчанию IBM
  --instances ЧИСЛО       Максимум клиентов источника / соединений потребителя (1..8), по умолчанию 1
  --duration СЕКУНДЫ      Измерительное окно (1..3600), по умолчанию 10
  --startup СЕКУНДЫ       Время до рабочей подписки (1..120), по умолчанию 10
  --rate ЗАПИСЕЙ_В_С     Нагрузка источника на клиента (1..1000000), по умолчанию 1000
  --batch ЧИСЛО           Максимум записей в кадре DATA (1..128), по умолчанию 100
  --history-bound ЧИСЛО   Знаковая сырая нижняя граница HISTORY потребителя, по умолчанию 0
  --verify-stream         Проверять IBM TimeAndSale STREAM до закрытия источника
  --qd-text               Дополнительно выводить скорость в форме QD (lag недоступен)
  --help / --help-ru      Показать справку без открытия файлов и сокетов

Источник выводит PORT, ждёт подходящую подписку и публикует записи. Потребитель
подключается и считает декодированные записи. RESULT разделяет метрики по границам
измерения. Остановка ограничена отдельным сроком.
Проверяющий потребитель выводит ACTIVE при первой записи, CHECKSUM после корректного EOF.
Его duration задаёт срок закрытия источника, предел составляет 3 600 000 000 записей. Периодические heartbeat потребителя отключены.
Коды выхода: 0 прогон или справка, 1 ошибка входа/сети/протокола, 2 неверные параметры.
)";

template <class T> T number(std::string_view text, T lower, T upper) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (error != std::errc{} || end != text.data() + text.size() || value < lower || value > upper)
        throw std::invalid_argument("Numeric value is out of range");

    return value;
}

std::u16string utf16(std::string_view text, std::size_t capacity) {
    std::u16string result;

    for (std::size_t offset = 0; offset < text.size();) {
        const auto first = static_cast<unsigned char>(text[offset++]);
        unsigned extra = 0;
        std::uint32_t code = first;
        std::uint32_t minimum = 0;

        if (first >= 0xc2 && first <= 0xdf) {
            code = first & 0x1f;
            extra = 1;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            code = first & 0x0f;
            extra = 2;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            code = first & 0x07;
            extra = 3;
            minimum = 0x10000;
        } else if (first >= 0x80) {
            throw std::invalid_argument("Malformed UTF-8 selector");
        }

        if (extra > text.size() - offset)
            throw std::invalid_argument("Truncated UTF-8 selector");

        for (unsigned i = 0; i < extra; ++i) {
            const auto next = static_cast<unsigned char>(text[offset++]);

            if ((next & 0xc0) != 0x80)
                throw std::invalid_argument("Malformed UTF-8 selector");

            code = (code << 6) | (next & 0x3f);
        }

        if (code < minimum || code == 0 || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            throw std::invalid_argument("Invalid Unicode selector");

        if (code < 0x10000) {
            result.push_back(static_cast<char16_t>(code));
        } else {
            code -= 0x10000;
            result.push_back(static_cast<char16_t>(0xd800 | (code >> 10)));
            result.push_back(static_cast<char16_t>(0xdc00 | (code & 0x3ff)));
        }

        if (result.size() > capacity)
            throw std::invalid_argument("Selector is too long");
    }

    if (result.empty() || result.front() == u'*')
        throw std::invalid_argument("Expected a nonempty exact selector");

    return result;
}

Options parse(std::span<const std::string_view> arguments) {
    if (arguments.empty() || (arguments.front() != "p" && arguments.front() != "c"))
        throw std::invalid_argument("Expected mode p or c");

    Options options;
    options.mode = arguments.front() == "p" ? Mode::PRODUCER : Mode::CONSUMER;
    const auto producer = options.mode == Mode::PRODUCER;
    std::vector<std::string_view> used;

    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const auto key = arguments[index];

        if (key != "--symbol" && std::ranges::find(used, key) != used.end())
            throw std::invalid_argument("Duplicate argument " + std::string(key));

        if (key != "--symbol")
            used.push_back(key);

        if (key == "--verify-stream" && !producer) {
            options.verifyStream = true;
            continue;
        }

        if (key == "--qd-text") {
            options.qdText = true;
            continue;
        }

        if (++index == arguments.size() || arguments[index].starts_with("--") ||
            arguments[index].size() > MAX_ARGUMENT_UNITS)
            throw std::invalid_argument("Expected bounded value for " + std::string(key));

        const auto value = arguments[index];

        if (key == "--bundle") {
            options.bundle = std::filesystem::path(std::u8string(value.begin(), value.end()));
        } else if (key == "--address") {
            if (value.empty() || value.size() > MAX_ADDRESS_UNITS ||
                value.find_first_not_of("0123456789abcdefABCDEF:.") != std::string_view::npos)
                throw std::invalid_argument("Expected a numeric IP address");

            options.address = value;
        } else if (key == "--port") {
            options.port = number<std::uint16_t>(value, producer ? 0 : 1, 65535);
            options.hasPort = true;
        } else if (key == "--record") {
            options.record = utf16(value, RECORD_LIMITS.nameUnits);
        } else if (key == "--contract") {
            if (value == "ticker")
                options.family = SubscriptionFamily::TICKER;
            else if (value == "stream")
                options.family = SubscriptionFamily::STREAM;
            else if (value == "history")
                options.family = SubscriptionFamily::HISTORY;
            else
                throw std::invalid_argument("Unknown delivery contract");
        } else if (key == "--symbol") {
            auto symbol = utf16(value, MAX_SYMBOL_UNITS);

            if (options.symbols.size() == MAX_SYMBOLS ||
                std::ranges::find(options.symbols, symbol) != options.symbols.end())
                throw std::invalid_argument("Too many or repeated symbols");

            options.symbols.push_back(std::move(symbol));
        } else if (key == "--instances") {
            options.instances = number<std::size_t>(value, 1, MAX_CONNECTIONS);
        } else if (key == "--duration") {
            options.duration = std::chrono::seconds(number<unsigned>(value, 1, 3600));
        } else if (key == "--startup") {
            options.startup = std::chrono::seconds(number<unsigned>(value, 1, 120));
        } else if (key == "--rate" && producer) {
            options.rate = number<std::uint32_t>(value, 1, 1000000);
        } else if (key == "--batch" && producer) {
            options.batch = number<std::size_t>(value, 1, MAX_BATCH);
        } else if (key == "--history-bound" && !producer) {
            options.historyBound = number<std::int64_t>(value, std::numeric_limits<std::int64_t>::min(),
                                                        std::numeric_limits<std::int64_t>::max());
        } else {
            throw std::invalid_argument("Unknown argument for this mode " + std::string(key));
        }
    }

    if (!options.hasPort || options.bundle.empty())
        throw std::invalid_argument("--bundle and --port are required");

    if (options.family != SubscriptionFamily::HISTORY && std::ranges::find(used, "--history-bound") != used.end())
        throw std::invalid_argument("--history-bound requires history");

    if (options.symbols.empty())
        options.symbols.emplace_back(u"IBM");

    if (options.verifyStream && (options.family != SubscriptionFamily::STREAM || options.record != u"TimeAndSale" ||
                                 options.instances != 1 || options.symbols.size() != 1 || options.symbols[0] != u"IBM"))
        throw std::invalid_argument("--verify-stream requires one IBM TimeAndSale STREAM consumer");

    return options;
}

std::string readFile(const std::filesystem::path &path, std::size_t max) {
    std::ifstream input(path, std::ios::binary);

    if (!input)
        throw std::runtime_error("Cannot open catalog file");

    std::string text(max + 1, '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    const auto count = static_cast<std::size_t>(input.gcount());

    if (input.bad() || count > max)
        throw std::runtime_error("Cannot read catalog file within its byte limit");

    text.resize(count);

    return text;
}

void print(const Options &options, const Report &report) {
    const auto seconds = std::chrono::duration<double>(report.measured).count();
    const auto records = options.mode == Mode::PRODUCER ? report.written : report.delivered;
    const auto rate = seconds > 0 ? static_cast<std::uint64_t>(static_cast<double>(records) / seconds) : 0;
    std::cout << "RESULT instances=" << options.instances << " connections=" << report.connections
              << " seconds=" << seconds << " generated=" << report.generated << " composed=" << report.composed
              << " written=" << report.written << " wireRecords=" << report.wireRecords
              << " delivered=" << report.delivered << " bytesRead=" << report.bytesRead
              << " bytesWritten=" << report.bytesWritten << " rps=" << rate << '\n';

    if (options.verifyStream)
        std::cout << "CHECKSUM " << report.checksum << '\n';

    if (options.qdText)
        std::cout << "data " << rate << " rps, lag unavailable, instances " << options.instances << ", connections "
                  << report.connections << '\n';
}

} // namespace

Catalog loadCatalog(const Options &options) {
    auto all = schema::loadBinaryJsonSchema(readFile(options.bundle / "schema.json", SCHEMA_BYTES));

    if (!all)
        throw std::runtime_error(std::string("Invalid schema: ") + schema::jsonSchemaErrorName(all.error()).data());

    schema::BinaryJsonSchemaLimits bounds;
    bounds.loading.documentBytes = PROVENANCE_BYTES;
    auto names =
        schema::loadBinaryJsonWireNames(readFile(options.bundle / "provenance.json", PROVENANCE_BYTES), *all, bounds);

    if (!names)
        throw std::runtime_error(std::string("Invalid provenance: ") +
                                 schema::jsonSchemaErrorName(names.error()).data());

    const auto records = all->records();
    const auto found = std::ranges::find(records, options.record, &core::LocalRecord::name);

    if (found == records.end())
        throw std::invalid_argument("Record is absent from the bundle");

    if (options.family == SubscriptionFamily::HISTORY && !found->indexedTimeLayout)
        throw std::invalid_argument("HISTORY requires an indexed record");

    if (found->fields.size() > MAX_FIELDS)
        throw std::invalid_argument("Record exceeds the NetTest field limit");

    auto namesForRecord = std::move((*names)[static_cast<std::size_t>(found->id)]);
    auto selected = *found;
    selected.id = SENDER_ID;
    auto single =
        core::LocalSchema::create(std::span(&selected, 1), RECORD_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);

    if (!single)
        throw std::runtime_error("Cannot create selected record schema");

    return {std::move(*single), std::move(namesForRecord)};
}

Messages messages(SubscriptionFamily family) {
    switch (family) {
    case SubscriptionFamily::TICKER:
        return {
            {10, u"TICKER_DATA", {}}, {11, u"TICKER_ADD_SUBSCRIPTION", {}}, {12, u"TICKER_REMOVE_SUBSCRIPTION", {}}};
    case SubscriptionFamily::STREAM:
        return {
            {15, u"STREAM_DATA", {}}, {16, u"STREAM_ADD_SUBSCRIPTION", {}}, {17, u"STREAM_REMOVE_SUBSCRIPTION", {}}};
    case SubscriptionFamily::HISTORY:
        return {
            {20, u"HISTORY_DATA", {}}, {21, u"HISTORY_ADD_SUBSCRIPTION", {}}, {22, u"HISTORY_REMOVE_SUBSCRIPTION", {}}};
    }

    throw std::logic_error("Invalid delivery contract");
}

protocol::LiveWork protocolWork() {
    // The binding planner charges one full scoped name budget for each candidate field.
    return {128, 256 * MIB, 4ULL * MIB * MIB, 64 * MIB};
}

protocol::LiveLimits sessionLimits() {
    protocol::LiveLimits result{};
    result.frame = {FRAME_BYTES, FRAME_BYTES + 9};
    result.protocol = {128, 128, {8192, 1024}, 32768};
    result.descriptions = {FRAME_BYTES, 1024, 256, 16384, {8192, 1024}, FRAME_BYTES};
    result.context = {RECORD_LIMITS, 1024, {16384, 1024, MIB}};
    result.data = {FRAME_BYTES,
                   4096,
                   16384,
                   4096,
                   32768,
                   FRAME_BYTES,
                   {FRAME_BYTES, MAX_SYMBOL_UNITS, MAX_SYMBOL_UNITS},
                   {FRAME_BYTES, 1024},
                   FRAME_BYTES,
                   FRAME_BYTES,
                   FRAME_BYTES,
                   FRAME_BYTES,
                   FRAME_BYTES,
                   FRAME_BYTES,
                   FRAME_BYTES,
                   FRAME_BYTES,
                   0};
    result.residentBytes = 24 * MIB;
    result.owners = 12;
    result.snapshotBytes = 3 * MIB;
    result.scratchBytes = MIB;
    result.eventBytes = 2 * MIB;
    result.outputBytes = FRAME_BYTES + 9;
    result.credentials = protocol::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    result.events = protocol::DataEventPolicy::BINARY_CATALOG_V1;
    result.historySnapshot = true;

    return result;
}

protocol::SubscriptionWriter writer(const Catalog &catalog) {
    const auto &record = catalog.schema.records().front();
    protocol::OutgoingRecord layout{SENDER_ID, record.id, record.name, {}};
    layout.fields.reserve(record.fields.size());

    for (std::size_t slot = 0; slot < record.fields.size(); ++slot) {
        layout.fields.push_back(
            {catalog.fieldNames[slot], record.fields[slot].serialTypeId, static_cast<std::int32_t>(slot)});
    }

    protocol::SubscriptionWriterWork work{64, MIB, MIB, 4 * MIB};
    auto result = protocol::SubscriptionWriter::create(
        catalog.schema, std::span(&layout, 1), {RECORD_LIMITS, 1, MAX_FIELDS, 1024, MIB},
        {FRAME_BYTES, FRAME_BYTES + 9, MAX_BATCH, MAX_SYMBOL_UNITS, {8192, 1024}, 256 * 1024, 256 * 1024, 4, true},
        work, core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1);

    if (!result)
        throw std::runtime_error("Cannot create record writer");

    return std::move(*result);
}

int entry(std::span<const std::string_view> arguments) {
    if (arguments.empty() || (arguments.size() == 1 && arguments.front() == "--help")) {
        std::cout << HELP;

        return std::cout ? 0 : 1;
    }

    if (arguments.size() == 1 && arguments.front() == "--help-ru") {
        std::cout << HELP_RU;

        return std::cout ? 0 : 1;
    }

    try {
        const auto options = parse(arguments);
        const auto catalog = loadCatalog(options);
        const auto result = options.mode == Mode::PRODUCER ? produce(options, catalog) : consume(options, catalog);
        print(options, result);

        return std::cout ? 0 : 1;
    } catch (const std::invalid_argument &failure) {
        std::cerr << "Invalid arguments: " << failure.what() << '\n';

        return 2;
    } catch (const std::exception &failure) {
        std::cerr << "NetTest failure: " << failure.what() << '\n';

        return 1;
    }
}
} // namespace org::ttldtor::qdcxx::nettest

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    std::vector<std::string> owned;

    for (int i = 1; i < argc; ++i) {
        const auto units = std::wstring_view(argv[i]);

        if (units.size() > INT_MAX)
            return 2;

        if (units.empty()) {
            owned.emplace_back();
            continue;
        }

        const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, units.data(),
                                               static_cast<int>(units.size()), nullptr, 0, nullptr, nullptr);

        if (count <= 0) {
            std::cerr << "Invalid UTF-16 arguments\n";

            return 2;
        }

        auto &text = owned.emplace_back(static_cast<std::size_t>(count), '\0');

        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, units.data(), static_cast<int>(units.size()),
                                text.data(), count, nullptr, nullptr) != count)
            return 2;
    }

    std::vector<std::string_view> arguments(owned.begin(), owned.end());
#else
int main(int argc, char **argv) {
    std::vector<std::string_view> arguments(argv + 1, argv + argc);
#endif

    return org::ttldtor::qdcxx::nettest::entry(arguments);
}
