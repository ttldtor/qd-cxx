// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "indexed_test_support.hpp"

#include <bit>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>

namespace {
std::ptrdiff_t allocationFailAfter = -1; ///< Ordinary allocation injection confined to this process.
}

/** Allocate test-process storage with one optional failure checkpoint.
 * @param size Requested bytes; zero still obtains deletable storage.
 * @return Storage or bad_alloc at the injected checkpoint.
 */
void *operator new(std::size_t size) {
    if (allocationFailAfter == 0) {
        throw std::bad_alloc();
    }

    if (allocationFailAfter > 0) {
        --allocationFailAfter;
    }

    if (auto *storage = std::malloc(size == 0 ? 1 : size)) {
        return storage;
    }

    throw std::bad_alloc();
}

/** Allocate an array using the same process-local failure seam.
 * @param size Requested bytes.
 * @return Storage or bad_alloc.
 */
void *operator new[](std::size_t size) {
    return ::operator new(size);
}

/** Release ordinary injected storage.
 * @param storage Previously allocated address or null.
 */
void operator delete(void *storage) noexcept {
    std::free(storage);
}

/** Release array storage.
 * @param storage Previously allocated address or null.
 */
void operator delete[](void *storage) noexcept {
    std::free(storage);
}

/** Release sized storage using the matching ordinary allocator.
 * @param storage Previously allocated address or null.
 * @param size Compiler-supplied storage size, not needed by free.
 */
void operator delete(void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

/** Release sized array storage.
 * @param storage Previously allocated address or null.
 * @param size Compiler-supplied storage size.
 */
void operator delete[](void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

namespace {
using namespace indexed_test;
constexpr auto POLICY = p::DataEventPolicy::BINARY_CATALOG_V1;
constexpr std::int64_t TIME = 0x1234567889abcdefLL; ///< Mixed high/low words from the production corpus.
std::filesystem::path fixtureRoot;                  ///< Explicit governed production capture directory.

/** Build one flags/NONE payload using real compact primitives.
 * @param flags Exact projected low32 flags.
 * @param time Raw removal value or indexed ordinary words.
 * @param sender Sparse described identity.
 * @return Owning data payload, excluding the envelope.
 */
std::vector<std::byte> payload(std::int32_t flags, std::int64_t time = TIME, std::int32_t sender = 0) {
    std::vector<std::byte> result{std::byte{0xf9}};
    compact(result, flags);
    result.push_back(std::byte{0xfe});
    compact(result, sender);

    if ((flags & 2) != 0) {
        compact(result, time);
    } else {
        const auto bits = std::bit_cast<std::uint64_t>(time);
        compact(result, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32)));
        compact(result, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits)));
        compact(result, 913);
    }

    return result;
}

/** Own a validated context after all temporary schemas/descriptions are destroyed.
 * @param record Local logical schema with explicit layout.
 * @param wire Source declaration, defaulting to the reordered pair source.
 * @return Independent immutable context.
 */
p::StreamDataContext context(c::LocalRecord record = pairRecord(), p::WireRecordDescription wire = description()) {
    const std::array records{record};
    auto schema = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const std::array declarations{wire};
    auto result = p::StreamDataContext::create(*schema, declarations, limits().context);
    REQUIRE(result);

    return std::move(*result);
}

/** Check signed words at deliberately reordered logical positions.
 * @param record Completed owned record.
 * @param time Expected exact raw time.
 */
void checkPair(const p::StreamDataRecord &record, std::int64_t time) {
    const auto bits = std::bit_cast<std::uint64_t>(time);
    REQUIRE(record.typedValues.size() >= 3);
    CHECK(record.typedValues[1].raw == std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32)));
    CHECK(record.typedValues[0].raw == std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits)));
}

/** Assemble the selected descriptor while preserving opt absence and empty text.
 * @param option Exact option text, or absent.
 * @param time Exact timing declaration, normally NONE.
 * @return Owning initial descriptor envelope.
 */
std::vector<std::byte> descriptor(std::optional<std::u16string_view> option, std::u16string_view time = u"NONE") {
    std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    compact(body, option ? 3 : 2);
    text(body, u"type");
    text(body, u"tape");

    if (option) {
        text(body, u"opt");
        text(body, *option);
    }

    text(body, u"time");
    text(body, time);
    compact(body, 1);
    compact(body, 15);
    text(body, u"STREAM_DATA");
    compact(body, 0);
    compact(body, 0);

    return frame(1, body);
}

