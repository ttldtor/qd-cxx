// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/events/quote.hpp"
#include "qd/protocol/tape_reader.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace p = org::ttldtor::qdcxx::protocol;
namespace q = org::ttldtor::qdcxx::events;
std::filesystem::path fixtures; ///< Caller-selected immutable Java artifact directory.

/** Supply finite test bounds covering the complete default adapter.
 * @return Explicit logical limits, independent of CLI defaults.
 */
p::TapeLimits limits() {
    return {{8192, 8201},
            65536,
            128,
            {16, 8, {1024, 256}, 2048},
            {{4, 32, 64, 64, 4096}, 32, {256, 64, 4096}},
            100000,
            {8192, 128, 1024, 128, 1024, 4096, {8192, 128, 128}}};
}

/** Build the explicit Quote adapter.
 * @return Owned immutable validated schema.
 */
c::LocalSchema schema() {
    auto result = q::defaultQuoteSchema(limits().context.schema);
    REQUIRE(result);

    return std::move(*result);
}

/** Read an exact artifact without rewriting it.
 * @param name Relative filename under the explicit fixture root.
 * @return Owned bytes.
 */
std::vector<std::byte> read(std::string_view name) {
    std::ifstream in(fixtures / name, std::ios::binary);
    REQUIRE(in.good());
    std::vector<char> chars((std::istreambuf_iterator<char>(in)), {});
    const auto bytes = std::as_bytes(std::span(chars));

    return {bytes.begin(), bytes.end()};
}

/** Parse a strict entire integral cell.
 * @tparam T Integral result type.
 * @param text Complete numeric cell.
 * @param base Decimal or hexadecimal radix.
 * @return Exact parsed value; invalid input fails the test.
 */
template <typename T> T number(std::string_view text, int base = 10) {
    T value{};
    const auto r = std::from_chars(text.data(), text.data() + text.size(), value, base);
    REQUIRE(r.ec == std::errc{});
    REQUIRE(r.ptr == text.data() + text.size());

    return value;
}

/** Split a complete row, preserving empty cells.
 * @param line TSV row without newline.
 * @return Ordered exact cells.
 */
std::vector<std::string> cells(const std::string &line) {
    std::vector<std::string> result;
    std::size_t start = 0;

    for (;;) {
        auto end = line.find('\t', start);
        result.push_back(line.substr(start, end - start));

        if (end == std::string::npos) {
            return result;
        }

        start = end + 1;
    }
}

/** Decode exact UTF-16 hex, without Unicode normalization.
 * @param text Lowercase four-digit units or the empty sentinel.
 * @return Owned code units.
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

/** Compare binary64 exactly, with explicit NaN classification.
 * @param actual Mapped binary64 result.
 * @param expected Observed Java canonical bits.
 */
void sameDouble(double actual, const std::string &expected) {
    REQUIRE(expected.size() == 16);
    REQUIRE(expected.find_first_not_of("0123456789abcdef") == std::string::npos);
    const auto bits = number<std::uint64_t>(expected, 16);

    if (std::isnan(std::bit_cast<double>(bits))) {
        CHECK(std::isnan(actual));
    }

    CHECK(std::bit_cast<std::uint64_t>(actual) == bits);
}

/** Complete owning tape observation after reader destruction. */
struct Result {
    p::TapeStatus status;                     ///< Explicit finish result.
    p::TapeDiagnostic error;                  ///< Exact first-error positions and reason.
    std::vector<p::StreamDataRecord> records; ///< Completed prefix only.
    std::size_t processed;                    ///< Charged attempts.
    std::size_t fields;                       ///< Charged fields.
    std::size_t construction;                 ///< Charged candidate work.
};

/** Consume a bounded schedule, keeping all output independently owned.
 * @param bytes Borrowed input during feeds only.
 * @param budget Explicit finite limits.
 * @param chunk Maximum feed size, positive.
 * @return Owned terminal observation.
 */
