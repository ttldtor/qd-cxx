// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: public native events, whole receiver callbacks and exact frontend output.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_output.hpp"

#include "qd/encoding/char_array.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/protocol/live_session.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <vector>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace e = root::encoding;
namespace p = root::protocol;
namespace m = root::mux;
namespace a = root::client;
namespace q = root::events;
namespace f = root::connect;
namespace t = root::transport;
using Type = c::SerialFieldType;
constexpr c::SchemaLimits SCHEMA_LIMITS{8, 64, 128, 128, 16384}; ///< Small complete local declarations.
constexpr q::MappingLimits MAPPING_LIMITS{64,  128, 32,   64,
                                          128, 128, 8192, 100000}; ///< Explicit prepared mapper bounds.
constexpr std::int32_t SENDER = 41; ///< Sparse incoming ID distinct from selected local position zero.
constexpr std::size_t EPOCH_BYTES = 16 * sizeof(m::QueueIdentity); ///< Independent application label allowance.
constexpr std::size_t PROGRESS_LIMIT = 20000; ///< Finite deterministic public-owner polling ceiling.

/** Supply one finite independent queue turn; no credit is replenished inside an owner call. */
m::QueueWork turn() {
    return {100000,
            1000000000,
            1000000000,
            1000000000,
            1000000000,
            10000,
            10000,
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000}};
}

/** Supply one finite caller-owned native decoding turn. */
p::LiveWork liveTurn() {
    return {10000, 10000000, 10000000, 10000000};
}

/** Supply one finite command-copy turn. */
m::QueueSubmitWork submitTurn() {
    return {10000, 1000000, 1000000, 1000000};
}

/** Supply one whole callback's explicit owning-projection allowance. */
a::ReceiverProjectionWork projectionTurn() {
    return {100000, 100000, 1000000};
}

/** Keep complete callback/resource reservations explicit and small. */
m::ClientLimits clientLimits() {
    return {32, 512 * 1024, 8, 128 * 1024, 4, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096};
}

/** Encode one real compact scalar with the public production codec. */
void number(std::vector<std::byte> &out, std::int64_t value) {
    std::array<std::byte, 9> bytes{};
    const auto encoded = e::writeCompactLong(value, bytes);
    REQUIRE(encoded.status == e::Status::OK);
    out.insert(out.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(encoded.written));
}

/** Encode exact native descriptor text with the public production codec. */
void text(std::vector<std::byte> &out, std::u16string_view value) {
    std::array<std::byte, 8192> bytes{};
    const auto encoded = e::writeUtfString(value, bytes, {8192, 1024});
    REQUIRE(encoded.status == e::Status::OK);
    out.insert(out.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(encoded.written));
}

/** Wrap the actual QTP message type and payload, without manufacturing parsed record views. */
std::vector<std::byte> frame(std::int32_t type, std::span<const std::byte> payload) {
    std::vector<std::byte> body;
    number(body, type);
    body.insert(body.end(), payload.begin(), payload.end());
    std::vector<std::byte> result;
    number(result, static_cast<std::int64_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());

    return result;
}

