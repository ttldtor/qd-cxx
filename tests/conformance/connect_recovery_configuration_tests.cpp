// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: pure recovery CLI grammar; no files, bootstrap, SDK or native peers.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_configuration.hpp"

#include <array>
#include <chrono>
#include <doctest/doctest.h>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace a = org::ttldtor::qdcxx::connect;
namespace c = org::ttldtor::qdcxx::core;
namespace d = org::ttldtor::qdcxx::client;
using namespace std::chrono_literals;
constexpr a::CompiledRecoveryFeatures ALL_FEATURES{true, true, true}; ///< Explicit parser-only availability.

/** Form ordinary complete working arguments with one caller-selected host presentation.
 * @param host Borrowed value whose storage must outlive the returned views.
 * @return Finite borrowed option/value sequence, with no schema or process setup.
 */
std::vector<std::string_view> arguments(std::string_view host = "127.0.0.1") {
    return {"--mode", "recovery", "--host", host, "--port", "1234", "--symbol", "IBM"};
}

/** Parse a complete ordinary selection with caller-selected additional pairs.
 * @param extra Borrowed optional pairs, valid through this synchronous call.
 * @param features Explicit parser availability under test.
 * @return Owned candidate or static refusal; no external fixture is needed.
 */
auto parseWith(std::initializer_list<std::string_view> extra, a::CompiledRecoveryFeatures features = ALL_FEATURES) {
    auto input = arguments();
    input.insert(input.end(), extra.begin(), extra.end());

    return a::parseRecoveryArguments(input, features);
}
} // namespace

TEST_CASE("recovery CLI defaults preserve explicit exact plain selection and independent timeout units") {
    const auto result = parseWith({});
    REQUIRE(result);
    CHECK(result->host == "127.0.0.1");
    CHECK(result->port == 1234);
    CHECK(result->family == c::SubscriptionFamily::STREAM);
    CHECK(result->schema == a::SchemaProfile::QUOTE);
    CHECK(result->output == a::OutputProfile::GENERIC);
    CHECK(result->recordId == 0);
    CHECK_FALSE(result->schemaJson);
    CHECK(result->symbols == std::vector<std::u16string>{u"IBM"});
    CHECK(result->selectors == c::SubscriptionSelectorPolicy::EXACT_ONLY);
    CHECK_FALSE(result->historyFromRaw);
    CHECK_FALSE(result->dnsServer);
    CHECK(result->dnsPort == 53);
    CHECK(result->attempts == 1);
    CHECK(result->retryDelay == 0ms);
    CHECK(result->duration == 5000ms);
    CHECK(result->connectTimeout == 1000ms);
    CHECK(result->readTimeout == result->duration);
    CHECK(result->writeTimeout == result->duration);
    CHECK(result->dnsTimeout == result->connectTimeout);
    CHECK(result->pollInterval == 1ms);
    CHECK(result->drain == 1000ms);
    CHECK(result->security == d::EndpointSecurity::CREDENTIAL_FREE_PLAIN);
    CHECK_FALSE(result->authTimeout);
}

TEST_CASE("recovery CLI owns every accepted text value and consumes option-looking symbol values literally") {
    std::vector<std::string> storage{"--mode",
                                     "recovery",
                                     "--host",
                                     "route.invalid",
                                     "--port",
                                     "1234",
                                     "--symbol",
                                     "--mode",
                                     "--dns-server",
                                     "127.0.0.1",
                                     "--schema-json",
                                     "unopened-schema.json",
                                     "--record-id",
                                     "0",
                                     "--security",
                                     "tls",
                                     "--tls-trust-pem",
                                     "unopened-trust.pem",
                                     "--tls-reference",
                                     "reference.invalid",
                                     "--auth-basic-token-file",
                                     "unopened-secret.token"};
    std::vector<std::string_view> input;

    for (const auto &value : storage) {
        input.push_back(value);
    }

    const auto result = a::parseRecoveryArguments(input, ALL_FEATURES);
    REQUIRE(result);

    for (auto &value : storage) {
        value.assign(value.size(), 'x');
    }

    CHECK(result->host == "route.invalid");
    CHECK(result->symbols == std::vector<std::u16string>{u"--mode"});
    CHECK(result->dnsServer == "127.0.0.1");
    CHECK(result->schemaJson == "unopened-schema.json");
    CHECK(result->tlsTrustPem == "unopened-trust.pem");
    CHECK(result->tlsReference == "reference.invalid");
    CHECK(result->authBasicTokenFile == "unopened-secret.token");
    CHECK(result->authTimeout == 1000ms);
}