Result run(std::span<const std::byte> bytes, p::TapeLimits budget, std::size_t chunk = 65536) {
    auto local = schema();
    p::TapeReader reader(local, budget);
    Result result{};

    while (!bytes.empty()) {
        auto next = reader.feed(bytes.first(std::min(chunk, bytes.size())));
        bytes = bytes.subspan(next.accepted);

        for (auto &record : next.records) {
            result.records.push_back(std::move(record));
        }

        if (next.status == p::TapeStatus::FAILED) {
            break;
        }

        REQUIRE(next.accepted > 0);
        CHECK(reader.feed({}).status == p::TapeStatus::NEED_INPUT);
    }

    result.status = reader.finish();
    result.error = reader.diagnostic();
    result.processed = reader.processedRecords();
    result.fields = reader.wireFields();
    result.construction = reader.constructionUnits();
    CHECK(reader.finish() == result.status);
    CHECK(reader.feed(bytes).accepted == 0);

    return result;
}

/** Compare every ordered production row with decoded and mapped values.
 * @param name Case filename stem.
 * @param actual Owned complete tape observation.
 */
void compare(std::string_view name, const Result &actual) {
    REQUIRE(actual.status == p::TapeStatus::FINISHED);
    std::ifstream in(fixtures / (std::string(name) + ".tsv"), std::ios::binary);
    REQUIRE(in.good());
    std::string line;
    auto next = [&]() {
        REQUIRE(static_cast<bool>(std::getline(in, line)));
        REQUIRE_FALSE(line.empty());
        REQUIRE(line.back() != '\r');

        return cells(line);
    };
    CHECK(next() == std::vector<std::string>{"case", std::string(name)});

    for (std::size_t i = 0; i < actual.records.size(); ++i) {
        const auto &record = actual.records[i];
        const auto header = next();
        REQUIRE(header.size() == 6);
        CHECK(header[0] == "record");
        CHECK(number<std::size_t>(header[1]) == i);
        CHECK(number<std::int32_t>(header[2]) == record.localId);
        CHECK(units(header[3]) == record.symbol.units);
        CHECK(header[4] == "0");
        CHECK(header[5] == "0");
        CHECK(record.symbol.eventFlags == 0);
        CHECK(record.values.empty());
        REQUIRE(record.typedValues.size() == 8);

        for (std::size_t j = 0; j < 8; ++j) {
            const auto row = next();
            REQUIRE(row.size() == 7);
            CHECK(row[0] == "field");
            CHECK(number<std::size_t>(row[1]) == i);
            CHECK(number<std::size_t>(row[2]) == j);
            const auto &value = record.typedValues[j];
            CHECK(number<std::int32_t>(row[3]) == static_cast<std::int32_t>(value.type));
            CHECK(number<std::int64_t>(row[4]) == value.raw);

            if (value.type == c::SerialFieldType::WIDE_DECIMAL) {
                const c::WideDecimal decimal{value.raw};
                CHECK(decimal.toString() == row[5]);
                sameDouble(decimal.toDouble(), row[6]);
            } else {
                CHECK(row[5] == "-");
                CHECK(row[6] == "-");
            }
        }

        auto quote = q::mapQuote(record.symbol.units, record.typedValues, record.symbol.units.size());
        REQUIRE(quote);
        const auto row = next();
        REQUIRE(row.size() == 15);
        CHECK(row[0] == "mapped");
        CHECK(number<std::size_t>(row[1]) == i);
        CHECK(units(row[2]) == quote->symbol);
        CHECK(number<std::int64_t>(row[3]) == quote->bidTimeMs);
        CHECK(number<unsigned>(row[4]) == quote->bidExchangeCode);
        sameDouble(quote->bidPrice.toDouble(), row[5]);
        sameDouble(quote->bidSize.toDouble(), row[6]);
        CHECK(number<std::int64_t>(row[7]) == quote->askTimeMs);
        CHECK(number<unsigned>(row[8]) == quote->askExchangeCode);
        sameDouble(quote->askPrice.toDouble(), row[9]);
        sameDouble(quote->askSize.toDouble(), row[10]);
        CHECK(number<std::int64_t>(row[11]) == quote->timeMs);
        CHECK(number<std::int32_t>(row[12]) == quote->sequence);
        CHECK(number<std::int32_t>(row[13]) == quote->timeNanoPart);
        CHECK(number<std::int64_t>(row[14]) == quote->eventTime);
    }

    CHECK(next() == std::vector<std::string>{"end", std::to_string(actual.records.size()), "0"});
    CHECK(next() == std::vector<std::string>{"complete", std::string(name)});
    CHECK_FALSE(static_cast<bool>(std::getline(in, line)));
}

