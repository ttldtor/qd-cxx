// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../src/protocol/local_event_owner.hpp"
#include "../../src/protocol/shared_record_access.hpp"
#include "../../src/protocol/stripe_event_access.hpp"
#include "../../src/protocol/tape_event_bridge.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <new>
#include <optional>
#include <thread>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace d = c::detail;
namespace p = org::ttldtor::qdcxx::protocol;
namespace detail = p::detail;
thread_local bool allocationFault = false;
thread_local std::size_t remainingAllocations = 0;

/** Inject failure only while an individual production selection executes. */
struct AllocationFault {
    /** @param allowed Number of successful allocation requests before failure. */
    explicit AllocationFault(std::size_t allowed) {
        remainingAllocations = allowed;
        allocationFault = true;
    }

    /** Restore ordinary allocation before doctest diagnostics execute. */
    ~AllocationFault() {
        allocationFault = false;
    }
};

/** @return Explicit generous finite live work for this bounded ownership test. */
p::LiveWork turn() {
    return {10000, 1000000, 1000000, 10000000};
}

/** @return Independent finite arena and epoch reservations for local test source owners. */
p::LiveLimits limits() {
    p::LiveLimits value{};
    value.residentBytes = 4 * 1024 * 1024;
    value.owners = 128;
    value.eventBytes = 64 * 1024;

    return value;
}

/** @return Explicit metadata/copy/retention capacities; no new source row ceiling. */
detail::StripeEventLimits sliceLimits() {
    return {128 * 1024, 128 * 1024, 1024 * 1024};
}

/** Build real complete declarations with retained provenance.
 * @param types Exact physical field identities.
 * @return Valid binary schema; caller supplies its own backing to source and payload owners.
 */
