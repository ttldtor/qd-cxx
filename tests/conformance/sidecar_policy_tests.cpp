// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "indexed_test_support.hpp"
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace {
std::ptrdiff_t allocationFailAfter = -1; ///< Isolated constructor fault checkpoint, disabled during assertions.
}

/** Track ordinary allocations only in this isolated regression executable.
 * @param size Requested bytes; zero still receives deletable storage.
 * @return Owned storage or injected bad_alloc.
 */
void *operator new(std::size_t size) {
    if (allocationFailAfter == 0) {
        throw std::bad_alloc{};
    }

    if (allocationFailAfter > 0) {
        --allocationFailAfter;
    }

    if (void *storage = std::malloc(size == 0 ? 1 : size)) {
        return storage;
    }

    throw std::bad_alloc{};
}

/** Release isolated test storage.
 * @param storage Pointer returned by this executable's operator new.
 */
void operator delete(void *storage) noexcept {
    std::free(storage);
}

/** Release isolated sized test storage.
 * @param storage Pointer returned by this executable's operator new.
 * @param size Unused allocation size.
 */
void operator delete(void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

namespace {
using namespace indexed_test;
constexpr auto CATALOG = p::TapeMode::BINARY_CATALOG_V1;
constexpr auto DISCOVERY = p::TapeDiscovery::BINARY_CATALOG_V1;
constexpr auto SIDECAR = p::TapeExternalTimePolicy::SIDECAR;
constexpr auto DISABLED = p::TapeExternalTimePolicy::DISABLED;
constexpr auto READ = p::FieldTimePolicy::READ;

/** Supply a nonindexed ordinary business schema, independent from time metadata.
 * @return Bounded owning binary schema.
 */
c::LocalSchema ordinarySchema() {
    const std::array records{c::LocalRecord{0, u"Sample", false, {{8}}, {{u"Value", 0}}}};
    auto schema = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    return std::move(*schema);
}

/** Encode an explicit initial header or timing-only active descriptor delta.
 * @param time Exact time token, with no case normalization.
 * @param initial Whether to include required type/options and initial send-set declaration.
 * @return Complete owning protocol envelope.
 */
std::vector<std::byte> timedHeader(std::u16string_view time, bool initial = true) {
    std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    compact(body, initial ? 3 : 1);

    if (initial) {
        text(body, u"type");
        text(body, u"tape");
        text(body, u"opt");
        text(body, u"");
    }

    text(body, u"time");
    text(body, time);
    compact(body, initial ? 1 : 0);

    if (initial) {
        compact(body, 15);
        text(body, u"STREAM_DATA");
        compact(body, 0);
    }

    compact(body, 0);

    return frame(1, body);
}

/** Construct a FIELD-prefixed ordinary record with exact small physical words.
 * @return Complete owning STREAM_DATA envelope.
 */
std::vector<std::byte> fieldData() {
    std::vector<std::byte> body{std::byte{0xfe}};

    for (const auto word : {91, 7, 8, 913}) {
        compact(body, word);
    }

    return frame(15, body);
}
} // namespace

TEST_CASE("external header admission is explicit exact and independent from FIELD") {
    auto schema = ordinarySchema();

    for (const auto token : {u"LONG", u"long", u"TEXT", u"text"}) {
        for (const auto fields : {p::FieldTimePolicy::DISABLED, READ}) {
            for (const auto external : {DISABLED, SIDECAR}) {
                p::TapeReader supplied(schema, limits(), CATALOG, fields, external);
                p::TapeReader discovered(limits(), DISCOVERY, fields, external);

                for (auto *reader : {&supplied, &discovered}) {
                    const auto result = reader->feed(timedHeader(token));
                    CHECK(result.status == (external == SIDECAR ? p::TapeStatus::EVENT : p::TapeStatus::FAILED));

                    if (external == SIDECAR) {
                        REQUIRE(reader->descriptor());
                        REQUIRE(reader->descriptor()->time);
                        CHECK(*reader->descriptor()->time == token);
                        CHECK_FALSE(reader->timing().timestampMs);
                        CHECK(reader->timing().eventTimeSequence == 0);
                        CHECK(reader->finish() == p::TapeStatus::FINISHED);
                    } else {
                        CHECK(reader->diagnostic().rejection == p::TapeRejection::TIME);
                        CHECK(reader->feed(timedHeader(u"NONE")).accepted == 0);
                    }
                }
            }
        }

        p::TapeReader oldSupplied(schema, limits(), CATALOG, READ);
        p::TapeReader oldDiscovered(limits(), DISCOVERY, READ);
        CHECK(oldSupplied.feed(timedHeader(token)).status == p::TapeStatus::FAILED);
        CHECK(oldDiscovered.feed(timedHeader(token)).status == p::TapeStatus::FAILED);
    }

    for (const auto token : {u"Long", u"Text", u"LONG ", u"", u"UTC", u" long"}) {
        p::TapeReader reader(limits(), DISCOVERY, READ, SIDECAR);
        CHECK(reader.feed(timedHeader(token)).status == p::TapeStatus::FAILED);
        CHECK(reader.diagnostic().rejection == p::TapeRejection::TIME);
    }

    for (const auto token : {u"NONE", u"none", u"MESSAGE", u"message", u"FIELD", u"field"}) {
        p::TapeReader reader(limits(), DISCOVERY, READ, SIDECAR);
        CHECK(reader.feed(timedHeader(token)).status == p::TapeStatus::EVENT);
    }

    p::TapeReader noField(limits(), DISCOVERY, p::FieldTimePolicy::DISABLED, SIDECAR);
    CHECK(noField.feed(timedHeader(u"FIELD")).status == p::TapeStatus::FAILED);
}

TEST_CASE("external admission preserves raw FIELD MESSAGE timing and partial-frame publication") {
    for (const auto token : {u"LONG", u"TEXT"}) {
        auto budget = limits();
        budget.data.ownedFieldTimeRecords = 8;
        p::TapeReader reader(ordinarySchema(), budget, CATALOG, READ, SIDECAR);
        accept(reader, timedHeader(token));
        const std::array fields{
            p::WireRecordDescription{91, u"Sample", {{u"EventTime", 56}, {u"EventSequence", 72}, {u"Value", 8}}}};
        accept(reader, descriptions(fields));
        const auto bytes = fieldData();
        const auto prefix = reader.feed(std::span(bytes).first(bytes.size() - 1));
        CHECK(prefix.status == p::TapeStatus::NEED_INPUT);
        CHECK(prefix.records.empty());
        auto first = reader.feed(std::span(bytes).last(1));
        REQUIRE(first.status == p::TapeStatus::EVENT);
        REQUIRE(first.records.size() == 1);
        REQUIRE(first.records[0].fieldTime);
        CHECK(first.records[0].fieldTime->rawSeconds == 7);
        CHECK(first.records[0].fieldTime->rawSequence == 8);
        CHECK(first.records[0].typedValues[0].raw == 913);
        REQUIRE(first.timing);
        CHECK_FALSE(first.timing->timestampMs);
        CHECK(first.timing->eventTimeSequence == 0);

        std::vector<std::byte> heartbeat;
        compact(heartbeat, 1);
        compact(heartbeat, 1700000000123LL);
        auto event = accept(reader, frame(0, heartbeat));
        REQUIRE(event.heartbeat);
        CHECK(event.heartbeat->timestampMs == 1700000000123LL);
        auto second = accept(reader, fieldData());
        REQUIRE(second.timing);
        CHECK(second.timing->timestampMs == 1700000000123LL);
        REQUIRE(second.records.size() == 1);
        REQUIRE(second.records[0].fieldTime);
        CHECK(second.records[0].fieldTime->rawSeconds == 7);
        CHECK(second.records[0].fieldTime->rawSequence == 8);
        CHECK(first.records[0].fieldTime->sourceOrdinal == 0);
        CHECK(second.records[0].fieldTime->sourceOrdinal == 1);
    }
}

TEST_CASE("external descriptor deltas preserve published prefix and terminal errors") {
    p::TapeReader reader(limits(), DISCOVERY, READ, SIDECAR);
    accept(reader, timedHeader(u"NONE"));
    accept(reader, timedHeader(u"LONG", false));
    REQUIRE(reader.descriptor());
    CHECK(reader.descriptor()->time == u"LONG");
    const auto *oldTime = reader.descriptor()->time->data();
    const auto rejected = reader.feed(timedHeader(u"Long", false));
    CHECK(rejected.status == p::TapeStatus::FAILED);
    CHECK(reader.diagnostic().rejection == p::TapeRejection::TIME);
    REQUIRE(reader.descriptor());
    CHECK(reader.descriptor()->time == u"LONG");
    CHECK(reader.descriptor()->time->data() == oldTime);
    CHECK(reader.feed(timedHeader(u"TEXT", false)).accepted == 0);
}

TEST_CASE("external policy validates before schema and framing allocation while old calls remain valid") {
    auto schema = ordinarySchema();
    auto bad = limits();
    bad.context.schema.records = 0;
    bad.frame = {0, 0};
    CHECK_THROWS_AS(p::TapeReader(schema, bad, CATALOG, READ, static_cast<p::TapeExternalTimePolicy>(99)),
                    std::invalid_argument);
    CHECK_THROWS_AS(p::TapeReader(bad, DISCOVERY, READ, static_cast<p::TapeExternalTimePolicy>(99)),
                    std::invalid_argument);

    for (const auto mode : {p::TapeMode::LEGACY, p::TapeMode::RAW_MESSAGE_V1, static_cast<p::TapeMode>(99)}) {
        CHECK_THROWS_AS(p::TapeReader(schema, bad, mode, p::FieldTimePolicy::DISABLED, SIDECAR), std::invalid_argument);
    }

    CHECK_THROWS_AS(p::TapeReader(bad, static_cast<p::TapeDiscovery>(99), READ, SIDECAR), std::invalid_argument);
    CHECK_THROWS_AS(p::TapeReader(bad, DISCOVERY, static_cast<p::FieldTimePolicy>(99), SIDECAR), std::invalid_argument);

    for (bool discovery : {false, true}) {
        bool invalid = false;
        bool allocated = false;
        allocationFailAfter = 1; // Permit exception-message storage, but no preceding schema/framing allocation.

        try {
            if (discovery) {
                p::TapeReader reader(limits(), DISCOVERY, READ, static_cast<p::TapeExternalTimePolicy>(99));
            } else {
                p::TapeReader reader(schema, limits(), CATALOG, READ, static_cast<p::TapeExternalTimePolicy>(99));
            }
        } catch (const std::invalid_argument &) {
            invalid = true;
        } catch (const std::bad_alloc &) {
            allocated = true;
        }

        allocationFailAfter = -1;
        CHECK(invalid);
        CHECK_FALSE(allocated);
    }

    static_assert(std::is_constructible_v<p::TapeReader, const c::LocalSchema &, p::TapeLimits>);
    static_assert(std::is_constructible_v<p::TapeReader, p::TapeLimits>);
    static_assert(std::is_constructible_v<p::TapeReader, p::TapeLimits, p::TapeDiscovery>);
    auto factory = &p::StreamDataContext::create;
    static_cast<void>(factory);
}