/** Append canonical compact bytes to a designed test payload.
 * @param out Owned destination.
 * @param value Signed raw compact value.
 */
void compact(std::vector<std::byte> &out, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    const auto written = e::writeCompactLong(value, buffer);
    REQUIRE(written.status == e::Status::OK);
    out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(written.written));
}

/** Append a small designed UTFString using the production primitive.
 * @param out Destination test bytes.
 * @param value Borrowed name within the fixed fixture bound.
 */
void text(std::vector<std::byte> &out, std::u16string_view value) {
    std::array<std::byte, 256> buffer{};
    const auto written = e::writeUtfString(value, buffer, {200, 100});
    REQUIRE(written.status == e::Status::OK);
    out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(written.written));
}

/** Encode a complete designed message envelope.
 * @param type Message identity.
 * @param body Complete payload.
 * @return Owned canonical message bytes.
 */
std::vector<std::byte> message(std::int32_t type, std::span<const std::byte> body) {
    std::vector<std::byte> content;
    compact(content, type);
    content.insert(content.end(), body.begin(), body.end());
    std::vector<std::byte> result;
    compact(result, static_cast<std::int64_t>(content.size()));
    result.insert(result.end(), content.begin(), content.end());

    return result;
}

/** Encode one bounded diagnostic description update, including deliberate duplicate IDs.
 * @param records Complete ordered descriptions.
 * @return Complete type-2 envelope.
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

/** Build a simple message-local empty-symbol record.
 * @param id Sender ID with one fresh symbol.
 * @param values Compact field values; UTF_CHAR cases append literal bytes separately.
 * @return Complete designed payload.
 */
std::vector<std::byte> payload(std::int32_t id, std::initializer_list<std::int64_t> values) {
    // 0xfc followed by zero byte length encodes a fresh empty UTF symbol.
    std::vector<std::byte> out{std::byte{0xfc}, std::byte{0}};
    compact(out, id);

    for (auto value : values) {
        compact(out, value);
    }

    return out;
}

/** Construct an independently owned exact Quote decoding context.
 * @param descriptions Complete designed descriptions.
 * @return Validated immutable context after the source schema has been destroyed.
 */
p::StreamDataContext context(std::vector<p::WireRecordDescription> descriptions) {
    auto result = p::StreamDataContext::create(schema(), descriptions, limits().context);
    REQUIRE(result);

    return std::move(*result);
}
} // namespace

TEST_CASE("all unchanged Java production tapes match every raw and mapped row") {
    std::size_t count = 0;

    for (const auto &entry : std::filesystem::directory_iterator(fixtures)) {
        if (entry.path().extension() != ".qtp") {
            continue;
        }

        const auto name = entry.path().stem().string();
        INFO(name);
        auto bytes = read(entry.path().filename().string());
        const auto whole = run(bytes, limits());
        compare(name, whole);
        compare(name, run(bytes, limits(), 1));
        compare(name, run(bytes, limits(), 7));
        ++count;
    }

    CHECK(count == 41);
}

