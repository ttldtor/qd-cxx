// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: bounded explicit local peers, no external endpoint or ambient input.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "connect_peer.hpp"

#include "configuration.hpp"
#include "example.hpp"
#include "output.hpp"

#include <doctest/doctest.h>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace d = root::client;
namespace e = root::events;
namespace t = root::transport;
namespace x = root::examples;
namespace a = root::connect;
namespace s = root::connect_tests;
using namespace std::chrono_literals;

/** Construct one caller-owned generic binary schema with extra nullable fields.
 * @return Validated immutable ordinary schema, independent of standard-event adapters.
 */
c::LocalSchema genericSchema() {
    const c::LocalRecord record{
        0, u"Custom", false, {{8}, {10}, {9}}, {{u"Value", 0}, {u"Text", 1}, {u"Bytes", 2}, {u"TextAlias", 1}}};
    auto schema =
        c::LocalSchema::create(std::span(&record, 1), {1, 3, 4, 128, 1024}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    return std::move(*schema);
}

/** Build full standard schema under explicit binary live profile before preparing projection.
 * @param profile Select Profile rather than Quote.
 * @return Immutable complete sixteen-field Profile or eight-field Quote schema.
 */
c::LocalSchema standardSchema(bool profile) {
    constexpr c::SchemaLimits LIMITS{16, 128, 256, 128, 16384};
    auto defaults = profile ? e::defaultProfileSchema(LIMITS) : e::defaultQuoteSchema(LIMITS);
    REQUIRE(defaults);
    auto schema = c::LocalSchema::create(defaults->records(), LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    return std::move(*schema);
}

/** Fill every real standard field with exact supported values, without reducing the schema.
 * @param schema Complete explicit Quote/Profile local declaration.
 * @return Complete typed local field order.
 */
std::vector<c::FieldValue> standardValues(const c::LocalSchema &schema) {
    std::vector<c::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        const auto type = static_cast<c::SerialFieldType>(field.serialTypeId);
        c::FieldValue value{type, 0};

        if (type == c::SerialFieldType::UTF_CHAR) {
            value.raw = u'Q';
        } else if (type == c::SerialFieldType::TIME_SECONDS) {
            value.raw = 3;
        } else if (type == c::SerialFieldType::UTF_CHAR_ARRAY) {
            value.units = u"sample";
        }

        result.push_back(std::move(value));
    }

    return result;
}

/** Simple caller-owned callback with optional explicit control access. */
class Recorder final : public d::Consumer {
    public:
    std::vector<std::u16string> symbols; ///< Independently copied application observations.
    std::function<void()> action;        ///< Optional synchronous callback action; test-owned lifetime.
    d::OwnedBatch retained;              ///< One detached immutable batch retained through native retirement.

    /** Record complete borrowed values and optionally issue a serialized command.
     * @param batch Callback-scoped immutable views.
     */
    void onBatch(const d::BorrowedBatch &batch) override {
        retained = batch.retain();

        for (const auto &record : batch.records()) {
            symbols.emplace_back(record.symbol);
        }

        if (action) {
            action();
        }
    }
};

/** Verify both completion domains without treating retained output as delivery acknowledgement.
 * @param result Completed explicit run.
 */
void retired(const x::StreamResult &result) {
    REQUIRE(result.local);
    REQUIRE(result.transport);
    CHECK(result.local->counts.failedBatches == 0);
    CHECK(result.transport->aborted);
}

/** Preserve the qualified Windows native-close ambiguity as failure rather than suppressing it.
 * @param result Actual completed native/local evidence.
 * @param target Whether the run required a record target.
 * @param cleanCode Expected status when native retirement reports no failure.
 */
void checkCutoffExit(const x::StreamResult &result, bool target, int cleanCode) {
#ifdef _WIN32

    if (result.transport && result.transport->primary.error == t::TcpError::READ_FAILURE &&
        result.transport->primary.nativeCode == 1236) {
        CHECK(result.transport->aborted);
        CHECK(result.transport->primary.stage == "socket-read");
        CHECK(result.transport->secondary.error == t::TcpError::NONE);
        CHECK(a::exitCode(result, target) == 1);

        return;
    }

#endif
    CHECK(a::exitCode(result, target) == cleanCode);
}

/** Reject publication only after setup has succeeded, testing callback-time sink failure. */
class FailingBuffer final : public std::stringbuf {
    public:
    bool fail = false; ///< Explicit application-owned injection switch.

    /** Reject writes only after the configured boundary.
     * @param text Borrowed output characters.
     * @param count Requested byte count.
     * @return Written prefix, zero after injection.
     */
    std::streamsize xsputn(const char *text, std::streamsize count) override {
        return fail ? 0 : std::stringbuf::xsputn(text, count);
    }

    /** Report final publication failure after the configured boundary.
     * @return Zero before injection, minus one after it.
     */
    int sync() override {
        return fail ? -1 : std::stringbuf::sync();
    }
};
} // namespace