/** Read a finite production artifact without changing its bytes.
 * @param name Exact fixture file name.
 * @return Owned binary contents.
 */
std::vector<std::byte> fixture(std::string_view name) {
    std::ifstream input(fixtureRoot / name, std::ios::binary);
    REQUIRE(input.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(input)), {});
    const auto bytes = std::as_bytes(std::span(chars));

    return {bytes.begin(), bytes.end()};
}

/** Split one finite canonical observation, preserving interior empty cells.
 * @param row Borrowed line.
 * @param delimiter Explicit field separator.
 * @return Owned ordered cells.
 */
std::vector<std::string> cells(std::string_view row, char delimiter = '\t') {
    std::vector<std::string> result;
    std::size_t start = 0;

    for (;;) {
        const auto end = row.find(delimiter, start);
        result.emplace_back(row.substr(start, end == row.npos ? end : end - start));

        if (end == row.npos) {
            return result;
        }

        start = end + 1;
    }
}

/** Convert the captured finite ASCII schema keys without inventing source aliases.
 * @param value Source metadata key.
 * @return Exact owned UTF-16.
 */
std::u16string sourceKey(std::string_view value) {
    REQUIRE(std::all_of(value.begin(), value.end(), [](unsigned char unit) {
        return unit < 128;
    }));

    return {value.begin(), value.end()};
}

/** Supply explicit indexing and the source-proven logical schema, with no name inference.
 * @param standard Select the full unchanged TimeAndSale schema instead of the synthetic pair.
 * @return Validated schema owning every logical field and source lookup.
 */
c::LocalSchema productionSchema(bool standard = false) {
    c::LocalRecord record{
        0, u"Pair", true, {{8}, {8}, {8}}, {{u"F0", 0}, {u"F1", 1}, {u"F2", 2}}, c::IndexedTimePairInt32{0, 1}};

    if (standard) {
        record = {0, u"TimeAndSale", true, {}, {}, c::IndexedTimePairInt32{0, 1}};
        std::ifstream input(fixtureRoot.parent_path() / "indexed/observations.tsv");
        REQUIRE(input.good());
        std::string line;

        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            const auto row = cells(line);

            if (row.size() == 5 && row[0] == "source-map" && row[1] == "TimeAndSale") {
                REQUIRE(std::stoull(row[3]) == record.fields.size());
                record.fields.push_back({std::stoi(row[4])});
            } else if (row.size() == 4 && row[0] == "source-lookup" && row[1] == "TimeAndSale" &&
                       row[3].starts_with("field:")) {
                record.lookups.push_back({sourceKey(row[2]), static_cast<std::size_t>(std::stoull(row[3].substr(6)))});
            }
        }

        REQUIRE(record.fields.size() == 10);
    }

    const std::array records{record};
    auto result = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Decode genuine captured STREAM messages; container policy is tested separately.
 * @param bytes Whole producer artifact including ordinary descriptions and optional header/footer.
 * @param local Explicit validated source-proven local schema.
 * @return Owned completed values from all type15 messages.
 */
std::vector<p::StreamDataRecord> productionRecords(std::span<const std::byte> bytes, const c::LocalSchema &local) {
    std::optional<p::StreamDataContext> bindings;
    std::vector<p::StreamDataRecord> records;
    std::size_t position = 0;

    while (position < bytes.size()) {
        const auto length = e::readCompactLong(bytes.subspan(position));
        REQUIRE(length.status == e::Status::OK);
        position += length.consumed;
        REQUIRE(length.value >= 0);
        REQUIRE(static_cast<std::size_t>(length.value) <= bytes.size() - position);
        const auto message = bytes.subspan(position, static_cast<std::size_t>(length.value));
        position += message.size();

        if (message.empty()) {
            continue;
        }

        const auto type = e::readCompactInt(message);
        REQUIRE(type.status == e::Status::OK);
        const auto body = message.subspan(type.consumed);

        if (type.value == 2) {
            const auto descriptions = p::parseDescribeRecords(body, {65536, 128, 1024, 10000, {4096, 1024}, 65536});
            REQUIRE(descriptions.status == p::DescriptionStatus::OK);
            auto next = p::StreamDataContext::create(local, descriptions.descriptions, limits().context);
            REQUIRE(next);
            bindings = std::move(*next);
        } else if (type.value == 15) {
            REQUIRE(bindings);
            auto result = p::decodeStreamDataWithEventPolicy(body, *bindings, limits().data, POLICY);
            REQUIRE(result.status == p::StreamDataStatus::OK);
            records.insert(records.end(), std::make_move_iterator(result.records.begin()),
                           std::make_move_iterator(result.records.end()));
        } else {
            REQUIRE((type.value == 1 || type.value == 0));
        }
    }

    return records;
}

