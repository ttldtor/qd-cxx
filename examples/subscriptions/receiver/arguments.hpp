// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "settings.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::examples::quote_subscription::cli {
/** Validated command-line input; constructing this value starts no schema or network work. */
struct Arguments {
    std::string host;       ///< Explicit numeric IPv4/IPv6 or ASCII/A-label DNS endpoint, without ambient resolution.
    std::uint16_t port = 0; ///< Required TCP port.
    std::u16string symbol;  ///< One exact printable ASCII selector, copied to UTF-16 without normalization.
    std::chrono::milliseconds duration = DEFAULT_DURATION; ///< Admission interval from explicit start.
    std::chrono::milliseconds drain = DEFAULT_DRAIN;       ///< Absolute local drain allowance after admission ends.
    std::optional<std::size_t> maximumRecords;     ///< Optional printed-row cutoff; duration always remains finite.
    std::optional<transport::DnsServer> dnsServer; ///< One explicitly supplied IPv4 DNS server; never discovered.
    std::uint16_t dnsPort = 53;                    ///< Explicit DNS UDP/TCP port; numeric endpoints bypass DNS.
};

/** Print complete help before schema/bootstrap/native work.
 * @param russian Select the explicit Russian help variant.
 * @return Zero on successful stdout output, one on an output failure.
 */
inline int help(bool russian) {
    if (russian) {
        std::cout
            << "Использование: qdcxx-quote-subscription --host HOST --port PORT --symbol SYMBOL [параметры]\n"
               "Консольная Quote-подписка: plain TCP, STREAM, canonical Exact, один поток опроса.\n"
               "  --host HOST         IPv4/IPv6 или ASCII/A-label DNS-имя без URI.\n"
               "  --dns-server IPv4   Один явный DNS-сервер; обязателен для DNS-имени. Системный DNS не читается.\n"
               "  --dns-port PORT     DNS-порт 1..65535 с --dns-server; по умолчанию 53.\n"
               "  --port PORT         TCP-порт 1..65535, обязателен.\n"
               "  --symbol SYMBOL     Один символ: 1..128 печатных ASCII-символов; первый не '*'.\n"
               "  --duration-ms N     Интервал приёма 1..600000 мс, по умолчанию 5000.\n"
               "  --maximum-records N Предел напечатанных записей 1..1000000; по умолчанию только время.\n"
               "  --drain-ms N        Конечный локальный drain 1..60000 мс, по умолчанию 1000.\n"
               "  --help              Эта справка на английском; без параметров выводится та же справка.\n"
               "  --help-ru           Эта справка на русском; сеть и схема не создаются.\n"
               "Вывод: symbol и точные raw bits bid/ask price/size в stdout; диагностика в stderr.\n"
               "По времени/числу строк запрашивается drain; SIGINT/SIGTERM и ошибки вызывают abort.\n"
               "Не более 3 попыток соединения; фиксированная пауза 250 мс. Числовые адреса обходят DNS.\n"
               "DNS добавляет 80 MiB логической резервации запрошенной памяти; это не измерение RSS.\n"
               "Предел — 1000000 ходов владельца, включая shutdown; бюджеты при остановке не увеличиваются.\n"
               "Коды: 0 — справка/завершённый локальный drain, 1 — ошибка, 2 — неверные параметры, 130 — сигнал.\n"
               "Drain не подтверждает отправку на сервер или его ACK; пример использует полный Quote из 8 полей.\n";
    } else {
        std::cout << "Usage: qdcxx-quote-subscription --host HOST --port PORT --symbol SYMBOL [options]\n"
                     "Console Quote subscription: plain TCP, STREAM, canonical Exact, one caller-driven thread.\n"
                     "  --host HOST         Numeric IPv4/IPv6 or ASCII/A-label DNS name, without a URI.\n"
                     "  --dns-server IPv4   One explicit DNS server, required for names; system DNS is not read.\n"
                     "  --dns-port PORT     DNS port in 1..65535 with --dns-server; default 53.\n"
                     "  --port PORT         Required TCP port in 1..65535.\n"
                     "  --symbol SYMBOL     One selector: 1..128 printable ASCII characters, not starting with '*'.\n"
                     "  --duration-ms N     Admission interval in 1..600000 ms; default 5000.\n"
                     "  --maximum-records N Printed-row limit in 1..1000000; default is the duration alone.\n"
                     "  --drain-ms N        Finite local drain allowance in 1..60000 ms; default 1000.\n"
                     "  --help              This help; no arguments produces the identical output.\n"
                     "  --help-ru           Russian help, without schema/bootstrap/network work.\n"
                     "Output: symbol and exact bid/ask price/size raw bits on stdout; diagnostics on stderr.\n"
                     "Time/row cutoffs request drain; SIGINT/SIGTERM and failures request immediate abort.\n"
                     "At most 3 connection attempts use a fixed 250 ms delay; numeric addresses bypass DNS.\n"
                     "DNS adds an 80 MiB logical requested-payload reservation, not measured process RSS.\n"
                     "At most 1000000 owner turns include shutdown; stop uses the same finite work profile.\n"
                     "Exit: 0 help/completed local drain, 1 failure, 2 invalid arguments, 130 interruption.\n"
                     "Drain does not certify server output/ACK; the example uses the complete eight-field Quote.\n";
    }

    std::cout.flush();

    return std::cout ? 0 : 1;
}

