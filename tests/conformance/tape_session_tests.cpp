// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "indexed_test_support.hpp"

#include <cstdlib>
#include <limits>

namespace {
using namespace indexed_test;
std::ptrdiff_t allocationFailAfter = -1; ///< Test-process-only ordinary allocation checkpoint.
} // namespace

/** Inject ordinary allocation failure only inside this focused single-threaded test process.
 * @param size Requested storage size.
 * @return Independently owned storage, or bad_alloc at the armed checkpoint.
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

TEST_CASE("descriptor delta allocation failures retain prior text addresses schema identity and work") {
    const auto delta = [](char16_t unit) {
        std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
        compact(body, 1);
        text(body, u"version");
        text(body, std::u16string(128, unit));
        compact(body, 0);
        compact(body, 0);

        return frame(1, body);
    };
    const auto first = delta(u'A');
    const auto replacement = delta(u'B');
    std::size_t failures = 0;
    std::size_t chargedFailures = 0;
    bool succeeded = false;

    for (std::ptrdiff_t checkpoint = 0; checkpoint < 64; ++checkpoint) {
        p::TapeReader reader(schema(), limits(), p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, header());
        accept(reader, first);
        const auto *prior = reader.descriptor()->version->data();
        const auto identity = reader.schema().recordContext(0);
        const auto before = reader.constructionUnits();
        allocationFailAfter = checkpoint;
        const auto result = reader.feed(replacement);
        allocationFailAfter = -1;
        CHECK(reader.schema().recordContext(0).matches(identity));
        CHECK(reader.constructionUnits() >= before);

        if (result.status == p::TapeStatus::EVENT) {
            succeeded = true;
            break;
        }

        ++failures;

        if (reader.constructionUnits() > before) {
            ++chargedFailures;
        }

        CHECK(reader.diagnostic().error == p::TapeError::ALLOCATION_FAILURE);
        CHECK(reader.descriptor()->version->data() == prior);
        CHECK(*reader.descriptor()->version == std::u16string(128, u'A'));
    }

    CHECK(succeeded);
    CHECK(failures > 0);
    CHECK(chargedFailures > 0);
}

/** Route ordinary arrays through the same test-only checkpoint.
 * @param size Requested bytes.
 * @return Owned storage or bad_alloc.
 */
void *operator new[](std::size_t size) {
    return ::operator new(size);
}

/** Release ordinary test-owned storage.
 * @param storage Previously allocated pointer or null.
 */
void operator delete(void *storage) noexcept {
    std::free(storage);
}

/** Release ordinary test-owned array storage.
 * @param storage Previously allocated pointer or null.
 */
void operator delete[](void *storage) noexcept {
    std::free(storage);
}

/** Release sized storage without depending on allocator metadata.
 * @param storage Previously allocated pointer.
 * @param size Compiler-provided size, unused.
 */