TEST_CASE("typed matching skips repeats missing values and exact primitive positions") {
    auto ctx = context({{17, u"Quote", {{u"BidPrice", 120}, {u"BidPrice", 120}, {u"ignored", 2}, {u"AskTime", 56}}},
                        {18, u"Unknown", {{u"wide", 120}, {u"char", 2}, {u"time", 56}, {u"int", 8}}}});
    auto bytes = payload(17, {384, 640});
    bytes.insert(bytes.end(), {std::byte{0xf0}, std::byte{0x90}, std::byte{0x80}, std::byte{0x80}});
    compact(bytes, std::numeric_limits<std::int64_t>::max());
    const auto result = p::decodeStreamData(bytes, ctx, limits().data);
    REQUIRE(result.status == p::StreamDataStatus::OK);
    REQUIRE(result.records.size() == 1);
    CHECK(result.wireFields == 4);
    CHECK(result.records[0].typedValues[2].raw == 640);
    CHECK(result.records[0].typedValues[4].raw == -1);
    CHECK(result.records[0].typedValues[6].raw == 0);
    CHECK(std::isnan(c::WideDecimal{result.records[0].typedValues[6].raw}.toDouble()));
    CHECK(result.consumed == bytes.size());

    for (std::size_t size = 0; size < bytes.size(); ++size) {
        const auto prefix = p::decodeStreamData(std::span(bytes).first(size), ctx, limits().data);
        CHECK(prefix.records.empty());
        CHECK(prefix.consumed <= size);

        if (size != 0) {
            CHECK(prefix.status == p::StreamDataStatus::TRUNCATED_PAYLOAD);
        }
    }

    auto unknown = payload(18, {std::numeric_limits<std::int64_t>::min()});
    unknown.push_back(std::byte{0x7f});
    compact(unknown, std::numeric_limits<std::int32_t>::min());
    compact(unknown, 42);
    auto skipped = p::decodeStreamData(unknown, ctx, limits().data);
    CHECK(skipped.status == p::StreamDataStatus::OK);
    CHECK(skipped.records.empty());
    CHECK(skipped.wireFields == 4);
    CHECK(skipped.consumed == unknown.size());
    auto chars = context({{0, u"Quote", {{u"BidExchangeCode", 2}}}});
    auto invalid = payload(0, {});
    const auto fieldStart = invalid.size();
    invalid.insert(invalid.end(), {std::byte{0xe0}, std::byte{0}, std::byte{0x80}});
    const auto rejected = p::decodeStreamData(invalid, chars, limits().data);
    CHECK(rejected.status == p::StreamDataStatus::INVALID_FIELD);
    CHECK(rejected.stage == p::StreamDataStage::FIELD);
    CHECK(rejected.errorOffset == fieldStart);
    CHECK(rejected.consumed == fieldStart + 3);
    auto prefixBytes = bytes;
    auto badId = payload(999, {});
    prefixBytes.insert(prefixBytes.end(), badId.begin(), badId.end());
    const auto prefix = p::decodeStreamData(prefixBytes, ctx, limits().data);
    CHECK(prefix.status == p::StreamDataStatus::UNDESCRIBED_ID);
    CHECK(prefix.records.size() == 1);
}

TEST_CASE("explicit profiles reject full type mismatches including supported primitive relatives") {
    auto local = schema();
    CHECK(local.profile() == c::LocalProfile::QUOTE_FIELDS_V1);
    CHECK(local.records()[0].fields.size() == 8);
    CHECK(local.records()[0].lookups.size() == 24);
    CHECK_FALSE(c::LocalSchema::create(local.records(), limits().context.schema));
    CHECK_FALSE(c::LocalSchema::create({}, limits().context.schema, static_cast<c::LocalProfile>(99)));

    for (auto type : {0, 1, 3, 24, 72, 104, 255}) {
        INFO(type);
        auto r = p::bindRecord({9, u"Unknown", {{u"x", type}}}, local, limits().context.bindings);
        REQUIRE_FALSE(r);
        CHECK(r.error() == p::BindingError::UNSUPPORTED_PROFILE);
    }

    for (auto type : {2, 8, 56}) {
        auto r = p::bindRecord({9, u"Quote", {{u"BidPrice", type}}}, local, limits().context.bindings);
        REQUIRE_FALSE(r);
        CHECK(r.error() == p::BindingError::UNSUPPORTED_PROFILE);
    }

    auto duplicate = p::StreamDataContext::create(
        local, std::vector<p::WireRecordDescription>{{0, u"Quote", {}}, {0, u"Quote", {}}}, limits().context);
    CHECK_FALSE(duplicate);
    auto budget = limits().context;
    budget.schema.fields = 7;
    CHECK_FALSE(p::StreamDataContext::create(local, {}, budget));
}

