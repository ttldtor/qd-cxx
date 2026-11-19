// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../src/core/shared_record_block.hpp"

#include <array>
#include <barrier>
#include <cstdlib>
#include <doctest/doctest.h>
#include <limits>
#include <new>
#include <thread>
#include <vector>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace d = c::detail;

thread_local bool observeAllocations = false;
thread_local std::size_t allocationCount = 0;
thread_local std::size_t allocationBytes = 0;
thread_local std::size_t allocationsBeforeFailure = std::numeric_limits<std::size_t>::max();

/** Observe only explicitly scoped production calls, never doctest diagnostics. */
struct AllocationScope {
    explicit AllocationScope(std::size_t successful = std::numeric_limits<std::size_t>::max()) {
        allocationCount = allocationBytes = 0;
        allocationsBeforeFailure = successful;
        observeAllocations = true;
    }

    ~AllocationScope() {
        observeAllocations = false;
    }
};

/** Create complete custom declarations from exact field identities.
 * @param types Ordered exact field identities.
 * @return Independent immutable schema in the binary profile.
 */
c::LocalSchema schema(std::span<const c::SerialFieldType> types) {
    c::LocalRecord record{0, u"Custom", false, {}, {}};

    for (std::size_t i = 0; i < types.size(); ++i) {
        record.fields.push_back({static_cast<std::int32_t>(types[i])});
        record.lookups.push_back({std::u16string(1, static_cast<char16_t>(u'A' + i)), i});
    }

    const auto result =
        c::LocalSchema::create(std::span(&record, 1), {16, 256, 256, 256, 4096}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return *result;
}

/** Check every exact descriptor and payload, including nullable empty views.
 * @param actual Independently backed values.
 * @param expected Borrowed comparison values.
 */
void sameFields(std::span<const c::FieldValueView> actual, std::span<const c::FieldValueView> expected) {
    REQUIRE(actual.size() == expected.size());

    for (std::size_t i = 0; i < actual.size(); ++i) {
        CHECK(actual[i].type == expected[i].type);
        CHECK(actual[i].raw == expected[i].raw);
        CHECK(actual[i].units.has_value() == expected[i].units.has_value());
        CHECK(actual[i].bytes.has_value() == expected[i].bytes.has_value());

        if (expected[i].units) {
            CHECK(actual[i].units == expected[i].units);
        }

        if (expected[i].bytes) {
            REQUIRE(actual[i].bytes);
            CHECK(std::ranges::equal(*actual[i].bytes, *expected[i].bytes));
        }
    }
}

struct Fixture {
    d::RecordByteBudget physical{1024 * 1024};
    std::shared_ptr<const d::RecordBlockBinding> binding;
    std::array<c::FieldValueView, 1> fields{{{c::SerialFieldType::COMPACT_INT, -42, {}, {}}}};
    std::array<d::RecordBlockInput, 3> rows;

    Fixture() {
        auto catalog = schema(std::array{c::SerialFieldType::COMPACT_INT});
        const auto context = catalog.recordContext(0);
        auto result = d::RecordBlockBinding::create(std::move(catalog), {19, 7}, physical);
        REQUIRE(result);
        binding = std::move(*result);

        for (std::size_t i = 0; i < rows.size(); ++i) {
            rows[i] = {context, u"ALPHA", fields, {}};
            rows[i].metadata.senderId = static_cast<std::int32_t>(100 + i);
            rows[i].metadata.cipher = -17;
            rows[i].metadata.flags = 31;
            rows[i].metadata.removalTime = std::numeric_limits<std::int64_t>::min();
            rows[i].metadata.fieldTime = d::RecordFieldTime{-123, 0, i};
            rows[i].metadata.tapeMessageTimeMs = 0;
        }
    }
};
} // namespace

/** Replace ordinary allocation only for scoped request-count and failure observations.
 * @param bytes Exact requested bytes.
 * @return Allocated backing or std::bad_alloc.
 */
void *operator new(std::size_t bytes) {
    if (observeAllocations) {
        if (allocationsBeforeFailure == 0) {
            throw std::bad_alloc{};
        }

        --allocationsBeforeFailure;
        ++allocationCount;
        allocationBytes += bytes;
    }

    if (auto *pointer = std::malloc(bytes == 0 ? 1 : bytes)) {
        return pointer;
    }

    throw std::bad_alloc{};
}