TEST_CASE("native connect options are complete bounded and fail before working setup") {
    const std::string_view valid[]{"--host", "127.0.0.1", "--port", "12345", "--symbol", "ALPHA"};
    auto parsed = a::parseArguments(valid);
    REQUIRE(parsed);
    CHECK(parsed->duration == 5000ms);
    CHECK(parsed->output == a::OutputProfile::GENERIC);
    CHECK(parsed->symbols == std::vector<std::u16string>{u"ALPHA"});

    for (const auto &arguments : std::vector<std::vector<std::string_view>>{
             {},
             {"--help"},
             {"--host", "example.invalid", "--port", "1", "--symbol", "A"},
             {"--host", "127.0.0.1", "--port", "0", "--symbol", "A"},
             {"--host", "127.0.0.1", "--port", "65536", "--symbol", "A"},
             {"--host", "127.0.0.1", "--port", "1", "--symbol", "*"},
             {"--host", "127.0.0.1", "--port", "1", "--symbol", "A", "--symbol", "A"},
             {"--host", "127.0.0.1", "--port", "1", "--symbol", "A", "--duration-ms", "1", "--remove-after-ms", "1"},
             {"--host", "127.0.0.1", "--port", "1", "--symbol", "A", "--schema-profile", "profile", "--output",
              "quote"}}) {
        CHECK_FALSE(a::parseArguments(arguments));
    }

    const std::string_view ipv6[]{"--host", "::1", "--port", "1", "--symbol", "A"};
    CHECK(a::parseArguments(ipv6));
    CHECK(a::help(false).find("default 5000") != std::string_view::npos);
    CHECK(a::help(true).find("5000") != std::string_view::npos);
}

TEST_CASE("actual byte fragmented TCP selects exact UTF16 and removes the full set before later data") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = genericSchema();
    const std::u16string exact{u'A', 0, static_cast<char16_t>(0xd800)};
    const std::vector<std::u16string> selected{exact, u"BETA"};
    const std::vector<c::FieldValue> values{
        {c::SerialFieldType::COMPACT_INT, 7},
        {c::SerialFieldType::UTF_CHAR_ARRAY, 0, std::u16string{}},
        {c::SerialFieldType::BYTE_ARRAY, 0, {}, std::vector<std::byte>{std::byte{0}, std::byte{255}}}};
    const std::vector<s::Record> records{{exact, values}, {u"BETA", values}, {u"UNREQUESTED", values}, {exact, values}};
    s::Peer peer(schema, records, selected, 1);
    auto consumer = std::make_shared<Recorder>();
    std::optional<d::ClientHandle> control;
    // One-byte native sends must tolerate the host's coarse sleep cadence without a throughput claim.
    x::StreamOptions options{"127.0.0.1", peer.port(), 0, selected, 8000ms, 5000ms};
    options.ownerOperationsPerTurn = 1;
    const auto result = x::runStream(*bootstrap, schema, options, consumer, control);
    retired(result);
    CHECK(result.failure == x::StreamFailure::NONE);
    CHECK(result.addApplied);
    CHECK(result.addComposed);
    CHECK(result.removeApplied);
    CHECK(result.removeComposed);
    CHECK(result.eventWorkYields > 0);
    CHECK(peer.adds() == 1);
    CHECK(peer.removes() == 1);
    CHECK_FALSE(peer.failed());
    CHECK(consumer->symbols == std::vector<std::u16string>{exact, u"BETA", exact});
    CHECK(result.local->counts.ignoredRecords == 5);
    REQUIRE(control);
    CHECK(control->terminal().has_value());
    std::ostringstream diagnostic;
    a::writeResult(diagnostic, result);
    INFO(diagnostic.str());
    checkCutoffExit(result, false, 0);
    REQUIRE(consumer->retained.records().size() == 1);
    CHECK(consumer->retained.records()[0].typedValues.size() == 3);
}

