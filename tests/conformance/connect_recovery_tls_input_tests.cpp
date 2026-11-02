// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: PROGRAM RUN; creates only its fresh owned RUN, removes its own files,
// and returns nonzero for assertion/I/O/cleanup failure. No SDK/bootstrap or TLS runtime is used.
#define DOCTEST_CONFIG_IMPLEMENT
#include "recovery_file_input.hpp"
#include "recovery_tls_input.hpp"

#include "qd/events/quote.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdio>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
namespace a = org::ttldtor::qdcxx::connect;
namespace c = org::ttldtor::qdcxx::client;
namespace e = org::ttldtor::qdcxx::events;
namespace fs = std::filesystem;
using namespace std::chrono_literals;
constexpr std::size_t MEBIBYTE = 1024 * 1024;     ///< Largest supported application document.
constexpr std::size_t TRUST_BYTES = 512 * 1024;   ///< Qualified trust-source ceiling.
constexpr std::size_t MAXIMUM_TOKEN_UNITS = 4094; ///< Complete prefix plus maximum complete payload quartets.
constexpr std::size_t TLS_CONTROL_BYTES = 65536;  ///< Selected test endpoint's explicit control/body capacities.
constexpr std::string_view TOKEN = "Basic cWRjeHg6dGVzdA=="; ///< Public synthetic repository credential only.
fs::path runRoot; ///< Main exclusively creates this supplied fresh directory, then removes it nonrecursively.
std::size_t caseSequence = 0; ///< Serial test-owned directory suffix, never an ownership test.

/** Convert native paths to explicit UTF-8 bytes without changing locale or process codepages.
 * @param path Existing controlled test path.
 * @return Owned UTF-8 presentation accepted by the application input helper.
 */
std::string utf8(const fs::path &path) {
    const auto bytes = path.generic_u8string();
    std::string result;
    result.reserve(bytes.size());

    for (const auto byte : bytes)
        result.push_back(static_cast<char>(byte));

    return result;
}

/** One exclusively created test directory; cleanup removes only its explicitly registered files. */
class Files {
    fs::path directory_;          ///< Owned fresh child of main's fresh RUN; never a caller-supplied deletion target.
    std::vector<fs::path> files_; ///< Only paths created by write; no recursive traversal or deletion.

    public:
    /** Create one exact owned child of the already owned run directory. */
    Files() : directory_(runRoot / ("input-case-" + std::to_string(++caseSequence))) {
        std::error_code error;
        REQUIRE(fs::create_directory(directory_, error));
        REQUIRE_FALSE(error);
    }

    /** Remove exact owned regular files and the resulting empty directory, without throwing. */
    ~Files() {
        std::error_code error;

        for (const auto &file : files_)
            fs::remove(file, error);

        fs::remove(directory_, error);
    }

    /** Create a uniquely named ordinary binary file in this owned directory.
     * @param name Controlled filename supplied by the test, never external input.
     * @param bytes Exact complete content, possibly empty or deliberately malformed.
     * @return Native owned file path, retained for cleanup.
     */
    fs::path write(const fs::path &name, std::string_view bytes) {
        const auto path = directory_ / name;
        REQUIRE_FALSE(fs::exists(path));
        files_.push_back(path);
        std::ofstream output(path, std::ios::binary);
        REQUIRE(output.good());
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        REQUIRE(output.good());
        output.close();
        REQUIRE_FALSE(output.fail());

        return path;
    }

    /** Borrow the still-owned directory for a deterministic nonregular input rejection.
     * @return Directory path, valid until this fixture retires.
     */
    const fs::path &directory() const noexcept {
        return directory_;
    }
};

/** Read one explicitly configured ordinary public synthetic trust fixture without SDK calls.
 * @return Exact PEM fixture bytes, without decoding or certificate validation.
 */
std::string trustFixture() {
    const auto path = fs::path(ORG_TTLDTOR_QDCXX_TLS_TEST_FIXTURES) / "ca.pem";
    auto result = a::readRecoveryFile(utf8(path), TRUST_BYTES);
    REQUIRE(result);

    return std::move(*result);
}

/** Parse the actual CLI grammar rather than constructing an assumed valid identity by hand.
 * @param trust Existing explicit file path.
 * @param token Optional explicit token file, empty for no provider.
 * @param reference Valid endpoint reference presentation, unchanged in the result.
 * @return Complete owned parsed TLS configuration, with no SDK or file access by the parser.
 */