/** Compare one STREAM projection against every actual Java cursor value, including long slots.
 * @param label Exact production observation label.
 * @param records Native owned wire projection.
 */
void compareProduction(std::string_view label, std::span<const p::StreamDataRecord> records) {
    std::ifstream input(fixtureRoot / "observations.tsv");
    REQUIRE(input.good());
    std::size_t index = 0;
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const auto row = cells(line);

        if (row.size() != 4 || row[0] != "callback" || row[1] != label || row[2] != "15") {
            continue;
        }

        REQUIRE(index < records.size());
        const auto &record = records[index++];
        const auto actual = cells(row[3], '|');
        REQUIRE(actual.size() >= 4);
        const auto flags = std::stoi(actual[1]);
        CHECK(record.symbol.eventFlags == flags);
        CHECK(record.symbol.units == u"FLAGS");
        CHECK(actual[3] == "0");
        CHECK(record.removalTime == ((flags & 2) != 0 ? std::optional{std::stoll(actual[2])} : std::nullopt));
        std::size_t slot = 4;

        for (const auto &value : record.typedValues) {
            REQUIRE(slot < actual.size());
            std::int64_t raw = std::stoll(actual[slot++]);

            if (value.type == c::SerialFieldType::LONG || value.type == c::SerialFieldType::WIDE_DECIMAL ||
                value.type == c::SerialFieldType::TIME_MILLIS) {
                REQUIRE(slot < actual.size());
                const auto bits = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw)) << 32) |
                                  static_cast<std::uint32_t>(std::stoll(actual[slot++]));
                raw = std::bit_cast<std::int64_t>(bits);
            }

            CHECK(value.raw == raw);
            CHECK_FALSE(value.units.has_value());
            CHECK_FALSE(value.bytes.has_value());
        }

        CHECK(slot == actual.size());
    }

    CHECK(index == records.size());
    CHECK(index > 0);
}
} // namespace

TEST_CASE("explicit catalog policy preserves all64 known flags and old overload refusals") {
    const auto bindings = context();

    for (int subset = 0; subset < 64; ++subset) {
        const auto flags = (subset & 31) | ((subset & 32) << 1);
        const auto body = payload(flags);
        auto result = p::decodeStreamDataWithEventPolicy(body, bindings, limits().data, POLICY);
        REQUIRE(result.status == p::StreamDataStatus::OK);
        REQUIRE(result.records.size() == 1);
        CHECK(result.consumed == body.size());
        CHECK(result.processedRecords == 1);
        CHECK(result.wireFields == ((flags & 2) != 0 ? 0 : 3));
        CHECK(result.records[0].symbol.eventFlags == flags);
        CHECK(result.records[0].symbol.eventFlagsBytes == (flags < 64 ? 1 : 2));
        CHECK(result.records[0].removalTime == ((flags & 2) != 0 ? std::optional{TIME} : std::nullopt));
        checkPair(result.records[0], TIME);
        CHECK(result.records[0].typedValues[2].raw == ((flags & 2) != 0 ? 0 : 913));
        CHECK(p::decodeStreamData(body, bindings, limits().data).status ==
              (flags == 0 ? p::StreamDataStatus::OK : p::StreamDataStatus::UNSUPPORTED_PROFILE));
        CHECK(p::decodeRawData(body, bindings, limits().data).status ==
              (flags == 0 ? p::StreamDataStatus::OK : p::StreamDataStatus::UNSUPPORTED_PROFILE));
    }
}

