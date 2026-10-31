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