/** Release replacement allocation.
 * @param pointer Previously allocated storage.
 */
void operator delete(void *pointer) noexcept {
    std::free(pointer);
}

void operator delete(void *pointer, std::size_t) noexcept {
    std::free(pointer);
}

void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

void operator delete[](void *pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void *pointer, std::size_t) noexcept {
    std::free(pointer);
}

TEST_CASE("bound block preserves every exact field identity and null empty content distinction") {
    const std::array types{
        c::SerialFieldType::VOID_FIELD,   c::SerialFieldType::BYTE,         c::SerialFieldType::UTF_CHAR,
        c::SerialFieldType::SHORT,        c::SerialFieldType::INT,          c::SerialFieldType::COMPACT_INT,
        c::SerialFieldType::DECIMAL,      c::SerialFieldType::SHORT_STRING, c::SerialFieldType::TIME_SECONDS,
        c::SerialFieldType::SEQUENCE,     c::SerialFieldType::DATE,         c::SerialFieldType::LONG,
        c::SerialFieldType::WIDE_DECIMAL, c::SerialFieldType::TIME_MILLIS,  c::SerialFieldType::UTF_CHAR_ARRAY,
        c::SerialFieldType::BYTE_ARRAY,   c::SerialFieldType::STRING,       c::SerialFieldType::CUSTOM_OBJECT,
        c::SerialFieldType::SERIAL_OBJECT};
    auto catalog = schema(types);
    const auto context = catalog.recordContext(0);
    const auto identity = catalog.identity();
    d::RecordByteBudget physical(1024 * 1024);
    auto bound = d::RecordBlockBinding::create(std::move(catalog), {0, 0}, physical);
    REQUIRE(bound);
    CHECK_FALSE(catalog.identity().valid());
    std::u16string symbol{u'A', 0, static_cast<char16_t>(0xd800)};
    std::u16string text{0, static_cast<char16_t>(0xdc00), u'Z'};
    const std::array opaque{std::byte{0}, std::byte{0xff}, std::byte{0x80}};
    std::array<std::array<c::FieldValueView, types.size()>, 3> fields{};
    std::array<d::RecordBlockInput, 3> rows{};

    for (std::size_t row = 0; row < rows.size(); ++row) {
        for (std::size_t i = 0; i < types.size(); ++i) {
            auto &field = fields[row][i];
            field = {types[i], -1, {}, {}};

            if (types[i] == c::SerialFieldType::VOID_FIELD) {
                field.raw = 0;
            } else if (types[i] == c::SerialFieldType::UTF_CHAR) {
                field.raw = 0x10ffff;
            } else if (types[i] == c::SerialFieldType::LONG || types[i] == c::SerialFieldType::WIDE_DECIMAL ||
                       types[i] == c::SerialFieldType::TIME_MILLIS) {
                field.raw = std::numeric_limits<std::int64_t>::min();
            } else if (types[i] == c::SerialFieldType::UTF_CHAR_ARRAY || types[i] == c::SerialFieldType::STRING) {
                field.raw = 0;

                if (row) {
                    field.units = std::u16string_view(text).substr(0, row == 1 ? 0 : text.size());
                }
            } else if (types[i] == c::SerialFieldType::BYTE_ARRAY || types[i] == c::SerialFieldType::CUSTOM_OBJECT ||
                       types[i] == c::SerialFieldType::SERIAL_OBJECT) {
                field.raw = 0;

                if (row) {
                    field.bytes = std::span(opaque).first(row == 1 ? 0 : opaque.size());
                }
            }
        }

        rows[row] = {context, symbol, fields[row], {}};
    }

    auto block = d::SharedRecordBlock::copy(*bound, rows, physical);
    REQUIRE(block);
    CHECK((*block)->binding->schema.identity().matches(identity));
    CHECK((*block)->binding->source == d::RecordSourceGeneration{0, 0});

    for (std::size_t row = 0; row < rows.size(); ++row) {
        CHECK((*block)->rows[row].symbol() == symbol);
        sameFields((*block)->rows[row].fields(), fields[row]);
    }

    const auto savedSymbol = (*block)->rows[0].symbol();
    const auto savedText = *(*block)->rows[2].fields()[14].units;
    symbol.assign(20, u'X');
    text.assign(20, u'Y');
    CHECK(savedSymbol == std::u16string_view(u"A\0\xd800", 3));
    CHECK(savedText == std::u16string_view(u"\0\xdc00Z", 3));
}

