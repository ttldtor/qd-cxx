// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/events/quote.hpp"
#include "qd/protocol/tape_reader.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <string>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace p = org::ttldtor::qdcxx::protocol;
namespace q = org::ttldtor::qdcxx::events;
std::filesystem::path fixtures; ///< Explicit immutable fixture root from CTest.

/** Finite independent test bounds.
 * @return Reader configuration including a real metadata allowance.
 */
p::TapeLimits limits() {
    return {{8192, 8201},
            65536,
            128,
            {16, 8, {1024, 256}, 2048},
            {{10, 32, 64, 64, 4096}, 32, {256, 64, 4096}},
            100000,
            {8192, 128, 1024, 128, 1024, 4096, {8192, 128, 128}},
            128};
}

/** Construct the unchanged local profiles used by the oracle.
 * @param quote Whether to select default composite Quote.
 * @return Owned validated schema.
 */
c::LocalSchema schema(bool quote = false) {
    auto local =
        quote ? q::defaultQuoteSchema(limits().context.schema)
              : c::LocalSchema::create(
                    std::vector<c::LocalRecord>{{0, u"S", false, {{8}}, {{u"v", 0}}},
                                                {1, u"R", false, {{8}, {8}, {8}}, {{u"y", 0}, {u"x", 1}, {u"z", 2}}},
                                                {2, u"E", false, {}, {}}},
                    limits().context.schema);
    REQUIRE(local);

    return std::move(*local);
}

/** Read an unchanged Java artifact.
 * @param name Filename stem.
 * @return Owned binary tape bytes.
 */
std::vector<std::byte> input(std::string_view name) {
    std::ifstream in(fixtures / (std::string(name) + ".qtp"), std::ios::binary);
    REQUIRE(in.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)), {});
    const auto bytes = std::as_bytes(std::span(chars));

    return {bytes.begin(), bytes.end()};
}

/** Parse an integral cell without accepting suffixes.
 * @tparam T Integral result type.
 * @param text Entire cell.
 * @param base Numeric radix.
 * @return Exact value, or fail the test on malformed evidence.
 */
template <typename T> T number(std::string_view text, int base = 10) {
    T value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, base);
    REQUIRE(parsed.ec == std::errc{});
    REQUIRE(parsed.ptr == text.data() + text.size());

    return value;
}

/** Split a row without discarding empty cells.
 * @param line Complete TSV row.
 * @return Ordered cells.
 */
std::vector<std::string> cells(const std::string &line) {
    std::vector<std::string> out;
    std::size_t start = 0;

    for (;;) {
        const auto end = line.find('\t', start);
        out.push_back(line.substr(start, end - start));

        if (end == std::string::npos) {
            return out;
        }

        start = end + 1;
    }
}

/** Decode exact UTF-16 fixture cells.
 * @param text Four hex digits per code unit, or the empty sentinel.
 * @return Owned exact units.
 */
std::u16string units(const std::string &text) {
    if (text == "-") {
        return {};
    }

    REQUIRE_FALSE(text.empty());
    REQUIRE(text.size() % 4 == 0);
    REQUIRE(text.find_first_not_of("0123456789abcdef") == std::string::npos);
    std::u16string out;

    for (std::size_t i = 0; i < text.size(); i += 4) {
        out.push_back(static_cast<char16_t>(number<unsigned>(text.substr(i, 4), 16)));
    }

    return out;
}

/** One fully owned record and its tape metadata. */
struct Record {
    p::StreamDataRecord value; ///< Exact fields and symbol.
    p::TapeTiming timing;      ///< Copied frame metadata.
};

/** Complete observation independent of caller input and reader lifetime. */
struct Observation {
    p::TapeStatus status{};                   ///< Terminal EOF result.
    p::TapeDiagnostic error;                  ///< First failure, if any.
    std::vector<Record> records;              ///< Completed output prefix.
    std::vector<p::TapeHeartbeat> heartbeats; ///< Explicit accepted messages, including at EOF.
    std::vector<p::Frame> frames;             ///< Actual complete envelopes and offsets.
    p::TapeTiming timing;                     ///< Last committed state.
    std::optional<p::TapeDescriptor> header;  ///< Owned accepted descriptor.
    std::size_t timingEvents{};               ///< Charged metadata attempts.
    std::size_t processed{};                  ///< Charged records including skips.
    std::size_t fields{};                     ///< Charged fields including skips.
    std::size_t construction{};               ///< Cumulative reconstruction units.
    std::size_t markers{};                    ///< Empty envelope markers.
};

/** Feed arbitrary schedules without retaining any borrowed input.
 * @param bytes Borrowed tape bytes.
 * @param budget Finite reader budgets.
 * @param quote Select default Quote local schema.
 * @param chunk Maximum positive chunk size.
 * @param first Optional first split, zero uses chunk.
 * @param mode Explicit policy.
 * @return Owned records, metadata, positions and counters after destruction.
 */
