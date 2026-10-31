// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/protocol/subscription_writer.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/symbol_reader.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <new>
#include <sstream>
#include <utility>

namespace {
namespace p = org::ttldtor::qdcxx::protocol;
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
using Error = p::SubscriptionWriterError;
using Message = p::SubscriptionMessage;
bool failAllocations = false;
std::size_t allocationsRemaining = 0;

/** Narrow ordinary-allocation injection, disabled before assertions or framework work. */
struct AllocationFault {
    /** Arm one production call.
     * @param count Successful allocations permitted before failure.
     */
    explicit AllocationFault(std::size_t count) {
        allocationsRemaining = count;
        failAllocations = true;
    }

    /** Restore ordinary allocation. */
    ~AllocationFault() {
        failAllocations = false;
    }
};

/** Explicit generous work for one test turn.
 * @return Independent finite credits.
 */
p::SubscriptionWriterWork turn() {
    return {10000, 1000000, 1000000, 1000000};
}

/** Fixed synthetic indexed schema in native local order.
 * @return Owned independently created schema provenance.
 */
c::LocalSchema schema() {
    const c::LocalRecord records[] = {
        {0, u"Alpha", false, {{8}}, {{u"x", 0}}},
        {1, u"Beta", false, {{8}, {8}}, {{u"x", 0}, {u"y", 1}}},
        {2, u"Indexed", true, {{8}, {8}, {8}}, {{u"hi", 0}, {u"lo", 1}, {u"x", 2}}, c::IndexedTimePairInt32{0, 1}}};
    auto result = c::LocalSchema::create(records, {3, 6, 6, 7, 64}, c::LocalProfile::BINARY_FIELDS_V1);

    REQUIRE(result);

    return std::move(*result);
}

/** Explicit native-to-sender remapping and complete reordered Beta fields.
 * @return Owned declarations; no inferred field names.
 */
std::vector<p::OutgoingRecord> layouts() {
    return {{41, 0, u"Alpha", {{u"x", 8, 0}}},
            {3, 1, u"Beta", {{u"y", 8, 1}, {u"x", 8, 0}}},
            {7, 2, u"Indexed", {{u"hi", 8, 0}, {u"lo", 8, 1}, {u"x", 8, 2}}}};
}

/** Metadata bounds used by the synthetic fixture.
 * @return Required complete copy limits.
 */
p::LayoutLimits metadataLimits() {
    return {{3, 6, 6, 7, 64}, 3, 6, 7, 64};
}

/** Finite operation and retained-output limits.
 * @return Test capacities, not production defaults.
 */
p::SubscriptionWriterLimits limits() {
    return {65536, 65541, 256, 128, {1024, 128}, 131082, 131082, 256};
}

/** Create one bounded composer, asserting only after production allocation has returned.
 * @param source Borrowed immutable source.
 * @param bounds Explicit finite output limits.
 * @return Independent composer sharing only schema provenance.
 */
p::SubscriptionWriter writer(const c::LocalSchema &source, p::SubscriptionWriterLimits bounds = limits()) {
    auto work = turn();
    const auto declarations = layouts();
    auto result = p::SubscriptionWriter::create(source, declarations, metadataLimits(), bounds, work);

    REQUIRE(result);

    return std::move(*result);
}

/** Convert exact fixture hex to byte values, independent of the implementation's codecs.
 * @param value Even-length lowercase hex string.
 * @return Owned expected bytes.
 */
std::vector<std::byte> hex(std::string_view value) {
    std::vector<std::byte> result;

    for (std::size_t i = 0; i < value.size(); i += 2) {
        const auto nibble = [](char unit) {
            return unit <= '9' ? unit - '0' : unit - 'a' + 10;
        };

        result.push_back(static_cast<std::byte>((nibble(value[i]) << 4) | nibble(value[i + 1])));
    }

    return result;
}

/** Compare a whole envelope against fixed independently specified bytes.
 * @param output Production encoded output.
 * @param expected Fixed expected hex.
 */
void checkHex(const p::PendingSubscriptionOutput &output, std::string_view expected) {
    const auto bytes = hex(expected);

    CHECK(std::ranges::equal(output.bytes(), bytes));
}

/** Borrow one exact key with explicit provenance and optional raw bound.
 * @param source Immutable schema supplying provenance.
 * @param id Native local record ID.
 * @param symbol Borrowed non-null exact units.
 * @param time Optional raw signed64 bound.
 * @return Borrowed request.
 */
c::SubscriptionRequest request(const c::LocalSchema &source, std::int32_t id, std::u16string_view symbol,
                               std::optional<std::int64_t> time = std::nullopt) {
    return {{source.recordContext(id), symbol}, time};
}

/** Exact UTF-16 code-unit hex for fixture interoperability.
 * @param value Borrowed exact units, including empty and lone surrogates.
 * @return Lowercase four-digit hex per code unit.
 */
std::string utf16Hex(std::u16string_view value) {
    std::ostringstream output;

    output << std::hex << std::setfill('0');

    for (char16_t unit : value) {
        output << std::setw(4) << static_cast<unsigned>(unit);
    }

    return output.str();
}
} // namespace