/** Parse a complete unsigned decimal argument without whitespace, signs or partial acceptance.
 * @param text Borrowed command-line value.
 * @param maximum Maximum accepted positive value.
 * @return Value in1..maximum, or absent for invalid/out-of-range input.
 */
inline std::optional<std::uint64_t> positive(std::string_view text, std::uint64_t maximum) {
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);

    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value == 0 || value > maximum) {
        return std::nullopt;
    }

    return value;
}

/** Parse a bounded numeric IPv4 presentation without activating any resolver or socket API.
 * @param text Four decimal octets, with no leading zeroes except the single zero octet.
 * @return Network-order octets or absent for a nonnumeric/invalid presentation.
 */
inline std::optional<std::array<std::uint8_t, 4>> ipv4(std::string_view text) {
    std::array<std::uint8_t, 4> octets{};
    std::size_t offset = 0;

    for (std::size_t index = 0; index < octets.size(); ++index) {
        const auto end = text.find('.', offset);
        const auto token = text.substr(offset, end == std::string_view::npos ? end : end - offset);

        if (token.empty() || token.size() > 3 || (token.size() > 1 && token.front() == '0')) {
            return std::nullopt;
        }

        unsigned value = 0;
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);

        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() || value > 255 ||
            ((index + 1 == octets.size()) != (end == std::string_view::npos))) {
            return std::nullopt;
        }

        octets[index] = static_cast<std::uint8_t>(value);
        offset = end == std::string_view::npos ? text.size() : end + 1;
    }

    return octets;
}

/** Distinguish valid numeric IPv4/IPv6 input from names before requiring explicit DNS settings.
 * @param host Bare numeric presentation; IPv6 brackets and zone IDs are not part of this profile.
 * @return True for valid numeric syntax, without resolving or checking route availability.
 */
inline bool numericHost(std::string_view host) {
    if (host.find(':') == std::string_view::npos) {
        return ipv4(host).has_value();
    }

    const auto groups = [](std::string_view part) -> std::optional<std::size_t> {
        std::size_t count = 0;
        std::size_t offset = 0;

        while (offset < part.size()) {
            const auto end = part.find(':', offset);
            const auto token = part.substr(offset, end == std::string_view::npos ? end : end - offset);

            if (token.find('.') != std::string_view::npos) {
                if (end != std::string_view::npos || !ipv4(token)) {
                    return std::nullopt;
                }

                count += 2;
            } else {
                if (token.empty() || token.size() > 4 || !std::ranges::all_of(token, [](char unit) {
                        return (unit >= '0' && unit <= '9') || (unit >= 'a' && unit <= 'f') ||
                               (unit >= 'A' && unit <= 'F');
                    })) {
                    return std::nullopt;
                }

                ++count;
            }

            if (end == std::string_view::npos) {
                break;
            }

            offset = end + 1;

            if (offset == part.size()) {
                return std::nullopt;
            }
        }

        return count;
    };
    const auto compressed = host.find("::");

    if (compressed == std::string_view::npos) {
        const auto count = groups(host);

        return count && *count == 8;
    }

    if (host.find("::", compressed + 2) != std::string_view::npos ||
        host.substr(0, compressed).find('.') != std::string_view::npos) {
        return false;
    }

    const auto left = groups(host.substr(0, compressed));
    const auto right = groups(host.substr(compressed + 2));

    return left && right && *left + *right < 8;
}

