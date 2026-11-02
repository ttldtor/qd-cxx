// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_configuration.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <climits>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::connect {
namespace {
constexpr std::size_t MAXIMUM_SYMBOLS = 16;         ///< Maximum distinct CLI selectors in one registration.
constexpr std::size_t MAXIMUM_SYMBOL_UNITS = 128;   ///< Maximum printable ASCII units in one selector.
constexpr std::size_t MAXIMUM_HOST_BYTES = 254;     ///< Complete DNS presentation, including an optional final dot.
constexpr std::size_t MAXIMUM_LABEL_BYTES = 63;     ///< Maximum ASCII DNS label length.
constexpr std::size_t MAXIMUM_PATH_BYTES = 4096;    ///< Application argument bound, not an OS path-length guarantee.
constexpr unsigned MAXIMUM_DURATION_MS = 600000;    ///< Independent finite admission interval.
constexpr unsigned MAXIMUM_DRAIN_MS = 60000;        ///< Independent finite local retirement allowance.
constexpr unsigned MAXIMUM_RECORDS = 1000000;       ///< Maximum explicit delivered-record target.
constexpr unsigned MAXIMUM_PORT = 65535;            ///< Largest numeric TCP/DNS port.
constexpr unsigned IPV4_PARTS = 4;                  ///< Complete IPv4 octet count.
constexpr unsigned MAXIMUM_IPV4_OCTET = 255;        ///< Largest unsigned IPv4 octet.
constexpr std::size_t MAXIMUM_IPV4_OCTET_BYTES = 3; ///< Largest ordinary IPv4 decimal component.
constexpr std::size_t IPV6_GROUPS = 8;              ///< Complete IPv6 hexadecimal group count.
constexpr std::size_t MAXIMUM_IPV6_GROUP_BYTES = 4; ///< Largest IPv6 hexadecimal component.
constexpr unsigned char FIRST_PRINTABLE_ASCII = 32; ///< First ordinary CLI selector unit.
constexpr unsigned char LAST_PRINTABLE_ASCII = 126; ///< Last ordinary CLI selector unit.
constexpr auto DEFAULT_RETRY_DELAY = std::chrono::milliseconds{250};   ///< Initial fixed multi-attempt delay.
constexpr auto DEFAULT_AUTH_TIMEOUT = std::chrono::milliseconds{1000}; ///< Initial bounded authentication deadline.

/** Internal option identity; its position matches the finite grammar table. */
enum class Option {
    MODE,
    HOST,
    PORT,
    SYMBOL,
    SCHEMA_PROFILE,
    SCHEMA_JSON,
    RECORD_ID,
    OUTPUT,
    FAMILY,
    HISTORY_FROM_RAW,
    SELECTORS,
    DNS_SERVER,
    DNS_PORT,
    ATTEMPTS,
    RETRY_DELAY,
    DURATION,
    REMOVE_AFTER,
    MAX_RECORDS,
    DRAIN,
    CONNECT_TIMEOUT,
    READ_TIMEOUT,
    WRITE_TIMEOUT,
    POLL_INTERVAL,
    DNS_TIMEOUT,
    SECURITY,
    TLS_TRUST,
    TLS_REFERENCE,
    TLS_HANDSHAKE,
    TLS_CLOSE,
    AUTH_FILE,
    AUTH_TIMEOUT,
    COUNT
};
constexpr std::size_t OPTION_COUNT = static_cast<std::size_t>(Option::COUNT); ///< Distinct singleton slots.
constexpr std::array<std::string_view, OPTION_COUNT> OPTION_NAMES{"--mode",
                                                                  "--host",
                                                                  "--port",
                                                                  "--symbol",
                                                                  "--schema-profile",
                                                                  "--schema-json",
                                                                  "--record-id",
                                                                  "--output",
                                                                  "--family",
                                                                  "--history-from-raw",
                                                                  "--selectors",
                                                                  "--dns-server",
                                                                  "--dns-port",
                                                                  "--attempts",
                                                                  "--retry-delay-ms",
                                                                  "--duration-ms",
                                                                  "--remove-after-ms",
                                                                  "--max-records",
                                                                  "--drain-ms",
                                                                  "--connect-timeout-ms",
                                                                  "--read-timeout-ms",
                                                                  "--write-timeout-ms",
                                                                  "--poll-interval-ms",
                                                                  "--dns-timeout-ms",
                                                                  "--security",
                                                                  "--tls-trust-pem",
                                                                  "--tls-reference",
                                                                  "--tls-handshake-ms",
                                                                  "--tls-close-ms",
                                                                  "--auth-basic-token-file",
                                                                  "--auth-timeout-ms"};
using SeenOptions = std::array<bool, OPTION_COUNT>;
using ParseResult = std::expected<void, std::string_view>;

/** Parse one exact unsigned decimal inside the caller's explicit inclusive bounds.
 * @param text Borrowed complete spelling, without whitespace, signs or exponent notation.
 * @param minimum Inclusive lower bound.
 * @param maximum Inclusive upper bound.
 * @return Parsed value or a static refusal; no input is copied to diagnostics.
 */
std::expected<std::uint64_t, std::string_view> unsignedNumber(std::string_view text, std::uint64_t minimum,
                                                              std::uint64_t maximum) {
    if (text.empty()) {
        return std::unexpected("numeric recovery option is outside its complete decimal range");
    }

    std::uint64_t number = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);

    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || number < minimum || number > maximum) {
        return std::unexpected("numeric recovery option is outside its complete decimal range");
    }

    return number;
}