TEST_CASE("full standard Quote and Profile reach existing prepared projection through actual TCP") {
    for (const bool profile : {false, true}) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        auto schema = standardSchema(profile);
        CHECK(schema.records()[0].fields.size() == (profile ? 16 : 8));
        const std::vector<std::u16string> selected{u"IBM"};
        const std::vector<s::Record> records{{u"IBM", standardValues(schema)}};
        s::Peer peer(schema, records, selected);
        std::ostringstream sink;
        auto consumer = std::make_shared<a::OutputConsumer>(
            sink, schema, profile ? a::OutputProfile::PROFILE : a::OutputProfile::QUOTE);
        consumer->begin();
        std::optional<d::ClientHandle> control;
        const auto result =
            x::runStream(*bootstrap, schema, {"127.0.0.1", peer.port(), 0, selected, 2000ms, {}, 1}, consumer, control);
        retired(result);
        CHECK(result.targetReached);
        CHECK(result.local->counts.completedRecords == 1);
        CHECK(a::exitCode(result, true) == 0);
        CHECK(sink.str().find(profile ? "profile\t1\t00490042004D" : "quote\t1\t00490042004D\t3000\t3000") !=
              std::string::npos);
        CHECK(peer.adds() == 1);
        CHECK_FALSE(peer.failed());
    }
}

TEST_CASE("generic rows preserve extra fields aliases nullable text opaque bytes and provenance metadata") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = genericSchema();
    const std::vector<std::u16string> selected{u"NULL", u"EMPTY"};
    const std::vector<s::Record> records{{u"NULL",
                                          {{c::SerialFieldType::COMPACT_INT, -17},
                                           {c::SerialFieldType::UTF_CHAR_ARRAY, 0},
                                           {c::SerialFieldType::BYTE_ARRAY, 0}}},
                                         {u"EMPTY",
                                          {{c::SerialFieldType::COMPACT_INT, 23},
                                           {c::SerialFieldType::UTF_CHAR_ARRAY, 0, std::u16string{}},
                                           {c::SerialFieldType::BYTE_ARRAY, 0, {}, std::vector<std::byte>{}}}}};
    s::Peer peer(schema, records, selected);
    std::ostringstream sink;
    auto consumer = std::make_shared<a::OutputConsumer>(sink, schema, a::OutputProfile::GENERIC);
    consumer->begin();
    std::optional<d::ClientHandle> control;
    const auto result =
        x::runStream(*bootstrap, schema, {"127.0.0.1", peer.port(), 0, selected, 2000ms, {}, 2}, consumer, control);
    retired(result);
    CHECK(a::exitCode(result, true) == 0);
    const auto output = sink.str();
    INFO(output);
    CHECK(output.starts_with("qdcxx-stream\t1\n"));
    CHECK(output.find("layout\t0\t1\t10\t00540065007800740041006C006900610073\n") != std::string::npos);
    CHECK(output.find("record\t1\t0\t17\t1\t2\t0\t0\t004E0055004C004C\tnull\tnull\tnull\tnull\n") != std::string::npos);
    CHECK(output.find("field\t1\t0\t8\t-17\tnull\tnull\n") != std::string::npos);
    CHECK(output.find("field\t1\t1\t10\t0\tnull\tnull\n") != std::string::npos);
    CHECK(output.find("field\t2\t1\t10\t0\t\tnull\n") != std::string::npos);
    CHECK(output.find("field\t2\t2\t9\t0\tnull\t\n") != std::string::npos);
}