TEST_CASE("copy accounting matches actual allocation requests and retention performs no allocation or payload copy") {
    Fixture f;
    std::expected<std::shared_ptr<const d::BoundRecordBatch>, d::RecordBlockError> block;
    std::size_t calls = 0, bytes = 0;
    {
        AllocationScope observation;
        block = d::SharedRecordBlock::copy(f.binding, f.rows, f.physical);
        calls = allocationCount;
        bytes = allocationBytes;
    }
    REQUIRE(block);
    const auto &accounting = (*block)->accounting;
    CHECK(calls == accounting.dataAllocationCount);
    CHECK(calls == 2);
    CHECK(bytes == accounting.backingAllocationBytes + accounting.controlAllocationBytes);
    CHECK(accounting.payloadCopyBytes == f.rows.size() * f.rows[0].symbol.size() * sizeof(char16_t));
    CHECK(accounting.backingAllocationBytes == accounting.payloadCopyBytes + accounting.fieldDescriptorBytes +
                                                   accounting.rowDescriptorBytes + accounting.recordMetadataBytes +
                                                   accounting.paddingBytes);
    d::RecordByteBudget consumer(1024 * 1024);
    {
        AllocationScope observation(0);
        auto retained = d::SharedRecordBlock::retain(*block, 1, 1, consumer);
        observeAllocations = false;
        REQUIRE(retained);
        CHECK(retained->owner().get() == block->get());
        CHECK(retained->size() == 1);
        CHECK(consumer.usedBytes() == (*block)->rows[0].residentBytes());
        CHECK(allocationCount == 0);
    }
    CHECK(consumer.usedBytes() == 0);
    auto independent = d::SharedRecordBlock::copyRange(**block, 1, 1, f.physical);
    REQUIRE(independent);
    CHECK((*independent)->binding == f.binding);
    CHECK((*independent)->rows[0].fields().data() != (*block)->rows[1].fields().data());
    CHECK((*independent)->rows[0].residentBytes() < (*block)->rows[1].residentBytes());
    CHECK((*independent)->metadata[0].source == f.rows[1].metadata);
}

TEST_CASE("schema binding is reused and every pure owner is destroyed through base row aliases on another thread") {
    Fixture f;
    const auto context = f.rows[0].context;
    auto block = d::SharedRecordBlock::copy(f.binding, f.rows, f.physical);
    REQUIRE(block);
    auto laterSchema = c::LocalSchema(f.binding->schema);
    auto later = d::RecordBlockBinding::create(std::move(laterSchema), {19, 8}, f.physical);
    REQUIRE(later);
    auto next = d::SharedRecordBlock::copy(*later, f.rows, f.physical);
    REQUIRE(next);
    CHECK((*block)->binding->source.generation == 7);
    CHECK((*next)->binding->source.generation == 8);
    std::weak_ptr<const d::BoundRecordBatch> weakBlock = *block;
    std::weak_ptr<const d::RecordBlockBinding> weakBinding = f.binding;
    auto alias = std::shared_ptr<const d::SharedRecordPayload>(*block, (*block)->rows + 1);
    const auto blockControl = (*block)->accounting.controlAllocationBytes;
    const auto bindingControl = f.binding->controlAllocationBytes;
    block->reset();
    next->reset();
    later->reset();
    f.binding.reset();
    std::atomic<bool> valid{false};
    std::thread release([held = std::move(alias), &valid, context, &weakBlock]() mutable {
        const auto snapshot = weakBlock.lock();
        valid = held->fields()[0].raw == -42 && snapshot && snapshot->binding->schema.recordId(context) == 0 &&
                snapshot->metadata[1].source.senderId == 101;
        held.reset();
    });
    release.join();
    CHECK(valid.load());
    CHECK(weakBlock.expired());
    CHECK(weakBinding.expired());
    // Weak controls still exist and their actual requests stay charged until deallocation.
    CHECK(f.physical.usedBytes() == blockControl + bindingControl);
    weakBlock.reset();
    weakBinding.reset();
    CHECK(f.physical.usedBytes() == 0);
}