TEST_CASE("unknown projected bits refuse before sender time and output while overlong flags retain low32") {
    const auto bindings = context();

    for (auto flags : {0x20, 0x80, 0x100, std::numeric_limits<int>::min(), -1}) {
        auto body = payload(flags);
        auto allowance = limits().data;
        allowance.emittedValues = 0;
        const auto result = p::decodeStreamDataWithEventPolicy(body, bindings, allowance, POLICY);
        CHECK(result.status == p::StreamDataStatus::UNSUPPORTED_PROFILE);
        CHECK(result.stage == p::StreamDataStage::FLAGS);
        CHECK(result.processedRecords == 1);
        CHECK(result.wireFields == 0);
        CHECK(result.records.empty());
        CHECK(result.consumed < body.size());
    }

    auto body = payload(4);
    body.erase(body.begin() + 1);
    std::vector<std::byte> wide;
    compact(wide, 0x100000004LL);
    body.insert(body.begin() + 1, wide.begin(), wide.end());
    const auto result = p::decodeStreamDataWithEventPolicy(body, bindings, limits().data, POLICY);
    REQUIRE(result.status == p::StreamDataStatus::OK);
    CHECK(result.records[0].symbol.eventFlags == 4);
    CHECK(result.records[0].symbol.eventFlagsBytes == 5);

    auto allowance = limits().data;
    allowance.payloadBytes = 0;
    CHECK(p::decodeStreamDataWithEventPolicy(body, bindings, allowance, static_cast<p::DataEventPolicy>(99)).status ==
          p::StreamDataStatus::UNSUPPORTED_PROFILE);
    CHECK(p::decodeStreamData(body, bindings, allowance).status == p::StreamDataStatus::RESOURCE_LIMIT);
    auto ordinarySchema = c::LocalSchema::create({}, limits().context.schema);
    REQUIRE(ordinarySchema);
    auto ordinary = p::StreamDataContext::create(*ordinarySchema, {}, limits().context);
    REQUIRE(ordinary);
    CHECK(p::decodeStreamDataWithEventPolicy(body, *ordinary, allowance, POLICY).status ==
          p::StreamDataStatus::UNSUPPORTED_PROFILE);
}

TEST_CASE("raw removal time survives all signed64 values dense layouts and owner destruction") {
    for (auto time : {std::numeric_limits<std::int64_t>::min(), std::int64_t{-1}, std::int64_t{0}, TIME,
                      std::numeric_limits<std::int64_t>::max()}) {
        const auto result = p::decodeStreamDataWithEventPolicy(payload(2, time), context(), limits().data, POLICY);
        REQUIRE(result.status == p::StreamDataStatus::OK);
        REQUIRE(result.records.size() == 1);
        REQUIRE(result.records[0].removalTime.has_value());
        CHECK(*result.records[0].removalTime == time);
        checkPair(result.records[0], time);
        CHECK(result.records[0].context.valid());

        for (const auto type : {104, 120, 152}) {
            c::LocalRecord single{0,
                                  u"Single",
                                  true,
                                  {{8}, {type}, {137}, {9}},
                                  {{u"Sentinel", 0}, {u"Index", 1}, {u"Text", 2}, {u"Bytes", 3}},
                                  c::IndexedTimeSingleInt64{1}};
            auto singleContext = context(single, {0, u"Single", {{u"Index", type}, {u"Sentinel", 8}}});
            const auto output =
                p::decodeStreamDataWithEventPolicy(payload(2, time), singleContext, limits().data, POLICY);
            REQUIRE(output.status == p::StreamDataStatus::OK);
            const auto &record = output.records[0];
            REQUIRE(record.typedValues.size() == 4);
            CHECK(record.typedValues[0].raw == 0);
            CHECK(record.typedValues[1].raw == time);
            CHECK_FALSE(record.typedValues[2].units.has_value());
            CHECK_FALSE(record.typedValues[3].bytes.has_value());
            CHECK(record.removalTime == time);
        }

        auto nonindexed = pairRecord();
        nonindexed.hasTime = false;
        nonindexed.indexedTimeLayout.reset();
        const auto plain =
            p::decodeStreamDataWithEventPolicy(payload(2, time), context(nonindexed), limits().data, POLICY);
        REQUIRE(plain.status == p::StreamDataStatus::OK);
        CHECK(plain.records[0].removalTime == time);
        CHECK(std::all_of(plain.records[0].typedValues.begin(), plain.records[0].typedValues.end(),
                          [](const auto &value) {
                              return value.raw == 0;
                          }));
    }

    const auto skipped = p::decodeStreamDataWithEventPolicy(
        payload(2), context(pairRecord(), {0, u"Unknown", {{u"X", 137}}}), limits().data, POLICY);
    CHECK(skipped.status == p::StreamDataStatus::OK);
    CHECK(skipped.processedRecords == 1);
    CHECK(skipped.wireFields == 0);
    CHECK(skipped.records.empty());
    const auto empty =
        p::decodeStreamDataWithEventPolicy(payload(2, 0), context({0, u"Pair", false, {}, {}}), limits().data, POLICY);
    CHECK(empty.status == p::StreamDataStatus::OK);
    CHECK(empty.records[0].typedValues.empty());
    CHECK(empty.records[0].removalTime == 0);
}

