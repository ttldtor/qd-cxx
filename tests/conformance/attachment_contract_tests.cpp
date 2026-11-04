// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/encoding/compact.hpp"
#include "qd/protocol/tape_reader.hpp"

#include <algorithm>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
using Bytes = std::vector<std::byte>;
std::filesystem::path fixtures;

/** Decode an exact oracle hex cell without executing the payload.
 * @param text Lowercase hex, null or empty.
 * @return Independently owned nullable bytes; malformed hex throws invalid_argument.
 */
std::optional<Bytes> value(std::string_view text) {
    if (text == "null") {
        return std::nullopt;
    }

    Bytes result;

    if (text == "empty") {
        return result;
    }

    REQUIRE(text.size() % 2 == 0);

    for (std::size_t i = 0; i < text.size(); i += 2) {
        const auto digit = [](char ch) -> unsigned {
            if (ch >= '0' && ch <= '9') {
                return static_cast<unsigned>(ch - '0');
            }

            if (ch >= 'a' && ch <= 'f') {
                return static_cast<unsigned>(ch - 'a') + 10;
            }

            throw std::invalid_argument("noncanonical oracle hex");
        };
        result.push_back(static_cast<std::byte>(digit(text[i]) * 16 + digit(text[i + 1])));
    }

    return result;
}

/** Finite independent budgets for these small production records.
 * @return Explicit payload, work, schema and output bounds.
 */
p::TapeLimits limits() {
    p::TapeLimits result{{8192, 8201},
                         65536,
                         128,
                         {16, 8, {1024, 256}, 2048},
                         {{8, 64, 128, 128, 8192}, 32, {256, 128, 8192}},
                         100000,
                         {8192, 128, 1024, 128, 2048, 4096, {8192, 128, 128}, {1024, 256}, 4096, 1024, 1024},
                         128};
    result.data.fieldPayloadEncodedBytes = 4096;
    result.data.fieldPayloadBytes = 4096;
    result.data.payloadEncodedBytes = 4096;
    result.data.payloadWorkBytes = 4096;
    result.data.ownedPayloadBytes = 4096;

    return result;
}

/** Caller-selected ordinary generic schema, with independent local record IDs.
 * @param name Actual standard record name, or Unknown to test skipped records.
 * @return Owned validated schema; no typed event adapter is involved.
 */
c::LocalSchema schema(std::string_view name) {
    c::LocalRecord record{0,
                          name == "Message"         ? u"Message"
                          : name == "Configuration" ? u"Configuration"
                                                    : u"Unknown",
                          false,
                          {},
                          {}};

    if (name == "Configuration") {
        record.fields.push_back({8});
        record.lookups.push_back({u"Version", 0});
    }

    record.lookups.push_back({name == "Configuration" ? u"Configuration" : u"Message", record.fields.size()});
    record.fields.push_back({249});
    auto local =
        c::LocalSchema::create(std::vector{record}, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);

    return std::move(*local);
}

/** Read original production FileWriter bytes.
 * @param stem Exact fixture identity.
 * @param extension Actual tape or composer artifact suffix.
 * @return Owned bytes; missing input fails the test.
 */
Bytes input(const std::string &stem, std::string_view extension = ".tape") {
    std::ifstream file(fixtures / (stem + std::string(extension)), std::ios::binary);
    REQUIRE(file.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(file)), {});
    const auto bytes = std::as_bytes(std::span(chars));

    return {bytes.begin(), bytes.end()};
}

/** Completed generic values outlive all source and parser state. */
struct Observation {
    std::vector<p::StreamDataRecord> records;
    p::TapeStatus status;
    p::TapeDiagnostic diagnostic;
};

/** Feed one finite chunk schedule through the current public generic API.
 * @param wire Borrowed tape, copied by the reader as required.
 * @param name Explicit local standard record selector.
 * @param chunk Positive feed size.
 * @param budget Independent finite reader limits.
 * @return Fully owned completed prefix and terminal status.
 */
Observation read(std::span<const std::byte> wire, std::string_view name, std::size_t chunk,
                 p::TapeLimits budget = limits()) {
    p::TapeReader reader(schema(name), budget);
    Observation result{};

    while (!wire.empty()) {
        auto next = reader.feed(wire.first(std::min(chunk, wire.size())));
        result.records.insert(result.records.end(), std::make_move_iterator(next.records.begin()),
                              std::make_move_iterator(next.records.end()));
        wire = wire.subspan(next.accepted);

        if (next.status == p::TapeStatus::FAILED) {
            break;
        }

        REQUIRE(next.accepted > 0);
    }

    result.status = reader.finish();
    result.diagnostic = reader.diagnostic();

    return result;
}

/** Split a strict observation row while retaining empty cells.
 * @param text Original row without LF.
 * @return Ordered complete cell copies.
 */
std::vector<std::string> cells(const std::string &text) {
    std::vector<std::string> result;
    std::size_t start = 0;

    for (;;) {
        const auto end = text.find('\t', start);
        result.push_back(text.substr(start, end - start));

        if (end == std::string::npos) {
            return result;
        }

        start = end + 1;
    }
}