/** Replace only this isolated executable's ordinary allocation route.
 * @param count Requested ordinary byte count.
 * @return Malloc-backed storage, or bad_alloc at the armed boundary.
 */
void *operator new(std::size_t count) {
    if (failAllocations) {
        if (allocationsRemaining == 0) {
            throw std::bad_alloc{};
        }

        --allocationsRemaining;
    }

    if (void *storage = std::malloc(count == 0 ? 1 : count)) {
        return storage;
    }

    throw std::bad_alloc{};
}

/** Route ordinary arrays through the same isolated fault injection.
 * @param count Requested ordinary array byte count.
 * @return Owned ordinary storage or bad_alloc.
 */
void *operator new[](std::size_t count) {
    return ::operator new(count);
}

/** Release ordinary storage.
 * @param storage Matching allocated storage or null.
 */
void operator delete(void *storage) noexcept {
    std::free(storage);
}

/** Release ordinary array storage.
 * @param storage Matching allocated array storage or null.
 */
void operator delete[](void *storage) noexcept {
    std::free(storage);
}

/** Release sized ordinary storage.
 * @param storage Matching allocated storage or null.
 * @param count Ignored allocation size.
 */
void operator delete(void *storage, std::size_t count) noexcept {
    static_cast<void>(count);
    std::free(storage);
}

/** Release sized ordinary array storage.
 * @param storage Matching allocated array storage or null.
 * @param count Ignored allocation size.
 */
void operator delete[](void *storage, std::size_t count) noexcept {
    static_cast<void>(count);
    std::free(storage);
}

TEST_CASE("live subscription six families have exact symbol-first remapped bytes") {
    const auto source = schema();
    auto composer = writer(source);
    auto work = turn();
    const std::array messages{Message::TICKER_ADD,    Message::TICKER_REMOVE, Message::STREAM_ADD,
                              Message::STREAM_REMOVE, Message::HISTORY_ADD,   Message::HISTORY_REMOVE};
    const std::array expected{"070bfd014129ff03", "070cfd014129ff03",     "0710fd014129ff03",
                              "0711fd014129ff03", "0915fd0141077fff0700", "0716fd014107ff07"};

    for (std::size_t i = 0; i < messages.size(); ++i) {
        const bool history = i >= 4;
        const bool addTime = i == 4;
        const c::SubscriptionRequest inputs[] = {
            request(source, history ? 2 : 0, u"A", addTime ? std::optional<std::int64_t>(-1) : std::nullopt),
            request(source, history ? 2 : 1, u"A", addTime ? std::optional<std::int64_t>(0) : std::nullopt)};
        auto output = composer.compose(messages[i], inputs, work);

        REQUIRE(output);
        checkHex(*output, expected[i]);
        auto repeatedCall = composer.compose(messages[i], inputs, work);

        REQUIRE(repeatedCall);
        CHECK(std::ranges::equal(output->bytes(), repeatedCall->bytes()));
    }
}

