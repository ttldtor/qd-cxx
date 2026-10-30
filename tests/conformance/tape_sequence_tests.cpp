// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../tools/tape-inspect/tape_sequence.hpp"
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <new>
#include <string>
#include <type_traits>

namespace ti = org::ttldtor::qdcxx::tape_inspect;

namespace {
thread_local bool countAllocations = false;
thread_local std::size_t observedAllocations = 0;
} // namespace

void *operator new(std::size_t size) {
    if (countAllocations) {
        ++observedAllocations;
    }

    if (void *memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }

    throw std::bad_alloc();
}

void *operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void *memory) noexcept {
    std::free(memory);
}

void operator delete[](void *memory) noexcept {
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {
/** Independent inventory of each aggregate capacity exposed by the source contract. */
struct BudgetCase {
    const char *name;
    std::uint64_t (*read)(const ti::SequenceReservations &);
    void (*write)(ti::SequenceReservations &, std::uint64_t);
};

#define BUDGET(field)                                                                                                  \
    BudgetCase {                                                                                                       \
        #field,                                                                                                        \
            [](const ti::SequenceReservations &value) -> std::uint64_t {                                               \
                return value.field;                                                                                    \
            },                                                                                                         \
            [](ti::SequenceReservations &value, std::uint64_t count) {                                                 \
                value.field = static_cast<decltype(value.field)>(count);                                               \
            }                                                                                                          \
    }
const std::array BUDGETS{BUDGET(tape.frame.maximumBodyBytes),
                         BUDGET(tape.frame.maximumPendingBytes),
                         BUDGET(tape.inputBytes),
                         BUDGET(tape.frames),
                         BUDGET(tape.descriptor.properties),
                         BUDGET(tape.descriptor.messages),
                         BUDGET(tape.descriptor.text.bodyBytes),
                         BUDGET(tape.descriptor.text.codeUnits),
                         BUDGET(tape.descriptor.textUnits),
                         BUDGET(tape.context.schema.records),
                         BUDGET(tape.context.schema.fields),
                         BUDGET(tape.context.schema.lookups),
                         BUDGET(tape.context.schema.nameUnits),
                         BUDGET(tape.context.schema.totalNameUnits),
                         BUDGET(tape.context.descriptions),
                         BUDGET(tape.context.bindings.fields),
                         BUDGET(tape.context.bindings.nameUnits),
                         BUDGET(tape.context.bindings.totalNameUnits),
                         BUDGET(tape.constructionUnits),
                         BUDGET(tape.data.payloadBytes),
                         BUDGET(tape.data.processedRecords),
                         BUDGET(tape.data.wireFields),
                         BUDGET(tape.data.emittedRecords),
                         BUDGET(tape.data.emittedValues),
                         BUDGET(tape.data.ownedSymbolUnits),
                         BUDGET(tape.data.symbol.inputBytes),
                         BUDGET(tape.data.symbol.codeUnits),
                         BUDGET(tape.data.symbol.stateUnits),
                         BUDGET(tape.data.fieldString.encodedBytes),
                         BUDGET(tape.data.fieldString.codeUnits),
                         BUDGET(tape.data.fieldBytes),
                         BUDGET(tape.data.fieldUnits),
                         BUDGET(tape.data.ownedFieldUnits),
                         BUDGET(tape.data.fieldPayloadEncodedBytes),
                         BUDGET(tape.data.fieldPayloadBytes),
                         BUDGET(tape.data.payloadEncodedBytes),
                         BUDGET(tape.data.payloadWorkBytes),
                         BUDGET(tape.data.ownedPayloadBytes),
                         BUDGET(tape.data.ownedFieldTimeRecords),
                         BUDGET(tape.timingEvents),
                         BUDGET(tape.snapshotUnits),
                         BUDGET(dataContainer.compressedBytes),
                         BUDGET(dataContainer.inflatedBytes),
                         BUDGET(dataContainer.expansionRatio),
                         BUDGET(dataContainer.ratioSlack),
                         BUDGET(dataContainer.gzipMembers),
                         BUDGET(dataContainer.zipEntries),
                         BUDGET(dataContainer.headerBytes),
                         BUDGET(dataContainer.totalHeaderBytes),
                         BUDGET(dataContainer.workBytes),
                         BUDGET(dataContainer.inflateCalls),
                         BUDGET(dataContainer.retainedBytes),
                         BUDGET(dataContainer.inflaterBytes),
                         BUDGET(dataContainer.chunkBytes),
                         BUDGET(companionContainer.compressedBytes),
                         BUDGET(companionContainer.inflatedBytes),
                         BUDGET(companionContainer.expansionRatio),
                         BUDGET(companionContainer.ratioSlack),
                         BUDGET(companionContainer.gzipMembers),
                         BUDGET(companionContainer.zipEntries),
                         BUDGET(companionContainer.headerBytes),
                         BUDGET(companionContainer.totalHeaderBytes),
                         BUDGET(companionContainer.workBytes),
                         BUDGET(companionContainer.inflateCalls),
                         BUDGET(companionContainer.retainedBytes),
                         BUDGET(companionContainer.inflaterBytes),
                         BUDGET(companionContainer.chunkBytes),
                         BUDGET(scanner.inputBytes),
                         BUDGET(scanner.lineBytes),
                         BUDGET(scanner.lines),
                         BUDGET(scanner.marks),
                         BUDGET(scanner.workBytes),
                         BUDGET(scanner.retainedBytes),
                         BUDGET(chunkBytes),
                         BUDGET(outputBytes),
                         BUDGET(eventUnits),
                         BUDGET(physicalProbes),
                         BUDGET(inflatedProbes)};
#undef BUDGET

/** Small valid configuration with deliberately distinct and independent nested capacities.
 * @return Caller-owned occurrence with an explicit companion, without creating either path.
 */
ti::TapeSegmentSpec pairedSegment() {
    ti::TapeSegmentSpec segment;

    segment.path = "never-open-this-data.qds";
    segment.containerLimits = {31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 3};
    segment.limits.chunkSize = 5;
    segment.limits.output = 83;
    segment.limits.eventUnits = 89;
    segment.limits.tape = {
        {7, 16},
        97,
        101,
        {103, 107, {109, 113}, 127},
        {{131, 137, 139, 149, 151}, 157, {163, 167, 173}},
        179,
        {181, 191, 193, 197, 199, 211, {223, 227, 229}, {233, 239}, 241, 251, 257, 263, 269, 271, 277, 281, 283},
        293,
        307};
    segment.companion = ti::TapeCompanionSpec{"never-open-this-time.time",
                                              ti::ContainerKind::NONE,
                                              {311, 313, 317, 331, 337, 347, 349, 353, 359, 367, 373, 379, 7},
                                              {383, 389, 397, 401, 409, 419}};

    return segment;
}

/** Set exact one-occurrence capacities independently from the admission implementation.
 * @param segment Literal per-source configuration.
 * @return Explicit exact aggregate options for a paired occurrence.
 */
ti::TapeSequenceOptions exactOptions(const ti::TapeSegmentSpec &segment) {
    ti::TapeSequenceOptions options;

    options.time = ti::SourceTimePolicy::LONG;
    options.aggregate.tape = segment.limits.tape;
    options.aggregate.dataContainer = segment.containerLimits;
    options.aggregate.companionContainer = segment.companion->containerLimits;
    options.aggregate.scanner = segment.companion->scannerLimits;
    options.aggregate.chunkBytes = segment.limits.chunkSize;
    options.aggregate.outputBytes = segment.limits.output;
    options.aggregate.eventUnits = segment.limits.eventUnits;
    options.aggregate.physicalProbes = 2;
    options.aggregate.inflatedProbes = 2;

    return options;
}
} // namespace

TEST_CASE("sequence admission covers every nested full capacity at zero and L-1/L/L+1") {
    const std::array segments{pairedSegment()};
    const auto exact = exactOptions(segments.front());

    REQUIRE(BUDGETS.size() == 78);
    REQUIRE(ti::admitSequence(segments, exact).diagnostic.error == ti::SequenceError::NONE);

    for (const auto &budget : BUDGETS) {
        CAPTURE(budget.name);
        const auto required = budget.read(exact.aggregate);

        REQUIRE(required > 0);

        for (const auto capacity : {std::uint64_t{0}, required - 1, required, required + 1}) {
            auto options = exact;

            budget.write(options.aggregate, capacity);
            const auto admitted = ti::admitSequence(segments, options);

            CHECK(admitted.diagnostic.error ==
                  (capacity < required ? ti::SequenceError::RESOURCE_LIMIT : ti::SequenceError::NONE));
        }
    }
}

TEST_CASE("sequence repeated occurrences reserve every capacity again without borrowing") {
    const auto segment = pairedSegment();
    const std::array segments{segment, segment};
    auto options = exactOptions(segment);

    for (const auto &budget : BUDGETS) {
        budget.write(options.aggregate, budget.read(options.aggregate) * 2);
    }

    const auto exact = options;

    REQUIRE(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::NONE);

    for (const auto &budget : BUDGETS) {
        CAPTURE(budget.name);
        options = exact;
        budget.write(options.aggregate, budget.read(exact.aggregate) - 1);
        const auto admitted = ti::admitSequence(segments, options);

        CHECK(admitted.diagnostic.error == ti::SequenceError::RESOURCE_LIMIT);
        CHECK(admitted.diagnostic.sourceOrdinal == 1);
    }
}

TEST_CASE("sequence aggregate overflow is diagnosed before an overflowing addition") {
    auto segment = pairedSegment();
    auto options = exactOptions(segment);

    segment.limits.tape.data.ownedFieldTimeRecords = std::numeric_limits<std::size_t>::max();
    options.aggregate.tape.data.ownedFieldTimeRecords = std::numeric_limits<std::size_t>::max();
    const std::array segments{segment, segment};

    for (const auto &budget : BUDGETS) {
        if (std::string_view(budget.name) != "tape.data.ownedFieldTimeRecords") {
            budget.write(options.aggregate, budget.read(options.aggregate) * 2);
        }
    }

    const auto admitted = ti::admitSequence(segments, options);

    CHECK(admitted.diagnostic.error == ti::SequenceError::UNREPRESENTABLE);
    CHECK(admitted.diagnostic.sourceOrdinal == 1);
    CHECK(admitted.diagnostic.component == "tape.data.ownedFieldTimeRecords");
}

TEST_CASE("sequence names storage diagnostics handles and terminal output have literal boundaries") {
    const std::array segments{pairedSegment()};
    const auto exact = exactOptions(segments.front());
    const auto required = ti::admitSequence(segments, exact);
    const auto largestName =
        std::max(segments.front().path.native().size(), segments.front().companion->path.native().size());

    REQUIRE(required.diagnostic.error == ti::SequenceError::NONE);

    struct NameCase {
        std::size_t ti::TapeSequenceOptions::*member;
        std::size_t required;
    };

    const std::array cases{
        NameCase{&ti::TapeSequenceOptions::occurrences, 1},
        NameCase{&ti::TapeSequenceOptions::pathNativeUnits, largestName},
        NameCase{&ti::TapeSequenceOptions::totalPathNativeUnits, required.pathNativeUnits},
        NameCase{&ti::TapeSequenceOptions::pathEncodedBytes, required.pathEncodedBytes},
        NameCase{&ti::TapeSequenceOptions::pathStorageBytes, required.pathStorageBytes},
        NameCase{&ti::TapeSequenceOptions::diagnosticBytes, 4 * sizeof(ti::SourceDiagnostic)},
        NameCase{&ti::TapeSequenceOptions::sourceHandles, 2},
        NameCase{&ti::TapeSequenceOptions::outputBytes, exact.aggregate.outputBytes + exact.terminalBytes}};

    for (const auto &test : cases) {
        for (const auto capacity : {std::size_t{0}, test.required - 1, test.required, test.required + 1}) {
            auto options = exact;

            options.*test.member = capacity;
            CHECK(ti::admitSequence(segments, options).diagnostic.error ==
                  (capacity < test.required ? ti::SequenceError::RESOURCE_LIMIT : ti::SequenceError::NONE));
        }
    }

    auto options = exact;

    options.terminalBytes = options.outputBytes + 1;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::RESOURCE_LIMIT);
}