TEST_CASE("typed value budgets ownership and mapping validation are explicit") {
    auto ctx = context({{0, u"Quote", {{u"BidPrice", 120}}}});
    auto bytes = payload(0, {384});
    auto budget = limits().data;
    budget.payloadBytes = bytes.size();
    budget.processedRecords = 1;
    budget.wireFields = 1;
    budget.emittedRecords = 1;
    budget.emittedValues = 8;
    budget.ownedSymbolUnits = 0;
    const auto good = p::decodeStreamData(bytes, ctx, budget);
    REQUIRE(good.status == p::StreamDataStatus::OK);
    REQUIRE(good.records.size() == 1);

    for (auto bound : {&p::StreamDataLimits::processedRecords, &p::StreamDataLimits::wireFields,
                       &p::StreamDataLimits::emittedRecords, &p::StreamDataLimits::emittedValues}) {
        auto shortBudget = budget;
        shortBudget.*bound = 0;
        CHECK(p::decodeStreamData(bytes, ctx, shortBudget).status == p::StreamDataStatus::RESOURCE_LIMIT);
    }

    budget.emittedValues = 7;
    CHECK(p::decodeStreamData(bytes, ctx, budget).status == p::StreamDataStatus::RESOURCE_LIMIT);
    auto fields = good.records[0].typedValues;
    auto mapped = q::mapQuote(u"IBM", fields, 3);
    REQUIRE(mapped);
    fields.clear();
    bytes.clear();
    CHECK(mapped->symbol == u"IBM");
    CHECK(mapped->bidPrice.raw == 384);
    CHECK(q::mapQuote(u"IBM", good.records[0].typedValues, 2).error() == q::QuoteError::RESOURCE_LIMIT);
    CHECK(q::mapQuote(u"", {}, 0).error() == q::QuoteError::INVALID_SHAPE);

    for (std::size_t i = 0; i < good.records[0].typedValues.size(); ++i) {
        CAPTURE(i);
        fields = good.records[0].typedValues;
        // Ordinary type 8 is supported, but differs from every default Quote slot's exact type.
        fields[i].type = c::SerialFieldType::COMPACT_INT;
        const auto wrongType = q::mapQuote(u"", fields, 0);
        REQUIRE_FALSE(wrongType);
        CHECK(wrongType.error() == q::QuoteError::INVALID_SHAPE);
    }

    fields = good.records[0].typedValues;
    fields[1].raw = 0x110000;
    CHECK(q::mapQuote(u"", fields, 0).error() == q::QuoteError::INVALID_SHAPE);
    fields[1].raw = 0x10ffff;
    fields[0].raw = std::numeric_limits<std::int64_t>::max();
    CHECK(q::mapQuote(u"", fields, 0).error() == q::QuoteError::INVALID_SHAPE);
}

