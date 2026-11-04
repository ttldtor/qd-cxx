// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/events/text_events.hpp"
#include "qd/protocol/tape_reader.hpp"
#include <algorithm>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
namespace p = org::ttldtor::qdcxx::protocol;
std::filesystem::path fixtures;

p::TapeLimits limits() {
    return {{8192, 8201},
            65536,
            128,
            {16, 8, {1024, 256}, 2048},
            {{8, 64, 128, 128, 8192}, 32, {256, 128, 8192}},
            100000,
            {8192, 128, 1024, 128, 2048, 4096, {8192, 128, 128}, {1024, 256}, 4096, 1024, 1024},
            128};
}

TEST_CASE("Java FileWriter text records reach native typed mappers") {
    constexpr c::SchemaLimits schemaLimits{8, 64, 128, 128, 8192};
    constexpr e::MappingLimits mappingLimits{32, 64, 8, 16, 128, 128, 4096, 100000};

    for (const bool configuration : {false, true}) {
        for (const auto *id : {"null", "empty", "units"}) {
            const std::string stem = std::string(configuration ? "TextConfiguration-" : "TextMessage-") + id;
            CAPTURE(stem);
            std::ifstream file(fixtures / (stem + ".tape"), std::ios::binary);
            REQUIRE(file.good());
            const std::vector<char> chars((std::istreambuf_iterator<char>(file)), {});
            auto input = std::as_bytes(std::span(chars));
            auto schema = configuration ? e::defaultTextConfigurationSchema(schemaLimits)
                                        : e::defaultTextMessageSchema(schemaLimits);
            REQUIRE(schema);
            p::TapeReader reader(*schema, limits());
            std::vector<p::StreamDataRecord> records;

            while (!input.empty()) {
                auto result = reader.feed(input.first(std::min<std::size_t>(1, input.size())));
                records.insert(records.end(), std::make_move_iterator(result.records.begin()),
                               std::make_move_iterator(result.records.end()));
                REQUIRE(result.status != p::TapeStatus::FAILED);
                REQUIRE(result.accepted > 0);
                input = input.subspan(result.accepted);
            }

            REQUIRE(reader.finish() == p::TapeStatus::FINISHED);
            REQUIRE(records.size() == 1);
            const auto &record = records.front();
            std::vector<c::FieldValueView> values;

            for (const auto &field : record.typedValues) {
                values.push_back(
                    {field.type, field.raw,
                     field.units ? std::optional<std::u16string_view>(*field.units) : std::nullopt,
                     field.bytes ? std::optional<std::span<const std::byte>>(*field.bytes) : std::nullopt});
            }

            std::optional<std::u16string> expected;

            if (std::string_view(id) == "empty")
                expected = u"";

            if (std::string_view(id) == "units")
                expected = std::u16string{u'A', u'\0', char16_t{0xd800}, char16_t{0xdc00}, char16_t{0xd800}};

            if (configuration) {
                auto mapper = e::PreparedTextConfiguration::prepare(*schema, 0, mappingLimits);
                REQUIRE(mapper);
                auto event = mapper->mapView(record.symbol.units, {record.context, values}, {16, 16, 32});
                REQUIRE(event);
                CHECK(event->symbol == u"TEXT");
                CHECK(event->text == expected);
                CHECK(event->version == -7);
                CHECK(event->getTime() == -877);
                CHECK(event->getSequence() == 9);
            } else {
                auto mapper = e::PreparedTextMessage::prepare(*schema, 0, mappingLimits);
                REQUIRE(mapper);
                auto event = mapper->mapView(record.symbol.units, {record.context, values}, {16, 16, 32});
                REQUIRE(event);
                CHECK(event->symbol == u"TEXT");
                CHECK(event->text == expected);
                CHECK(event->getTime() == -877);
                CHECK(event->getSequence() == 9);
            }
        }
    }
}

TEST_CASE("Java text payload enforces UTF-16 ownership bound") {
    std::ifstream file(fixtures / "TextMessage-units.tape", std::ios::binary);
    REQUIRE(file.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(file)), {});
    const auto input = std::as_bytes(std::span(chars));
    auto schema = e::defaultTextMessageSchema({8, 64, 128, 128, 8192});
    REQUIRE(schema);
    auto bounded = limits();
    bounded.data.fieldString.codeUnits = 4;
    p::TapeReader reader(*schema, bounded);
    auto remaining = input;
    bool failed = false;

    while (!remaining.empty()) {
        auto result = reader.feed(remaining);
        CHECK(result.records.empty());
        remaining = remaining.subspan(result.accepted);

        if (result.status == p::TapeStatus::FAILED) {
            failed = true;
            break;
        }

        REQUIRE(result.accepted > 0);
    }

    CHECK(failed);
    CHECK(reader.diagnostic().error == p::TapeError::RESOURCE_LIMIT);
}

int main(int argc, char **argv) {
    if (argc < 2)
        return 2;

    fixtures = argv[1];
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