TEST_CASE("sequence configuration refuses invalid selection before nonexistent paths can open") {
    std::array segments{pairedSegment()};
    auto options = exactOptions(segments.front());

    CHECK(ti::admitSequence({}, options).diagnostic.error == ti::SequenceError::EMPTY_SEQUENCE);
    options.time = static_cast<ti::SourceTimePolicy>(99);
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);

    for (const auto time : {ti::SourceTimePolicy::NONE, ti::SourceTimePolicy::MESSAGE, ti::SourceTimePolicy::FIELD}) {
        options.time = time;
        CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::UNSUPPORTED_SOURCE_PROFILE);
    }

    options.time = ti::SourceTimePolicy::TEXT;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::NONE);
    segments.front().companion.reset();
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::UNSUPPORTED_SOURCE_PROFILE);
    options.time = ti::SourceTimePolicy::NONE;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::NONE);

    segments.front().path.clear();
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
    segments.front().path =
        std::filesystem::path::string_type{std::filesystem::path::value_type{'a'}, std::filesystem::path::value_type{},
                                           std::filesystem::path::value_type{'b'}};
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
}

TEST_CASE("sequence receives exact effective capacities and never repairs configuration") {
    std::array segments{pairedSegment()};
    auto options = exactOptions(segments.front());
    auto &segment = segments.front();

    segment.limits.tape.data.emittedRecords = 0;
    segment.limits.tape.data.symbol.inputBytes = 0;
    segment.limits.tape.data.symbol.stateUnits = 0;
    segment.limits.tape.data.payloadBytes = 0;
    const auto admitted = ti::admitSequence(segments, options);

    REQUIRE(admitted.diagnostic.error == ti::SequenceError::NONE);
    CHECK(admitted.reserved.tape.data.emittedRecords == 0);
    CHECK(admitted.reserved.tape.data.symbol.inputBytes == 0);
    CHECK(admitted.reserved.tape.data.symbol.stateUnits == 0);
    CHECK(admitted.reserved.tape.data.payloadBytes == 0);

    segment.limits.tape.frame.maximumPendingBytes = segment.limits.tape.frame.maximumBodyBytes + 8;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
    segment.limits.tape.frame.maximumPendingBytes += 1;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::NONE);

    segment.containerLimits.chunkBytes = 0;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
}