TEST_CASE("unchanged Quote tape EOF budgets prefix and atomic description updates") {
    auto bytes = read("default-quote.qtp");
    auto baseline = run(bytes, limits());
    REQUIRE(baseline.status == p::TapeStatus::FINISHED);
    REQUIRE(baseline.records.size() == 5);
    auto exact = limits();
    exact.inputBytes = bytes.size();
    exact.data.processedRecords = 5;
    exact.data.wireFields = 40;
    exact.data.emittedValues = 40;
    exact.constructionUnits = baseline.construction;
    compare("default-quote", run(bytes, exact));
    --exact.data.emittedValues;
    auto shortValues = run(bytes, exact);
    CHECK(shortValues.error.error == p::TapeError::RESOURCE_LIMIT);
    CHECK(shortValues.records.size() == 4);
    exact = limits();
    exact.constructionUnits = baseline.construction - 1;
    CHECK(run(bytes, exact).error.error == p::TapeError::RESOURCE_LIMIT);

    for (std::size_t n = 0; n < bytes.size(); ++n) {
        auto result = run(std::span(bytes).first(n), limits());
        CHECK(result.records.size() <= 5);
        CHECK(result.error.accepted <= n);
    }

    // Recover complete original frame bytes using the public framing API; no header stripping in acceptance.
    p::FrameReader frames(limits().frame);
    std::vector<p::Frame> list;
    auto remaining = std::span<const std::byte>(bytes);

    while (!remaining.empty()) {
        auto result = frames.feed(remaining);
        remaining = remaining.subspan(result.accepted);
        REQUIRE(result.status != p::FrameStatus::FAILED);

        if (result.frame) {
            list.push_back(std::move(*result.frame));
        }
    }

    REQUIRE(list.size() >= 3);
    auto local = schema();
    p::TapeReader reader(local, limits());
    auto consumeFrame = [&](const p::Frame &frame) {
        return reader.feed(
            std::span<const std::byte>(bytes).subspan(static_cast<std::size_t>(frame.startOffset),
                                                      static_cast<std::size_t>(frame.endOffset - frame.startOffset)));
    };
    REQUIRE(consumeFrame(list[0]).status == p::TapeStatus::EVENT);
    REQUIRE(consumeFrame(list[1]).status == p::TapeStatus::EVENT);
    const auto *old = reader.context();
    REQUIRE(old->find(0) != nullptr);
    CHECK(consumeFrame(list[1]).status == p::TapeStatus::FAILED);
    CHECK(reader.context() == old);
    CHECK(old->find(0) != nullptr);
    CHECK(reader.feed(bytes).accepted == 0);
}

TEST_CASE("typed late descriptions cumulative output budgets and per-message symbol reset") {
    const auto original = read("default-quote.qtp");
    p::FrameReader framer(limits().frame);
    const auto first = framer.feed(original);
    REQUIRE(first.frame);
    const auto initial = std::span(original).first(first.accepted);
    auto local = schema();
    p::TapeReader reader(local, limits());
    REQUIRE(reader.feed(initial).status == p::TapeStatus::EVENT);
    REQUIRE(reader.feed(descriptions({{7, u"Quote", {{u"BidPrice", 120}}}})).status == p::TapeStatus::EVENT);
    auto firstData = reader.feed(message(15, payload(7, {384})));
    REQUIRE(firstData.status == p::TapeStatus::EVENT);
    REQUIRE(firstData.records.size() == 1);
    REQUIRE(reader.feed(descriptions({{9, u"Quote", {{u"AskSize", 120}}}})).status == p::TapeStatus::EVENT);
    auto secondData = reader.feed(message(15, payload(9, {640})));
    REQUIRE(secondData.status == p::TapeStatus::EVENT);
    REQUIRE(secondData.records.size() == 1);
    CHECK(secondData.records[0].typedValues[7].raw == 640);
    CHECK(firstData.records[0].typedValues[2].raw == 384);
    const auto *old = reader.context();
    CHECK(reader.feed(descriptions({{11, u"Quote", {}}, {7, u"Quote", {}}})).status == p::TapeStatus::FAILED);
    CHECK(reader.context() == old);
    CHECK(old->find(11) == nullptr);
    CHECK(old->find(9) != nullptr);
    auto budget = limits();
    budget.data.emittedValues = 15;
    p::TapeReader bounded(local, budget);
    REQUIRE(bounded.feed(initial).status == p::TapeStatus::EVENT);
    REQUIRE(bounded.feed(descriptions({{0, u"Quote", {}}})).status == p::TapeStatus::EVENT);
    REQUIRE(bounded.feed(message(15, payload(0, {}))).records.size() == 1);
    CHECK(bounded.feed(message(15, payload(0, {}))).status == p::TapeStatus::FAILED);
    CHECK(bounded.diagnostic().error == p::TapeError::RESOURCE_LIMIT);
    auto ctx = context({{0, u"Quote", {}}});
    REQUIRE(p::decodeStreamData(payload(0, {}), ctx, limits().data).status == p::StreamDataStatus::OK);
    // 0xff means repeat: a new message has no preceding symbol to repeat.
    const std::vector<std::byte> repeat{std::byte{0xff}, std::byte{0}};
    CHECK(p::decodeStreamData(repeat, ctx, limits().data).status == p::StreamDataStatus::INVALID_SYMBOL);
    CHECK(p::decodeStreamData(payload(0, {}), ctx, limits().data).status == p::StreamDataStatus::OK);
}