TEST_CASE("permanent oversize transient pressure and allocation failure publish no partial block") {
    Fixture f;
    const auto baseline = f.physical.usedBytes();
    const auto measured = d::SharedRecordBlock::measure(*f.binding, f.rows);
    REQUIRE(measured);
    const auto required = measured->backingAllocationBytes + measured->controlAllocationBytes;
    d::RecordByteBudget small(required - 1);
    auto oversized = d::SharedRecordBlock::copy(f.binding, f.rows, small);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == d::RecordBlockError::CAPACITY_EXCEEDED);
    CHECK(small.usedBytes() == 0);
    d::RecordByteBudget one(required);
    auto first = d::SharedRecordBlock::copy(f.binding, f.rows, one);
    REQUIRE(first);
    auto pending = d::SharedRecordBlock::copy(f.binding, f.rows, one);
    REQUIRE_FALSE(pending);
    CHECK(pending.error() == d::RecordBlockError::WOULD_BLOCK);
    CHECK(one.usedBytes() == required);
    first->reset();
    CHECK(one.usedBytes() == 0);
    auto retry = d::SharedRecordBlock::copy(f.binding, f.rows, one);
    REQUIRE(retry);

    for (std::size_t allowed = 0; allowed < 2; ++allowed) {
        std::expected<std::shared_ptr<const d::BoundRecordBatch>, d::RecordBlockError> result;
        {
            AllocationScope failure(allowed);
            result = d::SharedRecordBlock::copy(f.binding, f.rows, f.physical);
        }
        REQUIRE_FALSE(result);
        CHECK(result.error() == d::RecordBlockError::ALLOCATION_FAILURE);
        CHECK(f.physical.usedBytes() == baseline);
    }

    d::RecordByteBudget consumer(1);
    auto rejected = d::SharedRecordBlock::retain(*retry, 0, 1, consumer);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == d::RecordBlockError::CAPACITY_EXCEEDED);
    CHECK(consumer.usedBytes() == 0);
    CHECK((*retry)->rows[0].fields()[0].raw == -42);
}

TEST_CASE("foreign schema invalid shape ranges and checked arithmetic are rejected before allocation") {
    Fixture f;
    auto foreign = schema(std::array{c::SerialFieldType::COMPACT_INT});
    auto rows = f.rows;
    rows[1].context = foreign.recordContext(0);
    const auto before = f.physical.usedBytes();
    auto rejected = d::SharedRecordBlock::copy(f.binding, rows, f.physical);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == d::RecordBlockError::WRONG_CONTEXT);
    rows = f.rows;
    rows[0].fields = {};
    rejected = d::SharedRecordBlock::copy(f.binding, rows, f.physical);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == d::RecordBlockError::INVALID_RECORD);
    CHECK(f.physical.usedBytes() == before);
    auto block = d::SharedRecordBlock::copy(f.binding, f.rows, f.physical);
    REQUIRE(block);
    auto invalid = d::SharedRecordBlock::copyRange(**block, 2, 2, f.physical);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error() == d::RecordBlockError::INVALID_RANGE);
    CHECK_THROWS_AS(d::recordByteSum(std::numeric_limits<std::size_t>::max(), 1), d::RecordBlockError);
    auto empty = d::SharedRecordBlock::copyRange(**block, 0, 0, f.physical);
    REQUIRE(empty);
    CHECK((*empty)->count == 0);
}

TEST_CASE("binding admission failure preserves the caller schema and original unbound packing stays available") {
    auto catalog = schema(std::array{c::SerialFieldType::COMPACT_INT});
    const auto origin = catalog.identity();
    d::RecordByteBudget empty(0);
    auto rejected = d::RecordBlockBinding::create(std::move(catalog), {1, 1}, empty);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == d::RecordBlockError::CAPACITY_EXCEEDED);
    CHECK(catalog.identity().matches(origin));
    d::RecordByteBudget enough(1024 * 1024);
    {
        AllocationScope failure(0);
        rejected = d::RecordBlockBinding::create(std::move(catalog), {1, 1}, enough);
    }
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == d::RecordBlockError::ALLOCATION_FAILURE);
    CHECK(enough.usedBytes() == 0);
    CHECK(catalog.identity().matches(origin));
    const std::array<c::FieldValueView, 1> fields{{{c::SerialFieldType::COMPACT_INT, 7, {}, {}}}};
    const std::array<d::RecordBlockInput, 1> rows{{{catalog.recordContext(0), u"", fields, {}}}};
    auto unbound = d::SharedRecordPayload::copyBatch(std::span<const d::RecordBlockInput>(rows));
    auto row = d::SharedRecordPayload::row(unbound, 0);
    CHECK(row->symbol().empty());
    CHECK(row->fields()[0].raw == 7);
    CHECK(row->residentBytes() == d::SharedRecordPayload::batchBytes(std::span<const d::RecordBlockInput>(rows)));
}

