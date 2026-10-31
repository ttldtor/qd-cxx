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
        const std::array<std::string_view, 8> names{"--host",   "--port",        "--schema-profile",  "--output",
                                                    "--symbol", "--duration-ms", "--remove-after-ms", "--max-records"};
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

        if (option == "--host") {
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
            const unsigned maximum = option == "--port"          ? 65535
                                     : option == "--max-records" ? MAXIMUM_RECORDS
                                                                 : MAXIMUM_DURATION_MS;
            const auto number = positive(value, maximum);

            if (!number) {
                return std::unexpected(number.error());
            }

            if (option == "--port") {
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
  --help / --help-ru         Отдельная справка EN/RU, без рабочих входов/bootstrap/сети.
Без аргументов: та же английская справка, что и --help. Повторять можно лишь --symbol.
Цель прекращает новый приём; уже принятый допустимый префикс может дать records сверх цели.
Результат: записи в stdout; отдельные local/transport результаты и pending/uncertain bytes в stderr.
Эффекты: одно TCP-соединение, protocol/description/add и optional remove; временные signal handlers.
Остановка по времени/цели/сигналу закрывает TCP с отменой pending output, затем завершает local prefix.
Запись в сокет не подтверждает приём peer; завершение local не означает полного сетевого drain.
SIGINT/SIGTERM инициируют cooperative stop; нет жёсткой гарантии задержки OS/блокирующего sink.
Нет TLS, auth, reconnect, wildcard, MUX, stdin-команд или совместимости Java CLI.
Коды: 0 intentional cutoff/достигнутая цель; 1 input/setup/peer/protocol/callback/sink failure;
      2 истёк интервал до add или недостигнута заданная цель; 130 interrupt.
Ошибка вывода справки тоже даёт ненулевой код.
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
  --help / --help-ru         Standalone EN/RU help without working inputs/bootstrap/network.
No arguments prints the same English help as --help. Only --symbol may be repeated.
A target stops new admission; the already admitted eligible prefix can finish beyond that target.
Result: records to stdout; separate local/transport outcomes and pending/uncertain bytes to stderr.
Effects: one TCP connection, protocol/description/add and optional remove; temporary signal handlers.
Duration/target/signal cutoff aborts pending TCP output, then retires the admitted local prefix.
Socket acceptance is not peer acknowledgement; local completion is not a whole-network drain.
SIGINT/SIGTERM request cooperative stop; OS completion/blocking sinks have no hard latency guarantee.
No TLS, auth, reconnect, wildcard, MUX, stdin commands or Java CLI compatibility.
Exit: 0 intentional cutoff/data target reached; 1 input/setup/peer/protocol/callback/sink failure;
      2 interval expired before add or requested target not reached; 130 interrupt.
Help output failure also returns nonzero.
)";
}
} // namespace org::ttldtor::qdcxx::connect