TEST_CASE("callback published handle abort is applied before another runtime admission") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = standardSchema(false);
    const std::vector<std::u16string> selected{u"IBM"};
    const std::vector<s::Record> records{{u"IBM", standardValues(schema)}, {u"IBM", standardValues(schema)}};
    s::Peer peer(schema, records, selected);
    auto consumer = std::make_shared<Recorder>();
    std::optional<d::ClientHandle> control;
    consumer->action = [&] {
        REQUIRE(control);
        CHECK(control->requestStop(d::StopMode::ABORT));
    };
    x::StreamOptions options{"127.0.0.1", peer.port(), 0, selected, 2000ms};
    options.ownerOperationsPerTurn = 1;
    const auto result = x::runStream(*bootstrap, schema, options, consumer, control);
    retired(result);
    CHECK(result.local->outcome == d::DispatchOutcome::ABORTED);
    CHECK(result.local->counts.completedRecords == 1);
    CHECK(result.eventWorkYields > 0);
    CHECK(result.local->counts.discardedRecords == 1);
    CHECK(consumer->symbols.size() == 1);
    CHECK(control->terminal().has_value());
    const auto duplicate =
        x::runStream(*bootstrap, schema, {"127.0.0.1", peer.port(), 0, selected, 2000ms}, consumer, control);
    CHECK(duplicate.failure == x::StreamFailure::CONFIGURATION);
    CHECK(control->terminal()->counts.completedRecords == 1);
}

TEST_CASE("clean early peer end and truncated frame stay distinct from intentional cutoff") {
    for (const auto mode : {s::PeerMode::CLEAN_END, s::PeerMode::PARTIAL_END}) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        auto schema = standardSchema(false);
        const std::vector<std::u16string> selected{u"IBM"};
        const std::vector<s::Record> records{{u"IBM", standardValues(schema)}};
        s::Peer peer(schema, records, selected, 8192, mode);
        auto consumer = std::make_shared<Recorder>();
        std::optional<d::ClientHandle> control;
        const auto result =
            x::runStream(*bootstrap, schema, {"127.0.0.1", peer.port(), 0, selected, 2000ms}, consumer, control);
        REQUIRE(result.transport);
        REQUIRE(result.local);
        CHECK(consumer->symbols.size() == 1);
        CHECK_FALSE(result.intervalExpired);
        CHECK(a::exitCode(result, false) == 1);
        CHECK(result.transport->peerEnded == (mode == s::PeerMode::CLEAN_END));
        CHECK((result.transport->primary.error == t::TcpError::PROTOCOL_FAILURE) == (mode == s::PeerMode::PARTIAL_END));
    }
}

TEST_CASE("callback sink failure is never retried and retires both native and local owners") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = standardSchema(false);
    const std::vector<std::u16string> selected{u"IBM"};
    const std::vector<s::Record> records{{u"IBM", standardValues(schema)}, {u"IBM", standardValues(schema)}};
    s::Peer peer(schema, records, selected);
    FailingBuffer buffer;
    std::ostream sink(&buffer);
    auto consumer = std::make_shared<a::OutputConsumer>(sink, schema, a::OutputProfile::GENERIC);
    consumer->begin();
    buffer.fail = true;
    std::optional<d::ClientHandle> control;
    const auto result =
        x::runStream(*bootstrap, schema, {"127.0.0.1", peer.port(), 0, selected, 2000ms}, consumer, control);
    REQUIRE(result.local);
    REQUIRE(result.transport);
    CHECK(result.local->outcome == d::DispatchOutcome::FAILED);
    CHECK(result.local->primary == d::ClientError::CALLBACK_FAILURE);
    CHECK(result.local->counts.startedBatches == 1);
    CHECK(result.local->counts.failedBatches == 1);
    CHECK(result.local->counts.completedRecords == 0);
    CHECK(a::exitCode(result, false) == 1);
}