TEST_CASE("byte admission is one atomic request across competing threads and batch size is byte bounded") {
    Fixture f;
    const auto measured = d::SharedRecordBlock::measure(*f.binding, f.rows);
    REQUIRE(measured);
    const auto requested = measured->backingAllocationBytes + measured->controlAllocationBytes;
    d::RecordByteBudget physical(requested);
    std::barrier rendezvous(3);
    std::atomic<std::size_t> accepted{0}, blocked{0};
    const auto attempt = [&] {
        rendezvous.arrive_and_wait();
        auto result = d::SharedRecordBlock::copy(f.binding, f.rows, physical);

        if (result) {
            accepted.fetch_add(1);
        } else if (result.error() == d::RecordBlockError::WOULD_BLOCK) {
            blocked.fetch_add(1);
        }

        rendezvous.arrive_and_wait();
    };
    std::thread first(attempt), second(attempt);
    rendezvous.arrive_and_wait();
    rendezvous.arrive_and_wait();
    first.join();
    second.join();
    CHECK(accepted.load() == 1);
    CHECK(blocked.load() == 1);
    CHECK(physical.usedBytes() == 0);
    const std::vector<d::RecordBlockInput> many(1025, f.rows[0]);
    auto large = d::SharedRecordBlock::copy(f.binding, many, f.physical);
    REQUIRE(large);
    CHECK((*large)->count == many.size());
    CHECK((*large)->rows[1024].fields()[0].raw == -42);

    struct OverflowingRows {
        std::size_t size() const noexcept {
            return std::numeric_limits<std::size_t>::max();
        }

        d::RecordBlockInput operator[](std::size_t) const {
            std::abort();
        }
    };

    auto overflow = d::SharedRecordBlock::measure(*f.binding, OverflowingRows{});
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error() == d::RecordBlockError::SIZE_OVERFLOW);
}

TEST_CASE("successful repeated copy and measurement reuse the qualified shared control shape") {
    Fixture f;
    auto warm = d::SharedRecordBlock::copy(f.binding, f.rows, f.physical);
    REQUIRE(warm);
    const auto probes = d::recordControlMeasurementCount.load();

    for (std::size_t repeat = 0; repeat < 12; ++repeat) {
        auto measured = d::SharedRecordBlock::measure(*f.binding, f.rows);
        REQUIRE(measured);
        auto copied = d::SharedRecordBlock::copy(f.binding, f.rows, f.physical);
        REQUIRE(copied);
        CHECK((*copied)->accounting.controlAllocationBytes == measured->controlAllocationBytes);
    }

    CHECK(d::recordControlMeasurementCount.load() == probes);
}

TEST_CASE("separate binding residency preserves complete retained capacity and range policy") {
    Fixture f;
    auto block = d::SharedRecordBlock::copy(f.binding, f.rows, f.physical, d::RecordPayloadResidency::SEPARATE_BINDING);
    REQUIRE(block);
    const auto payload = d::SharedRecordPayload::row(*block, 0);
    CHECK(payload->fixedOwnerIdentity() == f.binding.get());
    CHECK(payload->fixedOwnerBytes() == f.binding->retainedCapacityBytes());
    CHECK(payload->residentBytes() + payload->fixedOwnerBytes() ==
          ((*block)->accounting.backingAllocationBytes + (*block)->accounting.controlAllocationBytes +
           f.binding->retainedCapacityBytes()));
    auto selection = d::SharedRecordBlock::copyRange(**block, 1, 1, f.physical);
    REQUIRE(selection);
    CHECK((*selection)->residency == d::RecordPayloadResidency::SEPARATE_BINDING);
    CHECK((*selection)->rows[0].fixedOwnerIdentity() == f.binding.get());
    CHECK((*selection)->metadata[0].source == f.rows[1].metadata);
    CHECK((*selection)->rows[0].fields()[0].raw == f.rows[1].fields[0].raw);
    const auto full = ((*selection)->accounting.backingAllocationBytes +
                       (*selection)->accounting.controlAllocationBytes + f.binding->retainedCapacityBytes());
    d::RecordByteBudget consumer(full);
    auto retained = d::SharedRecordBlock::retain(*selection, 0, 1, consumer);
    REQUIRE(retained);
    CHECK(consumer.usedBytes() == full);
    CHECK(consumer.usedBytes() > (*selection)->rows[0].residentBytes());
    retained = std::unexpected(d::RecordBlockError::INVALID_RANGE);
    CHECK(consumer.usedBytes() == 0);
}