Observation run(std::span<const std::byte> bytes, p::TapeLimits budget = limits(), bool quote = false,
                std::size_t chunk = 65536, std::size_t first = 0, p::TapeMode mode = p::TapeMode::RAW_MESSAGE_V1) {
    auto local = schema(quote);
    p::TapeReader reader(local, budget, mode);
    Observation out;
    std::uint64_t offset = 0;

    while (!bytes.empty()) {
        const auto count = std::min(bytes.size(), offset == 0 && first != 0 ? first : chunk);
        auto event = reader.feed(bytes.first(count));
        CHECK(event.accepted <= count);
        offset += event.accepted;
        CHECK(reader.acceptedBytes() == offset);
        bytes = bytes.subspan(event.accepted);

        if (!event.records.empty()) {
            REQUIRE(event.timing);
        }

        for (auto &record : event.records) {
            out.records.push_back({std::move(record), *event.timing});
        }

        if (event.heartbeat) {
            out.heartbeats.push_back(*event.heartbeat);
        }

        if (event.frame) {
            CHECK(event.frame->endOffset == offset);
            CHECK(event.frame->startOffset == (out.frames.empty() ? 0 : out.frames.back().endOffset));

            if (mode == p::TapeMode::RAW_MESSAGE_V1 && event.frame->type == 5 &&
                reader.diagnostic().stage != "timing") {
                CHECK(event.timing.has_value());
            }

            out.frames.push_back(std::move(*event.frame));
        }

        if (event.status == p::TapeStatus::FAILED) {
            break;
        }

        REQUIRE(event.accepted > 0);
        CHECK(reader.feed({}).status == p::TapeStatus::NEED_INPUT);
    }

    out.status = reader.finish();
    out.error = reader.diagnostic();
    out.timing = reader.timing();
    out.header = reader.descriptor();
    out.timingEvents = reader.timingEvents();
    out.processed = reader.processedRecords();
    out.fields = reader.wireFields();
    out.construction = reader.constructionUnits();
    out.markers = reader.markerCount();
    CHECK(reader.finish() == out.status);
    CHECK(reader.feed(bytes).accepted == 0);

    return out;
}

/** Canonical semantic projection for strict comparator and mutation checks.
 * @param actual Owned decoded records.
 * @return Ordered rows covering identity, exact symbols/flags, timing and field types/raw values.
 */
std::vector<std::string> projection(const Observation &actual) {
    std::vector<std::string> out;

    for (const auto &record : actual.records) {
        const auto &value = record.value;
        std::string row = std::to_string(value.localId) + ":" + std::to_string(value.symbol.eventFlags) + ":" +
                          std::to_string(record.timing.eventTimeSequence) + ":" +
                          (record.timing.timestampMs ? std::to_string(*record.timing.timestampMs) : "absent");

        for (auto unit : value.symbol.units) {
            row += ":u" + std::to_string(unit);
        }

        for (auto field : value.values) {
            row += ":8=" + std::to_string(field);
        }

        for (const auto &field : value.typedValues) {
            row += ":" + std::to_string(static_cast<std::int32_t>(field.type)) + "=" + std::to_string(field.raw);
        }

        out.push_back(std::move(row));
    }

    return out;
}

/** Compare the full accepted Java grammar and its STREAM callback projection.
 * @param name Immutable case stem.
 * @param actual C++ observation, one record per wire record.
 */
