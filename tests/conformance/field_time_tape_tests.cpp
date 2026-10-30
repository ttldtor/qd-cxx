// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "indexed_test_support.hpp"

#include <bit>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <new>
#include <sstream>

namespace {
std::ptrdiff_t allocationFailAfter = -1; ///< One test-process allocation checkpoint, disabled outside scoped calls.
}

/** Allocate ordinary test storage with a deterministic optional failure.
 * @param size Requested bytes, with zero receiving deletable storage.
 * @return Storage or bad_alloc at the selected checkpoint.
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

/** Allocate an array through the same process-local checkpoint.
 * @param size Requested bytes.
 * @return Storage or bad_alloc.
 */
void *operator new[](std::size_t size) {
    return ::operator new(size);
}

/** Release ordinary test storage.
 * @param storage Owned allocation or null.
 */
void operator delete(void *storage) noexcept {
    std::free(storage);
}

/** Release array test storage.
 * @param storage Owned allocation or null.
 */
void operator delete[](void *storage) noexcept {
    std::free(storage);
}

/** Release sized ordinary storage.
 * @param storage Owned allocation or null.
 * @param size Compiler-provided size, unused by free.
 */
void operator delete(void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

/** Release sized array storage.
 * @param storage Owned allocation or null.
 * @param size Compiler-provided size, unused by free.
 */
void operator delete[](void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

namespace {
using namespace indexed_test;
constexpr auto READ = p::FieldTimePolicy::READ;
std::array<std::filesystem::path, 2> fixtureRoots; ///< Explicit immutable producer-pin expectations.

/** Supply FIELD limits with a separately finite fixed-owner allowance.
 * @return Independent generous test limits.
 */
p::TapeLimits fieldLimits() {
    auto result = limits();
    result.data.ownedFieldTimeRecords = 100;

    return result;
}

/** Supply an ordinary business schema, with a deliberate metadata-name/type collision.
 * @return Owned validated declarations.
 */
c::LocalSchema fieldSchema() {
    const std::array records{
        c::LocalRecord{0, u"Test", false, {{8}, {8}, {8}}, {{u"Value", 0}, {u"EventPrice", 1}, {u"EventTime", 2}}}};
    auto result = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Construct a sender declaration with a metadata pair and an ordinary leading Event value.
 * @param sender Sparse source ID.
 * @return Owning physical fields in wire order.
 */
p::WireRecordDescription fieldDescription(std::int32_t sender = 91) {
    return {sender, u"Test", {{u"EventTime", 56}, {u"EventSequence", 72}, {u"EventPrice", 8}, {u"Value", 8}}};
}

/** Construct an independent snapshot after temporary sources die.
 * @param description Exact physical fields.
 * @return Owned immutable plan/defaults.
 */
p::StreamDataContext fieldContext(p::WireRecordDescription description = fieldDescription()) {
    auto schema = fieldSchema();
    const std::array descriptions{description};
    auto result = p::StreamDataContext::createWithFieldTimePolicy(schema, descriptions, limits().context, READ);
    REQUIRE(result);

    return std::move(*result);
}

/** Assemble raw compact values without duplicating decoding/projection.
 * @param words Physical values after symbol and ID.
 * @param sender Sender ID.
 * @param flags Exact known event flags.
 * @return Owned complete data body.
 */
std::vector<std::byte> body(std::initializer_list<std::int64_t> words, std::int32_t sender = 91,
                            std::int32_t flags = 0) {
    std::vector<std::byte> result;

    if (flags != 0) {
        result.push_back(std::byte{0xf9});
        compact(result, flags);
    }

    result.push_back(std::byte{0xfe});
    compact(result, sender);

    for (const auto word : words) {
        compact(result, word);
    }

    return result;
}

/** Decode with the explicit known-flags policy.
 * @param bytes Complete borrowed body.
 * @param context Validated owned bindings.
 * @param budget Finite per-message allowances.
 * @return Owned prefix and first failure.
 */
p::StreamDataResult decode(std::span<const std::byte> bytes, const p::StreamDataContext &context,
                           p::StreamDataLimits budget = fieldLimits().data) {
    return p::decodeStreamDataWithEventPolicy(bytes, context, budget, p::DataEventPolicy::BINARY_CATALOG_V1);
}
} // namespace

TEST_CASE("FIELD raw words and exact low32 projection survive all owners") {
    auto context = fieldContext();
    auto copy = context;
    const auto bytes = body({std::numeric_limits<std::int64_t>::min(), -1, 71, 93});
    auto result = decode(bytes, copy);
    REQUIRE(result.status == p::StreamDataStatus::OK);
    REQUIRE(result.records.size() == 1);
    const auto &record = result.records[0];
    REQUIRE(record.fieldTime);
    CHECK(record.fieldTime->rawSeconds == std::numeric_limits<std::int64_t>::min());
    CHECK(record.fieldTime->rawSequence == -1);
    CHECK(record.fieldTime->packed() == 0xffffffffLL);
    CHECK(record.fieldTime->eventTimeMs() == 1023);
    CHECK(record.typedValues[0].raw == 93);
    CHECK(record.typedValues[1].raw == 71);
    CHECK(record.typedValues[2].raw == 0);
    CHECK(result.ownedFieldTimeRecords == 1);
    auto factory = &p::StreamDataContext::create;
    static_cast<void>(factory);
}

TEST_CASE("FIELD finite projection covers signed extrema and sequence bit boundaries") {
    for (const auto seconds : {INT64_MIN, INT64_MAX, std::int64_t{-1}, std::int64_t{0}, std::int64_t{1}}) {
        for (const auto sequence : {INT64_MIN, INT64_MAX, std::int64_t{-1}, std::int64_t{0}, std::int64_t{1},
                                    std::int64_t{0x7fffffff}, std::int64_t{0x80000000}}) {
            p::FieldEventTime value{seconds, sequence, 0};
            const auto high = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(seconds));
            const auto low = static_cast<std::uint32_t>(sequence);
            CHECK(value.packed() ==
                  std::bit_cast<std::int64_t>((std::uint64_t{static_cast<std::uint32_t>(seconds)} << 32) | low));
            CHECK(value.eventTimeMs() == std::int64_t{high} * 1000 + ((low >> 22) & 1023));
        }
    }
}

TEST_CASE("FIELD recognizes only the exact raw first prefix before VOID elision") {
    auto schema = fieldSchema();

    for (auto fields : {std::vector<p::WireFieldDescription>{{u"EventTime", 56}},
                        std::vector<p::WireFieldDescription>{{u"EventTime", 56}, {u"Void", 0}, {u"EventSequence", 72}},
                        std::vector<p::WireFieldDescription>{{u"Void", 0}, {u"EventTime", 56}},
                        std::vector<p::WireFieldDescription>{{u"eventTime", 56}},
                        std::vector<p::WireFieldDescription>{{u"EventSequence", 72}, {u"EventTime", 56}}}) {
        const std::array descriptions{p::WireRecordDescription{91, u"Other", fields}};
        auto context = p::StreamDataContext::createWithFieldTimePolicy(schema, descriptions, limits().context, READ);
        REQUIRE(context);
        const auto *plan = context->find(91);
        REQUIRE(plan);
        CHECK(plan->eventTimeFieldCount == (fields[0].name == u"EventTime" ? 1 : 0));
    }
}

TEST_CASE("FIELD mapped output metadata has an early separate finite budget") {
    const auto context = fieldContext();
    const auto first = body({0, 0, 1, 2});
    auto twice = first;
    twice.insert(twice.end(), first.begin(), first.end());

    for (std::size_t limit : {0U, 1U, 2U, 3U}) {
        auto budget = fieldLimits().data;
        budget.ownedFieldTimeRecords = limit;
        auto result = decode(twice, context, budget);
        CHECK(result.records.size() == std::min(limit, std::size_t{2}));
        CHECK(result.ownedFieldTimeRecords == result.records.size());
        CHECK(result.status == (limit < 2 ? p::StreamDataStatus::RESOURCE_LIMIT : p::StreamDataStatus::OK));

        if (limit < 2) {
            CHECK(result.stage == p::StreamDataStage::OUTPUT);
            CHECK(result.wireFields == limit * 4);
        }
    }
}

TEST_CASE("FIELD truncations never publish partial current metadata") {
    const auto context = fieldContext();
    const auto first = body({INT64_MAX, INT64_MIN, 71, 93});

    for (std::size_t cut = 1; cut < first.size(); ++cut) {
        auto bytes = first;
        bytes.insert(bytes.end(), first.begin(), first.begin() + static_cast<std::ptrdiff_t>(cut));
        const auto result = decode(bytes, context);
        CHECK(result.status != p::StreamDataStatus::OK);
        REQUIRE(result.records.size() == 1);
        CHECK(result.ownedFieldTimeRecords == 1);
        CHECK(result.records[0].fieldTime->rawSeconds == INT64_MAX);
    }
}

TEST_CASE("FIELD REMOVE consumes all leading ordinary Event actions before removal time") {
    const auto context = fieldContext();
    auto bytes = body({7, -1, 91, 1234}, 91, 2);
    const auto next = body({8, 0, 72, 94});
    bytes.insert(bytes.end(), next.begin(), next.end());
    const auto result = decode(bytes, context);
    REQUIRE(result.status == p::StreamDataStatus::OK);
    REQUIRE(result.records.size() == 2);
    CHECK(result.records[0].fieldTime->rawSeconds == 7);
    CHECK(result.records[0].typedValues[1].raw == 91);
    CHECK(result.records[0].typedValues[0].raw == 0);
    CHECK(result.records[0].removalTime == 1234);
    CHECK(result.records[1].fieldTime->sourceOrdinal == 1);
    CHECK(result.records[1].typedValues[0].raw == 94);
}

TEST_CASE("FIELD skips unknown records safely and counts their source attempts without owners") {
    auto schema = fieldSchema();
    auto unknown = fieldDescription(92);
    unknown.name = u"Unknown";
    const std::array declarations{unknown, fieldDescription()};
    auto context = p::StreamDataContext::createWithFieldTimePolicy(schema, declarations, limits().context, READ);
    REQUIRE(context);
    auto bytes = body({INT64_MAX, -1, 4, 5}, 92);
    const auto known = body({0, 0, 6, 7});
    bytes.insert(bytes.end(), known.begin(), known.end());
    auto budget = fieldLimits().data;
    budget.ownedFieldTimeRecords = 1;
    const auto result = decode(bytes, *context, budget);
    REQUIRE(result.status == p::StreamDataStatus::OK);
    REQUIRE(result.records.size() == 1);
    CHECK(result.records[0].fieldTime->sourceOrdinal == 1);
    CHECK(result.wireFields == 8);
    CHECK(result.ownedFieldTimeRecords == 1);
}

TEST_CASE("FIELD reader retains cumulative ordinals across all four families and metadata owners") {
    auto budget = fieldLimits();
    budget.data.ownedFieldTimeRecords = 4;
    p::TapeReader reader(budget, p::TapeDiscovery::BINARY_CATALOG_V1, READ);
    const std::array declarations{fieldDescription()};
    accept(reader, descriptions(declarations));
    CHECK(reader.schema().records()[0].fields.size() == 2);
    CHECK(reader.context()->find(91)->eventTimeFieldCount == 2);
    std::uint64_t ordinal = 0;

    for (const auto family : {5, 10, 15, 20}) {
        const auto event = accept(reader, frame(family, body({0, 0, 1, 2})));
        REQUIRE(event.records.size() == 1);
        REQUIRE(event.records[0].fieldTime);
        CHECK(event.records[0].fieldTime->sourceOrdinal == ordinal++);
        CHECK(event.timing.has_value());
    }

    CHECK(reader.ownedFieldTimeRecords() == 4);
    const auto failure = reader.feed(frame(15, body({0, 0, 1, 2})));
    CHECK(failure.status == p::TapeStatus::FAILED);
    CHECK(reader.diagnostic().error == p::TapeError::RESOURCE_LIMIT);
    CHECK(reader.feed({}).accepted == 0);
}

TEST_CASE("FIELD READ policy refuses old modes and invalid enums before input") {
    const auto schema = fieldSchema();
    CHECK_THROWS_AS(p::TapeReader(schema, fieldLimits(), p::TapeMode::LEGACY, READ), std::invalid_argument);
    CHECK_THROWS_AS(
        p::TapeReader(fieldLimits(), p::TapeDiscovery::BINARY_CATALOG_V1, static_cast<p::FieldTimePolicy>(-1)),
        std::invalid_argument);
    const auto context = p::StreamDataContext::createWithFieldTimePolicy(schema, {}, limits().context,
                                                                         static_cast<p::FieldTimePolicy>(-1));
    CHECK_FALSE(context);
    CHECK(context.error() == p::BindingError::UNSUPPORTED_PROFILE);

    for (const auto mode : {p::TapeMode::LEGACY, p::TapeMode::BINARY_CATALOG_V1}) {
        bool invalid = false;
        bool allocation = false;
        // Allow the exception's own message allocation; no schema/default/framer allocation may precede it.
        allocationFailAfter = 1;

        try {
            p::TapeReader reader(schema, fieldLimits(), mode,
                                 mode == p::TapeMode::LEGACY ? READ : static_cast<p::FieldTimePolicy>(-1));
        } catch (const std::invalid_argument &) {
            invalid = true;
        } catch (const std::bad_alloc &) {
            allocation = true;
        }

        allocationFailAfter = -1;
        CHECK(invalid);
        CHECK_FALSE(allocation);
    }
}

TEST_CASE("FIELD physical REMOVE prefixes retain wrong types and explicit VOID work") {
    const std::vector<std::vector<p::WireFieldDescription>> layouts{
        {{u"EventTime", 56}, {u"EventSequence", 72}, {u"Gap", 0}, {u"EventPrice", 8}, {u"Value", 8}},
        {{u"Gap", 0}, {u"EventTime", 56}, {u"EventSequence", 72}, {u"EventPrice", 8}, {u"Value", 8}},
        {{u"EventTime", 56}, {u"Gap", 0}, {u"EventSequence", 72}, {u"EventPrice", 8}, {u"Value", 8}},
        {{u"EventTime", 56}, {u"EventSequence", 8}, {u"EventPrice", 8}, {u"Gap", 0}, {u"Value", 8}},
        {{u"EventTime", 8}, {u"EventSequence", 8}, {u"EventPrice", 8}, {u"Value", 8}}};

    for (std::size_t index = 0; index < layouts.size(); ++index) {
        CAPTURE(index);
        p::TapeReader reader(fieldLimits(), p::TapeDiscovery::BINARY_CATALOG_V1, READ);
        const std::array declarations{p::WireRecordDescription{91, u"Test", layouts[index]}};
        accept(reader, descriptions(declarations));
        const auto *plan = reader.context()->find(91);
        REQUIRE(plan);
        const std::size_t metadata = index == 0 ? 2 : (index == 2 || index == 3 ? 1 : 0);
        CHECK(plan->eventTimeFieldCount == metadata);
        CHECK(plan->removalPrefixFieldCount == layouts[index].size() - 1);
        const auto event = accept(reader, frame(15, body({7, 8, 9, 1234}, 91, 2)));
        REQUIRE(event.records.size() == 1);
        const auto &record = event.records[0];
        CHECK(record.fieldTime.has_value() == (metadata != 0));
        CHECK(record.removalTime == 1234);
        CHECK(record.typedValues.back().raw == 0);
        CHECK(reader.wireFields() == layouts[index].size() - 1);
        const auto &local = reader.schema().records()[0];

        for (const auto &lookup : local.lookups) {
            if (lookup.name == u"EventPrice") {
                CHECK(record.typedValues[lookup.fieldId].raw == 9);
            }

            if (lookup.name == u"EventSequence") {
                CHECK(record.typedValues[lookup.fieldId].raw == 8);
            }
        }
    }
}

TEST_CASE("FIELD intermediate replacement validates the same prelookup policy atomically") {
    auto schema = fieldSchema();
    p::TapeReader reader(schema, fieldLimits(), p::TapeMode::BINARY_CATALOG_V1, READ);
    const std::array initial{fieldDescription()};
    accept(reader, descriptions(initial));
    const auto *old = reader.context();
    const auto *oldRaw = reader.descriptions().data();
    auto bad = fieldDescription();
    bad.fields.push_back({u"Unsupported", 255});
    const std::array batch{fieldDescription(), bad, fieldDescription()};
    const auto result = reader.feed(descriptions(batch));
    CHECK(result.status == p::TapeStatus::FAILED);
    CHECK(reader.context() == old);
    CHECK(reader.descriptions().data() == oldRaw);
    CHECK(reader.context()->find(91)->eventTimeFieldCount == 2);
    CHECK(reader.descriptions()[0].fields.size() == 4);
}

TEST_CASE("FIELD reader publication allocation failures retain borrowed addresses and old owned outputs") {
    const auto initial = descriptions(std::array{fieldDescription()});
    auto replacement = fieldDescription();
    replacement.fields.erase(replacement.fields.begin() + 1);
    const auto update = descriptions(std::array{replacement});
    std::size_t failures = 0;
    bool completed = false;

    for (std::ptrdiff_t checkpoint = 0; checkpoint < 160; ++checkpoint) {
        auto schema = fieldSchema();
        p::TapeReader reader(schema, fieldLimits(), p::TapeMode::BINARY_CATALOG_V1, READ);
        accept(reader, initial);
        auto prior = accept(reader, frame(15, body({0, 0, 71, 93})));
        auto oldContext = *reader.context();
        const auto *oldPlan = reader.context()->find(91);
        const auto *oldRaw = reader.descriptions().data();
        const auto *oldSchema = reader.schema().records().data();
        allocationFailAfter = checkpoint;
        const auto result = reader.feed(update);
        allocationFailAfter = -1;

        if (result.status == p::TapeStatus::FAILED) {
            ++failures;
            CHECK(reader.diagnostic().error == p::TapeError::ALLOCATION_FAILURE);
            CHECK(reader.context()->find(91) == oldPlan);
            CHECK(reader.descriptions().data() == oldRaw);
            CHECK(reader.schema().records().data() == oldSchema);
            CHECK(oldContext.find(91)->eventTimeFieldCount == 2);
            CHECK(prior.records[0].fieldTime->packed() == 0);
        } else {
            CHECK(reader.context()->find(91)->eventTimeFieldCount == 1);
            CHECK(oldContext.find(91)->eventTimeFieldCount == 2);
            CHECK(prior.records[0].typedValues[0].raw == 93);
            completed = true;
            break;
        }
    }

    CHECK(failures > 0);
    CHECK(completed);
}

TEST_CASE("FIELD fixed plan tags consume both intermediate and final construction units") {
    auto declaration = fieldDescription();
    declaration.name = u"Unknown";
    const auto bytes = descriptions(std::array{declaration});
    auto schema = fieldSchema();
    p::TapeReader disabled(schema, fieldLimits(), p::TapeMode::BINARY_CATALOG_V1);
    accept(disabled, bytes);
    p::TapeReader enabled(schema, fieldLimits(), p::TapeMode::BINARY_CATALOG_V1, READ);
    accept(enabled, bytes);
    CHECK(enabled.constructionUnits() == disabled.constructionUnits() + 4);
    const auto required = enabled.constructionUnits();

    for (const auto limit : {required - 1, required, required + 1, std::numeric_limits<std::size_t>::max()}) {
        auto budget = fieldLimits();
        budget.constructionUnits = limit;
        p::TapeReader reader(schema, budget, p::TapeMode::BINARY_CATALOG_V1, READ);
        const auto event = reader.feed(bytes);
        CHECK(event.status == (limit < required ? p::TapeStatus::FAILED : p::TapeStatus::EVENT));
    }
}

TEST_CASE("FIELD removal metadata coexists with every admitted known flag subset") {
    const auto context = fieldContext();

    for (std::int32_t flags = 0; flags < 128; ++flags) {
        CAPTURE(flags);
        const auto result = decode(body({7, 8, 9, 10}, 91, flags), context);

        if ((flags & ~0x5f) != 0) {
            CHECK(result.status == p::StreamDataStatus::UNSUPPORTED_PROFILE);
            CHECK(result.stage == p::StreamDataStage::FLAGS);
            CHECK(result.records.empty());
        } else {
            REQUIRE(result.status == p::StreamDataStatus::OK);
            REQUIRE(result.records.size() == 1);
            CHECK(result.records[0].symbol.eventFlags == flags);
            CHECK(result.records[0].fieldTime->rawSeconds == 7);
            CHECK(result.records[0].fieldTime->rawSequence == 8);
            CHECK(result.records[0].typedValues[1].raw == 9);
            CHECK(result.records[0].typedValues[0].raw == ((flags & 2) != 0 ? 0 : 10));
            CHECK(result.records[0].removalTime.has_value() == ((flags & 2) != 0));
        }
    }
}

TEST_CASE("FIELD prefix charges exact wire attempts before current publication") {
    const auto context = fieldContext();

    for (std::size_t fields = 0; fields <= 5; ++fields) {
        auto budget = fieldLimits().data;
        budget.wireFields = fields;
        const auto result = decode(body({0, 0, 1, 2}), context, budget);
        CHECK(result.wireFields == std::min(fields, std::size_t{4}));
        CHECK(result.status == (fields < 4 ? p::StreamDataStatus::RESOURCE_LIMIT : p::StreamDataStatus::OK));
        CHECK(result.ownedFieldTimeRecords == (fields < 4 ? 0 : 1));
    }

    auto budget = fieldLimits().data;
    budget.processedRecords = std::numeric_limits<std::size_t>::max();
    budget.wireFields = std::numeric_limits<std::size_t>::max();
    budget.ownedFieldTimeRecords = std::numeric_limits<std::size_t>::max();
    CHECK(decode(body({0, 0, 1, 2}), context, budget).status == p::StreamDataStatus::OK);
}

TEST_CASE("FIELD discovery retains ordinary names across prefix to business to prefix generations") {
    p::TapeReader reader(fieldLimits(), p::TapeDiscovery::BINARY_CATALOG_V1, READ);
    const auto prefix = fieldDescription();
    accept(reader, descriptions(std::array{prefix}));
    auto business = prefix;
    business.fields[0].serialTypeId = 8;
    business.fields[1].serialTypeId = 8;
    accept(reader, descriptions(std::array{business}));
    CHECK(reader.context()->find(91)->eventTimeFieldCount == 0);
    accept(reader, descriptions(std::array{prefix}));
    CHECK(reader.context()->find(91)->eventTimeFieldCount == 2);
    const auto event = accept(reader, frame(15, body({7, 8, 9, 10})));
    REQUIRE(event.records.size() == 1);
    const auto &local = reader.schema().records()[0];
    CHECK(local.fields.size() == 4);

    for (const auto &lookup : local.lookups) {
        if (lookup.name == u"EventTime" || lookup.name == u"EventSequence") {
            CHECK(event.records[0].typedValues[lookup.fieldId].raw == 0);
        }
    }

    auto conflicting = prefix;
    conflicting.fields.insert(conflicting.fields.begin(), {u"Other", 8});
    const auto *old = reader.schema().records().data();
    const auto *oldPlan = reader.context()->find(91);
    const auto failure = reader.feed(descriptions(std::array{conflicting}));
    CHECK(failure.status == p::TapeStatus::FAILED);
    CHECK(reader.diagnostic().rejection == p::TapeRejection::TYPE);
    CHECK(reader.schema().records().data() == old);
    CHECK(reader.context()->find(91) == oldPlan);
}

TEST_CASE("FIELD replacement snapshot peak rejects L minus one and preserves all prior owners") {
    auto schema = fieldSchema();
    auto changed = fieldDescription();
    changed.fields.insert(changed.fields.end() - 1, {u"EventPrice", 8});
    const auto initial = descriptions(std::array{fieldDescription()});
    const auto update = descriptions(std::array{changed});
    const auto admits = [&](std::size_t maximum) {
        auto budget = fieldLimits();
        budget.snapshotUnits = maximum;

        try {
            p::TapeReader reader(schema, budget, p::TapeMode::BINARY_CATALOG_V1, READ);

            if (reader.feed(initial).status != p::TapeStatus::EVENT) {
                return false;
            }

            return reader.feed(update).status == p::TapeStatus::EVENT;
        } catch (const std::invalid_argument &) {
            return false;
        }
    };
    std::size_t lower = 0;
    std::size_t upper = fieldLimits().snapshotUnits;
    REQUIRE(admits(upper));

    while (upper - lower > 1) {
        const auto middle = lower + (upper - lower) / 2;

        if (admits(middle)) {
            upper = middle;
        } else {
            lower = middle;
        }
    }

    for (const auto maximum : {upper - 1, upper, upper + 1}) {
        auto budget = fieldLimits();
        budget.snapshotUnits = maximum;
        p::TapeReader reader(schema, budget, p::TapeMode::BINARY_CATALOG_V1, READ);
        accept(reader, initial);
        const auto *oldRaw = reader.descriptions().data();
        const auto *oldPlan = reader.context()->find(91);
        const auto *oldSchema = reader.schema().records().data();
        const auto event = reader.feed(update);
        CHECK(event.status == (maximum < upper ? p::TapeStatus::FAILED : p::TapeStatus::EVENT));

        if (maximum < upper) {
            CHECK(reader.descriptions().data() == oldRaw);
            CHECK(reader.context()->find(91) == oldPlan);
            CHECK(reader.schema().records().data() == oldSchema);
            CHECK(reader.context()->find(91)->eventTimeFieldCount == 2);
        }
    }
}

TEST_CASE("FIELD both-pin production fixtures match raw metadata and observed ordinary values") {
    const std::array<std::u16string_view, 5> business{u"Time", u"Value", u"EventPrice", u"EventSequence", u"EventTime"};

    for (const auto &root : fixtureRoots) {
        CAPTURE(root.string());
        std::ifstream observations(root / "canonical.tsv");
        REQUIRE(observations);
        std::map<std::string, std::vector<std::vector<std::string>>> cases;
        std::string line;
        std::size_t rows = 0;
        bool complete = false;

        while (std::getline(observations, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line == "all\tPASS") {
                complete = true;
                break;
            }

            std::stringstream parser(line);
            std::vector<std::string> cells;
            std::string cell;

            while (std::getline(parser, cell, '\t')) {
                cells.push_back(cell);
            }

            REQUIRE(cells.size() == 14);
            REQUIRE(cells[0] == "record");
            cases[cells[1]].push_back(cells);
            ++rows;
        }

        REQUIRE(complete);
        REQUIRE(rows == 115);
        REQUIRE(cases.size() == 25);
        std::ifstream deliveries(root / "observations.tsv");
        REQUIRE(deliveries);
        std::map<std::string, std::vector<std::pair<std::int64_t, std::int64_t>>> fileTiming;

        while (std::getline(deliveries, line)) {
            std::stringstream parser(line);
            std::vector<std::string> cells;
            std::string cell;

            while (std::getline(parser, cell, '\t')) {
                cells.push_back(cell);
            }

            if (cells.size() == 13 && cells[0] == "record" && cells[2] == "file-null") {
                REQUIRE(std::stoull(cells[4]) == fileTiming[cells[1]].size());
                fileTiming[cells[1]].push_back({std::stoll(cells[6]), std::stoll(cells[7])});
            }
        }

        REQUIRE(fileTiming.size() == 7);

        for (const auto &[name, expected] : cases) {
            CAPTURE(name);
            std::ifstream file(root / (name + ".qtp"), std::ios::binary);
            REQUIRE(file);
            const std::vector<char> captured{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            std::vector<std::byte> bytes;

            for (const auto value : captured) {
                bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
            }

            p::TapeReader reader(fieldLimits(), p::TapeDiscovery::BINARY_CATALOG_V1, READ);
            std::vector<p::StreamDataRecord> records;
            std::size_t offset = 0;

            while (offset < bytes.size()) {
                auto event = reader.feed(std::span<const std::byte>(bytes).subspan(offset));
                REQUIRE(event.accepted > 0);
                REQUIRE(event.status != p::TapeStatus::FAILED);
                offset += event.accepted;

                if (!event.records.empty()) {
                    REQUIRE(event.frame);
                    CHECK(event.frame->type == std::stoi(expected[records.size()][2]));

                    if (fileTiming.contains(name)) {
                        for (std::size_t index = 0; index < event.records.size(); ++index) {
                            const auto &record = event.records[index];
                            const bool messageWins = event.timing && event.timing->eventTimeSequence != 0;
                            const auto packed = messageWins ? event.timing->eventTimeSequence
                                                            : (record.fieldTime ? record.fieldTime->packed() : 0);
                            const auto millis = messageWins ? event.timing->eventTimeMs()
                                                            : (record.fieldTime ? record.fieldTime->eventTimeMs() : 0);
                            CHECK(packed == fileTiming[name][records.size() + index].first);
                            CHECK(millis == fileTiming[name][records.size() + index].second);
                        }
                    }
                }

                records.insert(records.end(), std::make_move_iterator(event.records.begin()),
                               std::make_move_iterator(event.records.end()));
            }

            REQUIRE(reader.finish() == p::TapeStatus::FINISHED);
            REQUIRE(records.size() == expected.size());
            const auto &local = reader.schema().records()[0];

            for (std::size_t index = 0; index < records.size(); ++index) {
                CAPTURE(index);
                const auto &actual = records[index];
                const auto &cells = expected[index];
                CHECK(actual.symbol.eventFlags == std::stoi(cells[4]));
                REQUIRE(actual.fieldTime.has_value() == (cells[5] != "absent"));

                if (actual.fieldTime) {
                    CHECK(actual.fieldTime->sourceOrdinal == index);
                    CHECK(actual.fieldTime->rawSeconds == std::stoll(cells[5]));
                    REQUIRE(actual.fieldTime->rawSequence.has_value() == (cells[6] != "absent"));

                    if (actual.fieldTime->rawSequence) {
                        CHECK(*actual.fieldTime->rawSequence == std::stoll(cells[6]));
                    }

                    CHECK(actual.fieldTime->packed() == std::stoll(cells[7]));
                    CHECK(actual.fieldTime->eventTimeMs() == std::stoll(cells[8]));
                }

                for (std::size_t column = 0; column < business.size(); ++column) {
                    for (const auto &lookup : local.lookups) {
                        if (lookup.name == business[column]) {
                            CHECK(actual.typedValues[lookup.fieldId].raw == std::stoll(cells[9 + column]));
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("FIELD actual standard indexed TimeAndSale retains independent ordinary Time") {
    for (const auto &root : fixtureRoots) {
        CAPTURE(root.string());
        std::ifstream observed(root / "standard.tsv");
        REQUIRE(observed);
        std::vector<std::vector<std::string>> rows;
        std::string line;

        while (std::getline(observed, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            std::stringstream parser(line);
            std::vector<std::string> cells;
            std::string cell;

            while (std::getline(parser, cell, '\t')) {
                cells.push_back(cell);
            }

            rows.push_back(std::move(cells));
        }

        REQUIRE(rows.size() == 27);
        REQUIRE(rows[0] == std::vector<std::string>{"schema", "TimeAndSale", "true", "15", "0"});
        REQUIRE(rows[1] == std::vector<std::string>{"slot", "0", "56", "false", "Time"});
        REQUIRE(rows[2] == std::vector<std::string>{"slot", "1", "72", "false", "Sequence"});
        c::LocalRecord local{0, u"TimeAndSale", true, {}, {}, c::IndexedTimePairInt32{0, 1}};
        std::vector<std::size_t> physicalSlots;

        for (std::size_t slot = 0; slot < 15; ++slot) {
            const auto &cells = rows[1 + slot];
            REQUIRE(cells.size() == 5);
            const auto type = std::stoi(cells[2]);

            if (type == 0) {
                continue;
            }

            std::u16string property;

            for (const auto value : cells[4]) {
                property.push_back(static_cast<char16_t>(value));
            }

            local.lookups.push_back({property, local.fields.size()});
            local.fields.push_back({type});
            physicalSlots.push_back(slot);
        }

        auto schema =
            c::LocalSchema::create(std::array{local}, fieldLimits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);
        std::ifstream input(root / "standard-indexed.qtp", std::ios::binary);
        REQUIRE(input);
        const std::vector<char> captured{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        std::vector<std::byte> bytes;

        for (const auto value : captured) {
            bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
        }

        std::vector<p::StreamDataRecord> records;

        {
            p::TapeReader reader(*schema, fieldLimits(), p::TapeMode::BINARY_CATALOG_V1, READ);
            std::size_t offset = 0;

            while (offset < bytes.size()) {
                auto event = reader.feed(std::span<const std::byte>(bytes).subspan(offset));
                REQUIRE(event.status != p::TapeStatus::FAILED);
                REQUIRE(event.accepted > 0);
                offset += event.accepted;
                records.insert(records.end(), std::make_move_iterator(event.records.begin()),
                               std::make_move_iterator(event.records.end()));
            }

            CHECK(reader.finish() == p::TapeStatus::FINISHED);
        }

        REQUIRE(records.size() == 5);

        for (std::size_t index = 0; index < records.size(); ++index) {
            const auto &record = records[index];
            const auto &expected = rows[16 + index];
            REQUIRE(expected.size() == 21);
            REQUIRE(record.fieldTime);
            CHECK(record.fieldTime->sourceOrdinal == index);
            CHECK(record.fieldTime->packed() == std::stoll(expected[3]));
            CHECK(record.fieldTime->eventTimeMs() == std::stoll(expected[4]));
            const auto ordinary =
                (static_cast<std::uint64_t>(static_cast<std::uint32_t>(record.typedValues[0].raw)) << 32) |
                static_cast<std::uint32_t>(record.typedValues[1].raw);
            CHECK(std::bit_cast<std::int64_t>(ordinary) == std::stoll(expected[5]));
            CHECK(record.typedValues[0].raw == 100 + static_cast<std::int64_t>(index));
            REQUIRE(record.typedValues.size() == physicalSlots.size());

            for (std::size_t field = 0; field < physicalSlots.size(); ++field) {
                const auto slot = physicalSlots[field];
                std::int64_t raw = std::stoll(expected[6 + slot]);

                if (rows[1 + slot][3] == "true") {
                    const auto bits = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw)) << 32) |
                                      static_cast<std::uint32_t>(std::stoll(expected[7 + slot]));
                    raw = std::bit_cast<std::int64_t>(bits);
                }

                CHECK(record.typedValues[field].raw == raw);
            }
        }
    }
}

/** Run native checks against both explicit immutable producer-pin directories.
 * @param argc Argument count.
 * @param argv Baseline/current fixtures followed by optional doctest switches.
 * @return Doctest result, or usage failure without running tests.
 */
int main(int argc, char **argv) {
    if (argc < 3) {
        return 2;
    }

    fixtureRoots = {argv[1], argv[2]};
    doctest::Context context;
    context.applyCommandLine(argc - 2, argv + 2);

    return context.run();
}