c::LocalSchema schema(std::span<const c::SerialFieldType> types) {
    c::LocalRecord record{0, u"StripeOwnership", false, {}, {}};

    for (std::size_t index = 0; index < types.size(); ++index) {
        record.fields.push_back({static_cast<std::int32_t>(types[index])});
        record.lookups.push_back({std::u16string(1, static_cast<char16_t>(u'A' + index)), index});
    }

    auto result =
        c::LocalSchema::create(std::span(&record, 1), {4, 64, 64, 128, 1024}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Create a whole eager legacy event with separately populated compact and tagged values.
 * @param catalog Independent declarations matching every typed field.
 * @param bridge Counted existing tape-to-live operational epoch.
 * @return Three-row admitted event with exact source metadata and owned nullable payloads.
 */
p::LiveEvent legacyEvent(const c::LocalSchema &catalog, const detail::TapeEventBridge &bridge) {
    p::TapeFeedResult source;
    source.status = p::TapeStatus::EVENT;
    source.frame = p::Frame{p::FrameKind::MESSAGE, 15, {}, 0, 0};
    source.timing = p::TapeTiming{0, 77};

    for (std::int32_t index = 0; index < 3; ++index) {
        p::StreamDataRecord record{};
        record.localId = 0;
        record.context = catalog.recordContext(0);
        record.senderId = 101 + index;
        record.symbol.units = std::u16string{u'A', static_cast<char16_t>(0), static_cast<char16_t>(0xD800 + index)};
        record.symbol.cipher = -7123 - index;
        record.symbol.eventFlags = 0x11 + index;
        record.removalTime = std::numeric_limits<std::int64_t>::min() + index;
        record.fieldTime = p::FieldEventTime{-42 - index, 0, static_cast<std::uint64_t>(index)};
        record.values = {index, -100 - index};
        record.typedValues = {
            {c::SerialFieldType::COMPACT_INT, -42 - index},
            {c::SerialFieldType::UTF_CHAR_ARRAY,
             0,
             std::u16string{u'X', static_cast<char16_t>(0), static_cast<char16_t>(0xDC00)},
             {}},
            {c::SerialFieldType::BYTE_ARRAY, 0, {}, std::vector<std::byte>{std::byte{1}, std::byte{0}, std::byte{255}}},
            {c::SerialFieldType::BYTE_ARRAY, 0, {}, std::vector<std::byte>{}},
            {c::SerialFieldType::UTF_CHAR_ARRAY, 0, std::u16string{}, {}},
            {c::SerialFieldType::BYTE_ARRAY, 0, {}, {}},
            {c::SerialFieldType::UTF_CHAR_ARRAY, 0, {}, {}}};
        source.records.push_back(std::move(record));
    }

    auto work = turn();
    auto event = bridge.copy(source, 15, work);
    REQUIRE(event);

    return std::move(*event);
}
} // namespace

/** Inject only explicitly scoped ordinary allocation failures.
 * @param bytes Complete allocation request.
 * @return Allocated storage or std::bad_alloc.
 */
void *operator new(std::size_t bytes) {
    if (allocationFault) {
        if (remainingAllocations == 0) {
            throw std::bad_alloc{};
        }

        --remainingAllocations;
    }

    if (auto *pointer = std::malloc(bytes ? bytes : 1)) {
        return pointer;
    }

    throw std::bad_alloc{};
}

/** @param pointer Ordinary allocation to release. */
void operator delete(void *pointer) noexcept {
    std::free(pointer);
}

/** @param pointer Ordinary allocation to release.
 * @param size Compiler-provided requested byte count, unused by malloc.
 */
void operator delete(void *pointer, std::size_t size) noexcept {
    (void)size;
    std::free(pointer);
}

/** @param bytes Complete array request.
 * @return Allocated storage or std::bad_alloc.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** @param pointer Array allocation to release. */
void operator delete[](void *pointer) noexcept {
    ::operator delete(pointer);
}

/** @param pointer Array allocation to release.
 * @param size Compiler-provided requested byte count.
 */
void operator delete[](void *pointer, std::size_t size) noexcept {
    ::operator delete(pointer, size);
}

TEST_CASE("stripe legacy slices independently retain exact nullable payloads scalars and source metadata") {
    std::optional<p::LiveEvent> retained;
    const std::array selection{std::size_t{0}, std::size_t{2}};
    std::optional<std::int64_t> messageTime;

    {
        const std::array types{c::SerialFieldType::COMPACT_INT,    c::SerialFieldType::UTF_CHAR_ARRAY,
                               c::SerialFieldType::BYTE_ARRAY,     c::SerialFieldType::BYTE_ARRAY,
                               c::SerialFieldType::UTF_CHAR_ARRAY, c::SerialFieldType::BYTE_ARRAY,
                               c::SerialFieldType::UTF_CHAR_ARRAY};
        auto catalog = schema(types);
        auto work = turn();
        auto bridge = detail::TapeEventBridge::create(limits(), work);
        REQUIRE(bridge);
        auto source = legacyEvent(catalog, *bridge);
        const auto sourceRows = source.records();
        messageTime = sourceRows.front().tapeMessageTimeMs;
        work = turn();
        auto measured = detail::StripeEventAccess::measure(source, selection, sliceLimits(), work);
        REQUIRE(measured);
        CHECK(measured->records == 2);
        CHECK(measured->fields == 14);
        CHECK(measured->legacyValues == 4);
        CHECK(measured->retainedPayloadBytes == 0);
        work = turn();
        auto sliced = detail::StripeEventAccess::select(source, selection, sliceLimits(), work);
        REQUIRE(sliced);
        CHECK(sliced->snapshot().identity().sameEpoch(source.snapshot().identity()));
        CHECK(sliced->records()[0].symbol.data() != sourceRows[0].symbol.data());
        CHECK(sliced->records()[0].values.data() != sourceRows[0].values.data());
        CHECK(sliced->residentBytes() == measured->metadataBytes + measured->copiedPayloadBytes);
        CHECK_FALSE(detail::SharedRecordAccess::hasSharedRecords(*sliced));
        CHECK(source.records().data() == sourceRows.data());
        retained.emplace(std::move(*sliced));
    }

    REQUIRE(retained);
    REQUIRE(retained->records().size() == 2);

    for (std::size_t selected = 0; selected < selection.size(); ++selected) {
        const auto original = static_cast<std::int32_t>(selection[selected]);
        const auto &row = retained->records()[selected];
        CHECK(row.senderId == 101 + original);
        CHECK(row.cipher == -7123 - original);
        CHECK(row.eventFlags == 0x11 + original);
        CHECK(row.symbol.size() == 3);
        CHECK(row.symbol[1] == 0);
        CHECK(row.symbol[2] == static_cast<char16_t>(0xD800 + original));
        CHECK(row.values[0] == original);
        CHECK(row.values[1] == -100 - original);
        CHECK(row.removalTime == std::numeric_limits<std::int64_t>::min() + original);
        REQUIRE(row.fieldTime);
        CHECK(row.fieldTime->rawSeconds == -42 - original);
        CHECK(row.fieldTime->rawSequence == 0);
        CHECK(row.fieldTime->sourceOrdinal == static_cast<std::uint64_t>(original));
        CHECK(row.tapeMessageTimeMs == messageTime);
        REQUIRE(row.typedValues.size() == 7);
        CHECK(row.typedValues[0].raw == -42 - original);
        REQUIRE(row.typedValues[1].units);
        CHECK(row.typedValues[1].units->size() == 3);
        CHECK((*row.typedValues[1].units)[2] == static_cast<char16_t>(0xDC00));
        REQUIRE(row.typedValues[2].bytes);
        CHECK(row.typedValues[2].bytes->size() == 3);
        CHECK(row.typedValues[2].bytes->back() == std::byte{255});
        REQUIRE(row.typedValues[3].bytes);
        CHECK(row.typedValues[3].bytes->empty());
        REQUIRE(row.typedValues[4].units);
        CHECK(row.typedValues[4].units->empty());
        CHECK_FALSE(row.typedValues[5].bytes);
        CHECK_FALSE(row.typedValues[6].units);
    }
}

TEST_CASE("stripe slice failures preserve the complete source ownership and current byte reservation") {
    const std::array types{c::SerialFieldType::COMPACT_INT,    c::SerialFieldType::UTF_CHAR_ARRAY,
                           c::SerialFieldType::BYTE_ARRAY,     c::SerialFieldType::BYTE_ARRAY,
                           c::SerialFieldType::UTF_CHAR_ARRAY, c::SerialFieldType::BYTE_ARRAY,
                           c::SerialFieldType::UTF_CHAR_ARRAY};
    auto catalog = schema(types);
    auto work = turn();
    auto bridge = detail::TapeEventBridge::create(limits(), work);
    REQUIRE(bridge);
    auto source = legacyEvent(catalog, *bridge);
    const auto *original = source.records().data();
    const auto physical = detail::SharedRecordAccess::physicalState(source);
    const auto used = physical->used.load();
    const std::array valid{std::size_t{0}, std::size_t{2}};

    for (const auto selection :
         std::array{std::array{std::size_t{2}, std::size_t{0}}, std::array{std::size_t{1}, std::size_t{1}},
                    std::array{std::size_t{0}, std::size_t{3}}}) {
        work = turn();
        CHECK(detail::StripeEventAccess::select(source, selection, sliceLimits(), work).error() ==
              p::LiveError::INVALID_CONFIGURATION);
        CHECK(physical->used.load() == used);
    }

    work = turn();
    work.visits = 0;
    CHECK(detail::StripeEventAccess::select(source, valid, sliceLimits(), work).error() == p::LiveError::WORK_LIMIT);
    auto bounded = sliceLimits();
    bounded.copiedPayloadBytes = 0;
    work = turn();
    CHECK(detail::StripeEventAccess::select(source, valid, bounded, work).error() == p::LiveError::RESOURCE_LIMIT);

    for (std::size_t allowed = 0; allowed < 2; ++allowed) {
        work = turn();
        std::optional<p::LiveError> failure;

        {
            AllocationFault fault(allowed);
            auto result = detail::StripeEventAccess::select(source, valid, sliceLimits(), work);

            if (!result) {
                failure = result.error();
            }
        }

        CHECK(failure == p::LiveError::ALLOCATION_FAILURE);
        CHECK(physical->used.load() == used);
        CHECK(source.records().data() == original);
        CHECK(source.records().size() == 3);
    }

    work = turn();
    REQUIRE(detail::StripeEventAccess::select(source, valid, sliceLimits(), work));
}

TEST_CASE("stripe mixed slices preserve pure aliases and release every operational source before detached release") {
    std::shared_ptr<const d::SharedRecordPayload> retained;
    std::shared_ptr<d::RecordByteState> operational;
    d::RecordByteBudget physical(1024 * 1024);

    {
        const std::array types{c::SerialFieldType::COMPACT_INT};
        auto catalog = schema(types);
        const auto context = catalog.recordContext(0);
        auto binding = d::RecordBlockBinding::create(std::move(catalog), {19, 7}, physical);
        REQUIRE(binding);
        const std::array<c::FieldValueView, 1> fields{{{c::SerialFieldType::COMPACT_INT, -42, {}, {}}}};
        const std::array<d::RecordBlockInput, 1> inputs{{{context, u"ALPHA", fields, {}}}};
        auto block = d::SharedRecordBlock::copy(*binding, inputs, physical);
        REQUIRE(block);
        auto payload = d::SharedRecordPayload::row(*block, 0);
        std::u16string callerSymbol(payload->symbol());
        callerSymbol.reserve(128);
        auto work = turn();
        auto owner = detail::LocalEventOwner::create(limits(), work, *binding);
        REQUIRE(owner);
        const std::array<detail::LocalStreamRow, 2> rows{
            {{0, context, callerSymbol, payload->fields(), payload}, {0, context, u"BETA", fields, {}}}};
        auto source = owner->stream(rows, work, true);
        REQUIRE(source);
        operational = detail::SharedRecordAccess::physicalState(*source);
        const std::array selection{std::size_t{0}, std::size_t{1}};
        work = turn();
        auto measured = detail::StripeEventAccess::measure(*source, selection, sliceLimits(), work);
        REQUIRE(measured);
        CHECK(measured->retainedPayloadBytes == payload->residentBytes());
        CHECK(measured->copiedTextUnits == 4);
        work = turn();
        auto sliced = detail::StripeEventAccess::select(*source, selection, sliceLimits(), work);
        REQUIRE(sliced);
        CHECK(source->records()[0].symbol.data() == callerSymbol.data());
        REQUIRE(sliced->records()[0].symbol.data() == payload->symbol().data());
        CHECK(sliced->records()[1].symbol.data() != source->records()[1].symbol.data());
        retained = detail::SharedRecordAccess::retain(*sliced, sliced->records()[0]);
        CHECK(retained.get() == payload.get());
        CHECK_FALSE(detail::SharedRecordAccess::retain(*sliced, sliced->records()[1]));
        auto refusal = sliceLimits();
        refusal.retainedPayloadBytes = 0;
        work = turn();
        CHECK(detail::StripeEventAccess::select(*source, selection, refusal, work).error() ==
              p::LiveError::RESOURCE_LIMIT);
        callerSymbol.assign(u"CHANGED");
        callerSymbol.shrink_to_fit();
        CHECK(sliced->records()[0].symbol == u"ALPHA");
    }

    REQUIRE(retained);
    CHECK(retained->symbol() == u"ALPHA");
    CHECK(retained->fields()[0].raw == -42);
    REQUIRE(retained->schema());
    CHECK(operational->used.load() == d::RecordByteBudget::metadataAllocationBytes());
    CHECK(physical.usedBytes() > 0);
    std::thread release([payload = std::move(retained)]() mutable {
        payload.reset();
    });
    release.join();
    CHECK(physical.usedBytes() == 0);
}