TEST_CASE("finite empty feed target and explicit interrupt report honest retirement") {
    for (const bool interrupt : {false, true}) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        auto schema = standardSchema(false);
        const std::vector<std::u16string> selected{u"IBM"};
        s::Peer peer(schema, {}, selected, 8192, s::PeerMode::SILENT);
        auto consumer = std::make_shared<Recorder>();
        std::optional<d::ClientHandle> control;
        volatile std::sig_atomic_t flag = interrupt ? 1 : 0;
        const auto result = x::runStream(
            *bootstrap, schema, {"127.0.0.1", peer.port(), 0, selected, 150ms, {}, 1, &flag}, consumer, control);
        retired(result);
        CHECK(consumer->symbols.empty());
        CHECK(result.interrupted == interrupt);
        std::ostringstream diagnostic;
        a::writeResult(diagnostic, result);
        INFO(diagnostic.str());
        checkCutoffExit(result, true, interrupt ? 130 : 2);
    }
}

TEST_CASE("documented programmatic generic example compiles and runs against the controlled peer") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = genericSchema();
    const std::vector<std::u16string> selected{u"ALPHA"};
    const std::vector<s::Record> records{{u"ALPHA",
                                          {{c::SerialFieldType::COMPACT_INT, 42},
                                           {c::SerialFieldType::UTF_CHAR_ARRAY, 0},
                                           {c::SerialFieldType::BYTE_ARRAY, 0}}}};
    s::Peer peer(schema, records, selected);
    std::ostringstream sink;
    const auto result = x::runSubscriptionExample(*bootstrap, schema, peer.port(), sink);
    retired(result);
    CHECK(result.targetReached);
    CHECK(result.local->counts.completedRecords == 1);
    CHECK(sink.str() == "local-id=0 sender-id=17 fields=3\n");
    CHECK(peer.adds() == 1);
    CHECK_FALSE(peer.failed());
}

TEST_CASE("record target stops further admission while the whole admitted eligible prefix finishes") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = standardSchema(false);
    const std::vector<std::u16string> selected{u"IBM"};
    const auto values = standardValues(schema);
    const std::vector<s::Record> records{{u"IBM", values}, {u"OTHER", values}, {u"IBM", values}, {u"IBM", values}};
    s::Peer peer(schema, records, selected);
    auto consumer = std::make_shared<Recorder>();
    std::optional<d::ClientHandle> control;
    const auto result =
        x::runStream(*bootstrap, schema, {"127.0.0.1", peer.port(), 0, selected, 2000ms, {}, 1}, consumer, control);
    retired(result);
    CHECK(result.targetReached);
    CHECK(result.local->counts.completedRecords == 3);
    CHECK(result.local->counts.ignoredRecords == 1);
    CHECK(consumer->symbols.size() == 3);
    CHECK(a::exitCode(result, true) == 0);
}

TEST_CASE("snapshot work limits retain credit caps through controlled retirement") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = standardSchema(false);
    const std::vector<std::u16string> selected{u"IBM"};
    const auto values = standardValues(schema);
    const std::vector<s::Record> records{{u"IBM", values}};
    s::Peer peer(schema, records, selected);
    auto settings = d::controlledStreamSettings(1000ms);
    REQUIRE(settings);
    settings->host = "127.0.0.1";
    settings->port = peer.port();
    settings->budgets.dispatchTurn.fields = 1;
    auto endpoint = d::EndpointOptions::create(schema, *settings);
    REQUIRE(endpoint);
    CHECK(endpoint->requirePlainStream());
    auto consumer = std::make_shared<Recorder>();
    std::optional<d::ClientHandle> control;
    const auto result = x::runStream(*bootstrap, *endpoint, {"", 0, 0, selected, 1000ms}, consumer, control);
    CHECK(result.failure == x::StreamFailure::CONFIGURATION);
    REQUIRE(result.diagnostic);
    CHECK(result.diagnostic->stage == "endpoint-record-work");
    CHECK_FALSE(result.local);
    CHECK_FALSE(result.transport);
    CHECK_FALSE(control);
    CHECK(peer.adds() == 0);
}