TEST_CASE("standard Java attachments remain exact owned serialized bytes through the public generic reader") {
    std::ifstream table(fixtures / "oracle.tsv");
    REQUIRE(table.good());
    std::string line;
    std::size_t count = 0;
    std::size_t textFields = 0;

    while (std::getline(table, line)) {
        const auto row = cells(line);
        REQUIRE_FALSE(row.empty());

        if (row[0] == "text-field") {
            REQUIRE(row.size() == 4);
            CHECK(row[2] == "Text");
            CHECK(row[3] == "10");
            ++textFields;
            continue;
        }

        REQUIRE(row.size() == 9);
        REQUIRE(row[0] == "event");
        REQUIRE(row[4] == "249");
        const auto name = row[1].substr(0, row[1].find('-'));
        const auto expected = value(row[5]);
        auto wire = input(row[1]);

        for (const auto chunk : {std::size_t{1}, std::size_t{7}, std::size_t{65536}}) {
            auto saved = [&] {
                auto transientInput = wire;

                return read(transientInput, name, chunk);
            }();
            REQUIRE(saved.status == p::TapeStatus::FINISHED);
            REQUIRE(saved.records.size() == 1);
            auto &fields = saved.records[0].typedValues;
            REQUIRE(fields.size() == (name == "Configuration" ? 2 : 1));
            CHECK(fields.back().type == c::SerialFieldType::SERIAL_OBJECT);
            CHECK(fields.back().bytes == expected);
            CHECK_FALSE(fields.back().units);
            CHECK(fields.back().raw == 0);
            CHECK(c::validFieldValue(fields.back()));

            if (name == "Configuration") {
                CHECK(fields.front().raw == -7);
            }

            auto copy = fields.back();

            if (copy.bytes && !copy.bytes->empty()) {
                copy.bytes->front() ^= std::byte{0xff};
                CHECK(fields.back().bytes == expected);
                CHECK(copy != fields.back());
            }
        }

        const auto skipped = read(wire, "Unknown", 1);
        CHECK(skipped.status == p::TapeStatus::FINISHED);
        CHECK(skipped.records.empty());

        if (expected && !expected->empty()) {
            auto budget = limits();
            budget.data.fieldPayloadBytes = expected->size() - 1;
            const auto rejected = read(wire, name, 7, budget);
            CHECK(rejected.status == p::TapeStatus::FAILED);
            CHECK(rejected.diagnostic.error == p::TapeError::RESOURCE_LIMIT);
            CHECK(rejected.records.empty());
            budget.data.fieldPayloadBytes = expected->size();
            CHECK(read(wire, name, 7, budget).status == p::TapeStatus::FINISHED);
        }

        // A FileWriter trailer can be removed without truncating a record. Keep its protocol
        // header, then truncate the separately captured composer data frame instead.
        const auto header = org::ttldtor::qdcxx::encoding::readCompactInt(wire);
        REQUIRE(header.status == org::ttldtor::qdcxx::encoding::Status::OK);
        wire.resize(header.consumed + static_cast<std::size_t>(header.value));
        auto composed = input(row[1], ".qtp");
        composed.pop_back();
        wire.insert(wire.end(), composed.begin(), composed.end());
        const auto truncated = read(wire, name, 7);
        CHECK(truncated.status == p::TapeStatus::FAILED);
        CHECK(truncated.diagnostic.error == p::TapeError::MALFORMED);
        CHECK(truncated.records.empty());
        ++count;
    }

    CHECK(count == 14);
    CHECK(textFields == 2);
}

TEST_CASE("serialized Java byte arrays differ from raw empty or raw content attachments") {
    const auto serializedEmpty = read(input("Message-bytes-empty"), "Message", 1);
    const auto rawEmpty = read(input("Message-raw-empty"), "Message", 1);
    const auto nullValue = read(input("Message-null"), "Message", 1);
    REQUIRE(serializedEmpty.records.size() == 1);
    REQUIRE(rawEmpty.records.size() == 1);
    REQUIRE(nullValue.records.size() == 1);
    const auto &array = serializedEmpty.records[0].typedValues[0];
    const auto &empty = rawEmpty.records[0].typedValues[0];
    const auto &null = nullValue.records[0].typedValues[0];
    CHECK(array.bytes == value("aced0005757200025b42acf317f8060854e0020000787000000000"));
    REQUIRE(empty.bytes);
    CHECK(empty.bytes->empty());
    CHECK_FALSE(null.bytes);
    CHECK(array != empty);
    CHECK(empty != null);
    auto zeroBodyBudget = limits();
    zeroBodyBudget.data.fieldPayloadBytes = 0;
    CHECK(read(input("Message-null"), "Message", 1, zeroBodyBudget).status == p::TapeStatus::FINISHED);
    CHECK(read(input("Message-raw-empty"), "Message", 1, zeroBodyBudget).status == p::TapeStatus::FINISHED);
    CHECK(read(input("Message-bytes-empty"), "Message", 1, zeroBodyBudget).diagnostic.error ==
          p::TapeError::RESOURCE_LIMIT);
    const auto malformed = read(input("Message-raw-malformed"), "Message", 1);
    REQUIRE(malformed.records.size() == 1);
    CHECK(malformed.records[0].typedValues[0].bytes == value("010203"));
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    fixtures = argv[1];
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