void compare(std::string_view name, const Observation &actual) {
    REQUIRE(actual.status == p::TapeStatus::FINISHED);
    std::ifstream in(fixtures / (std::string(name) + ".tsv"), std::ios::binary);
    REQUIRE(in.good());
    std::string line;
    std::size_t allRecords = 0;
    std::size_t emitted = 0;
    std::size_t callback = 0;
    std::size_t fields = 0;
    std::size_t current = 0;
    std::size_t heartbeatIndex = 0;
    std::string callbackType;
    bool selected = false;
    bool mapped = false;
    bool ended = false;
    bool complete = false;
    bool started = false;
    std::optional<std::int64_t> timestamp;

    while (std::getline(in, line)) {
        REQUIRE_FALSE(complete);
        REQUIRE_FALSE(line.empty());
        REQUIRE(line.back() != '\r');
        const auto row = cells(line);
        REQUIRE_FALSE((ended && row[0] != "complete"));

        if (!started) {
            REQUIRE(row.size() == 3);
            CHECK(row[0] == "case");
            CHECK(row[1] == name);
            CHECK((row[2] == "FileWriter" || row[2] == "DXEndpoint-TapeConnector-FileWriterHandler" ||
                   row[2] == "diagnostic-wire"));
            started = true;
            continue;
        }

        if (row[0] == "descriptor") {
            REQUIRE(row.size() == 6);
            CHECK(number<std::size_t>(row[1]) == callback++);
        } else if (row[0] == "property") {
            REQUIRE(row.size() == 3);
            static_cast<void>(units(row[1]));
            static_cast<void>(units(row[2]));
        } else if (row[0] == "heartbeat") {
            REQUIRE(row.size() == 7);
            CHECK(number<std::size_t>(row[1]) == callback++);

            if (row[2] != "-") {
                timestamp = number<std::int64_t>(row[2]);
            }

            CHECK(row[3] == "-");
            CHECK(row[4] == "-");
            CHECK(row[5] == "-");
            CHECK(row[6] == (row[2] == "-" ? "true" : "false"));

            while (heartbeatIndex < actual.heartbeats.size() && !actual.heartbeats[heartbeatIndex].content) {
                ++heartbeatIndex;
            }

            REQUIRE(heartbeatIndex < actual.heartbeats.size());
            const auto &beat = actual.heartbeats[heartbeatIndex++];
            CHECK(beat.content == (row[2] == "-" ? 0 : 1));
            CHECK(beat.timestampMs ==
                  (row[2] == "-" ? std::optional<std::int64_t>{} : std::optional{number<std::int64_t>(row[2])}));
        } else if (row[0] == "callback") {
            REQUIRE(row.size() == 3);
            CHECK(number<std::size_t>(row[1]) == callback++);
            CHECK((row[2] == "10" || row[2] == "15" || row[2] == "20"));
            callbackType = row[2];
        } else if (row[0] == "record") {
            REQUIRE(row.size() == 8);

            if (selected) {
                CHECK(fields ==
                      actual.records[current].value.values.size() + actual.records[current].value.typedValues.size());
                CHECK(mapped == !actual.records[current].value.typedValues.empty());
            }

            CHECK(number<std::size_t>(row[1]) == allRecords++);
            CHECK(row[2] == callbackType);
            selected = row[2] == "15";
            fields = 0;
            mapped = false;

            if (selected) {
                REQUIRE(emitted < actual.records.size());
                current = emitted++;
                const auto &record = actual.records[current];
                CHECK(number<std::int32_t>(row[3]) == record.value.localId);
                CHECK(units(row[4]) == (record.value.typedValues.empty()
                                            ? std::array<std::u16string, 3>{u"S", u"R", u"E"}.at(
                                                  static_cast<std::size_t>(record.value.localId))
                                            : u"Quote"));
                CHECK(units(row[5]) == record.value.symbol.units);
                CHECK(number<std::int32_t>(row[6]) == record.value.symbol.eventFlags);
                CHECK(number<std::int64_t>(row[7]) == record.timing.eventTimeSequence);
                CHECK(timestamp == record.timing.timestampMs);
            }
        } else if (row[0] == "field") {
            REQUIRE(row.size() == 5);
            CHECK(number<std::size_t>(row[1]) + 1 == allRecords);
            CHECK(number<std::size_t>(row[2]) == fields);

            if (selected) {
                const auto &record = actual.records[current].value;

                if (record.typedValues.empty()) {
                    REQUIRE(fields < record.values.size());
                    CHECK(row[3] == "8");
                    CHECK(number<std::int32_t>(row[4]) == record.values[fields]);
                } else {
                    REQUIRE(fields < record.typedValues.size());
                    CHECK(number<std::int32_t>(row[3]) == static_cast<std::int32_t>(record.typedValues[fields].type));
                    CHECK(number<std::int64_t>(row[4]) == record.typedValues[fields].raw);
                }
            }

            ++fields;
        } else if (row[0] == "mapped") {
            REQUIRE(row.size() == 13);
            CHECK_FALSE(mapped);
            mapped = true;
            CHECK(number<std::size_t>(row[1]) + 1 == allRecords);

            if (selected) {
                const auto &record = actual.records[current];
                auto quote = q::mapQuote(record.value.symbol.units, record.value.typedValues, 128);
                REQUIRE(quote);
                CHECK(number<std::int64_t>(row[2]) == quote->bidTimeMs);
                CHECK(number<std::int64_t>(row[3]) == quote->askTimeMs);
                CHECK(number<unsigned>(row[4]) == quote->bidExchangeCode);
                CHECK(number<unsigned>(row[5]) == quote->askExchangeCode);
                CHECK(number<std::uint64_t>(row[6], 16) == std::bit_cast<std::uint64_t>(quote->bidPrice.toDouble()));
                CHECK(number<std::uint64_t>(row[7], 16) == std::bit_cast<std::uint64_t>(quote->bidSize.toDouble()));
                CHECK(number<std::uint64_t>(row[8], 16) == std::bit_cast<std::uint64_t>(quote->askPrice.toDouble()));
                CHECK(number<std::uint64_t>(row[9], 16) == std::bit_cast<std::uint64_t>(quote->askSize.toDouble()));
                CHECK(number<std::int64_t>(row[10]) == record.timing.eventTimeMs());
                CHECK(number<std::int32_t>(row[11]) == quote->sequence);
                CHECK(number<std::int32_t>(row[12]) == quote->timeNanoPart);
                CHECK(quote->eventTime == 0);
            }
        } else if (row[0] == "end") {
            REQUIRE(row.size() == 4);
            CHECK(number<std::size_t>(row[1]) == allRecords);
            CHECK(row[2] == "0");
            CHECK(number<std::size_t>(row[3]) == callback);
            ended = true;
        } else if (row[0] == "complete") {
            CHECK(row == std::vector<std::string>{"complete", std::string(name)});
            REQUIRE(ended);
            complete = true;
        } else {
            FAIL("unknown Java fixture row");
        }
    }

    REQUIRE(complete);
    CHECK(emitted == actual.records.size());
    CHECK(allRecords == emitted * 3);

    while (heartbeatIndex < actual.heartbeats.size() && !actual.heartbeats[heartbeatIndex].content) {
        ++heartbeatIndex;
    }

    CHECK(heartbeatIndex == actual.heartbeats.size());
}