TEST_CASE("live descriptions preserve explicit record identity and complete reordered fields") {
    const auto source = schema();
    auto composer = writer(source);
    auto work = turn();
    const std::int32_t ids[] = {3, 41};
    auto output = composer.describe(ids, work);

    REQUIRE(output);
    checkHex(*output, "1902030442657461020179080178082905416c70686101017808");
    auto noRecords = composer.describe({}, work);

    REQUIRE(noRecords);
    checkHex(*noRecords, "0102");
    const auto declarations = layouts();
    auto tape = p::OutgoingLayouts::create(source, declarations, metadataLimits());

    REQUIRE_FALSE(tape);
    CHECK(tape.error().error == p::WriterError::UNSUPPORTED);
}

TEST_CASE("live history raw signed64 domain and remove has no trailing time") {
    const auto source = schema();
    auto composer = writer(source);
    auto work = turn();

    for (unsigned bit = 0; bit < 64; ++bit) {
        const auto raw = std::bit_cast<std::int64_t>(std::uint64_t{1} << bit);
        const c::SubscriptionRequest input[] = {request(source, 2, u"A", raw)};
        auto output = composer.compose(Message::HISTORY_ADD, input, work);

        REQUIRE(output);
        const auto frame = e::readCompactInt(output->bytes());
        const auto value = e::readCompactLong(output->bytes().subspan(frame.consumed + 5));

        REQUIRE(value.status == e::Status::OK);
        CHECK(value.value == raw);
        CHECK(frame.consumed + 5 + value.consumed == output->bytes().size());
    }

    const c::SubscriptionRequest min[] = {request(source, 2, u"A", std::numeric_limits<std::int64_t>::min())};
    const c::SubscriptionRequest max[] = {request(source, 2, u"A", std::numeric_limits<std::int64_t>::max())};
    auto minOutput = composer.compose(Message::HISTORY_ADD, min, work);
    auto maxOutput = composer.compose(Message::HISTORY_ADD, max, work);

    REQUIRE(minOutput);
    REQUIRE(maxOutput);
    checkHex(*minOutput, "0e15fd014107ff8000000000000000");
    checkHex(*maxOutput, "0e15fd014107ff7fffffffffffffff");
    const c::SubscriptionRequest remove[] = {request(source, 2, u"A")};
    auto removed = composer.compose(Message::HISTORY_REMOVE, remove, work);

    REQUIRE(removed);
    checkHex(*removed, "0516fd014107");
}

TEST_CASE("live output partial cursor preserves every suffix and retransmission across moves") {
    const auto source = schema();
    const c::SubscriptionRequest input[] = {request(source, 0, u"AB*")};
    const auto expected = hex("0710fd0341422a29");

    for (std::size_t split = 0; split <= expected.size(); ++split) {
        auto composer = writer(source);
        auto work = turn();
        auto output = composer.compose(Message::STREAM_ADD, input, work);

        REQUIRE(output);
        CHECK(output->consume(split));
        CHECK(std::ranges::equal(output->pendingBytes(), std::span(expected).subspan(split)));
        auto moved = std::move(*output);

        CHECK(output->bytes().empty());
        CHECK(output->pendingBytes().empty());
        CHECK(std::ranges::equal(moved.bytes(), expected));
        const auto before = moved.pendingBytes();
        const auto rejected = moved.consume(before.size() + 1);

        REQUIRE_FALSE(rejected);
        CHECK(rejected.error() == Error::INVALID_CONSUMPTION);
        CHECK(moved.pendingBytes().data() == before.data());
        CHECK(moved.pendingBytes().size() == before.size());
        CHECK(moved.consume(before.size()));
        CHECK(moved.pendingBytes().empty());
        CHECK(std::ranges::equal(moved.bytes(), expected));
        CHECK(composer.retainedBytes() == expected.size());
    }

    auto survivor = [&] {
        auto composer = writer(source);
        auto work = turn();
        auto result = composer.compose(Message::STREAM_ADD, input, work);

        REQUIRE(result);
        auto movedWriter = std::move(composer);
        auto rejected = composer.compose(Message::STREAM_ADD, input, work);

        REQUIRE_FALSE(rejected);
        CHECK(rejected.error() == Error::INVALID_STATE);
        CHECK(movedWriter.retainedBytes() == expected.size());

        return std::move(*result);
    }();

    CHECK(std::ranges::equal(survivor.bytes(), expected));
    CHECK(survivor.consume(1));
    CHECK(std::ranges::equal(survivor.pendingBytes(), std::span(expected).subspan(1)));
}