a::RecoveryConfiguration configuration(const fs::path &trust, const fs::path &token = {},
                                       std::string_view reference = "localhost") {
    std::vector<std::string> storage{"--mode",
                                     "recovery",
                                     "--host",
                                     "127.0.0.1",
                                     "--port",
                                     "1234",
                                     "--symbol",
                                     "IBM",
                                     "--security",
                                     "tls",
                                     "--tls-trust-pem",
                                     utf8(trust),
                                     "--tls-reference",
                                     std::string(reference),
                                     "--tls-handshake-ms",
                                     "1234",
                                     "--tls-close-ms",
                                     "2345"};

    if (!token.empty()) {
        storage.push_back("--auth-basic-token-file");
        storage.push_back(utf8(token));
    }

    std::vector<std::string_view> arguments;

    for (const auto &value : storage)
        arguments.push_back(value);

    auto result = a::parseRecoveryArguments(arguments, {true, true, true});
    REQUIRE(result);

    return std::move(*result);
}

/** Check destination ownership/copy boundaries without logging the synthetic token.
 * @param provider Shared immutable provider, independently retained by the caller.
 * @param token Expected exact public synthetic bytes, widened only for equality.
 */
void checkCopies(const std::shared_ptr<c::CredentialProvider> &provider, std::string_view token) {
    REQUIRE(provider);
    std::u16string expected;

    for (const unsigned char byte : token)
        expected.push_back(static_cast<char16_t>(byte));

    std::vector<char16_t> shortBuffer(expected.size() - 1, u'!');
    const auto rejected = provider->basicToken(shortBuffer);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == c::CredentialError::REJECTED);
    CHECK(std::ranges::all_of(shortBuffer, [](char16_t unit) {
        return unit == u'!';
    }));
    CHECK_FALSE(provider->basicToken({}));

    for (unsigned repetition = 0; repetition < 2; ++repetition) {
        std::vector<char16_t> exact(expected.size(), u'!');
        const auto copied = provider->basicToken(exact);
        REQUIRE(copied);
        CHECK(*copied == expected.size());
        CHECK(std::u16string_view(exact.data(), exact.size()) == expected);
    }

    std::vector<char16_t> larger(expected.size() + 3, u'!');
    const auto copied = provider->basicToken(larger);
    REQUIRE(copied);
    CHECK(*copied == expected.size());
    CHECK(std::u16string_view(larger.data(), expected.size()) == expected);
    CHECK(std::ranges::all_of(std::span(larger).subspan(expected.size()), [](char16_t unit) {
        return unit == u'!';
    }));
}
} // namespace

TEST_CASE("recovery file input preserves exact stable bytes and Unicode canonical regular paths") {
    Files files;
    const auto path = files.write(fs::path(u8"вход-\U0001f642.txt"), "abc\r\nxyz\n");
    const auto presentation = utf8(path.parent_path() / "." / path.filename());
    auto result = a::readRecoveryFile(presentation, 9);
    REQUIRE(result);
    CHECK(*result == "abc\r\nxyz\n");
    CHECK(result->size() == 9);
    const auto empty = files.write("empty.txt", {});
    auto emptyResult = a::readRecoveryFile(utf8(empty), 1);
    REQUIRE(emptyResult);
    CHECK(emptyResult->empty());

    for (const auto cap : {std::size_t{4096}, TRUST_BYTES, MEBIBYTE}) {
        const auto exact = files.write("exact-" + std::to_string(cap), std::string(cap, 'x'));
        auto accepted = a::readRecoveryFile(utf8(exact), cap);
        REQUIRE(accepted);
        CHECK(accepted->size() == cap);
        CHECK_FALSE(a::readRecoveryFile(utf8(exact), cap - 1));
    }

    const auto nul = files.write("embedded-nul.txt", std::string("a\0b", 3));
    CHECK_FALSE(a::readRecoveryFile(utf8(nul), 3));
}