/** Append a compact primitive in designed boundary tests.
 * @param out Destination bytes.
 * @param value Exact signed value.
 */
void compact(std::vector<std::byte> &out, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    const auto written = e::writeCompactLong(value, buffer);
    REQUIRE(written.status == e::Status::OK);
    out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(written.written));
}

/** Encode a complete designed envelope.
 * @param type Wire message identity.
 * @param body Complete payload.
 * @return Owned canonical frame.
 */
std::vector<std::byte> message(std::int32_t type, std::span<const std::byte> body) {
    std::vector<std::byte> contents;
    compact(contents, type);
    contents.insert(contents.end(), body.begin(), body.end());
    std::vector<std::byte> out;
    compact(out, static_cast<std::int64_t>(contents.size()));
    out.insert(out.end(), contents.begin(), contents.end());

    return out;
}

/** Append bytes without borrowed retention.
 * @param out Destination tape.
 * @param bytes Temporary or stored source.
 */
void append(std::vector<std::byte> &out, std::span<const std::byte> bytes) {
    out.insert(out.end(), bytes.begin(), bytes.end());
}

/** Extract the immutable original header for designed boundaries.
 * @return Complete original descriptor envelope.
 */
std::vector<std::byte> header() {
    auto bytes = input("raw-synthetic");
    p::FrameReader reader(limits().frame);
    const auto first = reader.feed(bytes);
    REQUIRE(first.frame);
    bytes.resize(first.accepted);

    return bytes;
}

/** Encode a timestamp-only heartbeat.
 * @param timestamp Signed milliseconds.
 * @return Complete explicit type-zero frame.
 */
std::vector<std::byte> heartbeat(std::int64_t timestamp) {
    std::vector<std::byte> body{std::byte{1}};
    compact(body, timestamp);

    return message(0, body);
}

/** Encode bounded diagnostic name text.
 * @param out Destination payload.
 * @param value Exact name.
 */
void text(std::vector<std::byte> &out, std::u16string_view value) {
    std::array<std::byte, 256> buffer{};
    const auto written = e::writeUtfString(value, buffer, {200, 100});
    REQUIRE(written.status == e::Status::OK);
    append(out, std::span(buffer).first(written.written));
}

/** Encode a designed description update with intentional invalid cases supported.
 * @param records Ordered wire descriptions.
 * @return Complete type-two frame.
 */
std::vector<std::byte> descriptions(std::initializer_list<p::WireRecordDescription> records) {
    std::vector<std::byte> body;

    for (const auto &record : records) {
        compact(body, record.senderId);
        text(body, record.name);
        compact(body, static_cast<std::int64_t>(record.fields.size()));

        for (const auto &field : record.fields) {
            text(body, field.name);
            compact(body, field.serialTypeId);
        }
    }

    return message(2, body);
}

/** Encode a fresh empty-symbol record followed by compact field primitives.
 * @param id Sender identity.
 * @param values Complete compact wire fields.
 * @return Designed complete payload, for supported compact full types.
 */
std::vector<std::byte> payload(std::int32_t id, std::initializer_list<std::int64_t> values) {
    std::vector<std::byte> out{std::byte{0xFC}, std::byte{0}};
    compact(out, id);

    for (auto value : values) {
        compact(out, value);
    }

    return out;
}
} // namespace