TEST_CASE("removal truncation retains completed prefix actual positions and admission accounting") {
    const auto bindings = context();
    const auto first = payload(0);
    const auto removal = payload(2);
    const auto primitiveStart = std::size_t{4};

    for (std::size_t cut = primitiveStart; cut < removal.size(); ++cut) {
        auto body = first;
        body.insert(body.end(), removal.begin(), removal.begin() + static_cast<std::ptrdiff_t>(cut));
        const auto output = p::decodeStreamDataWithEventPolicy(body, bindings, limits().data, POLICY);
        REQUIRE(output.records.size() == 1);
        CHECK(output.status == p::StreamDataStatus::TRUNCATED_PAYLOAD);
        CHECK(output.stage == p::StreamDataStage::REMOVAL_TIME);
        CHECK(output.errorOffset == first.size() + primitiveStart);
        CHECK(output.consumed == body.size());
        CHECK(output.processedRecords == 2);
        CHECK(output.wireFields == 3);
        CHECK_FALSE(output.records[0].removalTime.has_value());
        checkPair(output.records[0], TIME);
    }

    auto body = removal;
    body.insert(body.end(), first.begin(), first.end());
    const auto output = p::decodeStreamDataWithEventPolicy(body, bindings, limits().data, POLICY);
    REQUIRE(output.records.size() == 2);
    CHECK(output.records[0].removalTime == TIME);
    CHECK(output.records[1].typedValues[2].raw == 913);
    CHECK(output.wireFields == 3);
}

TEST_CASE("omitted fields need dense value admission but no field or payload work budgets") {
    const auto bindings = context();
    const auto body = payload(2);

    for (const auto count : {0U, 2U, 3U, 4U}) {
        auto allowance = limits().data;
        allowance.wireFields = 0;
        allowance.emittedValues = count;
        const auto output = p::decodeStreamDataWithEventPolicy(body, bindings, allowance, POLICY);
        CHECK(output.status == (count >= 3 ? p::StreamDataStatus::OK : p::StreamDataStatus::RESOURCE_LIMIT));
        CHECK(output.processedRecords == 1);
        CHECK(output.wireFields == 0);
        CHECK(output.payloadWorkBytes == 0);
        CHECK(output.fieldBytes == 0);

        if (count < 3) {
            CHECK(output.stage == p::StreamDataStage::OUTPUT);
            CHECK(output.consumed == 4);
            CHECK(output.records.empty());
        }
    }

    for (int limit = -1; limit <= 1; ++limit) {
        auto allowance = limits().data;
        allowance.payloadBytes = body.size() + limit;
        CHECK(p::decodeStreamDataWithEventPolicy(body, bindings, allowance, POLICY).status ==
              (limit < 0 ? p::StreamDataStatus::RESOURCE_LIMIT : p::StreamDataStatus::OK));
    }

    for (const auto count : {0U, 1U, 2U}) {
        auto allowance = limits().data;
        allowance.processedRecords = count;
        const auto output = p::decodeStreamDataWithEventPolicy(body, bindings, allowance, POLICY);
        CHECK(output.status == (count == 0 ? p::StreamDataStatus::RESOURCE_LIMIT : p::StreamDataStatus::OK));
        CHECK(output.processedRecords == (count == 0 ? 0 : 1));
        allowance = limits().data;
        allowance.emittedRecords = count;
        CHECK(p::decodeStreamDataWithEventPolicy(body, bindings, allowance, POLICY).status ==
              (count == 0 ? p::StreamDataStatus::RESOURCE_LIMIT : p::StreamDataStatus::OK));
    }

    auto maximum = limits().data;
    maximum.emittedValues = std::numeric_limits<std::size_t>::max();
    maximum.processedRecords = std::numeric_limits<std::size_t>::max();
    CHECK(p::decodeStreamDataWithEventPolicy(body, bindings, maximum, POLICY).status == p::StreamDataStatus::OK);
}