TEST_CASE("live selectors contexts bounds and message errors preserve existing output") {
    const auto source = schema();
    const auto foreign = schema();
    auto composer = writer(source);
    auto work = turn();
    const c::SubscriptionRequest good[] = {request(source, 0, u"A")};
    auto existing = composer.compose(Message::STREAM_ADD, good, work);

    REQUIRE(existing);
    const auto original = hex("0510fd014129");
    auto input = good[0];
    const auto rejected = [&](Message message, Error expected) {
        auto result = composer.compose(message, std::span(&input, 1), work);

        REQUIRE_FALSE(result);
        CHECK(result.error() == expected);
        CHECK(std::ranges::equal(existing->bytes(), original));
        CHECK(composer.retainedBytes() == original.size());
    };

    input.key.symbol = {};
    rejected(Message::STREAM_ADD, Error::INVALID_SELECTOR);
    input.key.symbol = u"*reserved";
    rejected(Message::STREAM_ADD, Error::INVALID_SELECTOR);
    input = request(foreign, 0, u"A");
    rejected(Message::STREAM_ADD, Error::INVALID_CONTEXT);
    input.key.context = {};
    rejected(Message::STREAM_ADD, Error::INVALID_CONTEXT);
    input = good[0];
    rejected(Message::HISTORY_REMOVE, Error::INVALID_HISTORY);
    input = request(source, 2, u"A");
    rejected(Message::HISTORY_ADD, Error::INVALID_HISTORY);
    input.historyBound = 1;
    rejected(Message::HISTORY_REMOVE, Error::INVALID_HISTORY);
    rejected(Message::TICKER_ADD, Error::INVALID_HISTORY);
    rejected(static_cast<Message>(99), Error::INVALID_MESSAGE);
    const auto before = work.operations;
    work.operations = 0;
    rejected(Message::STREAM_ADD, Error::WORK_LIMIT);
    work.operations = before;
    const std::int32_t missing[] = {999};
    auto noDescription = composer.describe(missing, work);

    REQUIRE_FALSE(noDescription);
    CHECK(noDescription.error() == Error::INVALID_LAYOUT);
    auto recovered = composer.compose(Message::STREAM_ADD, good, work);

    REQUIRE(recovered);
    CHECK(std::ranges::equal(recovered->bytes(), original));
}

TEST_CASE("live per-operation resident peak and owner count are separate finite bounds") {
    const auto source = schema();
    const c::SubscriptionRequest input[] = {request(source, 0, u"A")};
    const auto checkRejected = [&](p::SubscriptionWriterLimits bounds) {
        auto composer = writer(source, bounds);
        auto work = turn();
        auto result = composer.compose(Message::STREAM_ADD, input, work);

        REQUIRE_FALSE(result);
        CHECK(result.error() == Error::RESOURCE_LIMIT);
        CHECK(composer.retainedBytes() == 0);
    };
    auto bounds = limits();

    bounds.frameBytes = 4;
    checkRejected(bounds);
    bounds = limits();
    bounds.operationBytes = 5;
    checkRejected(bounds);
    bounds = limits();
    bounds.operationEntries = 0;
    checkRejected(bounds);
    bounds = limits();
    bounds.symbolUnits = 0;
    checkRejected(bounds);
    bounds = limits();
    bounds.residentBytes = 5;
    checkRejected(bounds);
    bounds = limits();
    bounds.peakBytes = 5;
    checkRejected(bounds);
    bounds = limits();
    bounds.outputs = 0;
    checkRejected(bounds);
    bounds = limits();
    bounds.residentBytes = 6;
    bounds.peakBytes = 6;
    bounds.outputs = 1;
    auto composer = writer(source, bounds);
    auto work = turn();

    {
        auto output = composer.compose(Message::STREAM_ADD, input, work);

        REQUIRE(output);
        CHECK(output->consume(output->bytes().size()));
        auto rejected = composer.compose(Message::STREAM_ADD, input, work);

        REQUIRE_FALSE(rejected);
        CHECK(rejected.error() == Error::RESOURCE_LIMIT);
        CHECK(composer.retainedBytes() == 6);
    }

    CHECK(composer.retainedBytes() == 0);
    auto recovered = composer.compose(Message::STREAM_ADD, input, work);

    REQUIRE(recovered);
}