TEST_CASE("recovery CLI mode pairing singleton and required arguments fail without partial publication") {
    CHECK_FALSE(a::parseRecoveryArguments({}, ALL_FEATURES));
    CHECK_FALSE(a::parseRecoveryArguments(std::array<std::string_view, 2>{"--mode", "recovery"}, ALL_FEATURES));
    CHECK_FALSE(parseWith({"--mode", "recovery"}));
    CHECK_FALSE(parseWith({"--port", "2345"}));
    CHECK_FALSE(parseWith({"--duration-ms", "10", "--duration-ms", "20"}));
    CHECK_FALSE(parseWith({"--help", "x"}));
    CHECK_FALSE(parseWith({"--unknown", "private-sentinel"}));

    auto missingMode = arguments();
    missingMode.erase(missingMode.begin(), missingMode.begin() + 2);
    CHECK_FALSE(a::parseRecoveryArguments(missingMode, ALL_FEATURES));

    auto incomplete = arguments();
    incomplete.push_back("--family");
    CHECK_FALSE(a::parseRecoveryArguments(incomplete, ALL_FEATURES));

    auto wrongMode = arguments();
    wrongMode[1] = "legacy";
    CHECK_FALSE(a::parseRecoveryArguments(wrongMode, ALL_FEATURES));
}

TEST_CASE("recovery CLI feature refusal needs no reachable schema trust or secret files and never echoes them") {
    const auto recoveryOff = parseWith({}, {false, true, true});
    REQUIRE_FALSE(recoveryOff);
    CHECK(recoveryOff.error() == "subscription recovery is unavailable in this build");

    const auto jsonOff =
        parseWith({"--schema-json", "private-schema-sentinel", "--record-id", "0"}, {true, true, false});
    REQUIRE_FALSE(jsonOff);
    CHECK(jsonOff.error().find("private-schema-sentinel") == std::string_view::npos);

    const auto tlsOff = parseWith({"--tls-trust-pem", "private-trust-sentinel", "--tls-reference", "localhost",
                                   "--auth-basic-token-file", "private-secret-sentinel", "--security", "tls"},
                                  {true, false, true});
    REQUIRE_FALSE(tlsOff);
    CHECK(tlsOff.error() == "verified TLS recovery is unavailable in this build");
    CHECK(tlsOff.error().find("private-") == std::string_view::npos);
    CHECK(parseWith({}, {true, false, false}));
}

TEST_CASE("recovery CLI resolves duration timeout DNS and retry defaults independently of pair ordering") {
    const auto early =
        parseWith({"--read-timeout-ms", "12", "--duration-ms", "21", "--connect-timeout-ms", "17", "--attempts", "2"});
    const auto late =
        parseWith({"--attempts", "2", "--connect-timeout-ms", "17", "--duration-ms", "21", "--read-timeout-ms", "12"});
    REQUIRE(early);
    REQUIRE(late);
    CHECK(early->readTimeout == 12ms);
    CHECK(late->readTimeout == early->readTimeout);
    CHECK(early->writeTimeout == 21ms);
    CHECK(late->writeTimeout == early->writeTimeout);
    CHECK(early->dnsTimeout == 17ms);
    CHECK(late->dnsTimeout == early->dnsTimeout);
    CHECK(early->retryDelay == 250ms);
    CHECK(late->retryDelay == early->retryDelay);

    CHECK_FALSE(parseWith({"--retry-delay-ms", "1"}));
    CHECK_FALSE(parseWith({"--attempts", "1", "--retry-delay-ms", "1"}));
    CHECK(parseWith({"--retry-delay-ms", "2147483647", "--attempts", "4294967295"}));
    CHECK_FALSE(parseWith({"--remove-after-ms", "21", "--duration-ms", "21"}));
    CHECK_FALSE(parseWith({"--remove-after-ms", "22", "--duration-ms", "21"}));
    CHECK(parseWith({"--duration-ms", "21", "--remove-after-ms", "20"}));
}