/** Distinct named command-line settings, each accepted at most once. */
enum class Option {
    HOST,            ///< Explicit destination host.
    PORT,            ///< Explicit destination port.
    SYMBOL,          ///< Exact printable ASCII subscription key.
    DURATION,        ///< Admission duration in milliseconds.
    DRAIN,           ///< Local drain allowance in milliseconds.
    MAXIMUM_RECORDS, ///< Optional printed-row ceiling.
    DNS_SERVER,      ///< Explicit numeric IPv4 resolver.
    DNS_PORT         ///< Explicit resolver port.
};

/** Validate all CLI shape/ASCII/count limits before endpoint, schema or native initialization.
 * @param count Raw main argument count, including program name.
 * @param values Raw main arguments; help was already dispatched separately.
 * @return Complete application inputs or absent after one static usage error.
 */
inline std::optional<Arguments> arguments(int count, char **values) {
    constexpr std::array<std::string_view, 8> OPTION_NAMES{"--host",        "--port",     "--symbol",
                                                           "--duration-ms", "--drain-ms", "--maximum-records",
                                                           "--dns-server",  "--dns-port"};
    Arguments result;
    std::array<bool, OPTION_NAMES.size()> seen{};

    for (int index = 1; index < count; index += 2) {
        if (index + 1 >= count) {
            std::cerr << "Every option requires one value; use --help.\n";

            return std::nullopt;
        }

        const std::string_view name(values[index]);
        const std::string_view value(values[index + 1]);
        const auto found = std::ranges::find(OPTION_NAMES, name);

        if (found == OPTION_NAMES.end()) {
            return std::nullopt;
        }

        const auto position = static_cast<std::size_t>(found - OPTION_NAMES.begin());
        const auto option = static_cast<Option>(position);

        if (seen[position]) {
            return std::nullopt;
        }

        seen[position] = true;

        switch (option) {
        case Option::HOST:

            if (value.empty() || value.size() > 254) {
                return std::nullopt;
            }

            result.host = value;
            break;
        case Option::PORT: {
            const auto port = positive(value, std::numeric_limits<std::uint16_t>::max());

            if (!port) {
                return std::nullopt;
            }

            result.port = static_cast<std::uint16_t>(*port);
            break;
        }
        case Option::SYMBOL:

            if (value.empty() || value.size() > MAXIMUM_SYMBOL_UNITS || value.front() == '*' ||
                !std::ranges::all_of(value, [](unsigned char unit) {
                    return unit >= FIRST_PRINTABLE_ASCII && unit <= LAST_PRINTABLE_ASCII;
                })) {
                return std::nullopt;
            }

            result.symbol.assign(value.begin(), value.end());
            break;
        case Option::DURATION:
        case Option::DRAIN:
        case Option::MAXIMUM_RECORDS: {
            const auto maximum =
                option == Option::DURATION
                    ? static_cast<std::uint64_t>(MAXIMUM_DURATION.count())
                    : (option == Option::DRAIN ? static_cast<std::uint64_t>(MAXIMUM_DRAIN.count()) : MAXIMUM_RECORDS);
            const auto amount = positive(value, maximum);

            if (!amount) {
                return std::nullopt;
            }

            if (option == Option::DURATION) {
                result.duration = std::chrono::milliseconds(static_cast<std::int64_t>(*amount));
            } else if (option == Option::DRAIN) {
                result.drain = std::chrono::milliseconds(static_cast<std::int64_t>(*amount));
            } else {
                result.maximumRecords = static_cast<std::size_t>(*amount);
            }

            break;
        }
        case Option::DNS_SERVER: {
            const auto address = ipv4(value);

            if (!address || std::ranges::all_of(*address, [](auto octet) {
                    return octet == 0;
                })) {
                return std::nullopt;
            }

            result.dnsServer = transport::DnsServer{*address};
            break;
        }
        case Option::DNS_PORT: {
            const auto port = positive(value, std::numeric_limits<std::uint16_t>::max());

            if (!port) {
                return std::nullopt;
            }

            result.dnsPort = static_cast<std::uint16_t>(*port);
            break;
        }
        }
    }

    if (result.host.empty() || result.port == 0 || result.symbol.empty() ||
        (seen[static_cast<std::size_t>(Option::DNS_PORT)] && !result.dnsServer)) {
        return std::nullopt;
    }

    if (!numericHost(result.host) && !result.dnsServer) {
        std::cerr << "A DNS host requires an explicit --dns-server; system DNS is not used.\n";

        return std::nullopt;
    }

    return result;
}

} // namespace org::ttldtor::qdcxx::examples::quote_subscription::cli