/** Validate exact IPv4 octets with no native APIs, lookup or bootstrap.
 * @param text Borrowed bounded presentation.
 * @return True for exactly four ordinary decimal octets, each in 0..255.
 */
bool ipv4(std::string_view text) noexcept {
    unsigned parts = 0;

    while (!text.empty()) {
        const auto dot = text.find('.');
        const auto part = text.substr(0, dot);
        unsigned number = 0;
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), number);

        if (part.empty() || part.size() > MAXIMUM_IPV4_OCTET_BYTES || (part.size() > 1 && part.front() == '0') ||
            parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size() || number > MAXIMUM_IPV4_OCTET) {
            return false;
        }

        ++parts;

        if (dot == std::string_view::npos) {
            return parts == IPV4_PARTS;
        }

        text.remove_prefix(dot + 1);
    }

    return false;
}

/** Validate bounded IPv6 group syntax, including a final IPv4 suffix, without SDK calls.
 * @param text Borrowed presentation without brackets, URI or zone.
 * @return True for eight groups or one valid shortening double colon.
 */
bool ipv6(std::string_view text) noexcept {
    const auto compressed = text.find("::");

    if (text.empty() || (text.front() == ':' && !text.starts_with("::")) ||
        (text.back() == ':' && !text.ends_with("::")) ||
        (compressed != std::string_view::npos && text.find("::", compressed + 2) != std::string_view::npos) ||
        text.find(":::") != std::string_view::npos) {
        return false;
    }

    std::size_t groups = 0;

    for (std::size_t begin = 0; begin < text.size();) {
        if (text[begin] == ':') {
            ++begin;
            continue;
        }

        const auto colon = text.find(':', begin);
        const auto part = text.substr(begin, colon == std::string_view::npos ? text.size() - begin : colon - begin);

        if (part.find('.') != std::string_view::npos) {
            if (colon != std::string_view::npos || !ipv4(part)) {
                return false;
            }

            groups += 2;
        } else {
            if (part.size() > MAXIMUM_IPV6_GROUP_BYTES || !std::ranges::all_of(part, [](unsigned char unit) {
                    return (unit >= '0' && unit <= '9') || (unit >= 'a' && unit <= 'f') || (unit >= 'A' && unit <= 'F');
                })) {
                return false;
            }

            ++groups;
        }

        begin = colon == std::string_view::npos ? text.size() : colon + 1;
    }

    return compressed == std::string_view::npos ? groups == IPV6_GROUPS : groups < IPV6_GROUPS;
}