TEST_CASE("recovery CLI unsigned numeric options reject signs partial values overflow and crossed ceilings") {
    constexpr std::array<std::string_view, 9> malformed{
        "", "0", "-1", "+1", " 1", "1 ", "1x", "1e2", "18446744073709551616"};

    for (const auto value : malformed) {
        CAPTURE(value);
        CHECK_FALSE(parseWith({"--duration-ms", value}));
        CHECK_FALSE(parseWith({"--attempts", value}));
        CHECK_FALSE(parseWith({"--connect-timeout-ms", value}));
    }

    CHECK(parseWith({"--duration-ms", "600000", "--drain-ms", "60000", "--max-records", "1000000",
                     "--connect-timeout-ms", "2147483647", "--read-timeout-ms", "2147483647", "--write-timeout-ms",
                     "2147483647", "--poll-interval-ms", "2147483647", "--dns-timeout-ms", "2147483647"}));
    CHECK_FALSE(parseWith({"--duration-ms", "600001"}));
    CHECK_FALSE(parseWith({"--drain-ms", "60001"}));
    CHECK_FALSE(parseWith({"--max-records", "1000001"}));
    CHECK_FALSE(parseWith({"--attempts", "4294967296"}));
    CHECK_FALSE(parseWith({"--connect-timeout-ms", "2147483648"}));

    auto lowPort = arguments();
    lowPort[5] = "1";
    CHECK(a::parseRecoveryArguments(lowPort, ALL_FEATURES));
    lowPort[5] = "65535";
    CHECK(a::parseRecoveryArguments(lowPort, ALL_FEATURES));
    lowPort[5] = "65536";
    CHECK_FALSE(a::parseRecoveryArguments(lowPort, ALL_FEATURES));
    lowPort[5] = "0";
    CHECK_FALSE(a::parseRecoveryArguments(lowPort, ALL_FEATURES));
}

TEST_CASE("recovery CLI HISTORY retains full signed raw bounds and demands explicit external local identity") {
    for (const auto text : {"-9223372036854775808", "0", "9223372036854775807"}) {
        const auto parsed = parseWith({"--family", "history", "--schema-json", "unopened.json", "--record-id",
                                       "2147483647", "--history-from-raw", text});
        REQUIRE(parsed);
        CHECK(parsed->family == c::SubscriptionFamily::HISTORY);
        CHECK(parsed->recordId == std::numeric_limits<std::int32_t>::max());
        REQUIRE(parsed->historyFromRaw);

        if (std::string_view(text).front() == '-') {
            CHECK(*parsed->historyFromRaw == std::numeric_limits<std::int64_t>::min());
        } else if (std::string_view(text) == "0") {
            CHECK(*parsed->historyFromRaw == 0);
        } else {
            CHECK(*parsed->historyFromRaw == std::numeric_limits<std::int64_t>::max());
        }
    }

    for (const auto text : {"-9223372036854775809", "9223372036854775808", "+1", "1.0", "1e3", "1x", ""}) {
        CHECK_FALSE(parseWith(
            {"--family", "history", "--schema-json", "unopened.json", "--record-id", "0", "--history-from-raw", text}));
    }

    CHECK_FALSE(parseWith({"--family", "history", "--history-from-raw", "0"}));
    CHECK_FALSE(parseWith({"--family", "history", "--schema-json", "unopened.json", "--record-id", "0"}));
    CHECK_FALSE(parseWith({"--history-from-raw", "0"}));
    CHECK_FALSE(parseWith({"--family", "ticker", "--history-from-raw", "0"}));
    CHECK_FALSE(parseWith({"--schema-json", "unopened.json"}));
    CHECK_FALSE(parseWith({"--schema-json", "unopened.json", "--record-id", "-1"}));
    CHECK_FALSE(parseWith({"--schema-json", "unopened.json", "--record-id", "2147483648"}));
    CHECK_FALSE(parseWith({"--schema-json", "unopened.json", "--record-id", "0", "--schema-profile", "quote"}));
    CHECK_FALSE(parseWith({"--record-id", "1"}));
}