TEST_CASE("byte peak includes direct and throwing admission without charging rejected requests") {
    d::RecordByteBudget budget(16);
    const auto state = budget.state();
    CHECK(state->peak.load() == 0);
    const auto oversized = state->tryReserve(17);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == d::RecordBlockError::CAPACITY_EXCEEDED);
    CHECK(state->peak.load() == 0);

    {
        REQUIRE(state->tryReserve(8));
        const auto direct = d::RecordByteReservation::adopt(state, 8);
        CHECK(state->peak.load() == 8);
        state->reserve(4);
        const auto wrapped = d::RecordByteReservation::adopt(state, 4);
        CHECK(budget.usedBytes() == 12);
        CHECK(state->peak.load() == 12);
        const auto occupied = state->tryReserve(5);
        REQUIRE_FALSE(occupied);
        CHECK(occupied.error() == d::RecordBlockError::WOULD_BLOCK);
        CHECK_THROWS_AS(state->reserve(5), d::RecordBlockError);
        CHECK(budget.usedBytes() == 12);
        CHECK(state->peak.load() == 12);
    }

    CHECK(budget.usedBytes() == 0);
    CHECK(state->peak.load() == 12);
    {
        const d::RecordByteReservation wrapped(state, 16);
        CHECK(budget.usedBytes() == 16);
        CHECK(state->peak.load() == 16);
    }
    CHECK(budget.usedBytes() == 0);
    CHECK(state->peak.load() == 16);
}

TEST_CASE("nonthrow reservation preserves permanent and occupied errors and adopts one exact credit") {
    d::RecordByteBudget budget(8);
    static_assert(noexcept(budget.state()->tryReserve(1)));
    const auto oversized = budget.state()->tryReserve(9);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == d::RecordBlockError::CAPACITY_EXCEEDED);
    CHECK(budget.usedBytes() == 0);
    {
        const auto accepted = budget.state()->tryReserve(8);
        REQUIRE(accepted);
        auto owner = d::RecordByteReservation::adopt(budget.state(), 8);
        CHECK(budget.usedBytes() == 8);
        const auto occupied = budget.state()->tryReserve(1);
        REQUIRE_FALSE(occupied);
        CHECK(occupied.error() == d::RecordBlockError::WOULD_BLOCK);
        const auto tooLarge = budget.state()->tryReserve(9);
        REQUIRE_FALSE(tooLarge);
        CHECK(tooLarge.error() == d::RecordBlockError::CAPACITY_EXCEEDED);
        CHECK(budget.usedBytes() == 8);
        bool legacyBlocked = false;
        try {
            budget.state()->reserve(1);
        } catch (d::RecordBlockError error) {
            legacyBlocked = error == d::RecordBlockError::WOULD_BLOCK;
        }
        CHECK(legacyBlocked);
        CHECK(budget.usedBytes() == 8);
        auto finalOwner = std::move(owner);
        CHECK(budget.usedBytes() == 8);
    }
    CHECK(budget.usedBytes() == 0);
    CHECK_THROWS_AS(budget.state()->reserve(9), d::RecordBlockError);
    CHECK(budget.usedBytes() == 0);
    REQUIRE(budget.state()->tryReserve(8));
    {
        const auto owner = d::RecordByteReservation::adopt(budget.state(), 8);
        CHECK(budget.usedBytes() == 8);
    }
    CHECK(budget.usedBytes() == 0);
}