/** Inspect a numeric endpoint without DNS or native address APIs.
 * @param host Borrowed bounded presentation.
 * @return True only for accepted numeric IPv4/IPv6.
 */
bool numericHost(std::string_view host) noexcept {
    return host.find(':') == std::string_view::npos ? ipv4(host) : ipv6(host);
}

/** Match the existing endpoint presentation grammar before copying an argument.
 * @param host Borrowed numeric IP or ASCII/A-label DNS presentation.
 * @return True for a complete bounded presentation; no resolution or normalization occurs.
 */
bool validHost(std::string_view host) noexcept {
    if (host.empty() || host.size() > MAXIMUM_HOST_BYTES) {
        return false;
    }

    if (host.find(':') != std::string_view::npos) {
        return ipv6(host);
    }

    if (std::ranges::all_of(host, [](unsigned char unit) {
            return (unit >= '0' && unit <= '9') || unit == '.';
        })) {
        return ipv4(host);
    }

    if (host.back() == '.') {
        host.remove_suffix(1);
    }

    while (!host.empty()) {
        const auto dot = host.find('.');
        const auto label = host.substr(0, dot);

        if (label.empty() || label.size() > MAXIMUM_LABEL_BYTES || label.front() == '-' || label.back() == '-' ||
            !std::ranges::all_of(label, [](unsigned char unit) {
                return (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') || (unit >= '0' && unit <= '9') ||
                       unit == '-';
            })) {
            return false;
        }

        if (dot == std::string_view::npos) {
            return true;
        }

        host.remove_prefix(dot + 1);
    }

    return false;
}

/** Check a bounded opaque path argument without filesystem inspection or path rewriting.
 * @param value Borrowed exact non-NUL bytes.
 * @return True for a nonempty argument under the application byte ceiling.
 */
bool validPath(std::string_view value) noexcept {
    return !value.empty() && value.size() <= MAXIMUM_PATH_BYTES;
}

/** Select the directly stored millisecond field for an ordinary numeric option.
 * @param result Candidate configuration, not published yet.
 * @param option Current finite grammar identity.
 * @return Mutable field or null when the option has a different representation.
 */
std::chrono::milliseconds *durationField(RecoveryConfiguration &result, Option option) noexcept {
    switch (option) {
    case Option::RETRY_DELAY:
        return &result.retryDelay;
    case Option::DURATION:
        return &result.duration;
    case Option::DRAIN:
        return &result.drain;
    case Option::CONNECT_TIMEOUT:
        return &result.connectTimeout;
    case Option::READ_TIMEOUT:
        return &result.readTimeout;
    case Option::WRITE_TIMEOUT:
        return &result.writeTimeout;
    case Option::POLL_INTERVAL:
        return &result.pollInterval;
    case Option::DNS_TIMEOUT:
        return &result.dnsTimeout;
    case Option::TLS_HANDSHAKE:
        return &result.tlsHandshake;
    case Option::TLS_CLOSE:
        return &result.tlsClose;
    default:
        return nullptr;
    }
}

/** Apply one validated option/value pair only to an unpublished owned candidate.
 * @param result Candidate values, discarded on any failure.
 * @param option Known grammar identity; duplicate checks precede this call.
 * @param value Complete borrowed argument, without embedded NUL.
 * @param features Explicit compiled availability, never discovered from ambient state.
 * @return Success or static refusal, without I/O or external state mutation.
 */
ParseResult assignOption(RecoveryConfiguration &result, Option option, std::string_view value,
                         CompiledRecoveryFeatures features) {
    if (auto *field = durationField(result, option)) {
        const auto maximum = option == Option::DURATION ? MAXIMUM_DURATION_MS
                             : option == Option::DRAIN  ? MAXIMUM_DRAIN_MS
                                                        : static_cast<unsigned>(INT_MAX);
        const auto number = unsignedNumber(value, 1, maximum);

        if (!number) {
            return std::unexpected(number.error());
        }

        *field = std::chrono::milliseconds{*number};

        return {};
    }

    switch (option) {
    case Option::MODE:

        if (value != "recovery") {
            return std::unexpected("--mode requires recovery on this route");
        }

        break;
    case Option::HOST:
    case Option::TLS_REFERENCE:

        if (!validHost(value)) {
            return std::unexpected("host or TLS reference requires a bounded numeric IP or ASCII DNS presentation");
        }

        if (option == Option::HOST) {
            result.host = value;
        } else {
            result.tlsReference = value;
        }

        break;
    case Option::PORT:
    case Option::DNS_PORT:
    case Option::RECORD_ID:
    case Option::ATTEMPTS:
    case Option::MAX_RECORDS: {
        const auto minimum = option == Option::RECORD_ID ? 0u : 1u;
        const auto maximum = option == Option::RECORD_ID     ? static_cast<std::uint64_t>(INT32_MAX)
                             : option == Option::ATTEMPTS    ? static_cast<std::uint64_t>(UINT32_MAX)
                             : option == Option::MAX_RECORDS ? static_cast<std::uint64_t>(MAXIMUM_RECORDS)
                                                             : MAXIMUM_PORT;
        const auto number = unsignedNumber(value, minimum, maximum);

        if (!number) {
            return std::unexpected(number.error());
        }

        switch (option) {
        case Option::PORT:
            result.port = static_cast<std::uint16_t>(*number);
            break;
        case Option::DNS_PORT:
            result.dnsPort = static_cast<std::uint16_t>(*number);
            break;
        case Option::RECORD_ID:
            result.recordId = static_cast<std::int32_t>(*number);
            break;
        case Option::ATTEMPTS:
            result.attempts = static_cast<std::uint32_t>(*number);
            break;
        default:
            result.maximumRecords = static_cast<std::size_t>(*number);
            break;
        }

        break;
    }
    case Option::SYMBOL: {
        if (value.empty() || value.size() > MAXIMUM_SYMBOL_UNITS || result.symbols.size() == MAXIMUM_SYMBOLS ||
            value.front() == '*' || !std::ranges::all_of(value, [](unsigned char unit) {
                return unit >= FIRST_PRINTABLE_ASCII && unit <= LAST_PRINTABLE_ASCII;
            })) {
            return std::unexpected("--symbol requires distinct printable ASCII selectors under the finite CLI limits");
        }

        std::u16string symbol(value.begin(), value.end());

        if (std::ranges::find(result.symbols, symbol) != result.symbols.end()) {
            return std::unexpected("duplicate --symbol is not permitted");
        }

        result.symbols.push_back(std::move(symbol));

        break;
    }
    case Option::SCHEMA_PROFILE:

        if (value != "quote" && value != "profile") {
            return std::unexpected("--schema-profile requires quote or profile");
        }

        result.schema = value == "quote" ? SchemaProfile::QUOTE : SchemaProfile::PROFILE;
        break;
    case Option::SCHEMA_JSON:
    case Option::TLS_TRUST:
    case Option::AUTH_FILE:

        if (option == Option::SCHEMA_JSON && !features.json) {
            return std::unexpected("native binary JSON schema loading is unavailable in this build");
        }

        if (!validPath(value)) {
            return std::unexpected("file option requires a nonempty bounded opaque path");
        }

        if (option == Option::SCHEMA_JSON) {
            result.schemaJson = value;
        } else if (option == Option::TLS_TRUST) {
            result.tlsTrustPem = value;
        } else {
            result.authBasicTokenFile = value;
        }

        break;
    case Option::OUTPUT:

        if (value != "generic" && value != "quote" && value != "profile") {
            return std::unexpected("--output requires generic, quote or profile");
        }

        result.output = value == "quote"     ? OutputProfile::QUOTE
                        : value == "profile" ? OutputProfile::PROFILE
                                             : OutputProfile::GENERIC;
        break;
    case Option::FAMILY:

        if (value != "stream" && value != "ticker" && value != "history") {
            return std::unexpected("--family requires stream, ticker or history");
        }

        result.family = value == "stream"   ? core::SubscriptionFamily::STREAM
                        : value == "ticker" ? core::SubscriptionFamily::TICKER
                                            : core::SubscriptionFamily::HISTORY;
        break;
    case Option::HISTORY_FROM_RAW: {
        if (value.empty()) {
            return std::unexpected("--history-from-raw requires a complete signed int64 decimal");
        }

        std::int64_t number = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);

        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
            return std::unexpected("--history-from-raw requires a complete signed int64 decimal");
        }

        result.historyFromRaw = number;
        break;
    }
    case Option::SELECTORS:

        if (value != "exact-only" && value != "canonical-exact-v1") {
            return std::unexpected("--selectors requires exact-only or canonical-exact-v1 without All");
        }

        result.selectors = value == "exact-only" ? core::SubscriptionSelectorPolicy::EXACT_ONLY
                                                 : core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1;
        break;
    case Option::DNS_SERVER:

        if (!ipv4(value)) {
            return std::unexpected("--dns-server requires one numeric IPv4 resolver");
        }

        result.dnsServer = value;
        break;
    case Option::REMOVE_AFTER:
    case Option::AUTH_TIMEOUT: {
        const auto maximum = option == Option::REMOVE_AFTER ? MAXIMUM_DURATION_MS : static_cast<unsigned>(INT_MAX);
        const auto number = unsignedNumber(value, 1, maximum);

        if (!number) {
            return std::unexpected(number.error());
        }

        if (option == Option::REMOVE_AFTER) {
            result.removeAfter = std::chrono::milliseconds{*number};
        } else {
            result.authTimeout = std::chrono::milliseconds{*number};
        }

        break;
    }
    case Option::SECURITY:

        if (value != "plain" && value != "tls") {
            return std::unexpected("--security requires plain or tls");
        }

        if (value == "tls" && !features.tls) {
            return std::unexpected("verified TLS recovery is unavailable in this build");
        }

        result.security =
            value == "plain" ? client::EndpointSecurity::CREDENTIAL_FREE_PLAIN : client::EndpointSecurity::TLS;
        break;
    default:
        return std::unexpected("unknown recovery option identity");
    }

    return {};
}