TEST_CASE("recovery CLI supports explicit family mapper and selector choices without All or silent inference") {
    const auto ticker = parseWith({"--family", "ticker", "--schema-profile", "profile", "--output", "profile",
                                   "--selectors", "canonical-exact-v1"});
    REQUIRE(ticker);
    CHECK(ticker->family == c::SubscriptionFamily::TICKER);
    CHECK(ticker->schema == a::SchemaProfile::PROFILE);
    CHECK(ticker->output == a::OutputProfile::PROFILE);
    CHECK(ticker->selectors == c::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1);
    CHECK_FALSE(parseWith({"--family", "all"}));
    CHECK_FALSE(parseWith({"--selectors", "stream-all-v1"}));
    CHECK_FALSE(parseWith({"--schema-profile", "quote", "--output", "profile"}));
    CHECK_FALSE(parseWith({"--schema-profile", "profile", "--output", "quote"}));
    CHECK_FALSE(parseWith({"--schema-profile", "inferred"}));
    CHECK_FALSE(parseWith({"--output", "trade"}));
    CHECK(parseWith({"--schema-json", "unopened.json", "--record-id", "0", "--output", "profile"}));
}

TEST_CASE("recovery CLI numeric hosts bypass resolver requirements while names require explicit IPv4 resolver") {
    for (const auto host : {"127.0.0.1", "::1", "2001:db8::1", "::ffff:192.0.2.1", "1:2:3:4:5:6:7:8"}) {
        CAPTURE(host);
        CHECK(a::parseRecoveryArguments(arguments(host), ALL_FEATURES));
    }

    for (const auto host : {"route.invalid", "ROUTE.invalid.", "xn--example.invalid"}) {
        CAPTURE(host);
        auto input = arguments(host);
        CHECK_FALSE(a::parseRecoveryArguments(input, ALL_FEATURES));
        input.insert(input.end(), {"--dns-server", "127.0.0.1", "--dns-port", "65535"});
        const auto parsed = a::parseRecoveryArguments(input, ALL_FEATURES);
        REQUIRE(parsed);
        CHECK(parsed->host == host);
        CHECK(parsed->dnsPort == 65535);
    }

    for (const auto host : {"", "http://host", "[::1]", "fe80::1%2", "1:2:3:4:5:6:7", "1:::2", "1::2::3", "01.2.3.4",
                            "256.1.2.3", "-bad.invalid", "bad-.invalid", "bad..invalid", "bad_name"}) {
        auto input = arguments(host);
        input.insert(input.end(), {"--dns-server", "127.0.0.1"});
        CHECK_FALSE(a::parseRecoveryArguments(input, ALL_FEATURES));
    }

    CHECK_FALSE(parseWith({"--dns-port", "53"}));
    CHECK_FALSE(parseWith({"--dns-server", "::1"}));
    CHECK_FALSE(parseWith({"--dns-server", "resolver.invalid"}));
    CHECK_FALSE(parseWith({"--dns-server", "127.0.0.1", "--dns-port", "0"}));
    CHECK_FALSE(parseWith({"--dns-server", "127.0.0.1", "--dns-port", "65536"}));
}

TEST_CASE("recovery CLI exact symbol limits preserve spaces and reject duplicates wildcard control and overflow") {
    auto input = arguments();
    std::vector<std::string> additional;

    for (unsigned index = 0; index < 16; ++index) {
        additional.push_back("S" + std::to_string(index));
    }

    for (unsigned index = 0; index < 15; ++index) {
        input.insert(input.end(), {"--symbol", additional[index]});
    }

    const auto full = a::parseRecoveryArguments(input, ALL_FEATURES);
    REQUIRE(full);
    CHECK(full->symbols.size() == 16);
    input.insert(input.end(), {"--symbol", additional[15]});
    CHECK_FALSE(a::parseRecoveryArguments(input, ALL_FEATURES));
    CHECK_FALSE(parseWith({"--symbol", "IBM"}));
    CHECK_FALSE(parseWith({"--symbol", "*"}));
    CHECK_FALSE(parseWith({"--symbol", "*IBM"}));
    CHECK_FALSE(parseWith({"--symbol", ""}));
    CHECK_FALSE(parseWith({"--symbol", "bad\tvalue"}));
    CHECK_FALSE(parseWith({"--symbol", "nonascii\x7f"}));
    CHECK(parseWith({"--symbol", "IBM space"}));
    CHECK(parseWith({"--symbol", "ibm"}));

    const std::string longest(128, 'A');
    const std::string tooLong(129, 'A');
    CHECK(parseWith({"--symbol", longest}));
    CHECK_FALSE(parseWith({"--symbol", tooLong}));
}