TEST_CASE("unchanged production RAW tapes match every Java STREAM projection value and timing") {
    for (const auto *name : {"raw-synthetic", "raw-hs", "raw-quote", "connector-quote", "timing-sequence", "opt-absent",
                             "opt-empty", "opt-hs", "empty-envelope", "explicit-zero-empty", "heartbeat-absent"}) {
        INFO(name);
        const bool quote = std::string_view(name).find("quote") != std::string_view::npos;
        auto bytes = input(name);
        auto baseline = run(bytes, limits(), quote);
        compare(name, baseline);

        for (auto chunk : {1U, 2U, 7U, 19U}) {
            auto actual = run(bytes, limits(), quote, chunk);
            CHECK(projection(actual) == projection(baseline));
            CHECK(actual.status == baseline.status);
            CHECK(actual.heartbeats.size() == baseline.heartbeats.size());

            for (std::size_t i = 0; i < actual.heartbeats.size(); ++i) {
                CHECK(actual.heartbeats[i].content == baseline.heartbeats[i].content);
                CHECK(actual.heartbeats[i].timestampMs == baseline.heartbeats[i].timestampMs);
            }
        }

        for (std::size_t split = 1; split < bytes.size(); ++split) {
            CHECK(projection(run(bytes, limits(), quote, 65536, split)) == projection(baseline));
        }

        bytes.assign(bytes.size(), std::byte{0xFF});
        compare(name, baseline);
    }
}

TEST_CASE("wire heartbeat distinctions absent timestamp and EOF are preserved") {
    auto observed = run(input("timing-sequence"));
    REQUIRE(observed.records.size() == 7);
    CHECK_FALSE(observed.records[0].timing.timestampMs);
    CHECK(observed.records[1].timing.timestampMs == 0);
    CHECK(observed.records[2].timing.timestampMs == 1234);
    CHECK(observed.records[3].timing.timestampMs == 1234);
    CHECK(observed.records[4].timing.timestampMs == -1);
    CHECK(observed.records[5].timing.timestampMs == 77);
    CHECK(observed.records[6].timing.timestampMs == 77);
    CHECK(observed.timing.timestampMs == 999);
    CHECK(observed.heartbeats.size() == 7);
    auto bytes = header();
    append(bytes, heartbeat(44));
    append(bytes, message(0, {}));
    append(bytes, message(0, std::array{std::byte{0}}));
    bytes.push_back(std::byte{0});
    auto forms = run(bytes);
    REQUIRE(forms.status == p::TapeStatus::FINISHED);
    REQUIRE(forms.heartbeats.size() == 3);
    CHECK(forms.heartbeats[0].content == 1);
    CHECK_FALSE(forms.heartbeats[1].content);
    CHECK(forms.heartbeats[2].content == 0);
    CHECK(forms.timing.timestampMs == 44);
    CHECK(forms.markers == 1);
    CHECK(forms.timingEvents == 3);
    auto fresh = run(header());
    CHECK_FALSE(fresh.timing.timestampMs);
    CHECK(fresh.timing.eventTimeSequence == 0);
}

TEST_CASE("Java boundary packing preserves raw int64 timestamp and saturated cursor sequence") {
    std::ifstream in(fixtures / "packing.tsv");
    REQUIRE(in.good());
    std::string line;
    std::size_t count = 0;
    bool complete = false;

    while (std::getline(in, line)) {
        REQUIRE_FALSE(complete);
        const auto row = cells(line);

        if (row[0] == "complete") {
            CHECK(row == std::vector<std::string>{"complete", "packing"});
            complete = true;
            continue;
        }

        REQUIRE(row.size() == 3);
        CHECK(row[0] == "packing");
        const auto timestamp = number<std::int64_t>(row[1]);
        const auto sequence = number<std::int64_t>(row[2]);
        auto bytes = header();
        append(bytes, heartbeat(timestamp));
        auto observed = run(bytes);
        REQUIRE(observed.status == p::TapeStatus::FINISHED);
        CHECK(observed.timing.timestampMs == timestamp);
        CHECK(observed.timing.eventTimeSequence == sequence);
        compare("boundary-" + row[1], run(input("boundary-" + row[1])));
        ++count;
    }

    CHECK(complete);
    CHECK(count == 9);

    for (const auto *name : {"boundary--1000", "boundary--1001", "boundary--999", "boundary-999", "boundary-1000"}) {
        compare(name, run(input(name)));
    }
}