/** Describe an independently validated custom record, including every nullable field position. */
c::LocalSchema customSchema(bool history = false, bool compact = false) {
    c::LocalRecord record{0, u"Custom", history, {}, {}};
    const std::vector<std::int32_t> types = compact   ? std::vector<std::int32_t>{8, 8}
                                            : history ? std::vector<std::int32_t>{104, 8}
                                                      : std::vector<std::int32_t>{104, 104, 8, 10, 10, 10, 9, 9, 233};

    for (std::size_t index = 0; index < types.size(); ++index) {
        record.fields.push_back({types[index]});
        record.lookups.push_back({u"Value" + std::u16string(1, static_cast<char16_t>(u'A' + index)), index});
    }

    if (history)
        record.indexedTimeLayout = c::IndexedTimeSingleInt64{0};

    auto result = c::LocalSchema::create(std::span(&record, 1), SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Preserve the complete public Quote/Profile schema and append one unprojected generic extra. */
c::LocalSchema eventSchema(bool profile) {
    auto builtin = profile ? q::defaultProfileSchema(SCHEMA_LIMITS) : q::defaultQuoteSchema(SCHEMA_LIMITS);
    REQUIRE(builtin);
    std::vector<c::LocalRecord> records(builtin->records().begin(), builtin->records().end());
    const auto extra = records[0].fields.size();
    records[0].fields.push_back({233});
    records[0].lookups.push_back({u"Extra", extra});
    auto result = c::LocalSchema::create(records, SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Explicit small parser profile; metadata prefix count remains a separate fixture input. */
p::LiveLimits liveLimits() {
    p::LiveLimits limits{};
    limits.frame = {8192, 8201};
    limits.protocol = {128, 64, {8192, 1024}, 16384};
    limits.descriptions = {8192, 32, 32, 256, {8192, 1024}, 8192};
    limits.context = {{16, 128, 256, 128, 16384}, 32, {512, 1024, 16384}};
    limits.data = {8192, 64,   512,   64,    512,   8192, {8192, 128, 128}, {8192, 1024}, 8192, 8192, 8192,
                   8192, 8192, 16384, 16384, 16384, 64};
    limits.data.processedRecords = 256;
    limits.data.emittedRecords = 256;
    limits.data.emittedValues = 2048;
    limits.data.ownedFieldTimeRecords = 64;
    limits.residentBytes = 4 * 1024 * 1024;
    limits.owners = 128;
    limits.snapshotBytes = 65536;
    limits.scratchBytes = 65536;
    limits.eventBytes = 262144;
    limits.outputBytes = 8192;
    limits.credentials = p::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    limits.fieldTime = p::FieldTimePolicy::READ;
    limits.events = p::DataEventPolicy::BINARY_CATALOG_V1;
    limits.historySnapshot = true;

    return limits;
}

/** Obtain an actual immutable public parser event. */
p::LiveEvent admit(p::LiveSession &session, std::span<const std::byte> bytes) {
    auto work = liveTurn();
    auto result = session.feed(bytes, work);
    INFO("native error=", static_cast<int>(session.diagnostic().error), " stage=", session.diagnostic().stage);
    REQUIRE(result.status == p::LiveStatus::EVENT);
    REQUIRE(result.accepted == bytes.size());
    REQUIRE(result.event);

    return std::move(*result.event);
}

/** Compose one public native source, using zero/one/two physical FIELD prefix fields. */
p::LiveSession session(const c::LocalSchema &schema, unsigned metadataFields) {
    p::OutgoingRecord declaration{SENDER, 0, schema.records()[0].name, {}};
    std::vector<std::byte> description;
    number(description, SENDER);
    text(description, schema.records()[0].name);
    number(description, static_cast<std::int64_t>(schema.records()[0].fields.size() + metadataFields));

    if (metadataFields > 0) {
        text(description, u"EventTime");
        number(description, 56);
    }

    if (metadataFields > 1) {
        text(description, u"EventSequence");
        number(description, 72);
    }

    for (std::size_t index = 0; index < schema.records()[0].fields.size(); ++index) {
        const auto &record = schema.records()[0];
        const auto lookup = std::ranges::find_if(record.lookups, [index](const auto &entry) {
            return entry.fieldId == index;
        });
        REQUIRE(lookup != record.lookups.end());
        const auto type = record.fields[index].serialTypeId;
        declaration.fields.push_back({lookup->name, type, static_cast<std::int32_t>(index)});
        text(description, lookup->name);
        number(description, type);
    }

    p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(schema, std::span(&declaration, 1),
                                                {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, writerWork);
    REQUIRE(writer);
    auto copy = schema;
    auto work = liveTurn();
    auto result = p::LiveSession::create(std::move(copy), std::move(*writer), liveLimits(), work);
    REQUIRE(result);
    std::vector<std::byte> ready{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    number(ready, 0);
    number(ready, 3);

    for (const auto &[id, name] :
         std::array{std::pair{10, u"TICKER_DATA"}, std::pair{15, u"STREAM_DATA"}, std::pair{20, u"HISTORY_DATA"}}) {
        number(ready, id);
        text(ready, name);
        number(ready, 0);
    }

    number(ready, 0);
    (void)admit(*result, frame(1, ready));
    (void)admit(*result, frame(2, description));

    return std::move(*result);
}

/** Keep live owners in explicit declaration/retirement order. */
struct Fixture {
    c::LocalSchema schema;        ///< Exact origin shared by receiver and native source copies.
    p::LiveSession live;          ///< Native public parser owner.
    m::MuxQueue queue;            ///< Composed public family collector and callback owner.
    c::SubscriptionFamily family; ///< Immutable selected contract.
    unsigned metadataFields;      ///< Actual incoming FIELD descriptor prefix length.
};

/** Construct one public family graph; HISTORY capacities cover these few keys/points exactly. */
Fixture fixture(c::LocalSchema schema, c::SubscriptionFamily family = c::SubscriptionFamily::STREAM,
                unsigned metadataFields = 0) {
    auto live = session(schema, metadataFields);
    const c::SubscriptionLimits desired{16, 32, 128, 4096, 64, 8192, 64};
    const m::MuxLimits interests{16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024};
    const m::QueueLimits delivery{
        4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024};
    auto work = turn();
    std::expected<m::MuxQueue, m::QueueFailure> queue =
        std::unexpected(m::QueueFailure{m::QueueError::INVALID_STATE, {}});

    if (family == c::SubscriptionFamily::STREAM)
        queue = m::MuxQueue::createStream(schema, interests, desired, {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
                                          delivery, work);
    else if (family == c::SubscriptionFamily::TICKER)
        queue = m::MuxQueue::createTicker(schema, interests, {4, 32, 64, 4 * 1024 * 1024, 16 * 1024 * 1024, desired},
                                          {false, 100}, delivery, work);
    else
        queue = m::MuxQueue::createHistory(
            schema, interests, {4, 16, 128, 8, 4 * 1024 * 1024, 4, 16, 8, 4 * 1024 * 1024, 32 * 1024 * 1024, desired},
            {false, 100, 10000}, delivery, work);

    REQUIRE(queue);

    return {std::move(schema), std::move(live), std::move(*queue), family, metadataFields};
}

/** Observe only real callbacks, retaining generic leases rather than borrowing callback services. */
struct Observer final : a::SubscriptionConsumer {
    std::shared_ptr<f::RecoveryOutputConsumer> writer; ///< Stable application sink called synchronously.
    std::vector<m::OwnedMuxBatch> batches; ///< Detached immutable native results for independent assertions.
    std::vector<std::size_t> sizes;        ///< Actual callback batch cardinalities.
    m::BatchAction lastAction = m::BatchAction::PAUSE; ///< Actual successful sink return.

    m::BatchAction onBatch(const a::SubscriptionBatch &batch) override {
        sizes.push_back(batch.records().size());
        batches.push_back(batch.retain());
        lastAction = writer->onBatch(batch);

        return lastAction;
    }
};

/** Progress the assigned owner only, with explicit fresh finite turns. */
void until(Fixture &graph, const std::function<bool()> &ready, bool callbacks = true) {
    for (std::size_t attempt = 0; attempt < PROGRESS_LIMIT; ++attempt) {
        if (ready())
            return;

        auto work = turn();

        if (!callbacks)
            work.callbacks = 0;

        auto result = graph.queue.progress(work);
        INFO("queue error=", result ? -1 : static_cast<int>(result.error().error));
        REQUIRE(result);
    }

    FAIL("bounded queue progress did not reach the actual observation");
}

/** Open exactly the selected public generic/prepared route. */
m::MuxHandle open(Fixture &graph, const std::shared_ptr<Observer> &observer,
                  f::OutputProfile profile = f::OutputProfile::GENERIC) {
    std::vector<a::PreparedSubscriptionBinding> bindings;

    if (profile != f::OutputProfile::GENERIC)
        bindings.push_back(
            {0,
             profile == f::OutputProfile::QUOTE ? a::PreparedEventKind::QUOTE : a::PreparedEventKind::PROFILE,
             MAPPING_LIMITS,
             {32, 128, 256},
             a::ReceiverEventTimePolicy::FIELD_METADATA});

    a::ReceiverWork receiverWork{100000, 1000000};
    auto work = turn();
    auto handle =
        a::openSubscriptionReceiver(graph.queue, graph.schema, graph.family, c::SubscriptionSelectorPolicy::EXACT_ONLY,
                                    bindings, observer, {1, 64, 32}, clientLimits(), receiverWork, work);
    REQUIRE(handle);

    return std::move(*handle);
}

/** Submit one exact request; retain its actual completion for the control writer oracle. */
m::MuxCommandTicket activate(Fixture &graph, m::MuxHandle &handle) {
    const c::SubscriptionRequest request{{graph.schema.recordContext(0), u"SYM"},
                                         graph.family == c::SubscriptionFamily::HISTORY ? std::optional<std::int64_t>{0}
                                                                                        : std::nullopt};
    auto work = submitTurn();
    auto ticket = handle.replace(std::span(&request, 1), work);
    REQUIRE(ticket);
    CHECK_FALSE(ticket->result());
    until(graph, [&] {
        return ticket->result().has_value() && handle.state() == m::QueueClientState::ACTIVE;
    });
    REQUIRE(ticket->result());
    CHECK(ticket->result()->status == m::CommandStatus::APPLIED);

    return std::move(*ticket);
}

/** Source values retain null, empty and exact raw payloads in separate positions. */
std::vector<c::FieldValue> customValues() {
    return {{Type::LONG, std::numeric_limits<std::int64_t>::min()},
            {Type::LONG, std::numeric_limits<std::int64_t>::max()},
            {Type::COMPACT_INT, 0},
            {Type::UTF_CHAR_ARRAY, 0},
            {Type::UTF_CHAR_ARRAY, 0, u""},
            {Type::UTF_CHAR_ARRAY, 0, std::u16string{u'A', u'\0', char16_t{0x03a9}, char16_t{0xd800}}},
            {Type::BYTE_ARRAY, 0},
            {Type::BYTE_ARRAY, 0, {}, std::vector<std::byte>{}},
            {Type::CUSTOM_OBJECT, 0, {}, std::vector{std::byte{0}, std::byte{9}, std::byte{10}, std::byte{255}}}};
}

/** Explicit whole native record, not a fabricated SubscriptionBatch or MuxRecordView. */
struct Input {
    std::vector<c::FieldValue> fields;                       ///< Complete local-order owned values.
    std::int32_t flags = 0;                                  ///< Actual original source flags.
    p::FieldEventTime metadata{7, std::int64_t{5} << 22, 0}; ///< Independent real FIELD metadata.
};

/** Encode the finite fixture's actual serial representations through public codecs. */
void field(std::vector<std::byte> &out, const c::FieldValue &value) {
    if (value.type == Type::UTF_CHAR_ARRAY) {
        std::array<std::byte, 8192> bytes{};
        const auto encoded = e::writeCharArray(
            value.units ? std::optional<std::u16string_view>(*value.units) : std::nullopt, bytes, {8192, 1024});
        REQUIRE(encoded.status == e::Status::OK);
        out.insert(out.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(encoded.written));
    } else if (value.type == Type::BYTE_ARRAY || value.type == Type::CUSTOM_OBJECT) {
        number(out, value.bytes ? static_cast<std::int64_t>(value.bytes->size()) : -1);

        if (value.bytes)
            out.insert(out.end(), value.bytes->begin(), value.bytes->end());
    } else if (value.type == Type::UTF_CHAR) {
        std::array<std::byte, 4> bytes{};
        const auto encoded = e::writeUtfChar(static_cast<char32_t>(value.raw), bytes);
        REQUIRE(encoded.status == e::Status::OK);
        out.insert(out.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(encoded.written));
    } else {
        REQUIRE(c::serialTypeBits(std::to_underlying(value.type)) == 8);
        number(out, value.raw);
    }
}

/** Admit an entire native multi-record frame into the public queue. */
m::QueueIdentity send(Fixture &graph, std::initializer_list<Input> inputs) {
    std::vector<std::byte> payload;

    for (const auto &input : inputs) {
        payload.push_back(std::byte{0xf9});
        number(payload, input.flags);
        payload.push_back(std::byte{0xfd});
        number(payload, 3);

        for (const char symbol : std::string_view("SYM"))
            payload.push_back(static_cast<std::byte>(symbol));

        number(payload, SENDER);

        if (graph.metadataFields > 0)
            number(payload, input.metadata.rawSeconds);

        if (graph.metadataFields > 1)
            number(payload, input.metadata.rawSequence.value_or(0));

        if ((input.flags & 2) != 0) {
            // Native REMOVE carries the raw index only; ordinary values are materialized defaults.
            REQUIRE(graph.family == c::SubscriptionFamily::HISTORY);
            number(payload, input.fields[0].raw);
            continue;
        }

        for (const auto &value : input.fields)
            field(payload, value);
    }

    const auto message = graph.family == c::SubscriptionFamily::STREAM   ? 15
                         : graph.family == c::SubscriptionFamily::TICKER ? 10
                                                                         : 20;
    auto event = admit(graph.live, frame(message, payload));

    for (std::size_t attempt = 0; attempt < PROGRESS_LIMIT; ++attempt) {
        auto work = turn();
        auto accepted = graph.queue.admit(std::move(event), work);

        if (accepted)
            return accepted->epoch;

        REQUIRE(accepted.error().error == m::QueueError::BUSY);
        REQUIRE(graph.queue.progress(work));
    }

    FAIL("bounded source admission did not progress");

    return {};
}

/** Split independent output captures preserving empty trailing cells. */
std::vector<std::vector<std::string>> rows(const std::string &capture, std::string_view name = {}) {
    std::vector<std::vector<std::string>> result;
    std::istringstream stream(capture);

    for (std::string line; std::getline(stream, line);) {
        std::vector<std::string> cells;
        std::size_t begin = 0;

        for (;;) {
            const auto end = line.find('\t', begin);
            cells.push_back(line.substr(begin, end - begin));

            if (end == std::string::npos)
                break;

            begin = end + 1;
        }

        if (name.empty() || cells[0] == name)
            result.push_back(std::move(cells));
    }

    return result;
}

/** Sink failures can carry secrets; the frontend must replace arbitrary exception text. */
class BrokenBuffer final : public std::streambuf {
    bool throws_;           ///< Throw arbitrary foreign text instead of returning a short write.
    bool syncOnly_;         ///< Accept row writes and fail only at the publication boundary.
    std::size_t remaining_; ///< Successful prefix before failure.
    public:
    BrokenBuffer(bool throws, bool syncOnly = false, std::size_t remaining = 0)
        : throws_(throws), syncOnly_(syncOnly), remaining_(remaining) {
    }

    std::streamsize xsputn(const char *, std::streamsize count) override {
        if (syncOnly_)
            return count;

        if (throws_)
            throw std::runtime_error("SECRET-token/path/challenge");

        const auto accepted = std::min<std::size_t>(remaining_, static_cast<std::size_t>(count));
        remaining_ -= accepted;

        return static_cast<std::streamsize>(accepted);
    }

    int_type overflow(int_type character) override {
        if (syncOnly_)
            return character;

        if (throws_)
            throw std::runtime_error("SECRET-token/path/challenge");

        if (remaining_ == 0)
            return traits_type::eof();

        --remaining_;

        return character;
    }

    int sync() override {
        if (throws_)
            throw std::runtime_error("SECRET-token/path/challenge");

        return -1;
    }
};

/** Deliberately hostile formatting remains local to this stream. */
struct Grouping final : std::numpunct<char> {
    char do_thousands_sep() const override {
        return '_';
    }

    std::string do_grouping() const override {
        return "\3";
    }
};
} // namespace

TEST_CASE("recovery output selection and bounded labels refuse before publication") {
    auto schema = customSchema();
    std::ostringstream output;
    f::RecoveryEpochLabels labels(1, EPOCH_BYTES);
    f::RecoveryOutputConsumer writer(output, labels, schema, 0, c::SubscriptionFamily::STREAM,
                                     f::OutputProfile::GENERIC, projectionTurn());
    CHECK(output.str().empty());
    CHECK_FALSE(writer.failed());
    CHECK_FALSE(labels.label({}));
    CHECK_THROWS_WITH_AS(f::RecoveryEpochLabels(0, EPOCH_BYTES), "recovery output epoch capacity failure",
                         std::runtime_error);
    CHECK_THROWS_WITH_AS(f::RecoveryEpochLabels(1, sizeof(m::QueueIdentity)), "recovery output epoch capacity failure",
                         std::runtime_error);
    CHECK_THROWS_WITH_AS(f::RecoveryEpochLabels(std::numeric_limits<std::uint32_t>::max(), 0),
                         "recovery output epoch capacity failure", std::runtime_error);
    CHECK_THROWS_AS(f::RecoveryOutputConsumer(output, labels, schema, -1, c::SubscriptionFamily::STREAM,
                                              f::OutputProfile::GENERIC, projectionTurn()),
                    std::runtime_error);
    CHECK_THROWS_AS(f::RecoveryOutputConsumer(output, labels, schema, 0, static_cast<c::SubscriptionFamily>(99),
                                              f::OutputProfile::GENERIC, projectionTurn()),
                    std::runtime_error);
    writer.begin();
    CHECK(output.str() == "qdcxx-subscriptions\t1\nselection\tstream\t0\tgeneric\n");
    CHECK_THROWS_WITH_AS(writer.begin(), "recovery output lifecycle failure", std::runtime_error);
    writer.flush();
}

TEST_CASE("recovery generic output preserves exact null empty raw UTF16 and a whole three-record callback") {
    auto graph = fixture(customSchema());
    std::ostringstream output;
    output.imbue(std::locale(std::locale::classic(), new Grouping));
    output << std::hex << std::showbase << std::showpos << std::boolalpha << std::setw(25);
    f::RecoveryEpochLabels labels(1, EPOCH_BYTES);
    auto observer = std::make_shared<Observer>();
    observer->writer = std::make_shared<f::RecoveryOutputConsumer>(output, labels, graph.schema, 0, graph.family,
                                                                   f::OutputProfile::GENERIC, projectionTurn());
    auto handle = open(graph, observer);
    (void)activate(graph, handle);
    observer->writer->begin();
    send(graph, {{customValues()}, {customValues()}, {customValues()}});
    until(
        graph,
        [&] {
            return graph.queue.resources(handle)->queuedRecords == 3;
        },
        false);
    CHECK(observer->sizes.empty());
    until(graph, [&] {
        return observer->writer->completedRecords() == 3;
    });
    REQUIRE(observer->sizes == std::vector<std::size_t>{3});
    CHECK(observer->lastAction == m::BatchAction::CONTINUE);
    REQUIRE(observer->batches.size() == 1);
    CHECK(observer->batches[0].records().size() == 3);
    const std::string fields = "field\t1\t0\t104\t-9223372036854775808\tnull\tnull\n"
                               "field\t1\t1\t104\t9223372036854775807\tnull\tnull\n"
                               "field\t1\t2\t8\t0\tnull\tnull\n"
                               "field\t1\t3\t10\t0\tnull\tnull\n"
                               "field\t1\t4\t10\t0\t\tnull\n"
                               "field\t1\t5\t10\t0\t0041000003A9D800\tnull\n"
                               "field\t1\t6\t9\t0\tnull\tnull\n"
                               "field\t1\t7\t9\t0\tnull\t\n"
                               "field\t1\t8\t233\t0\tnull\t00090AFF\n";
    CHECK(output.str().find(fields) != std::string::npos);
    CHECK(rows(output.str(), "record").size() == 3);
    CHECK(rows(output.str(), "field").size() == 27);
    CHECK(rows(output.str(), "quote").empty());
    const auto source = rows(output.str(), "source");
    REQUIRE(source.size() == 3);
    const auto &sourceRecord = observer->batches[0].records()[0];
    REQUIRE(sourceRecord.source);
    CHECK(sourceRecord.source->senderId == SENDER);
    CHECK(sourceRecord.source->flags == 0);
    CHECK(sourceRecord.source->bindingGeneration > 0);
    CHECK(source[0] == std::vector<std::string>{"source", "1", "1", "41", std::to_string(sourceRecord.source->cipher),
                                                "0", std::to_string(sourceRecord.source->bindingGeneration), "null",
                                                "null", "null", "null"});
    CHECK(rows(output.str(), "trigger")[0] == std::vector<std::string>{"trigger", "1", "0", "null", "null", "null",
                                                                       "null", "null", "null", "null", "null"});
    CHECK(graph.queue.terminal() == std::nullopt);
    observer->writer->flush();
}

TEST_CASE("recovery prepared Quote adds its row and preserves actual optional FIELD metadata and extra fields") {
    for (const unsigned metadataFields : {0u, 1u, 2u}) {
        auto graph = fixture(eventSchema(false), c::SubscriptionFamily::STREAM, metadataFields);
        std::ostringstream output;
        f::RecoveryEpochLabels labels(1, EPOCH_BYTES);
        auto observer = std::make_shared<Observer>();
        observer->writer = std::make_shared<f::RecoveryOutputConsumer>(output, labels, graph.schema, 0, graph.family,
                                                                       f::OutputProfile::QUOTE, projectionTurn());
        auto handle = open(graph, observer, f::OutputProfile::QUOTE);
        (void)activate(graph, handle);
        observer->writer->begin();
        const std::vector<c::FieldValue> values{{Type::TIME_SECONDS, 2},
                                                {Type::UTF_CHAR, 0x1f600},
                                                {Type::WIDE_DECIMAL, 321},
                                                {Type::WIDE_DECIMAL, 123},
                                                {Type::TIME_SECONDS, 3},
                                                {Type::UTF_CHAR, 90},
                                                {Type::WIDE_DECIMAL, 654},
                                                {Type::WIDE_DECIMAL, 456},
                                                {Type::CUSTOM_OBJECT, 0, {}, std::vector{std::byte{0xff}}}};
        send(graph, {{values}});
        until(graph, [&] {
            return observer->writer->completedRecords() == 1;
        });
        REQUIRE(rows(output.str(), "field").size() == 9);
        CHECK(rows(output.str(), "field")[1] ==
              std::vector<std::string>{"field", "1", "1", "2", "128512", "null", "null"});
        CHECK(rows(output.str(), "field")[8] == std::vector<std::string>{"field", "1", "8", "233", "0", "null", "FF"});
        const auto projection = rows(output.str(), "quote");
        REQUIRE(projection.size() == 1);
        const auto origin = metadataFields == 0 ? "unspecified" : "field";
        const auto eventTime = metadataFields == 0 ? "null" : metadataFields == 1 ? "7000" : "7005";
        CHECK(projection[0] == std::vector<std::string>{"quote", "1", "00530059004D", "2000", "62976", "321", "123",
                                                        "3000", "90", "654", "456", "3000", "0", "0", origin,
                                                        eventTime});
        REQUIRE(observer->batches.size() == 1);
        const auto &record = observer->batches[0].records()[0];
        REQUIRE(record.source);
        CHECK(record.source->fieldTime.has_value() == (metadataFields > 0));

        if (metadataFields > 0) {
            CHECK(record.source->fieldTime->rawSequence.has_value() == (metadataFields > 1));
            CHECK(rows(output.str(), "source")[0][9] == (metadataFields > 1 ? "20971520" : "null"));
        }
    }
}

TEST_CASE("recovery prepared Profile preserves complete generic records and nullable owned projection text") {
    auto graph = fixture(eventSchema(true));
    std::ostringstream output;
    f::RecoveryEpochLabels labels(1, EPOCH_BYTES);
    auto observer = std::make_shared<Observer>();
    observer->writer = std::make_shared<f::RecoveryOutputConsumer>(output, labels, graph.schema, 0, graph.family,
                                                                   f::OutputProfile::PROFILE, projectionTurn());
    auto handle = open(graph, observer, f::OutputProfile::PROFILE);
    (void)activate(graph, handle);
    observer->writer->begin();
    std::vector<c::FieldValue> values;

    for (std::size_t index = 0; index < 14; ++index)
        values.push_back({q::PROFILE_FIELD_TYPES[index], static_cast<std::int64_t>(index + 1)});

    values.push_back({Type::UTF_CHAR_ARRAY, 0});
    values.push_back({Type::UTF_CHAR_ARRAY, 0, u""});
    values.push_back({Type::CUSTOM_OBJECT, 0, {}, std::vector{std::byte{9}}});
    auto second = values;
    second[14].units = std::u16string{u'\0', char16_t{0xdfff}};
    second[15].units = u"halt";
    send(graph, {{values}, {second}});
    until(
        graph,
        [&] {
            return graph.queue.resources(handle)->queuedRecords == 2;
        },
        false);
    until(graph, [&] {
        return observer->writer->completedRecords() == 2;
    });
    CHECK(rows(output.str(), "field").size() == 34);
    const auto projections = rows(output.str(), "profile");
    REQUIRE(projections.size() == 2);
    CHECK(projections[0] ==
          std::vector<std::string>{
              "profile", "1",  "00530059004D", "1",     "2",     "3",  "4",    "5", "6",           "7",   "8",
              "9",       "10", "11",           "12000", "13000", "14", "null", "",  "unspecified", "null"});
    CHECK(projections[1][17] == "0000DFFF");
    CHECK(projections[1][18] == "00680061006C0074");
    CHECK(rows(output.str(), "field")[16].back() == "09");
}

TEST_CASE("recovery projection refusal becomes one actual callback failure without replay or successful count") {
    // Seventy visits admit one complete nine-field/32-selection projection, but cannot admit two.
    constexpr std::size_t ONE_PROJECTION_VISITS = 70;

    for (const std::size_t visits : {std::size_t{0}, ONE_PROJECTION_VISITS}) {
        auto graph = fixture(eventSchema(false));
        std::ostringstream output;
        f::RecoveryEpochLabels labels(1, EPOCH_BYTES);
        auto observer = std::make_shared<Observer>();
        auto projectionWork = projectionTurn();
        projectionWork.visits = visits;
        observer->writer = std::make_shared<f::RecoveryOutputConsumer>(output, labels, graph.schema, 0, graph.family,
                                                                       f::OutputProfile::QUOTE, projectionWork);
        auto handle = open(graph, observer, f::OutputProfile::QUOTE);
        (void)activate(graph, handle);
        observer->writer->begin();
        std::vector<c::FieldValue> values;

        for (const auto type : q::QUOTE_FIELD_TYPES)
            values.push_back({type, 0});

        values.push_back({Type::CUSTOM_OBJECT, 0});
        send(graph, {{values}, {values}});
        until(
            graph,
            [&] {
                return graph.queue.resources(handle)->queuedRecords == 2;
            },
            false);
        until(graph, [&] {
            return handle.terminal().has_value();
        });
        REQUIRE(handle.terminal()->primary);
        CHECK(handle.terminal()->primary->error == m::QueueError::CALLBACK_FAILURE);
        CHECK(handle.terminal()->counts.callbackFailedBatches == 1);
        CHECK(handle.terminal()->counts.callbackFailedRecords == 2);
        CHECK(observer->sizes == std::vector<std::size_t>{2});
        CHECK(observer->writer->completedRecords() == 0);
        CHECK(observer->writer->failed());
        CHECK(rows(output.str(), "record").size() == (visits == 0 ? 0 : 1));
        CHECK(rows(output.str(), "quote").size() == (visits == 0 ? 0 : 1));
        CHECK_THROWS_WITH_AS(observer->writer->flush(), "recovery output lifecycle failure", std::runtime_error);
    }
}

TEST_CASE("recovery output labels survive real source resets and publication failure never acknowledges reset") {
    f::RecoveryEpochLabels labels(1, EPOCH_BYTES);
    m::QueueIdentity oldEpoch;
    {
        auto graph = fixture(customSchema(false, true));
        std::ostringstream output;
        auto observer = std::make_shared<Observer>();
        observer->writer = std::make_shared<f::RecoveryOutputConsumer>(output, labels, graph.schema, 0, graph.family,
                                                                       f::OutputProfile::GENERIC, projectionTurn());
        auto handle = open(graph, observer);
        auto ticket = activate(graph, handle);
        observer->writer->begin();
        oldEpoch = send(graph, {{{{Type::COMPACT_INT, 1}, {Type::COMPACT_INT, 2}}}});
        until(graph, [&] {
            return observer->writer->completedRecords() == 1;
        });
        CHECK(labels.label(oldEpoch) == 1);
        std::ostringstream evidence;
        f::writeRecoveryCommandAccepted(evidence, f::RecoveryOutputCommand::INITIAL_REPLACE, ticket);
        f::writeRecoveryCommandResult(evidence, f::RecoveryOutputCommand::INITIAL_REPLACE, *ticket.result());
        CHECK(rows(evidence.str(), "command-accepted")[0][2] == std::to_string(ticket.id()));
        CHECK(rows(evidence.str(), "command-result")[0][3] == "0");
        auto work = turn();
        REQUIRE(graph.queue.resetSource(work));
        until(graph, [&] {
            return handle.resetNotice().has_value() && handle.resetNotice()->toEpoch.has_value();
        });
        const auto notice = *handle.resetNotice();
        CHECK(notice.fromEpoch.matches(oldEpoch));
        REQUIRE(notice.toEpoch);
        BrokenBuffer broken(true);
        std::ostream failing(&broken);
        failing.exceptions(std::ios::badbit | std::ios::failbit);
        CHECK_THROWS_WITH_AS(f::writeRecoveryReset(failing, labels, notice), "recovery output publication failure",
                             std::runtime_error);
        REQUIRE(handle.resetNotice());
        CHECK(handle.resetNotice()->sequence == notice.sequence);
        CHECK(handle.state() == m::QueueClientState::RESET_PENDING);
        f::writeRecoveryReset(evidence, labels, notice);
        const auto reset = rows(evidence.str(), "reset");
        REQUIRE(reset.size() == 1);
        CHECK(reset[0][1] == std::to_string(notice.sequence));
        CHECK(reset[0][2] == "1");
        CHECK(reset[0][3] == "2");
        CHECK(reset[0][8] == "1");
        const auto admission = handle.acknowledgeReset(notice.sequence);
        f::writeRecoveryResetAcknowledgement(evidence, notice.sequence, admission);
        CHECK(rows(evidence.str(), "reset-ack")[0][2] == std::to_string(std::to_underlying(admission)));
        until(graph, [&] {
            return !handle.resetNotice() && handle.state() == m::QueueClientState::ACTIVE;
        });
        CHECK(labels.label(*notice.toEpoch) == 2);
        work = turn();
        REQUIRE(graph.queue.resetSource(work));
        until(graph, [&] {
            return handle.resetNotice().has_value() && handle.resetNotice()->toEpoch.has_value();
        });
        CHECK_THROWS_WITH_AS((void)labels.label(*handle.resetNotice()->toEpoch),
                             "recovery output epoch capacity failure", std::runtime_error);
        const auto stale = handle.acknowledgeReset(notice.sequence);
        CHECK(stale == m::ControlAdmission::STALE);
        f::writeRecoveryResetAcknowledgement(evidence, notice.sequence, stale);
        CHECK(rows(evidence.str(), "reset-ack").back()[2] == "2");
        CHECK(handle.requestStop(m::StopMode::ABORT) != m::ControlAdmission::CLOSED);
        until(graph, [&] {
            return handle.terminal().has_value();
        });
    }
    CHECK(labels.label(oldEpoch) == 1);
    CHECK_FALSE(labels.label({}));
}

TEST_CASE("recovery HISTORY output preserves real synthetic removal provenance and scoped range observations") {
    auto graph = fixture(customSchema(true), c::SubscriptionFamily::HISTORY);
    std::ostringstream output;
    f::RecoveryEpochLabels labels(4, EPOCH_BYTES);
    auto observer = std::make_shared<Observer>();
    observer->writer = std::make_shared<f::RecoveryOutputConsumer>(output, labels, graph.schema, 0, graph.family,
                                                                   f::OutputProfile::GENERIC, projectionTurn());
    auto handle = open(graph, observer);
    (void)activate(graph, handle);
    observer->writer->begin();
    auto point = [](std::int64_t time, std::int64_t value) {
        return std::vector<c::FieldValue>{{Type::LONG, time}, {Type::COMPACT_INT, value}};
    };
    auto work = turn();
    auto unknown = graph.queue.rangeStatus(handle, {graph.schema.recordContext(0), u"SYM"}, work);
    REQUIRE(unknown);
    CHECK_FALSE(unknown->completeDeliveredRange);
    std::ostringstream earlyEvidence;
    f::writeRecoveryHistory(earlyEvidence, labels, 0, u"SYM", *unknown);
    CHECK(rows(earlyEvidence.str(), "history")[0][16] == (unknown->source ? "1" : "0"));

    if (!unknown->source) {
        const auto source = rows(earlyEvidence.str(), "history-source")[0];
        CHECK(source.size() == 15);
        CHECK(std::ranges::all_of(source.begin() + 3, source.end(), [](const auto &cell) {
            return cell == "null";
        }));
    }

    send(graph, {{point(3, 30), 4}, {point(2, 20), 0}, {point(0, 0), 10}});
    work = turn();
    auto unstaged = graph.queue.rangeStatus(handle, {graph.schema.recordContext(0), u"SYM"}, work);
    REQUIRE(unstaged);
    CHECK(unstaged->acceptedUnstagedData);
    CHECK_FALSE(unstaged->completeDeliveredRange);
    f::writeRecoveryHistory(earlyEvidence, labels, 0, u"SYM", *unstaged);
    CHECK(rows(earlyEvidence.str(), "history").back()[9] == "1");
    until(graph, [&] {
        return observer->writer->completedRecords() >= 3;
    });
    work = turn();
    auto complete = graph.queue.rangeStatus(handle, {graph.schema.recordContext(0), u"SYM"}, work);
    REQUIRE(complete);
    REQUIRE(complete->source);
    CHECK(complete->completeDeliveredRange);
    std::ostringstream evidence;
    f::writeRecoveryHistory(evidence, labels, 0, u"SYM", *complete);
    const auto history = rows(evidence.str(), "history");
    REQUIRE(history.size() == 1);
    CHECK(history[0][7] == "0");
    CHECK(history[0][15] == "1");
    CHECK(rows(evidence.str(), "history-source")[0].back() == "1");
    send(graph, {{point(5, 50), 4}, {point(2, 22), 16}});
    until(graph, [&] {
        return observer->writer->completedRecords() >= 6;
    });
    std::size_t ordinal = 0;
    std::size_t syntheticOrdinal = 0;

    for (const auto &batch : observer->batches)
        for (const auto &record : batch.records()) {
            ++ordinal;

            if (record.rawTime != 3 || record.source)
                continue;

            syntheticOrdinal = ordinal;
            REQUIRE(record.trigger);
            CHECK(record.trigger->rawTime == 2);
            CHECK(record.trigger->flags == 16);
            CHECK(record.typedValues[1].raw == 0);
        }

    REQUIRE(syntheticOrdinal != 0);
    const auto sources = rows(output.str(), "source");
    const auto triggers = rows(output.str(), "trigger");
    const auto synthetic = std::ranges::find_if(sources, [&](const auto &row) {
        return row[1] == std::to_string(syntheticOrdinal);
    });
    REQUIRE(synthetic != sources.end());
    CHECK(*synthetic == std::vector<std::string>{"source", (*synthetic)[1], "0", "null", "null", "null", "null", "null",
                                                 "null", "null", "null"});
    const auto trigger = std::ranges::find_if(triggers, [&](const auto &row) {
        return row[1] == (*synthetic)[1];
    });
    REQUIRE(trigger != triggers.end());
    CHECK((*trigger)[2] == "1");
    CHECK((*trigger)[5] == "16");
    CHECK((*trigger)[7] == "2");
    work = turn();
    auto truncated = graph.queue.rangeStatus(handle, {graph.schema.recordContext(0), u"SYM"}, work);
    REQUIRE(truncated);
    REQUIRE(truncated->source);
    CHECK(truncated->source->snipFrontier == 2);
    f::writeRecoveryHistory(evidence, labels, 0, u"SYM", *truncated);
    CHECK(rows(evidence.str(), "history-source").back()[9] == "2");
    CHECK(handle.requestStop(m::StopMode::ABORT) != m::ControlAdmission::CLOSED);
    until(graph, [&] {
        return handle.terminal().has_value();
    });
    f::writeRecoveryQueueTerminal(evidence, labels, graph.family, *handle.terminal());
    CHECK(rows(evidence.str(), "queue-terminal").size() == 1);
    CHECK(rows(evidence.str(), "queue-counts").size() == 1);
    CHECK(rows(evidence.str(), "queue-terminal")[0][3] ==
          std::to_string(std::to_underlying(handle.terminal()->outcome)));
}

TEST_CASE("recovery publication failures redact foreign exceptions at begin evidence callback and final flush") {
    auto schema = customSchema(false, true);
    f::RecoveryEpochLabels labels(1, EPOCH_BYTES);

    for (const bool throws : {false, true}) {
        BrokenBuffer broken(throws);
        std::ostream output(&broken);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        f::RecoveryOutputConsumer writer(output, labels, schema, 0, c::SubscriptionFamily::STREAM,
                                         f::OutputProfile::GENERIC, projectionTurn());
        CHECK_THROWS_WITH_AS(writer.begin(), "recovery output begin failure", std::runtime_error);
        CHECK(writer.failed());
        CHECK_THROWS_WITH_AS(writer.flush(), "recovery output lifecycle failure", std::runtime_error);
        CHECK_THROWS_WITH_AS(f::writeRecoveryResetAcknowledgement(output, 9, m::ControlAdmission::STALE),
                             "recovery output publication failure", std::runtime_error);
    }

    for (const int failure : {0, 1, 2}) {
        auto graph = fixture(customSchema(false, true));
        f::RecoveryEpochLabels callbackLabels(1, EPOCH_BYTES);
        std::ostringstream initiallyGood;
        auto observer = std::make_shared<Observer>();
        observer->writer = std::make_shared<f::RecoveryOutputConsumer>(
            initiallyGood, callbackLabels, graph.schema, 0, graph.family, f::OutputProfile::GENERIC, projectionTurn());
        auto handle = open(graph, observer);
        (void)activate(graph, handle);
        observer->writer->begin();
        BrokenBuffer broken(failure != 0, failure == 2, 19);
        auto *old = static_cast<std::ostream &>(initiallyGood).rdbuf(&broken);
        initiallyGood.exceptions(std::ios::badbit | std::ios::failbit);
        send(graph, {{{{Type::COMPACT_INT, 1}, {Type::COMPACT_INT, 2}}}});
        until(graph, [&] {
            return handle.terminal().has_value();
        });
        REQUIRE(handle.terminal()->primary);
        CHECK(handle.terminal()->primary->error == m::QueueError::CALLBACK_FAILURE);
        CHECK(handle.terminal()->counts.callbackFailedBatches == 1);
        CHECK(observer->writer->completedRecords() == 0);
        CHECK(observer->writer->failed());
        CHECK(observer->sizes == std::vector<std::size_t>{1});
        static_cast<std::ostream &>(initiallyGood).rdbuf(old);
    }

    std::ostringstream finalOutput;
    f::RecoveryOutputConsumer finalWriter(finalOutput, labels, schema, 0, c::SubscriptionFamily::STREAM,
                                          f::OutputProfile::GENERIC, projectionTurn());
    finalWriter.begin();
    BrokenBuffer syncFailure(true, true);
    auto *finalBuffer = static_cast<std::ostream &>(finalOutput).rdbuf(&syncFailure);
    finalOutput.exceptions(std::ios::badbit | std::ios::failbit);
    CHECK_THROWS_WITH_AS(finalWriter.flush(), "recovery output flush failure", std::runtime_error);
    CHECK(finalWriter.failed());
    static_cast<std::ostream &>(finalOutput).rdbuf(finalBuffer);
}

TEST_CASE("recovery output rejects foreign equal-shaped context and wrong family through actual callbacks") {
    for (const bool wrongFamily : {false, true}) {
        auto graph = fixture(customSchema(false, true));
        auto independent = customSchema(false, true);
        CHECK_FALSE(independent.recordContext(0).matches(graph.schema.recordContext(0)));
        std::ostringstream output;
        f::RecoveryEpochLabels labels(1, EPOCH_BYTES);
        auto observer = std::make_shared<Observer>();
        observer->writer = std::make_shared<f::RecoveryOutputConsumer>(
            output, labels, wrongFamily ? graph.schema : independent, 0,
            wrongFamily ? c::SubscriptionFamily::TICKER : graph.family, f::OutputProfile::GENERIC, projectionTurn());
        auto handle = open(graph, observer);
        (void)activate(graph, handle);
        observer->writer->begin();
        send(graph, {{{{Type::COMPACT_INT, 1}, {Type::COMPACT_INT, 2}}}});
        until(graph, [&] {
            return handle.terminal().has_value();
        });
        REQUIRE(handle.terminal()->primary);
        CHECK(handle.terminal()->primary->error == m::QueueError::CALLBACK_FAILURE);
        CHECK(observer->writer->failed());
        CHECK(observer->writer->completedRecords() == 0);
        CHECK(rows(output.str(), "record").empty());
    }
}

TEST_CASE("recovery TICKER output keeps its actual family and complete compact fields") {
    auto graph = fixture(customSchema(false, true), c::SubscriptionFamily::TICKER);
    std::ostringstream output;
    f::RecoveryEpochLabels labels(1, EPOCH_BYTES);
    auto observer = std::make_shared<Observer>();
    observer->writer = std::make_shared<f::RecoveryOutputConsumer>(output, labels, graph.schema, 0, graph.family,
                                                                   f::OutputProfile::GENERIC, projectionTurn());
    auto handle = open(graph, observer);
    (void)activate(graph, handle);
    observer->writer->begin();
    send(graph, {{{{Type::COMPACT_INT, -9}, {Type::COMPACT_INT, 0}}}});
    until(graph, [&] {
        return observer->writer->completedRecords() == 1;
    });
    CHECK(rows(output.str(), "selection")[0] == std::vector<std::string>{"selection", "ticker", "0", "generic"});
    CHECK(rows(output.str(), "record")[0][2] == "ticker");
    CHECK(rows(output.str(), "field") ==
          std::vector<std::vector<std::string>>{{"field", "1", "0", "8", "-9", "null", "null"},
                                                {"field", "1", "1", "8", "0", "null", "null"}});
}

TEST_CASE("recovery evidence writers preserve detached numeric scopes and never echo arbitrary stage text") {
    namespace t = root::transport;
    // These detached-type formatting inputs do not claim a native recovery run or terminal observation.
    t::RecoveryStatus status;
    status.phase = t::RecoveryPhase::ACTIVE;
    status.attempts = 3;
    status.demandedFamilies = 6;
    status.convergedFamilies = 2;
    std::ostringstream output;
    f::writeRecoveryStatus(output, status);
    CHECK(output.str() == "recovery-status\t5\t3\t6\t2\t0\t0\n");
    t::RecoveryDiagnostic refusal;
    refusal.error = t::RecoveryError::WORK_LIMIT;
    refusal.stage = "SECRET-token/path/challenge";
    refusal.queue =
        m::QueueFailure{m::QueueError::WORK_LIMIT, m::QueueSourceError{root::collectors::HistoryError::WORK_LIMIT}};
    f::writeRecoveryHistoryUnavailable(output, 0, u"SYM", std::numeric_limits<std::int64_t>::min(), refusal);
    f::writeRecoveryDiagnostic(output, f::RecoveryOutputDiagnostic::HISTORY_QUERY, refusal);
    CHECK(rows(output.str(), "history-unavailable")[0] ==
          std::vector<std::string>{"history-unavailable", "0", "00530059004D", "-9223372036854775808", "4"});
    CHECK(rows(output.str(), "history").empty());
    CHECK(rows(output.str(), "history-source").empty());
    CHECK(output.str().find("SECRET") == std::string::npos);
    const auto diagnostics = rows(output.str(), "recovery-diagnostic");
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0][6] == std::to_string(std::to_underlying(m::QueueError::WORK_LIMIT)));
    CHECK(diagnostics[0][7] == "4");
    CHECK(diagnostics[0][8] == std::to_string(std::to_underlying(root::collectors::HistoryError::WORK_LIMIT)));
    t::RecoveryTerminal terminal;
    terminal.deadlineExpired = true;
    terminal.heldDataDiscarded = true;
    f::writeRecoveryTerminal(output, terminal);
    CHECK(rows(output.str(), "recovery-terminal")[0] ==
          std::vector<std::string>{"recovery-terminal", "1", "1", "1", "null", "0"});
    CHECK(rows(output.str(), "wire-terminal").empty());
    terminal.nativeOperationsAtCutoff = 2;
    terminal.wire.emplace();
    terminal.wire->writeAcceptanceUnknown = true;
    terminal.wire->reportedWriteBytes = 13;
    terminal.wire->discardedInputBytes = 17;
    terminal.wire->pendingOutputBytes = 19;
    f::writeRecoveryTerminal(output, terminal);
    CHECK(rows(output.str(), "wire-terminal")[0] ==
          std::vector<std::string>{"wire-terminal", "0", "0", "0", "0", "0", "0", "0", "0", "1", "13", "17", "19"});
}

#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
TEST_CASE("TLS diagnostic keeps numeric evidence and never publishes raw stage text") {
    std::ostringstream output;
    output << std::hex << std::boolalpha;
    t::TlsDiagnostic diagnostic;
    diagnostic.error = t::TlsError::TLS_POLICY_FAILURE;
    diagnostic.stage = "synthetic-secret-path-and-peer-text";
    diagnostic.sdkCode = std::numeric_limits<std::uint64_t>::max();
    diagnostic.verifyCode = -7;
    diagnostic.transport.nativeCode = -11;
    f::writeRecoveryTlsDiagnostic(output, f::RecoveryOutputDiagnostic::FIRST, diagnostic);
    const auto emitted = rows(output.str(), "tls-diagnostic");
    REQUIRE(emitted.size() == 1);
    REQUIRE(emitted[0].size() == 8);
    CHECK(emitted[0][1] == "first");
    CHECK(emitted[0][2] == std::to_string(static_cast<int>(diagnostic.error)));
    CHECK(emitted[0][3] == "18446744073709551615");
    CHECK(emitted[0][4] == "-7");
    CHECK(emitted[0][6] == "-11");
    CHECK(output.str().find("synthetic-secret") == std::string::npos);
}
#endif
