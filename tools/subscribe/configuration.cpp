// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "configuration.hpp"

#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <climits>

namespace org::ttldtor::qdcxx::connect {
namespace {
constexpr std::size_t MAXIMUM_SYMBOLS = 16;       ///< Finite CLI selection cardinality.
constexpr std::size_t MAXIMUM_SYMBOL_UNITS = 128; ///< Maximum exact selector length.
constexpr unsigned MAXIMUM_DURATION_MS = 600000;  ///< Ten-minute admission ceiling, separate from OS retirement.
constexpr unsigned MAXIMUM_RECORDS = 1000000;     ///< Maximum explicit data target.

/** Parse one complete positive decimal without signs or overflow.
 * @param text Borrowed decimal argument.
 * @param maximum Inclusive permitted ceiling.
 * @return Value or a static syntax/range failure.
 */
std::expected<unsigned, std::string_view> positive(std::string_view text, unsigned maximum) {
    unsigned value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);

    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value == 0 || value > maximum) {
        return std::unexpected("numeric option is outside its positive decimal range");
    }

    return value;
}

/** Validate a numeric literal using only the native text parser, without socket/bootstrap work.
 * @param host Owned NUL-terminated candidate.
 * @return True for an exact IPv4/IPv6 literal without a URI or zone identifier.
 */
bool numericHost(const std::string &host) {
    std::array<unsigned char, 16> address{};

    return inet_pton(AF_INET, host.c_str(), address.data()) == 1 ||
           inet_pton(AF_INET6, host.c_str(), address.data()) == 1;
}
} // namespace