TEST_CASE("mode descriptor and option matrix rejects only the explicit bounded combinations") {
    for (const auto *time : {"absent", "NONE", "MESSAGE", "FIELD"}) {
        for (const auto send : {-1, 5, 15}) {
            for (const auto actual : {5, 15}) {
                const auto name =
                    std::string("matrix-") + time + "-" + std::to_string(send) + "-" + std::to_string(actual);
                INFO(name);
                const auto result = run(input(name));
                const bool accepted = std::string_view(time) == "MESSAGE" && send != 15 && actual == 5;
                CHECK(result.status == (accepted ? p::TapeStatus::FINISHED : p::TapeStatus::FAILED));

                if (!accepted) {
                    CHECK(result.error.error == p::TapeError::UNSUPPORTED);
                    CHECK(result.error.rejection ==
                          (std::string_view(time) == "MESSAGE" || (std::string_view(time) == "absent" && send == 15)
                               ? p::TapeRejection::MESSAGE
                               : p::TapeRejection::TIME));
                }
            }
        }
    }

    for (const auto *name : {"opt-null", "opt-unknown", "opt-hs-unknown", "nonzero-flags", "nonzero-flags-empty-opt",
                             "unsupported-message", "stream-synthetic"}) {
        const auto result = run(input(name));
        CHECK(result.status == p::TapeStatus::FAILED);
        CHECK(result.error.error ==
              (std::string_view(name) == "opt-null" ? p::TapeError::MALFORMED : p::TapeError::UNSUPPORTED));
    }

    auto missing = run(input("opt-absent"));
    REQUIRE(missing.header);
    CHECK_FALSE(missing.header->options);
    CHECK(run(input("opt-empty")).header->options == u"");
    CHECK(run(input("opt-hs")).header->options == u"hs");
    CHECK(run(input("raw-synthetic")).header->time == u"message");
    CHECK(run(input("raw-synthetic")).header->explicitRawSend);
    CHECK(run(input("raw-synthetic"), limits(), false, 65536, 0, p::TapeMode::LEGACY).status == p::TapeStatus::FAILED);
    auto local = schema();
    CHECK_THROWS_AS(p::TapeReader(local, limits(), static_cast<p::TapeMode>(99)), std::invalid_argument);
}

TEST_CASE("heartbeat failures preserve committed timing and terminal exact primitive checkpoints") {
    const std::vector<std::pair<std::vector<std::byte>, p::TapeError>> cases{
        {{std::byte{0x80}}, p::TapeError::MALFORMED},
        {{std::byte{1}}, p::TapeError::MALFORMED},
        {{std::byte{1}, std::byte{0x80}}, p::TapeError::MALFORMED},
        {{std::byte{2}}, p::TapeError::UNSUPPORTED},
        {{std::byte{4}}, p::TapeError::UNSUPPORTED},
        {{std::byte{8}}, p::TapeError::UNSUPPORTED},
        {{std::byte{16}}, p::TapeError::UNSUPPORTED},
        {{std::byte{0x7F}}, p::TapeError::UNSUPPORTED},
        {{std::byte{0}, std::byte{0}}, p::TapeError::UNSUPPORTED},
        {{std::byte{1}, std::byte{7}, std::byte{0}}, p::TapeError::UNSUPPORTED}};

    for (std::size_t index = 0; index < cases.size(); ++index) {
        auto bytes = input("raw-synthetic");
        const auto prefix = run(bytes);
        const auto start = bytes.size();
        append(bytes, message(0, cases[index].first));
        append(bytes, heartbeat(987));
        const auto observed = run(bytes, limits(), false, 1);
        CHECK(observed.status == p::TapeStatus::FAILED);
        CHECK(observed.error.error == cases[index].second);
        CHECK(observed.error.stage == "heartbeat");
        CHECK(observed.error.frameStart == start);
        CHECK(observed.error.payloadStart == start + 2);
        CHECK(observed.error.errorOffset == (index == 1 || index == 2 || index == 8 ? 1 : index == 9 ? 2 : 0));
        CHECK(observed.timing.timestampMs == prefix.timing.timestampMs);
        CHECK(observed.timingEvents == prefix.timingEvents + 1);
        CHECK(projection(observed) == projection(prefix));
    }

    for (const auto *name :
         {"heartbeat-fields", "heartbeat-marks-no-time", "heartbeat-extension", "heartbeat-unknown-bit"}) {
        CHECK(run(input(name)).error.error == p::TapeError::UNSUPPORTED);
    }

    CHECK(run(input("heartbeat-truncated-0")).status == p::TapeStatus::FINISHED);

    for (int width = 1; width <= 20; ++width) {
        const auto result = run(input("heartbeat-truncated-" + std::to_string(width)));
        CHECK(result.status == p::TapeStatus::FAILED);
        CHECK((result.error.error == p::TapeError::MALFORMED || result.error.error == p::TapeError::UNSUPPORTED));
    }

    std::vector<std::byte> primitive;
    compact(primitive, std::numeric_limits<std::int64_t>::max());

    for (std::size_t width = 0; width < primitive.size(); ++width) {
        auto bytes = header();
        append(bytes, heartbeat(-77));
        std::vector<std::byte> body{std::byte{1}};
        append(body, std::span(primitive).first(width));
        append(bytes, message(0, body));
        const auto observed = run(bytes);
        CHECK(observed.error.error == p::TapeError::MALFORMED);
        CHECK(observed.error.errorOffset == 1);
        CHECK(observed.error.consumed == width + 1);
        CHECK(observed.timing.timestampMs == -77);
    }

    auto overlong = header();
    append(overlong, message(0, std::array{std::byte{0x80}, std::byte{1}, std::byte{0x80}, std::byte{1}}));
    CHECK(run(overlong).timing.timestampMs == 1);
    CHECK(run(overlong).status == p::TapeStatus::FINISHED);
}