TEST_CASE("live turn credits bound each work category without a lifetime quota") {
    const auto source = schema();
    auto composer = writer(source);
    const c::SubscriptionRequest input[] = {request(source, 0, u"A")};

    for (unsigned category = 0; category < 4; ++category) {
        auto work = turn();

        switch (category) {
        case 0:
            work.operations = 0;
            break;
        case 1:
            work.visits = 0;
            break;
        case 2:
            work.textUnits = 0;
            break;
        case 3:
            work.outputBytes = 0;
            break;
        }

        auto result = composer.compose(Message::STREAM_ADD, input, work);

        REQUIRE_FALSE(result);
        CHECK(result.error() == Error::WORK_LIMIT);
        CHECK(composer.retainedBytes() == 0);
    }

    // More operations than any simultaneous-output allowance, with a fresh explicit turn each time.
    for (unsigned iteration = 0; iteration < 1024; ++iteration) {
        auto work = turn();
        auto output = composer.compose(Message::STREAM_ADD, input, work);

        REQUIRE(output);
        CHECK(output->bytes().size() == 6);
    }

    CHECK(composer.retainedBytes() == 0);
}

TEST_CASE("live layout validation rejects sparse-ID mistakes and incomplete remapping") {
    const auto source = schema();
    auto declarations = layouts();
    auto work = turn();
    const auto reject = [&](Error expected) {
        auto result = p::SubscriptionWriter::create(source, declarations, metadataLimits(), limits(), work);

        REQUIRE_FALSE(result);
        CHECK(result.error() == expected);
    };

    declarations[0].senderId = -1;
    reject(Error::INVALID_LAYOUT);
    declarations[0].senderId = std::numeric_limits<std::int32_t>::max();
    reject(Error::INVALID_LAYOUT);
    declarations = layouts();
    declarations[0].senderId = declarations[1].senderId;
    reject(Error::INVALID_LAYOUT);
    declarations = layouts();
    declarations[0].localRecordId = 3;
    reject(Error::INVALID_LAYOUT);
    declarations = layouts();
    declarations[0].wireName = u"Other";
    reject(Error::INVALID_LAYOUT);
    declarations = layouts();
    declarations[0].fields.clear();
    reject(Error::INVALID_LAYOUT);
    declarations = layouts();
    declarations[1].fields[0].localFieldId = 0;
    reject(Error::INVALID_LAYOUT);
    declarations = layouts();
    declarations[0].fields[0].wireName = u"missing";
    reject(Error::INVALID_LAYOUT);
    declarations = layouts();
    declarations[0].fields[0].serialTypeId = 10;
    reject(Error::INVALID_LAYOUT);
    declarations = layouts();
    declarations[0].senderId = std::numeric_limits<std::int32_t>::max() - 1;
    auto sparse = p::SubscriptionWriter::create(source, declarations, metadataLimits(), limits(), work);

    REQUIRE(sparse);
    const c::SubscriptionRequest input[] = {request(source, 0, u"A")};
    auto output = sparse->compose(Message::STREAM_ADD, input, work);

    REQUIRE(output);
    CHECK(output->bytes().size() == 10);
    auto tooSmall = metadataLimits();
    tooSmall.records = 2;
    auto rejected = p::SubscriptionWriter::create(source, declarations, tooSmall, limits(), work);

    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == Error::RESOURCE_LIMIT);
}