TEST_CASE("recovery file input refuses invalid UTF8 shape IO and ceilings with static redaction") {
    Files files;
    const auto missing = utf8(files.directory() / "private-path-sentinel-missing");
    auto result = a::readRecoveryFile(missing, 4096);
    REQUIRE_FALSE(result);
    CHECK(result.error().find("private-path-sentinel") == std::string_view::npos);
    CHECK_FALSE(a::readRecoveryFile(utf8(files.directory()), 4096));
    CHECK_FALSE(a::readRecoveryFile({}, 4096));
    CHECK_FALSE(a::readRecoveryFile(std::string(4097, 'x'), 4096));
    CHECK_FALSE(a::readRecoveryFile(missing, 0));
    CHECK_FALSE(a::readRecoveryFile(missing, MEBIBYTE + 1));

    const std::vector<std::string> invalid{std::string("a\0b", 3), "\x80",         "\xc0\xaf",
                                           "\xe0\x80\xaf",         "\xed\xa0\x80", "\xf4\x90\x80\x80",
                                           "\xf0\x9f\x99",         "\xc2x"};

    for (const auto &path : invalid) {
        auto refusal = a::readRecoveryFile(path, 4096);
        REQUIRE_FALSE(refusal);
        CHECK(refusal.error() == "recovery input path is invalid");
    }
}

TEST_CASE("recovery TLS inputs retain actual trust identity deadlines and independent limits without SDK") {
    Files files;
    const auto pem = trustFixture();
    const auto path = files.write("trust.pem", pem);
    const auto reference = std::string(63, 'a') + "." + std::string(63, 'b') + "." + std::string(63, 'c') + "." +
                           std::string(61, 'd') + ".";
    REQUIRE(reference.size() == 254);
    const auto input = configuration(path, {}, reference);
    const auto prepared = a::prepareRecoveryTlsInput(input);
    REQUIRE(prepared);
    CHECK(prepared->endpoint.trustPem == pem);
    CHECK(prepared->endpoint.referenceIdentity == reference);
    CHECK(prepared->endpoint.handshakeTimeout == 1234ms);
    CHECK(prepared->endpoint.closeTimeout == 2345ms);
    CHECK_FALSE(prepared->endpoint.credentials);
    CHECK_FALSE(prepared->endpoint.tokenUnits);
    CHECK_FALSE(prepared->endpoint.authenticationTimeout);
    CHECK(prepared->limits.trustBytes == pem.size());
    CHECK(prepared->limits.anchors == 1);
    CHECK(prepared->limits.identityBytes == 254);
    CHECK(prepared->limits.certificateBytes == 65536);
    CHECK(prepared->limits.certificateRequestBytes == 65536);
    CHECK(*input.tlsReference == reference);

    auto settings = c::controlledStreamSettings();
    auto schema = e::defaultQuoteSchema({16, 128, 256, 128, 16384});
    REQUIRE(settings);
    REQUIRE(schema);
    settings->host = "127.0.0.1";
    settings->port = 1234;
    settings->security = c::EndpointSecurity::TLS;
    settings->tls = prepared->endpoint;
    CHECK(c::EndpointOptions::create(*schema, *settings));
    settings->tls->referenceIdentity = "not a valid identity";
    CHECK_FALSE(c::EndpointOptions::create(*schema, *settings));
    settings->tls = prepared->endpoint;
    settings->tls->handshakeTimeout.reset();
    CHECK_FALSE(c::EndpointOptions::create(*schema, *settings));
}

TEST_CASE("recovery TLS input counts all ordinary markers and enforces exact trust ceiling") {
    Files files;
    const auto pem = trustFixture();
    REQUIRE(pem.size() * 257 <= TRUST_BYTES);
    std::string many;

    for (std::size_t index = 0; index < 256; ++index)
        many += pem;

    auto accepted = a::prepareRecoveryTlsInput(configuration(files.write("anchors-256.pem", many)));
    REQUIRE(accepted);
    CHECK(accepted->limits.anchors == 256);
    CHECK(accepted->limits.trustBytes == many.size());
    many += pem;
    CHECK_FALSE(a::prepareRecoveryTlsInput(configuration(files.write("anchors-257.pem", many))));
    std::string maximum = pem;
    maximum.resize(TRUST_BYTES, '\n');
    auto exact = a::prepareRecoveryTlsInput(configuration(files.write("trust-exact.pem", maximum)));
    REQUIRE(exact);
    CHECK(exact->limits.trustBytes == TRUST_BYTES);
    maximum.push_back('\n');
    CHECK_FALSE(a::prepareRecoveryTlsInput(configuration(files.write("trust-over.pem", maximum))));

    const std::array<std::string_view, 5> invalid{"", "private-content-sentinel", "-----BEGIN CERTIFICATE-----\n",
                                                  "-----END CERTIFICATE-----\n",
                                                  "-----BEGIN PRIVATE KEY-----\nAAAA\n-----END PRIVATE KEY-----\n"};

    for (std::size_t index = 0; index < invalid.size(); ++index) {
        auto refused =
            a::prepareRecoveryTlsInput(configuration(files.write("bad-" + std::to_string(index), invalid[index])));
        REQUIRE_FALSE(refused);
        CHECK(refused.error().find("private-content-sentinel") == std::string_view::npos);
    }

    const auto notAsn1 =
        files.write("count-only.pem", "-----BEGIN CERTIFICATE-----\nAAAA\n-----END CERTIFICATE-----\n");
    CHECK(a::prepareRecoveryTlsInput(configuration(notAsn1)));
}