TEST_CASE("zero exact and excess cumulative budgets include metadata attempts and heartbeat only streams") {
    const auto bytes = input("raw-synthetic");
    const auto baseline = run(bytes);
    using Setter = std::function<void(p::TapeLimits &, std::size_t)>;
    const std::vector<std::pair<Setter, std::size_t>> budgets{{[](auto &l, auto n) {
                                                                   l.inputBytes = n;
                                                               },
                                                               bytes.size()},
                                                              {[](auto &l, auto n) {
                                                                   l.frames = n;
                                                               },
                                                               baseline.frames.size()},
                                                              {[](auto &l, auto n) {
                                                                   l.timingEvents = n;
                                                               },
                                                               baseline.timingEvents},
                                                              {[](auto &l, auto n) {
                                                                   l.context.descriptions = n;
                                                               },
                                                               3},
                                                              {[](auto &l, auto n) {
                                                                   l.constructionUnits = n;
                                                               },
                                                               baseline.construction},
                                                              {[](auto &l, auto n) {
                                                                   l.data.processedRecords = n;
                                                               },
                                                               3},
                                                              {[](auto &l, auto n) {
                                                                   l.data.wireFields = n;
                                                               },
                                                               4},
                                                              {[](auto &l, auto n) {
                                                                   l.data.emittedRecords = n;
                                                               },
                                                               3},
                                                              {[](auto &l, auto n) {
                                                                   l.data.emittedValues = n;
                                                               },
                                                               4},
                                                              {[](auto &l, auto n) {
                                                                   l.data.ownedSymbolUnits = n;
                                                               },
                                                               9}};

    for (const auto &[set, exact] : budgets) {
        for (const auto allowance : {std::size_t{0}, exact - 1, exact, exact + 1}) {
            auto budget = limits();
            set(budget, allowance);
            const auto observed = run(bytes, budget);
            CHECK(observed.status == (allowance >= exact ? p::TapeStatus::FINISHED : p::TapeStatus::FAILED));

            if (allowance < exact) {
                CHECK(observed.error.error == p::TapeError::RESOURCE_LIMIT);
            }
        }
    }

    auto beats = header();
    append(beats, heartbeat(1));
    append(beats, heartbeat(2));
    auto budget = limits();
    budget.timingEvents = 1;
    const auto limited = run(beats, budget);
    CHECK(limited.error.stage == "timing");
    CHECK(limited.timing.timestampMs == 1);
    CHECK(limited.heartbeats.size() == 1);
    CHECK(limited.timingEvents == 1);
    budget.timingEvents = 0;
    auto bad = header();
    append(bad, message(0, std::array{std::byte{0x80}}));
    CHECK(run(bad, budget).error.error == p::TapeError::RESOURCE_LIMIT);
    auto emptyData = header();
    append(emptyData, message(5, {}));
    CHECK(run(emptyData, budget).error.stage == "timing");
    budget.timingEvents = 1;
    CHECK(run(emptyData, budget).status == p::TapeStatus::FINISHED);
    CHECK(run(emptyData, budget).timingEvents == 1);
    budget.timingEvents = std::numeric_limits<std::size_t>::max();
    CHECK(run(beats, budget).status == p::TapeStatus::FINISHED);
}

TEST_CASE("semantic projection detects field time and order mutations") {
    const auto original = run(input("raw-synthetic"));
    auto changed = original;
    changed.records[0].value.values[0] += 1;
    CHECK(projection(changed) != projection(original));
    changed = original;
    changed.records[0].timing.eventTimeSequence += 1;
    CHECK(projection(changed) != projection(original));
    changed = original;
    changed.records[0].timing.timestampMs = 1;
    CHECK(projection(changed) != projection(original));
    changed = original;
    std::swap(changed.records[0], changed.records[1]);
    CHECK(projection(changed) != projection(original));
}