TEST_CASE("sequence admission owns only scalars and static diagnostics without allocation") {
    std::array segments{pairedSegment()};
    const auto exact = exactOptions(segments.front());

    observedAllocations = 0;
    countAllocations = true;
    const auto success = ti::admitSequence(segments, exact);
    countAllocations = false;

    CHECK(observedAllocations == 0);
    CHECK(success.diagnostic.error == ti::SequenceError::NONE);

    segments.front().companion->path.clear();
    observedAllocations = 0;
    countAllocations = true;
    const auto failure = ti::admitSequence(segments, exact);
    countAllocations = false;

    segments.front().path.clear();
    segments.front().companion.reset();
    CHECK(observedAllocations == 0);
    CHECK(failure.diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
    CHECK(failure.diagnostic.component == "path");
}

TEST_CASE("sequence validates incompatible per-instance storage before copying names") {
    const auto original = pairedSegment();
    const auto options = exactOptions(original);
    std::array segments{original};

    segments.front().container = static_cast<ti::ContainerKind>(99);
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
    segments.front() = original;
    segments.front().containerLimits.compressedBytes = std::numeric_limits<std::uint64_t>::max();
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
    segments.front() = original;
    segments.front().containerLimits.retainedBytes = segments.front().containerLimits.chunkBytes - 1;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
    segments.front() = original;
    segments.front().companion->scannerLimits.retainedBytes = segments.front().companion->scannerLimits.lineBytes - 1;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
    segments.front() = original;
    segments.front().limits.chunkSize = 0;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
    segments.front() = original;
    segments.front().limits.tape.frame.maximumBodyBytes = std::numeric_limits<std::size_t>::max();
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
    segments.front() = original;
    segments.front().limits.tape.snapshotUnits = 0;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
    segments.front() = original;
    segments.front().limits.tape.constructionUnits = 0;
    CHECK(ti::admitSequence(segments, options).diagnostic.error == ti::SequenceError::INVALID_ARGUMENT);
}