TEST_CASE("CLI overrides and native options share one immutable effective configuration") {
    const std::string_view arguments[]{"--host",
                                       "127.0.0.1",
                                       "--port",
                                       "3210",
                                       "--symbol",
                                       "IBM",
                                       "--read-timeout-ms",
                                       "77",
                                       "--duration-ms",
                                       "200",
                                       "--connect-timeout-ms",
                                       "88",
                                       "--write-timeout-ms",
                                       "99",
                                       "--poll-interval-ms",
                                       "2",
                                       "--security",
                                       "plain"};
    auto config = a::parseArguments(arguments);
    REQUIRE(config);
    auto schema = standardSchema(false);
    auto endpoint = a::effectiveEndpoint(*config, schema);
    REQUIRE(endpoint);
    CHECK(endpoint->settings().timeouts.read == 77ms);
    CHECK(endpoint->settings().timeouts.connect == 88ms);
    CHECK(endpoint->settings().timeouts.write == 99ms);
    CHECK(endpoint->settings().timeouts.poll == 2ms);
    auto settings = d::controlledStreamSettings(200ms);
    REQUIRE(settings);
    settings->host = "127.0.0.1";
    settings->port = 3210;
    settings->timeouts = {1000ms, 88ms, 77ms, 99ms, 2ms};
    auto native = d::EndpointOptions::create(schema, *settings);
    REQUIRE(native);
    CHECK(native->settings().timeouts.read == endpoint->settings().timeouts.read);
    CHECK(native->settings().budgets.dispatchTurn.operations == endpoint->settings().budgets.dispatchTurn.operations);

    for (const auto &invalid :
         std::vector<std::vector<std::string_view>>{{"--security", "tls"},
                                                    {"--security", "synthetic-secret"},
                                                    {"--connect-timeout-ms", "2147483648"},
                                                    {"--read-timeout-ms", "0"},
                                                    {"--poll-interval-ms", "-1"},
                                                    {"--write-timeout-ms", "99999999999999999999"}}) {
        auto rejected = a::parseArguments(invalid);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().find("synthetic-secret") == std::string_view::npos);
    }
}

TEST_CASE("snapshot operation caps allow bounded progress and independently omitted matching selectors") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = standardSchema(false);
    const std::vector<std::u16string> selected{u"IBM"};
    const auto values = standardValues(schema);
    const std::vector<s::Record> records{{u"IBM", values}};

    const std::pair<std::size_t, std::size_t> caps[]{{1, 10000}, {2, 1}, {3, 1}, {2, 2}, {3, 3}};

    for (const auto [cap, selectionCap] : caps) {
        s::Peer peer(schema, records, selected);
        auto settings = d::controlledStreamSettings(2000ms);
        REQUIRE(settings);
        settings->host = "127.0.0.1";
        settings->port = peer.port();
        settings->budgets.dispatchTurn.operations = cap;
        auto endpoint = d::EndpointOptions::create(schema, *settings);
        REQUIRE(endpoint);
        auto consumer = std::make_shared<Recorder>();
        std::optional<d::ClientHandle> control;
        x::StreamOptions options{"", 0, 0, selected, 2000ms, {}, 1};
        options.ownerOperationsPerTurn = selectionCap;

        if (cap == 2) {
            options.host = settings->host;
        }

        if (cap == 3) {
            options.port = settings->port;
        }

        auto result = x::runStream(*bootstrap, *endpoint, options, consumer, control);
        retired(result);
        CHECK(result.targetReached);
        CHECK(result.local->counts.completedRecords == 1);
        CHECK(result.eventWorkYields > 0);
        CHECK(result.failure == x::StreamFailure::NONE);
        CHECK_FALSE(peer.failed());
    }

    auto settings = d::controlledStreamSettings();
    REQUIRE(settings);
    settings->host = "127.0.0.1";
    settings->port = 3210;
    auto endpoint = d::EndpointOptions::create(schema, *settings);
    REQUIRE(endpoint);
    auto consumer = std::make_shared<Recorder>();
    std::optional<d::ClientHandle> control;
    auto wrongHost = x::runStream(*bootstrap, *endpoint, {"::1", 0, 0, selected}, consumer, control);
    auto wrongPort = x::runStream(*bootstrap, *endpoint, {"", 3211, 0, selected}, consumer, control);
    CHECK(wrongHost.failure == x::StreamFailure::CONFIGURATION);
    CHECK(wrongPort.failure == x::StreamFailure::CONFIGURATION);
    CHECK_FALSE(control);
    CHECK(consumer->symbols.empty());
}