TEST_CASE("live actual allocation failures release reservations and preserve all prior output") {
    const auto source = schema();
    const auto declarations = layouts();
    bool reachedSuccess = false;
    std::size_t failures = 0;

    for (std::size_t permitted = 0; permitted < 128; ++permitted) {
        auto work = turn();
        auto result = [&] {
            AllocationFault fault(permitted);

            return p::SubscriptionWriter::create(source, declarations, metadataLimits(), limits(), work);
        }();

        if (result) {
            reachedSuccess = true;

            break;
        }

        CHECK(result.error() == Error::ALLOCATION_FAILURE);
        ++failures;
    }

    CHECK(reachedSuccess);
    CHECK(failures > 1);
    auto composer = writer(source);
    auto work = turn();
    const c::SubscriptionRequest input[] = {request(source, 0, u"A")};
    auto original = composer.compose(Message::STREAM_ADD, input, work);

    REQUIRE(original);
    auto rejected = [&] {
        AllocationFault fault(0);

        return composer.compose(Message::STREAM_ADD, input, work);
    }();

    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == Error::ALLOCATION_FAILURE);
    CHECK(composer.retainedBytes() == original->bytes().size());
    checkHex(*original, "0510fd014129");
    auto recovered = composer.compose(Message::STREAM_ADD, input, work);

    REQUIRE(recovered);
    CHECK(composer.retainedBytes() == original->bytes().size() + recovered->bytes().size());
}

#ifdef QDCXX_SUBSCRIPTION_CAPTURE_DIR
TEST_CASE("live native fixture emits deterministic production-parser interoperability inputs") {
    const auto source = schema();
    auto composer = writer(source);
    auto work = turn();
    const std::filesystem::path directory{QDCXX_SUBSCRIPTION_CAPTURE_DIR};

    std::filesystem::create_directories(directory);
    std::ofstream bytes(directory / "native.qtp", std::ios::binary | std::ios::trunc);
    std::ofstream expected(directory / "expected.tsv", std::ios::binary | std::ios::trunc);
    std::ofstream times(directory / "expected-time-reads.tsv", std::ios::binary | std::ios::trunc);

    REQUIRE(bytes);
    REQUIRE(expected);
    REQUIRE(times);
    expected << "message\tindex\treceiver_id\trecord_utf16\tsymbol_utf16\traw_time\n";
    times << "index\tvalue\n";
    const auto append = [&](const p::PendingSubscriptionOutput &output) {
        bytes.write(reinterpret_cast<const char *>(output.bytes().data()),
                    static_cast<std::streamsize>(output.bytes().size()));
    };
    const std::int32_t descriptionIds[] = {41, 3, 7};
    auto descriptions = composer.describe(descriptionIds, work);

    REQUIRE(descriptions);
    append(*descriptions);
    const std::array messages{Message::TICKER_ADD,    Message::TICKER_REMOVE, Message::STREAM_ADD,
                              Message::STREAM_REMOVE, Message::HISTORY_ADD,   Message::HISTORY_REMOVE};
    const std::array<std::u16string, 7> symbols{
        u"AB*", u"AB*", u"A?", u"", std::u16string{u'A', 0, u'B'}, std::u16string{static_cast<char16_t>(0xd800)},
        u"tail"};
    auto historySymbols = symbols;
    historySymbols[1] = u"MAX";
    const std::array<std::int64_t, 4> rawTimes{std::numeric_limits<std::int64_t>::min(),
                                               std::numeric_limits<std::int64_t>::max(), -1, 0};
    const std::array<std::int32_t, 7> localIds{0, 1, 0, 1, 0, 1, 0};
    const std::array<std::int32_t, 3> receiverIds{2, 1, 0};
    std::size_t index = 0;
    std::size_t timeIndex = 0;

    for (const auto message : messages) {
        const bool history = message == Message::HISTORY_ADD || message == Message::HISTORY_REMOVE;
        const bool addTime = message == Message::HISTORY_ADD;
        std::vector<c::SubscriptionRequest> inputs;

        for (std::size_t i = 0; i < symbols.size(); ++i) {
            const auto &symbol = history ? historySymbols[i] : symbols[i];
            const auto localId = history ? 2 : localIds[i];
            const auto raw = addTime ? rawTimes[i % rawTimes.size()] : 0;

            inputs.push_back(request(source, localId, symbol, addTime ? std::optional(raw) : std::nullopt));
            expected << static_cast<std::int32_t>(message) << '\t' << index++ << '\t'
                     << receiverIds[static_cast<std::size_t>(localId)] << '\t'
                     << utf16Hex(source.records()[static_cast<std::size_t>(localId)].name) << '\t' << utf16Hex(symbol)
                     << '\t' << raw << '\n';

            if (addTime) {
                times << timeIndex++ << '\t' << raw << '\n';
            }
        }

        auto output = composer.compose(message, inputs, work);

        REQUIRE(output);
        append(*output);
    }

    bytes.close();
    expected.close();
    times.close();
    CHECK(bytes.good());
    CHECK(expected.good());
    CHECK(times.good());
}
#endif