TEST_CASE("late descriptions unknown skips and incomplete raw data preserve charged completed prefix") {
    auto bytes = header();
    append(bytes, heartbeat(-1001));
    append(bytes, descriptions({{7, u"S", {{u"unused", 8}, {u"v", 8}}}, {8, u"Unknown", {{u"x", 8}}}}));
    auto data = payload(8, {99});
    append(data, payload(7, {42, 9}));
    append(bytes, message(5, data));
    append(bytes, descriptions({{9, u"E", {}}}));
    append(bytes, message(5, payload(9, {})));
    const auto good = run(bytes);
    REQUIRE(good.status == p::TapeStatus::FINISHED);
    REQUIRE(good.records.size() == 2);
    CHECK(good.records[0].value.values == std::vector<std::int32_t>{9});
    CHECK(good.records[1].value.localId == 2);
    CHECK(good.records[0].timing.timestampMs == -1001);
    CHECK(good.records[1].timing.timestampMs == -1001);
    CHECK(good.processed == 3);
    CHECK(good.fields == 3);
    auto budget = limits();
    budget.data.processedRecords = 2;
    CHECK(run(bytes, budget).error.error == p::TapeError::RESOURCE_LIMIT);
    budget = limits();
    budget.data.wireFields = 2;
    CHECK(run(bytes, budget).error.error == p::TapeError::RESOURCE_LIMIT);

    const auto frameStart = bytes.size();
    data = payload(7, {1, 2});
    const auto completed = data.size();
    append(data, payload(7, {3}));
    data.push_back(std::byte{0x80});
    append(bytes, message(5, data));
    const auto truncated = run(bytes);
    CHECK(truncated.error.error == p::TapeError::MALFORMED);
    CHECK(truncated.error.stage == "data");
    CHECK(truncated.error.frameStart == frameStart);
    CHECK(truncated.error.errorOffset == completed + 4);
    REQUIRE(truncated.records.size() == 3);
    CHECK(truncated.records.back().value.values == std::vector<std::int32_t>{2});
    CHECK(truncated.records.back().timing.timestampMs == -1001);
    CHECK(truncated.processed == 5);
    CHECK(truncated.fields == 7);
    CHECK(truncated.error.consumed == data.size());

    for (auto chunk : {1U, 5U, 11U}) {
        auto observed = run(bytes, limits(), false, chunk);
        CHECK(projection(observed) == projection(truncated));
        CHECK(observed.error.accepted == truncated.error.accepted);
        CHECK(observed.error.payloadStart == truncated.error.payloadStart);
        CHECK(observed.error.errorOffset == truncated.error.errorOffset);
        CHECK(observed.error.consumed == truncated.error.consumed);
    }
}

TEST_CASE("failed descriptions preserve immutable context and timing without partial ID publication") {
    auto local = schema();
    p::TapeReader reader(local, limits(), p::TapeMode::RAW_MESSAGE_V1);
    REQUIRE(reader.feed(header()).status == p::TapeStatus::EVENT);
    REQUIRE(reader.feed(heartbeat(1234)).status == p::TapeStatus::EVENT);
    REQUIRE(reader.feed(descriptions({{7, u"S", {{u"v", 8}}}})).status == p::TapeStatus::EVENT);
    const auto *original = reader.context();
    REQUIRE(original->find(7));
    auto update = descriptions({{9, u"E", {}}, {7, u"S", {}}});
    CHECK(reader.feed(update).status == p::TapeStatus::FAILED);
    CHECK(reader.context() == original);
    CHECK_FALSE(reader.context()->find(9));
    CHECK(reader.timing().timestampMs == 1234);
    CHECK(reader.feed(heartbeat(99)).accepted == 0);
    CHECK(reader.finish() == p::TapeStatus::FAILED);

    auto unsupported = header();
    append(unsupported, descriptions({{7, u"Unknown", {{u"x", 9}}}}));
    CHECK(run(unsupported).error.error == p::TapeError::UNSUPPORTED);
    auto malformed = header();
    auto description = descriptions({{7, u"S", {{u"v", 8}}}});
    description.pop_back();
    append(malformed, description);
    CHECK(run(malformed).error.stage == "framing");
    CHECK(run(malformed).error.error == p::TapeError::MALFORMED);
}

TEST_CASE("fresh raw payload symbol state and EOF framing remain strict") {
    auto bytes = header();
    append(bytes, descriptions({{7, u"S", {{u"v", 8}}}}));
    append(bytes, message(5, payload(7, {42})));
    // Repeat is valid only after a prior symbol in this same complete data payload.
    append(bytes, message(5, std::array{std::byte{0xFF}, std::byte{7}, std::byte{1}}));
    const auto observed = run(bytes);
    CHECK(observed.error.error == p::TapeError::MALFORMED);
    REQUIRE(observed.records.size() == 1);
    CHECK(observed.records[0].value.values == std::vector<std::int32_t>{42});

    for (std::size_t size = 1; size < heartbeat(std::numeric_limits<std::int64_t>::max()).size(); ++size) {
        auto partial = header();
        auto beat = heartbeat(std::numeric_limits<std::int64_t>::max());
        append(partial, std::span(beat).first(size));
        CHECK(run(partial).error.error == p::TapeError::MALFORMED);
        CHECK_FALSE(run(partial).timing.timestampMs);
    }
}

/** Test runner requires an explicit fixture directory.
 * @param argc Argument count.
 * @param argv Executable and fixture directory, followed by doctest options.
 * @return Test failure status, or usage error 2.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    fixtures = argv[1];
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