std::expected<Configuration, std::string_view> parseArguments(std::span<const std::string_view> arguments) {
    Configuration result;
    unsigned seen = 0;

    for (std::size_t index = 0; index < arguments.size(); index += 2) {
        const auto option = arguments[index];

        if (index + 1 == arguments.size()) {
            return std::unexpected("each working option requires one value; help must stand alone");
        }

        const auto value = arguments[index + 1];
        const std::array<std::string_view, 13> names{"--host",
                                                     "--port",
                                                     "--schema-profile",
                                                     "--output",
                                                     "--symbol",
                                                     "--duration-ms",
                                                     "--remove-after-ms",
                                                     "--max-records",
                                                     "--connect-timeout-ms",
                                                     "--read-timeout-ms",
                                                     "--write-timeout-ms",
                                                     "--poll-interval-ms",
                                                     "--security"};
        const auto found = std::ranges::find(names, option);

        if (found == names.end()) {
            return std::unexpected("unknown option; use standalone --help");
        }

        const auto position = static_cast<unsigned>(found - names.begin());
        const unsigned bit = 1u << position;

        if (option != "--symbol" && (seen & bit) != 0) {
            return std::unexpected("only --symbol may be repeated");
        }

        seen |= bit;

        if (option == "--security") {
            if (value != "plain") {
                return std::unexpected(value == "tls" ? "TLS is unsupported by this plain-only client"
                                                      : "--security requires plain or tls; only plain executes");
            }
        } else if (option == "--host") {
            if (value.empty() || value.size() > 45) {
                return std::unexpected("--host requires a numeric IPv4/IPv6 literal");
            }

            result.host = value;

            if (!numericHost(result.host)) {
                return std::unexpected("--host requires a numeric IPv4/IPv6 literal without a zone");
            }
        } else if (option == "--schema-profile") {
            if (value != "quote" && value != "profile") {
                return std::unexpected("--schema-profile requires quote or profile");
            }

            result.schema = value == "quote" ? SchemaProfile::QUOTE : SchemaProfile::PROFILE;
        } else if (option == "--output") {
            if (value != "generic" && value != "quote" && value != "profile") {
                return std::unexpected("--output requires generic, quote or profile");
            }

            result.output = value == "quote"     ? OutputProfile::QUOTE
                            : value == "profile" ? OutputProfile::PROFILE
                                                 : OutputProfile::GENERIC;
        } else if (option == "--symbol") {
            if (value.empty() || value.size() > MAXIMUM_SYMBOL_UNITS || result.symbols.size() == MAXIMUM_SYMBOLS ||
                value.front() == '*' || std::ranges::any_of(value, [](unsigned char unit) {
                    return unit < 32 || unit > 126;
                })) {
                return std::unexpected(
                    "--symbol requires 1..128 printable ASCII units, no leading *, at most 16 symbols");
            }

            std::u16string symbol(value.begin(), value.end());

            if (std::ranges::find(result.symbols, symbol) != result.symbols.end()) {
                return std::unexpected("duplicate --symbol is not permitted");
            }

            result.symbols.push_back(std::move(symbol));
        } else {
            const bool timeout = option == "--connect-timeout-ms" || option == "--read-timeout-ms" ||
                                 option == "--write-timeout-ms" || option == "--poll-interval-ms";
            const unsigned maximum = timeout                     ? static_cast<unsigned>(INT_MAX)
                                     : option == "--port"        ? 65535
                                     : option == "--max-records" ? MAXIMUM_RECORDS
                                                                 : MAXIMUM_DURATION_MS;
            const auto number = positive(value, maximum);

            if (!number) {
                return std::unexpected(number.error());
            }

            if (timeout) {
                auto duration = std::chrono::milliseconds(*number);

                if (option == "--connect-timeout-ms") {
                    result.connectTimeout = duration;
                } else if (option == "--read-timeout-ms") {
                    result.readTimeout = duration;
                } else if (option == "--write-timeout-ms") {
                    result.writeTimeout = duration;
                } else {
                    result.pollInterval = duration;
                }
            } else if (option == "--port") {
                result.port = static_cast<std::uint16_t>(*number);
            } else if (option == "--duration-ms") {
                result.duration = std::chrono::milliseconds(*number);
            } else if (option == "--remove-after-ms") {
                result.removeAfter = std::chrono::milliseconds(*number);
            } else {
                result.maximumRecords = *number;
            }
        }
    }

    if (result.host.empty() || result.port == 0 || result.symbols.empty()) {
        return std::unexpected("--host, --port and at least one --symbol are required");
    }

    if (result.removeAfter && *result.removeAfter >= result.duration) {
        return std::unexpected("--remove-after-ms must be less than --duration-ms");
    }

    if ((result.output == OutputProfile::QUOTE && result.schema != SchemaProfile::QUOTE) ||
        (result.output == OutputProfile::PROFILE && result.schema != SchemaProfile::PROFILE)) {
        return std::unexpected("prepared output must match --schema-profile");
    }

    return result;
}

std::expected<client::EndpointOptions, client::EndpointDiagnostic> effectiveEndpoint(const Configuration &configuration,
                                                                                     const core::LocalSchema &schema) {
    auto settings = client::controlledStreamSettings(configuration.duration);

    if (!settings) {
        return std::unexpected(settings.error());
    }

    settings->host = configuration.host;
    settings->port = configuration.port;
    settings->timeouts.connect = configuration.connectTimeout.value_or(settings->timeouts.connect);
    settings->timeouts.read = configuration.readTimeout.value_or(settings->timeouts.read);
    settings->timeouts.write = configuration.writeTimeout.value_or(settings->timeouts.write);
    settings->timeouts.poll = configuration.pollInterval.value_or(settings->timeouts.poll);

    return client::EndpointOptions::create(schema, *settings);
}