TEST_CASE("snapshot runtime inactivity timeout reaches the existing native owner") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = standardSchema(false);
    const std::vector<std::u16string> selected{u"IBM"};
    s::Peer peer(schema, {}, selected, 8192, s::PeerMode::SILENT);
    auto settings = d::controlledStreamSettings(2000ms);
    REQUIRE(settings);
    settings->host = "127.0.0.1";
    settings->port = peer.port();
    settings->timeouts.read = 20ms;
    auto endpoint = d::EndpointOptions::create(schema, *settings);
    REQUIRE(endpoint);
    auto consumer = std::make_shared<Recorder>();
    std::optional<d::ClientHandle> control;
    auto result = x::runStream(*bootstrap, *endpoint, {"", 0, 0, selected, 2000ms}, consumer, control);
    REQUIRE(result.transport);
    CHECK(result.transport->primary.error == t::TcpError::DEADLINE);
    CHECK_FALSE(result.intervalExpired);
}

TEST_CASE("snapshot pristine dispatcher work refusal fails and aborts with unchanged credits") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = standardSchema(false);
    const std::vector<std::u16string> selected{u"IBM"};
    const auto values = standardValues(schema);
    const std::vector<s::Record> records{{u"IBM", values}, {u"IBM", values}};
    s::Peer peer(schema, records, selected);
    auto settings = d::controlledStreamSettings(2000ms);
    REQUIRE(settings);
    settings->host = "127.0.0.1";
    settings->port = peer.port();
    settings->budgets.client.batchRecords = 2;
    settings->budgets.dispatchTurn.fields = schema.records()[0].fields.size() * 2;
    auto endpoint = d::EndpointOptions::create(schema, *settings);
    REQUIRE(endpoint);
    REQUIRE(endpoint->requirePlainStream());
    auto consumer = std::make_shared<Recorder>();
    std::optional<d::ClientHandle> control;
    auto result = x::runStream(*bootstrap, *endpoint, {"", 0, 0, selected, 2000ms}, consumer, control);
    REQUIRE(result.local);
    REQUIRE(result.transport);
    CHECK(result.failure == x::StreamFailure::DISPATCH);
    REQUIRE(result.diagnostic);
    CHECK(result.diagnostic->error == t::TcpError::WORK_LIMIT);
    CHECK(result.diagnostic->stage == "endpoint-dispatch-work");
    CHECK(result.local->outcome == d::DispatchOutcome::ABORTED);
    CHECK(result.local->counts.completedRecords == 0);
    CHECK(result.local->counts.discardedRecords == 2);
    CHECK(result.eventWorkYields > 0);
    CHECK(consumer->symbols.empty());
    CHECK_FALSE(peer.failed());
    CHECK(endpoint->settings().budgets.dispatchTurn.fields == schema.records()[0].fields.size() * 2);
}