TEST_CASE("prepared Quote selects reordered explicit aliases and independent optional time fields") {
    c::LocalRecord record{0,
                          u"Configured",
                          false,
                          {{8}, {120}, {56}, {72}, {8}, {120}, {120}, {40}},
                          {{u"AskSize", 0},
                           {u"BidPrice", 1},
                           {u"BidTime", 2},
                           {u"Sequence", 3},
                           {u"TimeNanoPart", 4},
                           {u"AskPrice", 5},
                           {u"BidSize", 6},
                           {u"Extra", 7}}};
    auto local =
        c::LocalSchema::create(std::span(&record, 1), {1, 8, 8, 64, 1024}, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(local);
    auto prepared = q::PreparedQuote::prepare(*local, 0, {16, 32, 16, 32, 32, 64, 2048, 100000});
    REQUIRE(prepared);
    std::vector<c::FieldValue> fields{{c::SerialFieldType::COMPACT_INT, 0},
                                      {c::SerialFieldType::WIDE_DECIMAL, 32002},
                                      {c::SerialFieldType::TIME_SECONDS, -3},
                                      {c::SerialFieldType::SEQUENCE, -1},
                                      {c::SerialFieldType::COMPACT_INT, -2147483648LL},
                                      {c::SerialFieldType::WIDE_DECIMAL, 256},
                                      {c::SerialFieldType::WIDE_DECIMAL, -256},
                                      {c::SerialFieldType::SHORT_STRING, 42}};
    auto result = prepared->map(u"IBM", {local->recordContext(0), fields}, 3, 123);
    REQUIRE(result);
    CHECK(result->bidTimeMs == -3000);
    CHECK(result->askTimeMs == 0);
    CHECK(result->timeMs == 1023);
    CHECK(result->sequence == 4194303);
    CHECK(result->timeNanoPart == -2147483648LL);
    CHECK(result->eventTime == 123);
    CHECK(result->bidExchangeCode == 0);
    CHECK(result->askSize.toDouble() == 0.0);
    CHECK(result->bidPrice.raw == 32002);
    CHECK(result->bidSize.kind() == c::DecimalKind::NEGATIVE_INFINITY);
    CHECK(result->askPrice.kind() == c::DecimalKind::POSITIVE_INFINITY);
    CHECK(fields[7].raw == 42);
    fields[7].units = u"bad";
    CHECK(prepared->map(u"IBM", {local->recordContext(0), fields}, 0).error() == q::MappingError::INVALID_VALUES);
    fields[7].units.reset();
    CHECK(prepared->map(u"IBM", {local->recordContext(0), fields}, 2).error() == q::MappingError::RESOURCE_LIMIT);
}

TEST_CASE("prepared Quote missing local required fields wrong types contexts and ambiguous keys reject") {
    auto local = schema();
    auto prepared = q::PreparedQuote::prepare(local, 0, {16, 32, 16, 32, 32, 64, 2048, 100000});
    REQUIRE(prepared);
    std::vector<c::FieldValue> fields;

    for (const auto type : q::QUOTE_FIELD_TYPES) {
        fields.push_back({type, 0});
    }

    auto other = schema();
    CHECK(prepared->map(u"", {other.recordContext(0), fields}, 0).error() == q::MappingError::WRONG_CONTEXT);
    REQUIRE(prepared->map(u"", {local.recordContext(0), fields}, 0));
    auto record = local.records()[0];
    record.lookups.erase(std::remove_if(record.lookups.begin(), record.lookups.end(),
                                        [](const auto &lookup) {
                                            return lookup.fieldId == 2;
                                        }),
                         record.lookups.end());
    record.lookups.push_back({u"Other", 2});
    auto missing =
        c::LocalSchema::create(std::span(&record, 1), limits().context.schema, c::LocalProfile::QUOTE_FIELDS_V1);
    REQUIRE(missing);
    CHECK(q::PreparedQuote::prepare(*missing, 0, {16, 32, 16, 32, 32, 64, 2048, 100000}).error() ==
          q::MappingError::MISSING_REQUIRED);
    record = local.records()[0];
    record.lookups.push_back({u"Sequence", 2});
    auto wrongType =
        c::LocalSchema::create(std::span(&record, 1), limits().context.schema, c::LocalProfile::QUOTE_FIELDS_V1);
    REQUIRE(wrongType);
    CHECK(q::PreparedQuote::prepare(*wrongType, 0, {16, 32, 16, 32, 32, 64, 2048, 100000}).error() ==
          q::MappingError::INCOMPATIBLE_TYPE);
    record = local.records()[0];

    for (auto &lookup : record.lookups) {
        if (lookup.name == u"BidPrice") {
            lookup.fieldId = 6;
        }
    }

    auto ambiguous =
        c::LocalSchema::create(std::span(&record, 1), limits().context.schema, c::LocalProfile::QUOTE_FIELDS_V1);
    REQUIRE(ambiguous);
    CHECK(q::PreparedQuote::prepare(*ambiguous, 0, {16, 32, 16, 32, 32, 64, 2048, 100000}).error() ==
          q::MappingError::AMBIGUOUS_FIELD);
}

TEST_CASE("prepared Quote measured packed sequence boundaries preserve signed nano values") {
    auto record = schema().records()[0];
    record.fields.push_back({72});
    record.fields.push_back({8});
    record.lookups.push_back({u"Sequence", 8});
    record.lookups.push_back({u"TimeNanoPart", 9});
    auto local =
        c::LocalSchema::create(std::span(&record, 1), limits().context.schema, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(local);
    auto prepared = q::PreparedQuote::prepare(*local, 0, {16, 32, 16, 32, 32, 64, 2048, 100000});
    REQUIRE(prepared);
    std::vector<c::FieldValue> fields;

    for (const auto &field : record.fields) {
        fields.push_back({static_cast<c::SerialFieldType>(field.serialTypeId), 0});
    }

    fields[0].raw = -3;
    fields[4].raw = -2;
    const std::array<std::int32_t, 7> raw{-2147483647 - 1, -1, 0, 1, 4194303, 4194304, 2147483647};
    const std::array<std::int32_t, 7> sequence{0, 4194303, 0, 1, 4194303, 0, 4194303};
    const std::array<std::int64_t, 7> time{-1488, -977, -2000, -2000, -2000, -1999, -1489};

    for (std::size_t i = 0; i < raw.size(); ++i) {
        CAPTURE(i);
        fields[8].raw = raw[i];
        fields[9].raw = raw[i];
        auto result = prepared->map(u"", {local->recordContext(0), fields}, 0);
        REQUIRE(result);
        CHECK(result->timeMs == time[i]);
        CHECK(result->sequence == sequence[i]);
        CHECK(result->timeNanoPart == raw[i]);
        CHECK(result->eventTime == 0);
    }
}

/** Run conformance against a caller-selected immutable fixture directory.
 * @param argc Argument count including executable.
 * @param argv Executable and fixture directory.
 * @return Test process status, or usage failure.
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }

    fixtures = argv[1];
    doctest::Context context;

    return context.run();
}