/** Resolve cross-option restrictions and order-independent defaults before publication.
 * @param result Owned candidate with individually validated arguments.
 * @param seen Finite presence table; distinguishes explicit values from defaults.
 * @return Complete candidate readiness or a static contradiction/missing-value refusal.
 */
ParseResult finishConfiguration(RecoveryConfiguration &result, const SeenOptions &seen) {
    const auto supplied = [&seen](Option option) {
        return seen[static_cast<std::size_t>(option)];
    };

    if (!supplied(Option::MODE) || result.host.empty() || result.port == 0 || result.symbols.empty()) {
        return std::unexpected("--mode recovery, --host, --port and at least one --symbol are required");
    }

    if (result.schemaJson && supplied(Option::SCHEMA_PROFILE)) {
        return std::unexpected("--schema-json and --schema-profile are mutually exclusive");
    }

    if (result.schemaJson && !supplied(Option::RECORD_ID)) {
        return std::unexpected("external schema selection requires explicit --record-id");
    }

    if (!result.schemaJson && result.recordId != 0) {
        return std::unexpected("complete built-in schemas contain only local record zero");
    }

    if (result.family == core::SubscriptionFamily::HISTORY) {
        if (!result.historyFromRaw || !result.schemaJson) {
            return std::unexpected("HISTORY requires an external indexed schema and --history-from-raw");
        }
    } else if (result.historyFromRaw) {
        return std::unexpected("--history-from-raw is permitted only with HISTORY");
    }

    if (!result.schemaJson && ((result.output == OutputProfile::QUOTE && result.schema != SchemaProfile::QUOTE) ||
                               (result.output == OutputProfile::PROFILE && result.schema != SchemaProfile::PROFILE))) {
        return std::unexpected("prepared output must match the complete built-in schema");
    }

    if ((!numericHost(result.host) && !result.dnsServer) || (supplied(Option::DNS_PORT) && !result.dnsServer)) {
        return std::unexpected("named hosts and explicit DNS ports require --dns-server");
    }

    if (result.attempts == 1) {
        if (supplied(Option::RETRY_DELAY)) {
            return std::unexpected("--retry-delay-ms requires more than one attempt");
        }
    } else if (!supplied(Option::RETRY_DELAY)) {
        result.retryDelay = DEFAULT_RETRY_DELAY;
    }

    if (result.removeAfter && *result.removeAfter >= result.duration) {
        return std::unexpected("--remove-after-ms must be strictly before the admission duration");
    }

    if (!supplied(Option::READ_TIMEOUT)) {
        result.readTimeout = result.duration;
    }

    if (!supplied(Option::WRITE_TIMEOUT)) {
        result.writeTimeout = result.duration;
    }

    if (!supplied(Option::DNS_TIMEOUT)) {
        result.dnsTimeout = result.connectTimeout;
    }

    if (result.security == client::EndpointSecurity::CREDENTIAL_FREE_PLAIN) {
        if (result.tlsTrustPem || result.tlsReference || result.authBasicTokenFile || result.authTimeout ||
            supplied(Option::TLS_HANDSHAKE) || supplied(Option::TLS_CLOSE)) {
            return std::unexpected("TLS trust, identity, deadlines and credentials are forbidden with plain security");
        }
    } else {
        if (!result.tlsTrustPem || !result.tlsReference) {
            return std::unexpected("TLS requires explicit --tls-trust-pem and --tls-reference");
        }

        if (result.authTimeout && !result.authBasicTokenFile) {
            return std::unexpected("--auth-timeout-ms requires --auth-basic-token-file");
        }

        if (result.authBasicTokenFile && !result.authTimeout) {
            result.authTimeout = DEFAULT_AUTH_TIMEOUT;
        }
    }

    return {};
}
} // namespace