TEST_CASE("recovery CLI TLS requires explicit trust reference and paired authentication without plaintext downgrade") {
    CHECK_FALSE(parseWith({"--security", "unknown"}));
    CHECK_FALSE(parseWith({"--security", "tls"}));
    CHECK_FALSE(parseWith({"--security", "tls", "--tls-trust-pem", "unopened.pem"}));
    CHECK_FALSE(parseWith({"--security", "tls", "--tls-reference", "localhost"}));
    CHECK_FALSE(parseWith({"--tls-trust-pem", "unopened.pem"}));
    CHECK_FALSE(parseWith({"--tls-reference", "localhost"}));
    CHECK_FALSE(parseWith({"--tls-handshake-ms", "1"}));
    CHECK_FALSE(parseWith({"--tls-close-ms", "1"}));
    CHECK_FALSE(parseWith({"--auth-basic-token-file", "unopened.token"}));
    CHECK_FALSE(parseWith({"--auth-timeout-ms", "1"}));
    CHECK_FALSE(parseWith({"--security", "tls", "--tls-trust-pem", "unopened.pem", "--tls-reference", "localhost",
                           "--auth-timeout-ms", "1"}));
    CHECK_FALSE(parseWith({"--security", "tls", "--tls-trust-pem", "unopened.pem", "--tls-reference", "*.invalid"}));

    const auto tls = parseWith({"--tls-trust-pem", "unopened.pem", "--tls-reference", "reference.invalid",
                                "--tls-handshake-ms", "2147483647", "--tls-close-ms", "2147483647", "--auth-timeout-ms",
                                "2147483647", "--auth-basic-token-file", "unopened.token", "--security", "tls"});
    REQUIRE(tls);
    CHECK(tls->host == "127.0.0.1");
    CHECK(tls->tlsReference == "reference.invalid");
    CHECK(tls->security == d::EndpointSecurity::TLS);
    CHECK(tls->tlsHandshake == 2147483647ms);
    CHECK(tls->tlsClose == 2147483647ms);
    CHECK(tls->authTimeout == 2147483647ms);
}

TEST_CASE("recovery CLI opaque file arguments and host labels are finite and errors never echo caller bytes") {
    const std::string longestPath(4096, 'p');
    const std::string oversizedPath(4097, 'p');
    CHECK(parseWith({"--schema-json", longestPath, "--record-id", "0"}));
    CHECK_FALSE(parseWith({"--schema-json", oversizedPath, "--record-id", "0"}));
    CHECK_FALSE(parseWith({"--schema-json", "", "--record-id", "0"}));

    const std::string nulPath("private\0sentinel", 16);
    const auto embedded = parseWith({"--schema-json", nulPath, "--record-id", "0"});
    REQUIRE_FALSE(embedded);
    CHECK(embedded.error().find("private") == std::string_view::npos);

    const std::string label(63, 'a');
    const std::string goodHost = label + ".invalid";
    auto good = arguments(goodHost);
    good.insert(good.end(), {"--dns-server", "127.0.0.1"});
    CHECK(a::parseRecoveryArguments(good, ALL_FEATURES));

    const std::string oversizedLabel(64, 'a');
    const std::string badHost = oversizedLabel + ".invalid";
    auto bad = arguments(badHost);
    bad.insert(bad.end(), {"--dns-server", "127.0.0.1"});
    const auto rejected = a::parseRecoveryArguments(bad, ALL_FEATURES);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().find(oversizedLabel) == std::string_view::npos);
}