std::string_view help(bool russian) noexcept {
    if (russian) {
        return R"(qdcxx-connect: ограниченный клиент STREAM по обычному TCP для управляемого peer.
Использование: qdcxx-connect --host IP --port N --symbol SYMBOL [параметры]
  --host IP                  Обязательный числовой IPv4/IPv6 без URI/zone/DNS.
  --port N                   Обязательный порт 1..65535.
  --symbol SYMBOL            1..128 печатных ASCII без начального *; до 16 разных символов.
  --schema-profile quote|profile  Полная встроенная схема; по умолчанию quote.
  --output generic|quote|profile  Точные generic TSV v1 либо prepared summary; по умолчанию generic.
  --duration-ms N            Интервал приёма 1..600000 мс; по умолчанию 5000.
  --remove-after-ms N        Удалить весь исходный набор через N мс, 0<N<duration.
  --max-records N            Цель 1..1000000 завершённых records; по умолчанию нет цели.
  --connect-timeout-ms N     Deadline соединения1..2147483647 мс; по умолчанию1000.
  --read-timeout-ms N        Неактивность ввода1..2147483647 мс; по умолчанию duration.
  --write-timeout-ms N       Неактивность вывода1..2147483647 мс; по умолчанию duration.
  --poll-interval-ms N       Период прогресса1..2147483647 мс; по умолчанию1.
  --security plain|tls      По умолчанию plain; TLS отклоняется до рабочей подготовки.
  --help / --help-ru         Отдельная справка EN/RU, без рабочих входов/bootstrap/сети.
Без аргументов: та же английская справка, что и --help. Повторять можно лишь --symbol.
Цель прекращает новый приём; уже принятый допустимый префикс может дать records сверх цели.
Результат: записи в stdout; отдельные local/transport результаты и pending/uncertain bytes в stderr.
Эффекты: одно TCP-соединение, protocol/description/add и optional remove; временные signal handlers.
Остановка по времени/цели/сигналу закрывает TCP с отменой pending output, затем завершает local prefix.
Запись в сокет не подтверждает приём peer; завершение local не означает полного сетевого drain.
SIGINT/SIGTERM инициируют cooperative stop; нет жёсткой гарантии задержки OS/блокирующего sink.
В режиме по умолчанию нет TLS, auth, reconnect, wildcard, MUX, stdin-команд или совместимости Java CLI.
Коды: 0 intentional cutoff/достигнутая цель; 1 input/setup/peer/protocol/callback/sink failure;
      2 истёк интервал до add или недостигнута заданная цель; 130 interrupt.
Ошибка вывода справки тоже даёт ненулевой код.

Явный режим восстановления (нужна сборка с RECOVERY):
  qdcxx-connect --mode recovery --host HOST --port N --symbol SYMBOL [параметры]
  --family stream|ticker|history  По умолчанию stream; одна семья/запись/клиент за запуск.
  --schema-profile quote|profile  Полная встроенная схема; по умолчанию quote, record-id 0.
  --schema-json PATH         Явная native binary JSON схема, до 1 MiB; нужна сборка с JSON.
  --record-id N              Локальный ID 0..2147483647; обязателен с --schema-json.
  --history-from-raw N       Для HISTORY: обязательный signed int64 и внешняя indexed схема.
  --selectors exact-only|canonical-exact-v1  По умолчанию exact-only; All/wildcard не принимаются.
  --output generic|quote|profile  Всегда точные generic поля; optional prepared строки добавляются.
  --dns-server IPv4          Для DNS имени нужен явный числовой resolver; для numeric IP не нужен.
  --dns-port N               Порт resolver 1..65535, по умолчанию 53; требует --dns-server.
  --dns-timeout-ms N         Deadline 1..2147483647 мс; по умолчанию connect timeout.
  --attempts N               Положительный uint32, по умолчанию 1; действует предел таблицы epoch 1 MiB.
  --retry-delay-ms N         Для attempts>1: 1..2147483647 мс, по умолчанию 250; с attempts 1 запрещён.
  --drain-ms N               Локальное завершение 1..60000 мс, по умолчанию 1000.
  --security plain|tls       По умолчанию plain; TLS требует отдельной сборки, trust и reference.
  --tls-trust-pem PATH       Для TLS: явный файл обычных CERTIFICATE PEM, без системного trust fallback.
  --tls-reference NAME       Для TLS: проверяемый numeric IP или ASCII/A-label DNS, независимо от route.
  --tls-handshake-ms N       Для TLS:1..2147483647 мс, по умолчанию 1000.
  --tls-close-ms N           Для TLS:1..2147483647 мс, по умолчанию 1000.
  --auth-basic-token-file PATH  Только TLS: точные ASCII bytes полного Basic token; newline запрещён.
  --auth-timeout-ms N        Только с token file:1..2147483647 мс, по умолчанию 1000.
Общие --host/--port/--symbol/--duration-ms/--remove-after-ms/--max-records и четыре timeout/poll
параметра сохраняют указанные диапазоны/значения по умолчанию; read/write defaults равны duration.
Нельзя смешивать --schema-json и --schema-profile. HISTORY bound не является Unix/event timestamp.
Пути ограничены 4096 bytes; Windows native UTF16 преобразуется в UTF8 без смены кодировки процесса.
Фиксированный профиль: schema 16 records/128 fields/256 lookups, name 128 units/16384 units всего,
до 16 symbols по 128 ASCII units, кадр до 64 records. Превышение входных лимитов отвергается до активации.
Live данные могут исчерпать фиксированные очереди/кэши; это явный отказ, без скрытого роста бюджета.
Файлы читаются один раз после проверки выбранных возможностей сборки; stdin и скрытых defaults нет.
Результат stdout: qdcxx-subscriptions TSV v1; stderr: фактические команды/reset/HISTORY/terminal.
Печатается целая callback batch: цель max-records может быть превышена; новых строк внутри неё не отбрасываем.
Reset публикуется перед точным ACK. HISTORY — локальное AS-OF наблюдение; отказ означает unknown.
Effects: DNS только при явном resolver, конечные новые TCP/TLS попытки, временные signal handlers,
чтение явно заданных файлов и запись stdout/stderr. TLS никогда не откатывается в plaintext.
Drain завершает только принятый локальный префикс; это не peer ACK или полное network flush.
Коды нового режима:0 фактический DRAINED без expiry и выполненные readiness/цели/команды;
1 ошибка настройки/протокола/транспорта/вывода/завершения;2 usage или недостигнутая цель/readiness;
130 обработанный signal после фактического ABORTED. Ошибка имеет приоритет над signal.
Отсутствующая возможность сборки отвергается до файлов/bootstrap/signals/network.
)";
    }

    return R"(qdcxx-connect: controlled credential-free TCP STREAM client.