TEST_CASE("snapshot sufficient work profile restrictions do not invalidate generic handoff") {
    auto schema = standardSchema(false);
    auto settings = d::controlledStreamSettings();
    REQUIRE(settings);
    settings->host = "127.0.0.1";
    settings->port = 3210;
    settings->budgets.runtimeTurn.socketBytes = 1;
    auto endpoint = d::EndpointOptions::create(schema, *settings);
    REQUIRE(endpoint);
    auto driver = endpoint->requirePlainStream();
    REQUIRE_FALSE(driver);
    CHECK(driver.error().error == d::EndpointError::UNSUPPORTED_PROFILE);
    CHECK(driver.error().stage == "endpoint-driver-work");
}

TEST_CASE("snapshot callback stop throw and queued command abort retire with configured work") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto schema = standardSchema(false);
    const std::vector<std::u16string> selected{u"IBM"};
    const auto values = standardValues(schema);
    const std::vector<s::Record> records{{u"IBM", values}, {u"IBM", values}};

    for (unsigned mode = 0; mode < 3; ++mode) {
        s::Peer peer(schema, records, selected);
        auto settings = d::controlledStreamSettings(2000ms);
        REQUIRE(settings);
        settings->host = "127.0.0.1";
        settings->port = peer.port();
        settings->budgets.dispatchTurn.operations = 2;
        auto endpoint = d::EndpointOptions::create(schema, *settings);
        REQUIRE(endpoint);
        auto consumer = std::make_shared<Recorder>();
        std::weak_ptr<Recorder> weakConsumer = consumer;
        std::optional<d::ClientHandle> control;
        std::optional<d::CommandTicket> queued;
        const c::SubscriptionRequest request{{schema.recordContext(0), u"QUEUED"}, {}};
        const std::vector<c::SubscriptionRequest> requests(16, request);
        unsigned calls = 0;
        consumer->action = [&] {
            ++calls;
            REQUIRE(control);

            if (mode == 1) {
                throw std::runtime_error("synthetic callback failure");
            }

            if (mode == 2) {
                auto work = settings->budgets.submitTurn;
                auto accepted = control->add(requests, work);
                REQUIRE(accepted);
                queued.emplace(std::move(*accepted));
                CHECK(control->requestStop(d::StopMode::ABORT));
            } else {
                CHECK(control->requestStop(d::StopMode::GRACEFUL));
            }
        };
        x::StreamOptions options{"", 0, 0, selected, 2000ms};
        options.ownerOperationsPerTurn = 3;
        const auto result = x::runStream(*bootstrap, *endpoint, options, consumer, control);
        REQUIRE(result.local);
        REQUIRE(result.transport);
        REQUIRE(control);
        REQUIRE(control->terminal());
        CHECK(result.transport->aborted);
        CHECK(endpoint->settings().budgets.dispatchTurn.operations == 2);
        CHECK(endpoint->settings().budgets.dispatchTurn.commands == settings->budgets.dispatchTurn.commands);
        CHECK(endpoint->settings().budgets.dispatchTurn.bytes == settings->budgets.dispatchTurn.bytes);

        if (mode == 0) {
            CHECK(result.local->outcome == d::DispatchOutcome::DRAINED);
            CHECK(result.local->counts.completedRecords == 2);
            CHECK(calls == 2);
        } else if (mode == 1) {
            CHECK(result.local->outcome == d::DispatchOutcome::FAILED);
            CHECK(result.local->primary == d::ClientError::CALLBACK_FAILURE);
            CHECK(result.local->counts.failedBatches == 1);
            CHECK(result.local->counts.completedRecords == 0);
            CHECK(calls == 1);
        } else {
            CHECK(result.local->outcome == d::DispatchOutcome::ABORTED);
            CHECK(result.local->counts.completedRecords == 1);
            CHECK(result.local->counts.discardedRecords == 1);
            REQUIRE(queued);
            REQUIRE(queued->result());
            CHECK(queued->result()->status == d::CommandStatus::CANCELLED_CLOSING);
            CHECK(calls == 1);
            queued.reset();
        }

        CHECK_FALSE(peer.failed());
        consumer.reset();
        CHECK(weakConsumer.expired());
        CHECK(control->terminal()->counts.startedBatches == calls);
        control.reset();
    }
}