TEST_CASE("live late work failure rolls back reserved bytes and owner slots") {
    const auto source = schema();
    auto bounds = limits();
    bounds.outputs = 1;
    auto composer = writer(source, bounds);
    const c::SubscriptionRequest input[] = {request(source, 0, u"A")};

    for (unsigned category = 0; category < 3; ++category) {
        auto work = turn();

        if (category == 0) {
            work.visits = 2;
        } else if (category == 1) {
            work.textUnits = 3;
        } else {
            work.outputBytes = 13;
        }

        auto rejected = composer.compose(Message::STREAM_ADD, input, work);

        REQUIRE_FALSE(rejected);
        CHECK(rejected.error() == Error::WORK_LIMIT);
        CHECK(composer.retainedBytes() == 0);
        CHECK(work.operations == turn().operations - 1);
        auto fresh = turn();
        auto recovered = composer.compose(Message::STREAM_ADD, input, fresh);

        REQUIRE(recovered);
        checkHex(*recovered, "0510fd014129");
    }
}

TEST_CASE("live pending move assignment releases destination credits and preserves source cursor") {
    const auto source = schema();
    auto composer = writer(source);
    auto work = turn();
    const c::SubscriptionRequest first[] = {request(source, 0, u"A")};
    const c::SubscriptionRequest second[] = {request(source, 0, u"AB*")};
    auto destination = composer.compose(Message::STREAM_ADD, first, work);
    auto sourceOutput = composer.compose(Message::STREAM_ADD, second, work);

    REQUIRE(destination);
    REQUIRE(sourceOutput);
    REQUIRE(sourceOutput->consume(3));
    CHECK(composer.retainedBytes() == 14);
    *destination = std::move(*sourceOutput);

    CHECK(composer.retainedBytes() == 8);
    CHECK(sourceOutput->bytes().empty());
    CHECK(destination->pendingBytes().size() == 5);
    checkHex(*destination, "0710fd0341422a29");
    auto *self = &*destination;
    *destination = std::move(*self);

    CHECK(composer.retainedBytes() == 8);
    CHECK(destination->pendingBytes().size() == 5);
}