TEST_CASE("allocation failures cannot expose partial removal records or mutate immutable context") {
    const auto bindings = context();
    auto body = payload(2, 0);
    const auto second = payload(2);
    body.insert(body.end(), second.begin(), second.end());
    const auto *oldPlan = bindings.find(0);
    int failures = 0;
    int prefixFailures = 0;
    bool success = false;

    for (int checkpoint = 0; checkpoint < 20; ++checkpoint) {
        allocationFailAfter = checkpoint;
        const auto output = p::decodeStreamDataWithEventPolicy(body, bindings, limits().data, POLICY);
        allocationFailAfter = -1;
        CHECK(bindings.find(0) == oldPlan);

        if (output.status == p::StreamDataStatus::OK) {
            REQUIRE(output.records.size() == 2);
            success = true;
            break;
        }

        ++failures;
        CHECK(output.status == p::StreamDataStatus::ALLOCATION_FAILURE);
        CHECK(output.records.size() <= 1);

        if (!output.records.empty()) {
            ++prefixFailures;
            CHECK(output.records[0].removalTime == 0);
            checkPair(output.records[0], 0);
        }
    }

    CHECK(failures > 0);
    CHECK(prefixFailures > 0);
    CHECK(success);
}

TEST_CASE("catalog tape accepts exact hs absence and empty in all generic families while preserving old modes") {
    auto local = schema();
    const std::array wire{description()};
    std::vector<p::StreamDataRecord> retained;

    for (const auto option : {std::optional<std::u16string_view>{}, std::optional<std::u16string_view>{u""},
                              std::optional<std::u16string_view>{u"hs"}}) {
        p::TapeReader reader(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, descriptor(option));
        CHECK(reader.descriptor()->options == (option ? std::optional{std::u16string(*option)} : std::nullopt));
        accept(reader, descriptions(wire));

        for (int subset = 0; subset < 64; ++subset) {
            const auto flags = (subset & 31) | ((subset & 32) << 1);
            const auto output = accept(reader, frame(15, payload(flags)));
            REQUIRE(output.records.size() == 1);
            CHECK(output.records[0].symbol.eventFlags == flags);
        }

        auto record = accept(reader, frame(15, payload(2, 0))).records[0];
        CHECK(record.removalTime == 0);
        accept(reader, descriptions(wire));
        CHECK(record.context.valid());
        CHECK(record.removalTime == 0);
        CHECK_FALSE(record.context.matches(reader.schema().recordContext(0)));
        retained.push_back(record);

        for (auto family : {5, 10, 20}) {
            p::TapeReader guarded(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
            accept(guarded, descriptor(option));
            accept(guarded, descriptions(wire));
            const auto result = guarded.feed(frame(family, payload(2)));
            CHECK(result.status == p::TapeStatus::EVENT);
            REQUIRE(result.records.size() == 1);
            CHECK(result.records[0].removalTime == TIME);
            REQUIRE(result.frame);
            CHECK(result.frame->type == family);
        }
    }

    for (auto option : {u"unknown", u"hs,hs", u"hs,unknown", u" hs"}) {
        p::TapeReader reader(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
        CHECK(reader.feed(descriptor(option)).status == p::TapeStatus::FAILED);
        CHECK(reader.diagnostic().error == p::TapeError::UNSUPPORTED);
        CHECK(reader.diagnostic().rejection == p::TapeRejection::OPTIONS);
    }

    p::TapeReader field(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
    CHECK(field.feed(descriptor(u"hs", u"FIELD")).status == p::TapeStatus::FAILED);
    CHECK(field.diagnostic().rejection == p::TapeRejection::TIME);

    for (const auto &record : retained) {
        CHECK(record.context.valid());
        CHECK(record.removalTime == 0);
        checkPair(record, 0);
    }
}

TEST_CASE("symbol metadata defaults and message-local reuse preserve owned results") {
    e::SymbolValue value;
    p::StreamDataRecord record;
    CHECK(value.cipher == 0);
    CHECK(value.eventFlags == 0);
    CHECK(value.eventFlagsBytes == 0);
    CHECK(record.symbol.eventFlags == 0);
    CHECK_FALSE(record.removalTime.has_value());
    const auto bindings = context();
    auto first = payload(4);
    auto repeat = payload(4);
    repeat.erase(repeat.begin(), repeat.begin() + 3);
    repeat.insert(repeat.begin(), {std::byte{0xf8}, std::byte{0xff}});
    first.insert(first.end(), repeat.begin(), repeat.end());
    const auto output = p::decodeStreamDataWithEventPolicy(first, bindings, limits().data, POLICY);
    REQUIRE(output.records.size() == 2);
    CHECK(output.records[1].symbol.eventFlags == 4);
    CHECK(output.records[1].symbol.eventFlagsBytes == 1);
    const auto undefined = p::decodeStreamDataWithEventPolicy(repeat, bindings, limits().data, POLICY);
    CHECK(undefined.status == p::StreamDataStatus::INVALID_SYMBOL);
    auto mru = payload(0);
    mru.erase(mru.begin() + 1);
    mru[0] = std::byte{0xf8};
    const auto fresh = p::decodeStreamDataWithEventPolicy(mru, bindings, limits().data, POLICY);
    REQUIRE(fresh.status == p::StreamDataStatus::OK);
    CHECK(fresh.records[0].symbol.eventFlags == 1);

    for (const auto malformed :
         {std::vector<std::byte>{std::byte{0xf9}}, std::vector<std::byte>{std::byte{0xf9}, std::byte{0x80}},
          std::vector<std::byte>{std::byte{0xf8}, std::byte{0xf9}}}) {
        const auto failed = p::decodeStreamDataWithEventPolicy(malformed, bindings, limits().data, POLICY);
        CHECK(failed.records.empty());
        CHECK(failed.stage == p::StreamDataStage::SYMBOL);
        CHECK(failed.consumed == malformed.size());
    }
}

TEST_CASE("production stream flags and real FileWriter values match actual Java STREAM projections") {
    const auto local = productionSchema();

    for (const auto option : {"default", "empty", "hs", "unknown", "hs-hs", "hs-unknown"}) {
        const auto name = std::string("composer-15-") + option;
        const auto records = productionRecords(fixture(name + ".qtp"), local);
        REQUIRE(records.size() == 64);
        compareProduction(name, records);
    }

    const auto designed = productionRecords(fixture("designed-15-all64.qtp"), local);
    REQUIRE(designed.size() == 64);
    compareProduction("designed-15-all64", designed);

    for (const auto standard : {false, true}) {
        const auto schema = productionSchema(standard);

        for (const auto option : {"absent-setting", "empty", "hs"}) {
            const auto name = std::string("file-") + (standard ? "time-and-sale-" : "pair-") + option;
            const auto bytes = fixture(name + ".qtp");
            const auto records = productionRecords(bytes, schema);
            REQUIRE(records.size() == 7);
            compareProduction(name, records);
            p::TapeReader reader(schema, limits(), p::TapeMode::BINARY_CATALOG_V1);
            std::vector<p::StreamDataRecord> tapeRecords;
            std::size_t position = 0;

            while (position < bytes.size()) {
                auto event = reader.feed(std::span(bytes).subspan(position));
                REQUIRE(event.accepted > 0);
                REQUIRE(event.status == p::TapeStatus::EVENT);
                position += event.accepted;
                tapeRecords.insert(tapeRecords.end(), std::make_move_iterator(event.records.begin()),
                                   std::make_move_iterator(event.records.end()));
            }

            REQUIRE(reader.finish() == p::TapeStatus::FINISHED);
            compareProduction(name, tapeRecords);
        }
    }
}

/** Run isolated conformance tests against a supplied governed flags directory.
 * @param argc Argument count.
 * @param argv Fixture directory followed by optional doctest switches.
 * @return Doctest status, or usage failure before tests.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    fixtureRoot = argv[1];
    doctest::Context test;
    test.applyCommandLine(argc - 1, argv + 1);

    return test.run();
}