TEST_CASE("copy admission observes one validated layout before data allocation and cannot supply a foreign layout") {
    Fixture f;
    const auto baseline = f.physical.usedBytes();

    struct ObservedRows {
        const std::array<d::RecordBlockInput, 3> &rows;
        std::size_t &reads;

        std::size_t size() const noexcept {
            return rows.size();
        }

        const d::RecordBlockInput &operator[](std::size_t index) const {
            ++reads;

            return rows[index];
        }
    };

    std::size_t reads = 0, admissions = 0, readsAtAdmission = 0, descriptorsAtAdmission = 0;
    const ObservedRows rows{f.rows, reads};
    const auto decline = [&](const d::RecordBlockAccounting &layout) -> std::expected<void, d::RecordBlockError> {
        ++admissions;
        readsAtAdmission = reads;
        descriptorsAtAdmission = layout.rowDescriptorBytes;

        return std::unexpected(d::RecordBlockError::WOULD_BLOCK);
    };
    std::expected<std::shared_ptr<const d::BoundRecordBatch>, d::RecordBlockError> rejected;
    {
        AllocationScope noAllocation(0);
        rejected = d::SharedRecordBlock::copyWithAdmission(f.binding, rows, f.physical, decline);
    }
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == d::RecordBlockError::WOULD_BLOCK);
    CHECK(admissions == 1);
    CHECK(readsAtAdmission == rows.size());
    CHECK(descriptorsAtAdmission == rows.size() * sizeof(d::SharedRecordPayload));
    CHECK(reads == rows.size()); // Declining admission performs no fill or second validation pass.
    CHECK(f.physical.usedBytes() == baseline);
    auto invalid = f.rows;
    invalid[0].fields = {};
    {
        AllocationScope noAllocation(0);
        rejected = d::SharedRecordBlock::copyWithAdmission(f.binding, invalid, f.physical, decline);
    }
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == d::RecordBlockError::INVALID_RECORD);
    CHECK(admissions == 1); // Invalid source cannot reach the admission callback.
    CHECK(f.physical.usedBytes() == baseline);
}

TEST_CASE("callback failure and physical pressure roll back caller admission without publishing a partial block") {
    Fixture f;
    const auto baseline = f.physical.usedBytes();
    const auto measured = d::SharedRecordBlock::measure(*f.binding, f.rows);
    REQUIRE(measured);
    const auto requested = measured->backingAllocationBytes + measured->controlAllocationBytes;
    d::RecordByteBudget input(requested);

    struct AdmissionFailure {};

    std::size_t admissions = 0;
    const auto attempt = [&](const d::RecordByteBudget &physical, bool fail) {
        d::RecordByteReservation inputOwner;
        const auto admit = [&](const d::RecordBlockAccounting &layout) -> std::expected<void, d::RecordBlockError> {
            ++admissions;
            inputOwner =
                d::RecordByteReservation(input.state(), layout.backingAllocationBytes + layout.controlAllocationBytes);

            if (fail) {
                throw AdmissionFailure{};
            }

            return {};
        };

        return d::SharedRecordBlock::copyWithAdmission(f.binding, f.rows, physical, admit);
    };
    CHECK_THROWS_AS(attempt(f.physical, true), AdmissionFailure);
    CHECK(input.usedBytes() == 0);
    CHECK(f.physical.usedBytes() == baseline);
    d::RecordByteBudget small(requested - 1);
    auto rejected = attempt(small, false);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == d::RecordBlockError::CAPACITY_EXCEEDED);
    CHECK(input.usedBytes() == 0);
    CHECK(small.usedBytes() == 0);
    d::RecordByteBudget one(requested);
    auto existing = d::SharedRecordBlock::copy(f.binding, f.rows, one);
    REQUIRE(existing);
    rejected = attempt(one, false);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == d::RecordBlockError::WOULD_BLOCK);
    CHECK(input.usedBytes() == 0);
    CHECK(one.usedBytes() == requested);
    existing->reset();
    CHECK(one.usedBytes() == 0);

    for (std::size_t allowed = 0; allowed < 2; ++allowed) {
        const auto before = admissions;
        {
            AllocationScope failure(allowed);
            rejected = attempt(f.physical, false);
        }
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error() == d::RecordBlockError::ALLOCATION_FAILURE);
        CHECK(admissions == before + 1);
        CHECK(input.usedBytes() == 0);
        CHECK(f.physical.usedBytes() == baseline);
    }
}