Usage: qdcxx-connect --host IP --port N --symbol SYMBOL [options]
  --host IP                  Required numeric IPv4/IPv6; no URI, zone or DNS.
  --port N                   Required port in 1..65535.
  --symbol SYMBOL            1..128 printable ASCII units, no leading *; up to 16 distinct symbols.
  --schema-profile quote|profile  Complete built-in schema; default quote.
  --output generic|quote|profile  Exact generic TSV v1 or prepared summary; default generic.
  --duration-ms N            Admission interval in 1..600000 milliseconds; default 5000.
  --remove-after-ms N        Remove the entire initial set after N ms, 0<N<duration.
  --max-records N            Target 1..1000000 completed records; default no data target.
  --connect-timeout-ms N     Connect deadline1..2147483647 ms; default1000.
  --read-timeout-ms N        Input inactivity1..2147483647 ms; default duration.
  --write-timeout-ms N       Output inactivity1..2147483647 ms; default duration.
  --poll-interval-ms N       Caller progress cadence1..2147483647 ms; default1.
  --security plain|tls      Default plain; TLS is rejected before working setup.
  --help / --help-ru         Standalone EN/RU help without working inputs/bootstrap/network.
No arguments prints the same English help as --help. Only --symbol may be repeated.
A target stops new admission; the already admitted eligible prefix can finish beyond that target.
Result: records to stdout; separate local/transport outcomes and pending/uncertain bytes to stderr.
Effects: one TCP connection, protocol/description/add and optional remove; temporary signal handlers.
Duration/target/signal cutoff aborts pending TCP output, then retires the admitted local prefix.
Socket acceptance is not peer acknowledgement; local completion is not a whole-network drain.
SIGINT/SIGTERM request cooperative stop; OS completion/blocking sinks have no hard latency guarantee.
The default route has no TLS, auth, reconnect, wildcard, MUX, stdin commands or Java CLI compatibility.
Exit: 0 intentional cutoff/data target reached; 1 input/setup/peer/protocol/callback/sink failure;
      2 interval expired before add or requested target not reached; 130 interrupt.