std::expected<RecoveryConfiguration, std::string_view>
parseRecoveryArguments(std::span<const std::string_view> arguments, CompiledRecoveryFeatures features) {
    if (!features.recovery) {
        return std::unexpected("subscription recovery is unavailable in this build");
    }

    try {
        RecoveryConfiguration result;
        SeenOptions seen{};

        for (std::size_t index = 0; index < arguments.size(); index += 2) {
            if (index + 1 == arguments.size()) {
                return std::unexpected("each recovery option requires one value; help must stand alone");
            }

            const auto found = std::ranges::find(OPTION_NAMES, arguments[index]);

            if (found == OPTION_NAMES.end()) {
                return std::unexpected("unknown recovery option; use standalone --help");
            }

            const auto position = static_cast<std::size_t>(found - OPTION_NAMES.begin());
            const auto option = static_cast<Option>(position);

            if (seen[position] && option != Option::SYMBOL) {
                return std::unexpected("only --symbol may be repeated");
            }

            seen[position] = true;

            if (arguments[index + 1].find('\0') != std::string_view::npos) {
                return std::unexpected("recovery argument values must not contain embedded NUL");
            }

            const auto applied = assignOption(result, option, arguments[index + 1], features);

            if (!applied) {
                return std::unexpected(applied.error());
            }
        }

        const auto complete = finishConfiguration(result, seen);

        if (!complete) {
            return std::unexpected(complete.error());
        }

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected("recovery argument allocation failed");
    } catch (const std::length_error &) {
        return std::unexpected("recovery argument storage limit exceeded");
    }
}

} // namespace org::ttldtor::qdcxx::connect