TEST_CASE("live null versus empty exact selectors and description text bounds are explicit") {
    const auto source = schema();
    auto composer = writer(source);
    auto work = turn();
    const c::SubscriptionRequest empty[] = {request(source, 0, u"")};
    auto output = composer.compose(Message::STREAM_ADD, empty, work);

    REQUIRE(output);
    checkHex(*output, "0410fd0029");
    auto bounds = limits();
    bounds.frameBytes = 5;
    bounds.operationBytes = 6;
    auto exact = writer(source, bounds);
    const c::SubscriptionRequest one[] = {request(source, 0, u"A")};
    auto exactly = exact.compose(Message::STREAM_ADD, one, work);

    REQUIRE(exactly);
    checkHex(*exactly, "0510fd014129");
    bounds = limits();
    bounds.text.codeUnits = 4;
    auto shortNames = writer(source, bounds);
    const std::int32_t id[] = {41};
    auto rejected = shortNames.describe(id, work);

    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == Error::RESOURCE_LIMIT);
    bounds = limits();
    bounds.text.bodyBytes = 4;
    auto shortBody = writer(source, bounds);
    auto rejectedBody = shortBody.describe(id, work);

    REQUIRE_FALSE(rejectedBody);
    CHECK(rejectedBody.error() == Error::RESOURCE_LIMIT);
}

TEST_CASE("live compact frame and symbol prefixes preserve every partial-write boundary") {
    const auto source = schema();
    auto composer = writer(source);
    auto work = turn();

    struct Boundary {
        std::size_t symbolUnits;       ///< Exact ASCII symbol units.
        std::string_view framePrefix;  ///< Independently specified compact body-byte count.
        std::string_view symbolPrefix; ///< Independently specified compact unit count.
    };

    const Boundary boundaries[] = {{59, "3f", "3b"}, {60, "8040", "3c"}, {63, "8043", "3f"}, {64, "8045", "8040"}};

    for (const auto &boundary : boundaries) {
        const std::u16string symbol(boundary.symbolUnits, u'A');
        const c::SubscriptionRequest input[] = {request(source, 0, symbol)};
        auto expected = hex(boundary.framePrefix);
        const auto symbolPrefix = hex(boundary.symbolPrefix);

        expected.push_back(std::byte{0x10});
        expected.push_back(std::byte{0xfd});
        expected.insert(expected.end(), symbolPrefix.begin(), symbolPrefix.end());
        expected.insert(expected.end(), boundary.symbolUnits, std::byte{0x41});
        expected.push_back(std::byte{0x29});

        for (std::size_t split = 0; split <= expected.size(); ++split) {
            auto output = composer.compose(Message::STREAM_ADD, input, work);

            REQUIRE(output);
            CHECK(std::ranges::equal(output->bytes(), expected));
            REQUIRE(output->consume(split));
            CHECK(std::ranges::equal(output->pendingBytes(), std::span(expected).subspan(split)));
            CHECK(std::ranges::equal(output->bytes(), expected));
            REQUIRE(output->consume(expected.size() - split));
            CHECK(output->pendingBytes().empty());
            CHECK(composer.retainedBytes() == expected.size());
        }

        CHECK(composer.retainedBytes() == 0);
    }
}

TEST_CASE("live composer retains schema provenance after source destruction and refuses unmapped context") {
    auto retained = [] {
        const auto source = schema();
        auto composer = writer(source);
        const auto context = source.recordContext(2);

        return std::pair{std::move(composer), context};
    }();
    auto work = turn();
    const c::SubscriptionRequest indexed[] = {{{retained.second, u"A"}, -1}};
    auto output = retained.first.compose(Message::HISTORY_ADD, indexed, work);

    REQUIRE(output);
    checkHex(*output, "0615fd0141077f");
    const auto source = schema();
    auto declarations = layouts();

    declarations.pop_back();
    auto subset = p::SubscriptionWriter::create(source, declarations, metadataLimits(), limits(), work);

    REQUIRE(subset);
    const c::SubscriptionRequest unmapped[] = {request(source, 2, u"A", 0)};
    auto rejected = subset->compose(Message::HISTORY_ADD, unmapped, work);

    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == Error::INVALID_CONTEXT);
    CHECK(subset->retainedBytes() == 0);
    const c::SubscriptionRequest mapped[] = {request(source, 0, u"A")};
    auto recovered = subset->compose(Message::STREAM_ADD, mapped, work);

    REQUIRE(recovered);
    checkHex(*recovered, "0510fd014129");
}