Help output failure also returns nonzero.

Explicit recovery route (requires a RECOVERY build):
  qdcxx-connect --mode recovery --host HOST --port N --symbol SYMBOL [options]
  --family stream|ticker|history  Default stream; one family/record/client per invocation.
  --schema-profile quote|profile  Complete built-in schema; default quote, record-id 0.
  --schema-json PATH         Explicit native binary JSON schema, at most 1 MiB; requires JSON build.
  --record-id N              Local ID 0..2147483647; required with --schema-json.
  --history-from-raw N       HISTORY requires signed int64 input and an external indexed schema.
  --selectors exact-only|canonical-exact-v1  Default exact-only; no All or wildcard.
  --output generic|quote|profile  Always exact generic fields; optional prepared rows are additional.
  --dns-server IPv4          Named routes require an explicit numeric resolver; numeric routes do not.
  --dns-port N               Resolver port 1..65535, default 53; requires --dns-server.
  --dns-timeout-ms N         Deadline 1..2147483647 ms; default connect timeout.
  --attempts N               Positive uint32, default 1; also bounded by the 1 MiB epoch-label table.
  --retry-delay-ms N         For attempts>1: 1..2147483647 ms, default 250; rejected with attempts 1.
  --drain-ms N               Local retirement 1..60000 ms, default 1000.
  --security plain|tls       Default plain; TLS requires its separate build, trust and reference.
  --tls-trust-pem PATH       TLS: explicit ordinary CERTIFICATE PEM file; no system-trust fallback.
  --tls-reference NAME       TLS: verified numeric IP or ASCII/A-label DNS identity, independent of route.
  --tls-handshake-ms N       TLS:1..2147483647 ms, default 1000.
  --tls-close-ms N           TLS:1..2147483647 ms, default 1000.
  --auth-basic-token-file PATH  TLS only: exact ASCII bytes of a complete Basic token; no newline.
  --auth-timeout-ms N        With token file only:1..2147483647 ms, default 1000.
Shared host/port/symbol/duration/remove/record-target and four timeout/poll options retain the ranges
and defaults above; read/write defaults equal duration. --schema-json excludes --schema-profile.
HISTORY raw bound is not a Unix/event timestamp. Paths are bounded to 4096 bytes; Windows native
UTF16 arguments become UTF8 without changing process codepages.
Fixed profile: schema 16 records/128 fields/256 lookups,128 units/name/16384 total name units,
up to 16 symbols of 128 ASCII units, at most 64 records/frame. Input-profile excess refuses before activation.
Live data can exhaust fixed queues/caches; this is an explicit failure without hidden budget growth.
Explicit files are read once after build-feature checks; no stdin or implicit configuration sources.
Result stdout: qdcxx-subscriptions TSV v1; stderr: actual commands/reset/HISTORY/local terminal.
A whole callback batch is published even across max-records; the target does not discard its suffix.
Reset publication precedes exact ACK. HISTORY is local AS-OF evidence; refusal means unknown.
Effects: DNS with an explicit resolver, finite fresh TCP/TLS attempts, temporary signal handlers,
explicit file reads and stdout/stderr writes. TLS never falls back to plaintext.
Drain covers only the admitted local prefix; it does not establish peer ACK or whole-network flush.
New-route exits:0 actual DRAINED without expiry and satisfied readiness/targets/commands;
1 setup/protocol/transport/output/retirement failure;2 usage or unmet target/readiness;
130 handled signal after actual ABORTED. Failure takes precedence over signal.
Unavailable build features refuse before files/bootstrap/signals/network.
)";
}
} // namespace org::ttldtor::qdcxx::connect