TEST_CASE("recovery TLS provider accepts complete canonical zero one two padding and retains independent lifetime") {
    Files files;
    const auto trust = files.write("trust.pem", trustFixture());
    const std::array<std::string_view, 4> tokens{TOKEN, "Basic TWFu", "Basic TWE=", "Basic TQ=="};

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const auto file = files.write("token-" + std::to_string(index), tokens[index]);
        std::shared_ptr<c::CredentialProvider> provider;

        {
            const auto input = configuration(trust, file);
            auto prepared = a::prepareRecoveryTlsInput(input);
            REQUIRE(prepared);
            CHECK(prepared->endpoint.tokenUnits == tokens[index].size());
            CHECK(prepared->endpoint.authenticationTimeout == 1000ms);
            provider = prepared->endpoint.credentials;
        }

        REQUIRE(fs::remove(file));
        checkCopies(provider, tokens[index]);
    }
}

TEST_CASE("recovery TLS maximum complete token survives shared endpoint snapshots without SDK") {
    Files files;
    const auto trust = files.write("trust.pem", trustFixture());
    const auto token = std::string("Basic ") + std::string(MAXIMUM_TOKEN_UNITS - 6, 'A');
    const auto file = files.write("maximum.token", token);
    auto prepared = a::prepareRecoveryTlsInput(configuration(trust, file));
    REQUIRE(prepared);
    REQUIRE(prepared->endpoint.tokenUnits == MAXIMUM_TOKEN_UNITS);
    CHECK(3 * MAXIMUM_TOKEN_UNITS + 64 == 12346);
    checkCopies(prepared->endpoint.credentials, token);
    auto schema = e::defaultQuoteSchema({16, 128, 256, 128, 16384});
    auto settings = c::controlledStreamSettings();
    REQUIRE(schema);
    REQUIRE(settings);
    settings->host = "127.0.0.1";
    settings->port = 1234;
    settings->security = c::EndpointSecurity::TLS;
    settings->tls = prepared->endpoint;
    settings->budgets.session.frame = {TLS_CONTROL_BYTES, TLS_CONTROL_BYTES + 9};
    settings->budgets.session.outputBytes = TLS_CONTROL_BYTES;
    const auto narrowText = c::EndpointOptions::create(*schema, *settings);
    REQUIRE_FALSE(narrowText);
    CHECK(narrowText.error().stage == "endpoint-security");
    settings->budgets.session.protocol.text = {TLS_CONTROL_BYTES, 4096};
    const auto narrowWriter = c::EndpointOptions::create(*schema, *settings);
    REQUIRE_FALSE(narrowWriter);
    CHECK(narrowWriter.error().stage == "endpoint-budgets");
    settings->budgets.writer.frameBytes = TLS_CONTROL_BYTES;
    settings->budgets.writer.operationBytes = TLS_CONTROL_BYTES + 9;
    auto first = c::EndpointOptions::create(*schema, *settings);
    auto second = c::EndpointOptions::create(*schema, *settings);
    REQUIRE(first);
    REQUIRE(second);
    const auto provider = second->settings().tls->credentials;
    CHECK(provider == first->settings().tls->credentials);
    REQUIRE(fs::remove(file));
    REQUIRE(fs::remove(trust));
    prepared->endpoint.credentials.reset();
    settings->tls.reset();
    checkCopies(provider, token);
}