void operator delete(void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

/** Release sized array storage.
 * @param storage Previously allocated pointer.
 * @param size Compiler-provided size, unused.
 */
void operator delete[](void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

TEST_CASE("active sparse replacements publish fresh identities and stable first-seen order") {
    const auto local = schema();
    p::TapeReader reader(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
    const auto initialWork = reader.constructionUnits();
    CHECK(initialWork > 0);
    accept(reader, header());
    const auto first = description(std::numeric_limits<std::int32_t>::max());
    const auto second = description(7);
    const std::array incoming{first, second};
    accept(reader, descriptions(incoming));
    const auto oldProvenance = reader.schema().recordContext(0);
    auto oldSchema = reader.schema().boundedCopy(limits().context.schema);
    REQUIRE(oldSchema);
    const auto oldContext = *reader.context();
    auto owned = accept(reader, data(first.senderId, 305419896, -1985229329));
    REQUIRE(owned.records.size() == 1);
    REQUIRE(owned.records[0].context.matches(oldProvenance));
    const std::array equal{first};
    accept(reader, descriptions(equal));
    CHECK_FALSE(reader.schema().recordContext(0).matches(oldProvenance));
    CHECK(oldSchema->recordContext(0).matches(oldProvenance));
    CHECK(oldContext.find(first.senderId)->context.matches(oldProvenance));
    CHECK(owned.records[0].typedValues[0].raw == -1985229329);
    REQUIRE(reader.descriptions().size() == 2);
    CHECK(reader.descriptions()[0].senderId == first.senderId);
    CHECK(reader.descriptions()[1].senderId == second.senderId);
    const auto beforeEmpty = reader.schema().recordContext(0);
    const auto beforeWork = reader.constructionUnits();
    accept(reader, descriptions({}));
    CHECK(reader.schema().recordContext(0).matches(beforeEmpty));
    CHECK(reader.constructionUnits() == beforeWork);
    CHECK(reader.finish() == p::TapeStatus::FINISHED);
}

TEST_CASE("one batch validates every intermediate replacement and last valid shape wins") {
    const auto local = schema();
    p::TapeReader reader(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
    accept(reader, header());
    auto first = description();
    auto reordered = first;
    std::swap(reordered.fields[0], reordered.fields[1]);
    auto subset = first;
    subset.fields.erase(subset.fields.begin());
    const std::array batch{first, first, reordered, subset};
    accept(reader, descriptions(batch));
    REQUIRE(reader.descriptions().size() == 1);
    REQUIRE(reader.descriptions()[0].fields.size() == 2);
    CHECK(reader.descriptions()[0].fields[0].name == u"Low");
    std::vector<std::byte> body{std::byte{0xfe}};
    compact(body, 0);
    compact(body, -1985229329);
    compact(body, 913);
    auto values = accept(reader, frame(15, body));
    REQUIRE(values.records.size() == 1);
    CHECK(values.records[0].typedValues[1].raw == 0);
    CHECK(values.records[0].typedValues[0].raw == -1985229329);

    const auto *oldDescriptions = reader.descriptions().data();
    const auto *oldPlan = reader.context()->find(0);
    const auto oldIdentity = reader.schema().recordContext(0);
    auto unsupported = first;
    unsupported.fields[0].serialTypeId = 4;
    const std::array rejected{reordered, unsupported, first};
    const auto result = reader.feed(descriptions(rejected));
    CHECK(result.status == p::TapeStatus::FAILED);
    CHECK(reader.diagnostic().error == p::TapeError::UNSUPPORTED);
    CHECK(reader.descriptions().data() == oldDescriptions);
    CHECK(reader.context()->find(0) == oldPlan);
    CHECK(reader.schema().recordContext(0).matches(oldIdentity));
    CHECK(reader.descriptions()[0].fields.size() == 2);
}

TEST_CASE("late parse failures preserve all prior borrowed storage and provenance") {
    const auto local = schema();
    p::TapeReader reader(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
    accept(reader, header());
    const std::array first{description()};
    accept(reader, descriptions(first));
    const auto *oldRecords = reader.schema().records().data();
    const auto *oldDescriptions = reader.descriptions().data();
    const auto *oldPlan = reader.context()->find(0);
    const auto identity = reader.schema().recordContext(0);
    auto broken = description(3);
    broken.fields[1].serialTypeId = 256;
    const std::array batch{description(2), broken};
    const auto result = reader.feed(descriptions(batch));
    CHECK(result.status == p::TapeStatus::FAILED);
    CHECK(reader.diagnostic().error == p::TapeError::MALFORMED);
    CHECK(reader.schema().records().data() == oldRecords);
    CHECK(reader.descriptions().data() == oldDescriptions);
    CHECK(reader.context()->find(0) == oldPlan);
    CHECK(reader.schema().recordContext(0).matches(identity));
    CHECK(reader.descriptions().size() == 1);
}

TEST_CASE("constructor work is finite and rebuild storms cannot refund or retain history") {
    const auto local = schema();
    auto bounds = limits();
    bounds.constructionUnits = 0;
    CHECK_THROWS_AS(p::TapeReader(local, bounds, p::TapeMode::BINARY_CATALOG_V1), std::invalid_argument);
    bounds = limits();
    bounds.snapshotUnits = 0;
    CHECK_THROWS_AS(p::TapeReader(local, bounds, p::TapeMode::BINARY_CATALOG_V1), std::invalid_argument);

    auto measure = p::TapeReader(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
    accept(measure, header());
    const std::array batch{description()};
    const auto wire = descriptions(batch);
    accept(measure, wire);
    const auto firstWork = measure.constructionUnits();
    const auto before = measure.constructionUnits();
    accept(measure, wire);
    const auto replacementCost = measure.constructionUnits() - before;
    REQUIRE(replacementCost > 0);
    bounds = limits();
    bounds.constructionUnits = firstWork + replacementCost * 2;
    p::TapeReader bounded(local, bounds, p::TapeMode::BINARY_CATALOG_V1);
    accept(bounded, header());
    accept(bounded, wire);
    accept(bounded, wire);
    accept(bounded, wire);
    CHECK(bounded.constructionUnits() == bounds.constructionUnits);
    const auto identity = bounded.schema().recordContext(0);
    const auto failed = bounded.feed(wire);
    CHECK(failed.status == p::TapeStatus::FAILED);
    CHECK(bounded.diagnostic().error == p::TapeError::RESOURCE_LIMIT);
    CHECK(bounded.constructionUnits() == bounds.constructionUnits);
    CHECK(bounded.schema().recordContext(0).matches(identity));
    CHECK(bounded.descriptions().size() == 1);
}

TEST_CASE("exact schema staging and replacement peak units are finite and preserve rollback") {
    // Independent contract arithmetic: S=28, empty C=4, D=23, active C=9.
    // The effective NONE/empty-options descriptor contributes seven retained units.
    // First peak is old32 + header7 + candidate23 + new S28 + new C9 =99.
    // Replacement peak is old60 + header7 + raw23 + old candidate23 + replacement copy23 =136.
    constexpr std::size_t INITIAL_UNITS = 32;
    constexpr std::size_t FIRST_PEAK = 99;
    constexpr std::size_t REPLACEMENT_PEAK = 136;
    const auto local = schema();
    const std::array batch{description()};
    const auto wire = descriptions(batch);

    for (const auto maximum : {std::size_t{0}, INITIAL_UNITS - 1}) {
        auto bounds = limits();
        bounds.snapshotUnits = maximum;
        CHECK_THROWS_AS(p::TapeReader(local, bounds, p::TapeMode::BINARY_CATALOG_V1), std::invalid_argument);
    }

    for (const auto maximum : {FIRST_PEAK - 1, FIRST_PEAK, FIRST_PEAK + 1}) {
        auto bounds = limits();
        bounds.snapshotUnits = maximum;
        p::TapeReader reader(local, bounds, p::TapeMode::BINARY_CATALOG_V1);
        CHECK(reader.constructionUnits() == INITIAL_UNITS);
        accept(reader, header());
        const auto identity = reader.schema().recordContext(0);
        const auto *oldRecords = reader.schema().records().data();
        const auto result = reader.feed(wire);

        if (maximum < FIRST_PEAK) {
            CHECK(result.status == p::TapeStatus::FAILED);
            CHECK(reader.diagnostic().error == p::TapeError::RESOURCE_LIMIT);
            CHECK(reader.schema().recordContext(0).matches(identity));
            CHECK(reader.schema().records().data() == oldRecords);
        } else {
            CHECK(result.status == p::TapeStatus::EVENT);
            CHECK_FALSE(reader.schema().recordContext(0).matches(identity));
        }
    }

    for (const auto maximum : {REPLACEMENT_PEAK - 1, REPLACEMENT_PEAK, REPLACEMENT_PEAK + 1}) {
        auto bounds = limits();
        bounds.snapshotUnits = maximum;
        p::TapeReader reader(local, bounds, p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, header());
        accept(reader, wire);
        const auto identity = reader.schema().recordContext(0);
        const auto *oldDescription = reader.descriptions().data();
        const auto result = reader.feed(wire);

        if (maximum < REPLACEMENT_PEAK) {
            CHECK(result.status == p::TapeStatus::FAILED);
            CHECK(reader.diagnostic().error == p::TapeError::RESOURCE_LIMIT);
            CHECK(reader.schema().recordContext(0).matches(identity));
            CHECK(reader.descriptions().data() == oldDescription);
        } else {
            CHECK(result.status == p::TapeStatus::EVENT);
            CHECK_FALSE(reader.schema().recordContext(0).matches(identity));

            for (std::size_t repeat = 0; repeat < 100; ++repeat) {
                accept(reader, wire);
                CHECK(reader.descriptions().size() == 1);
            }
        }
    }
}

TEST_CASE("exact work boundaries include constructor parse validation copy and final rebuild") {
    // The full72 field type occupies two compact bytes: payload work is29, not28.
    // Header work is payload46 + candidate1 + property text reservations30 + message name13 =90.
    constexpr std::size_t FIRST_WORK = 238;
    constexpr std::size_t REPLACEMENT_WORK = 139;
    const auto local = schema();
    const std::array batch{description()};
    const auto wire = descriptions(batch);

    for (const auto maximum : {FIRST_WORK - 1, FIRST_WORK, FIRST_WORK + 1}) {
        auto bounds = limits();
        bounds.constructionUnits = maximum;
        p::TapeReader reader(local, bounds, p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, header());
        const auto identity = reader.schema().recordContext(0);
        const auto result = reader.feed(wire);
        CHECK(result.status == (maximum < FIRST_WORK ? p::TapeStatus::FAILED : p::TapeStatus::EVENT));

        if (maximum < FIRST_WORK) {
            CHECK(reader.diagnostic().error == p::TapeError::RESOURCE_LIMIT);
            CHECK(reader.schema().recordContext(0).matches(identity));
            CHECK(reader.constructionUnits() > 32);
        } else {
            CHECK(reader.constructionUnits() == FIRST_WORK);
        }
    }

    for (const auto maximum :
         {FIRST_WORK + REPLACEMENT_WORK - 1, FIRST_WORK + REPLACEMENT_WORK, FIRST_WORK + REPLACEMENT_WORK + 1}) {
        auto bounds = limits();
        bounds.constructionUnits = maximum;
        p::TapeReader reader(local, bounds, p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, header());
        accept(reader, wire);
        const auto identity = reader.schema().recordContext(0);
        const auto result = reader.feed(wire);
        CHECK(result.status ==
              (maximum < FIRST_WORK + REPLACEMENT_WORK ? p::TapeStatus::FAILED : p::TapeStatus::EVENT));

        if (result.status == p::TapeStatus::FAILED) {
            CHECK(reader.schema().recordContext(0).matches(identity));
            CHECK(reader.constructionUnits() > FIRST_WORK);
        } else {
            CHECK(reader.constructionUnits() == FIRST_WORK + REPLACEMENT_WORK);
        }
    }
}

TEST_CASE("every replacement allocation failure preserves old borrowed declarations and contexts") {
    const auto local = schema();
    const std::array first{description()};
    auto next = description();
    next.fields[0].name = u"AnExplicitUnknownFieldNameLongerThanSmallStringStorage";
    const std::array replacement{next, description(7)};
    const auto firstWire = descriptions(first);
    const auto nextWire = descriptions(replacement);
    std::size_t failedCheckpoints = 0;
    bool completed = false;

    for (std::ptrdiff_t checkpoint = 0; checkpoint < 256; ++checkpoint) {
        p::TapeReader reader(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, header());
        accept(reader, firstWire);
        const auto *oldRecords = reader.schema().records().data();
        const auto *oldDescriptions = reader.descriptions().data();
        const auto *oldPlan = reader.context()->find(0);
        const auto identity = reader.schema().recordContext(0);
        const auto beforeWork = reader.constructionUnits();
        allocationFailAfter = checkpoint;
        auto result = reader.feed(nextWire);
        allocationFailAfter = -1;

        if (result.status == p::TapeStatus::EVENT) {
            CHECK_FALSE(reader.schema().recordContext(0).matches(identity));
            CHECK(reader.descriptions().size() == 2);
            completed = true;
            break;
        }

        REQUIRE(result.status == p::TapeStatus::FAILED);
        CHECK(reader.diagnostic().error == p::TapeError::ALLOCATION_FAILURE);
        CHECK(reader.schema().records().data() == oldRecords);
        CHECK(reader.descriptions().data() == oldDescriptions);
        CHECK(reader.context()->find(0) == oldPlan);
        CHECK(reader.schema().recordContext(0).matches(identity));
        CHECK(reader.constructionUnits() >= beforeWork);
        ++failedCheckpoints;
    }

    CHECK(completed);
    CHECK(failedCheckpoints > 20);
}

TEST_CASE("fresh independent reader resets bindings while owned old outputs survive destruction") {
    p::TapeFeedResult oldOutput;

    {
        const auto local = schema();
        p::TapeReader reader(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, header());
        const std::array batch{description()};
        accept(reader, descriptions(batch));
        oldOutput = accept(reader, data(0, -1, 0));
        CHECK(reader.finish() == p::TapeStatus::FINISHED);
    }

    REQUIRE(oldOutput.records.size() == 1);
    CHECK(oldOutput.records[0].context.valid());
    CHECK(oldOutput.records[0].typedValues[1].raw == -1);
    const auto local = schema();
    p::TapeReader fresh(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
    accept(fresh, header());
    const auto result = fresh.feed(data(0, -1, 0));
    CHECK(result.status == p::TapeStatus::FAILED);
    CHECK(fresh.descriptions().empty());
    CHECK_FALSE(fresh.schema().recordContext(0).matches(oldOutput.records[0].context));
}

TEST_CASE("completed opaque payload ownership and work survive indexed description replacement") {
    auto record = pairRecord();
    record.fields.push_back({9});
    record.lookups.push_back({u"Bytes", 3});
    const std::array records{record};
    auto bounds = limits();
    bounds.data.fieldPayloadEncodedBytes = 16;
    bounds.data.fieldPayloadBytes = 8;
    bounds.data.payloadEncodedBytes = 32;
    bounds.data.payloadWorkBytes = 16;
    bounds.data.ownedPayloadBytes = 8;
    auto local = c::LocalSchema::create(records, bounds.context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    p::TapeReader reader(*local, bounds, p::TapeMode::BINARY_CATALOG_V1);
    accept(reader, header());
    auto raw = description();
    raw.fields.push_back({u"Bytes", 9});
    const std::array initial{raw};
    accept(reader, descriptions(initial));
    std::vector<std::byte> body{std::byte{0xfe}};
    compact(body, 0);
    compact(body, -1);
    compact(body, 0);
    compact(body, 913);
    compact(body, 8);
    body.insert(body.end(), 8, std::byte{0xa5});
    auto output = accept(reader, frame(15, body));
    REQUIRE(output.records.size() == 1);
    REQUIRE(output.records[0].typedValues[3].bytes);
    CHECK(output.records[0].typedValues[3].bytes->size() == 8);
    const auto oldIdentity = output.records[0].context;
    CHECK(reader.payloadWorkBytes() == 8);
    CHECK(reader.ownedPayloadBytes() == 8);
    accept(reader, descriptions(initial));
    CHECK_FALSE(reader.schema().recordContext(0).matches(oldIdentity));
    CHECK(output.records[0].typedValues[3].bytes->front() == std::byte{0xa5});
    CHECK(reader.payloadWorkBytes() == 8);
    CHECK(reader.ownedPayloadBytes() == 8);
    const auto failed = reader.feed(frame(15, body));
    CHECK(failed.status == p::TapeStatus::FAILED);
    CHECK(reader.diagnostic().error == p::TapeError::RESOURCE_LIMIT);
    CHECK(reader.ownedPayloadBytes() == 8);
    CHECK(output.records[0].typedValues[3].bytes->back() == std::byte{0xa5});
}

TEST_CASE("all incomplete replacement envelopes preserve the old generation until clean publication") {
    const auto local = schema();
    const std::array first{description()};
    auto changed = description(7);
    changed.name = u"A\u0416\U0001f600";
    const std::array next{changed};
    const auto update = descriptions(next);

    for (std::size_t cut = 1; cut < update.size(); ++cut) {
        p::TapeReader reader(local, limits(), p::TapeMode::BINARY_CATALOG_V1);
        accept(reader, header());
        accept(reader, descriptions(first));
        const auto *oldDescriptions = reader.descriptions().data();
        const auto identity = reader.schema().recordContext(0);
        const auto result = reader.feed(std::span(update).first(cut));
        CHECK(result.status == p::TapeStatus::NEED_INPUT);
        CHECK(reader.finish() == p::TapeStatus::FAILED);
        CHECK(reader.descriptions().data() == oldDescriptions);
        CHECK(reader.schema().recordContext(0).matches(identity));
    }
}

TEST_CASE("allocation-free session name admission preserves public UTFString grammar and error positions") {
    const auto local = schema();
    const std::array<std::vector<std::byte>, 6> names{
        {{std::byte{0x7f}},                                   // Null UTFString is structurally an invalid record name.
         {std::byte{0x7e}},                                   // Negative length other than null.
         {std::byte{0x00}},                                   // Empty record name.
         {std::byte{0x01}, std::byte{0x80}},                  // Invalid continuation lead.
         {std::byte{0x01}, std::byte{0xc2}, std::byte{0xa2}}, // Full UTF tail exceeds body length.
         {std::byte{0x01}, std::byte{'A'}}}};

    for (const auto nameUnits : {std::size_t{0}, std::size_t{1024}}) {
        for (const auto &encodedName : names) {
            auto bounds = limits();
            bounds.context.bindings.nameUnits = nameUnits;
            std::vector<std::byte> payload;
            compact(payload, 0);
            payload.insert(payload.end(), encodedName.begin(), encodedName.end());
            compact(payload, 0);
            p::TapeReader candidate(local, bounds, p::TapeMode::BINARY_CATALOG_V1);
            accept(candidate, header());
            const auto raw = p::parseDescribeRecords(payload, {bounds.frame.maximumBodyBytes,
                                                               bounds.context.descriptions,
                                                               bounds.context.bindings.fields,
                                                               bounds.context.bindings.fields,
                                                               {bounds.frame.maximumBodyBytes, nameUnits},
                                                               bounds.context.bindings.totalNameUnits});
            const auto result = candidate.feed(frame(2, payload));

            if (raw.status == p::DescriptionStatus::OK) {
                CHECK(result.status == p::TapeStatus::EVENT);
            } else {
                CHECK(result.status == p::TapeStatus::FAILED);
                CHECK(candidate.diagnostic().consumed == raw.consumed);
                CHECK(candidate.diagnostic().errorOffset == raw.errorOffset);
                CHECK(candidate.diagnostic().detail == static_cast<std::size_t>(raw.stage));
            }
        }
    }
}