TEST_CASE("recovery TLS token input refuses malformed exact bytes without secret diagnostics") {
    Files files;
    const auto trust = files.write("trust.pem", trustFixture());
    std::vector<std::string> invalid{"",
                                     "Basic ",
                                     "basic TWFu",
                                     "Bearer TWFu",
                                     "BasicTWFu",
                                     "Basic  TWFu",
                                     " Basic TWFu",
                                     "Basic T",
                                     "Basic TW",
                                     "Basic TWE",
                                     "Basic TQ",
                                     "Basic TQ=",
                                     "Basic TW-F",
                                     "Basic TW_F",
                                     "Basic A===",
                                     "Basic ====",
                                     "Basic AA=A",
                                     "Basic AB==",
                                     "Basic AAB=",
                                     "Basic TWFu\n",
                                     "Basic TWFu\r\n",
                                     "Basic TWFu ",
                                     "Basic \xc2\xa9"
                                     "AA",
                                     std::string("Basic AA\0A", 10),
                                     std::string(4097, 'x'),
                                     std::string("Basic ") + std::string(4090, 'A')};

    for (unsigned control = 0; control <= 0x1f; ++control)
        invalid.push_back("Basic AA" + std::string(1, static_cast<char>(control)) + "A");

    invalid.push_back("Basic AA\x7f"
                      "A");

    for (std::size_t index = 0; index < invalid.size(); ++index) {
        const auto path = files.write("private-token-sentinel-" + std::to_string(index), invalid[index]);
        auto result = a::prepareRecoveryTlsInput(configuration(trust, path));
        REQUIRE_FALSE(result);
        CHECK(result.error().find("private-token-sentinel") == std::string_view::npos);
        CHECK(result.error().find("TWFu") == std::string_view::npos);
    }
}

TEST_CASE("recovery TLS input rejects contradictory security and deadlines before opening files") {
    Files files;
    auto input = configuration(files.directory() / "private-unopened-sentinel");
    input.tlsHandshake = 0ms;
    auto result = a::prepareRecoveryTlsInput(input);
    REQUIRE_FALSE(result);
    CHECK(result.error() == "recovery TLS deadlines or credential selection are invalid");
    input.tlsHandshake = 1234ms;
    input.tlsClose = std::chrono::milliseconds(static_cast<long long>(INT_MAX) + 1);
    CHECK_FALSE(a::prepareRecoveryTlsInput(input));
    input.tlsClose = 2345ms;
    input.authTimeout = 1ms;
    CHECK_FALSE(a::prepareRecoveryTlsInput(input));
    input.authTimeout.reset();
    input.security = c::EndpointSecurity::CREDENTIAL_FREE_PLAIN;
    CHECK_FALSE(a::prepareRecoveryTlsInput(input));
    input.security = c::EndpointSecurity::TLS;
    input.tlsReference.reset();
    CHECK_FALSE(a::prepareRecoveryTlsInput(input));
}

/** Execute the bounded file/provider suite without exposing a second product CLI.
 * @param argc Must be two: the executable and a fresh owned RUN path.
 * @param argv Explicit runner arguments; no environment or current-directory fixture discovery.
 * @return Doctest status, or 2 for runner input/ordinary-file setup/cleanup failure.
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        std::fputs("input test runner requires one fresh owned RUN directory\n", stderr);

        return 2;
    }

    bool ownsRun = false;

    try {
        std::u8string path;

        for (const unsigned char byte : std::string_view(argv[1]))
            path.push_back(static_cast<char8_t>(byte));

        runRoot = fs::absolute(fs::path(path));
        std::error_code error;

        if (!fs::create_directory(runRoot, error) || error) {
            std::fputs("input test runner cannot create its fresh owned directory\n", stderr);

            return 2;
        }

        ownsRun = true;
        doctest::Context context;
        context.setOption("no-breaks", true);
        context.setOption("no-colors", true);
        const auto result = context.run();

        if (!fs::remove(runRoot, error) || error) {
            std::fputs("input test runner could not retire its owned files\n", stderr);

            return 2;
        }

        return result;
    } catch (...) {
        std::error_code error;

        if (ownsRun)
            fs::remove(runRoot, error);

        std::fputs("input test runner setup failed\n", stderr);

        return 2;
    }
}
