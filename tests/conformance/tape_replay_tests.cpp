// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/replay/tape_replay.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <new>
#include <stdexcept>

// Windows GDI's coordinate-mode macro must not replace the replay policy enumerator in this test.
#ifdef RELATIVE
#    undef RELATIVE
#endif

namespace allocation_test {
thread_local bool enabled = false;      ///< Fault only the explicitly armed replay invocation.
thread_local std::size_t remaining = 0; ///< Permitted successful ordinary allocations.

/** Allocate ordinary storage through the real malloc boundary.
 * @param size Requested bytes.
 * @return Owned storage or bad_alloc.
 */
void *allocate(std::size_t size) {
    if (enabled) {
        if (remaining == 0) {
            throw std::bad_alloc();
        }

        --remaining;
    }

    if (auto *value = std::malloc(size == 0 ? 1 : size)) {
        return value;
    }

    throw std::bad_alloc();
}
} // namespace allocation_test

/** Replace ordinary allocation only for scoped fault verification.
 * @param size Bytes requested.
 * @return Owned storage or bad_alloc.
 */
void *operator new(std::size_t size) {
    return allocation_test::allocate(size);
}

/** Replace array allocation for the same narrowly armed boundary.
 * @param size Bytes requested.
 * @return Owned storage or bad_alloc.
 */
void *operator new[](std::size_t size) {
    return allocation_test::allocate(size);
}

/** Release paired storage.
 * @param value Ordinary pointer or null.
 */
void operator delete(void *value) noexcept {
    std::free(value);
}

/** Release paired array storage.
 * @param value Ordinary pointer or null.
 */
void operator delete[](void *value) noexcept {
    std::free(value);
}

/** Release paired sized storage.
 * @param value Ordinary pointer or null.
 * @param size Supplied size.
 */
void operator delete(void *value, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(value);
}

/** Release paired sized array storage.
 * @param value Ordinary pointer or null.
 * @param size Supplied size.
 */
void operator delete[](void *value, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(value);
}

namespace {
namespace r = org::ttldtor::qdcxx::replay;
namespace p = org::ttldtor::qdcxx::protocol;
namespace c = org::ttldtor::qdcxx::core;
std::filesystem::path root; ///< Explicit source root, never inferred inside production code.

/** Finite generous admission fixture allowances.
 * @return Explicit independent reader/layout/storage budgets.
 */
p::AdmissionLimits admissionLimits() {
    c::SchemaLimits schema{256, 4096, 8192, 1024, 1048576};
    p::TapeLimits reader{{1048576, 1048585},
                         16777216,
                         65536,
                         {1024, 32, {1048576, 1024}, 1048576},
                         {schema, 1024, {16384, 1024, 1048576}},
                         67108864,
                         {1048576,
                          1048576,
                          8388608,
                          1048576,
                          8388608,
                          4194304,
                          {1048576, 16384, 16384},
                          {1048576, 16384},
                          16777216,
                          4194304,
                          4194304},
                         65536};

    return {16777216, reader, {schema, 1024, 16384, 1024, 1048576}, 67108864, 16777216, 4194304};
}

/** Finite writer allowances for native synthetic recording production.
 * @return Explicit output/work/scratch limits.
 */
p::WriterLimits writerLimits() {
    return {65536,   1000000, 1000,  10000,   100000, {10000, 10000}, 1000000, {10000, 10000},
            1000000, 1000000, 10000, 1000000, 65545,  10000,          10000000};
}

/** Finite replay defaults supplied solely by this test owner.
 * @return Explicit allowances, not production defaults.
 */
r::ReplayLimits replayLimits() {
    return {admissionLimits().reader, 1000, 1001, 100000, 10000, 1000000, 10000, 10000};
}

/** Exact all-type business values, including nullable isolated UTF-16 units.
 * @param ordinal Stable input record sequence.
 * @return Owned values independent of timestamp and scheduling.
 */
std::vector<c::FieldValue> values(std::int32_t ordinal) {
    return {{c::SerialFieldType::UTF_CHAR, 0x1f680},
            {c::SerialFieldType::COMPACT_INT, ordinal},
            {c::SerialFieldType::UTF_CHAR_ARRAY, 0, std::u16string{u'A', 0xd800, 0}},
            {c::SerialFieldType::SHORT_STRING, -123},
            {c::SerialFieldType::TIME_SECONDS, 99},
            {c::SerialFieldType::SEQUENCE, -7},
            {c::SerialFieldType::DATE, 12345},
            {c::SerialFieldType::WIDE_DECIMAL, std::numeric_limits<std::int64_t>::min() + ordinal}};
}

/** Produce and strictly admit a native writer recording with all exact existing types.
 * @param times One inherited raw timestamp per data frame; absent means no heartbeat.
 * @param counts Number of records in each data frame.
 * @param second Select the second layout, exercising a two-element sender lookup.
 * @return Immutable owned strictly admitted tape.
 */
p::AdmittedTape tape(std::span<const std::optional<std::int64_t>> times, std::span<const std::size_t> counts,
                     bool second = false) {
    REQUIRE(times.size() == counts.size());
    auto schema = c::LocalSchema::create(std::vector<c::LocalRecord>{{0,
                                                                      u"Item",
                                                                      false,
                                                                      {{2}, {8}, {10}, {40}, {56}, {72}, {88}, {120}},
                                                                      {{u"Char", 0},
                                                                       {u"Count", 1},
                                                                       {u"Text", 2},
                                                                       {u"Short", 3},
                                                                       {u"Seconds", 4},
                                                                       {u"Sequence", 5},
                                                                       {u"Date", 6},
                                                                       {u"Wide", 7}}},
                                                                     {1, u"Empty", false, {}, {}}},
                                         admissionLimits().layouts.schema, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(schema);
    auto layouts = p::OutgoingLayouts::create(*schema,
                                              std::vector<p::OutgoingRecord>{{37,
                                                                              0,
                                                                              u"Item",
                                                                              {{u"Char", 2, 0},
                                                                               {u"Count", 8, 1},
                                                                               {u"Text", 10, 2},
                                                                               {u"Short", 40, 3},
                                                                               {u"Seconds", 56, 4},
                                                                               {u"Sequence", 72, 5},
                                                                               {u"Date", 88, 6},
                                                                               {u"Wide", 120, 7}}},
                                                                             {127, 1, u"Empty", {}}},
                                              admissionLimits().layouts);
    REQUIRE(layouts);
    auto writer = p::TapeWriter::create(*layouts, {{}, {}, u"MESSAGE", true}, writerLimits());
    REQUIRE(writer);
    std::vector<std::byte> bytes;
    const auto append = [&](std::expected<p::WriterChunk, p::WriterDiagnostic> chunk) {
        REQUIRE(chunk);
        bytes.insert(bytes.end(), chunk->bytes.begin(), chunk->bytes.end());
    };
    append(writer->begin());
    const std::array<std::int32_t, 2> ids{37, 127};
    append(writer->describe(ids));
    std::int32_t ordinal = 0;

    for (std::size_t frame = 0; frame < times.size(); ++frame) {
        if (times[frame]) {
            append(writer->heartbeat({1, times[frame]}));
        }

        std::vector<std::vector<c::FieldValue>> storage;
        storage.reserve(counts[frame]);

        for (std::size_t i = 0; i < counts[frame]; ++i) {
            storage.push_back(second ? std::vector<c::FieldValue>{} : values(ordinal++));
        }

        std::vector<p::OutgoingRecordView> records;

        for (const auto &fields : storage) {
            records.push_back({second ? 127 : 37, u"IBM", fields});
        }

        append(writer->data(records));
    }

    append(writer->marker());
    append(writer->finish());
    auto admitted = p::admitTape(*schema, bytes, admissionLimits());
    REQUIRE(admitted);

    return std::move(*admitted);
}

/** Prompt injected cancellation and optional adverse poll behavior. */
struct Cancellation final : r::ReplayCancellation {
    std::size_t polls = 0;                   ///< Actual externally observed poll calls.
    std::function<bool(std::size_t)> action; ///< Synchronous bounded test mutation.

    /** Poll the injected behavior.
     * @return Requested cancellation or false.
     */
    bool isCancelled() override {
        ++polls;

        return action ? action(polls) : false;
    }
};

/** Deterministic clock with no wall-clock sampling or sleep. */
struct Clock final : r::ReplayClock {
    std::int64_t current = 0;            ///< Current deterministic monotonic nanoseconds.
    std::vector<std::int64_t> deadlines; ///< Owned observed wait deadlines.
    std::size_t samples = 0;             ///< Observed samples.
    std::function<std::expected<std::int64_t, r::ClockError>(std::size_t)> sample; ///< Adverse clock behavior.
    std::function<r::WaitResult(std::int64_t)> wait;                               ///< Adverse wait behavior.

    /** Sample injected monotonic time.
     * @return Current time or injected error.
     */
    std::expected<std::int64_t, r::ClockError> nowNs() override {
        ++samples;

        return sample ? sample(samples) : std::expected<std::int64_t, r::ClockError>(current);
    }

    /** Reach a deadline immediately in deterministic test time.
     * @param deadline Required deadline.
     * @param cancellation Borrowed cancellation, not retained.
     * @return Injected wait result or READY after advancing current.
     */
    r::WaitResult waitUntilNs(std::int64_t deadline, r::ReplayCancellation &cancellation) override {
        static_cast<void>(cancellation);
        deadlines.push_back(deadline);

        if (wait) {
            return wait(deadline);
        }

        current = deadline;

        return r::WaitResult::READY;
    }
};

/** Owned copy of delivered data, demonstrating the view expires without losing caller copies. */
struct Delivery {
    p::StreamDataRecord record; ///< Deep-owned decoded value/symbol and owned provenance.
    p::TapeTiming timing;       ///< Exact copied metadata.
    std::size_t frame;          ///< Envelope index.
    std::size_t index;          ///< Within-frame index.
    std::int32_t sender;        ///< Exact wire identity.
    std::int64_t now;           ///< Fake clock state at delivery.
};

/** Prompt synchronous observer with independently owned retained copies and failure behavior. */
struct Observer final : r::ReplayObserver {
    Clock &clock;                  ///< Borrowed fake clock, only for asserting schedule behavior.
    std::vector<Delivery> records; ///< Caller-owned copies surviving replay return.
    std::size_t attempts = 0;      ///< All record callbacks including failed attempts.
    std::size_t finishes = 0;      ///< All completion callbacks.
    std::function<r::ObserverResult(std::size_t)> action; ///< Optional record mutation.
    std::function<r::ObserverResult()> completion;        ///< Optional completion mutation.

    /** Bind test clock.
     * @param value Borrowed fake clock.
     */
    explicit Observer(Clock &value) : clock(value) {
    }

    /** Copy complete borrowed values before returning.
     * @param view Ephemeral reader-owned record.
     * @return Injected failure or OK after copying.
     */
    r::ObserverResult onRecord(const r::ReplayRecordView &view) override {
        const auto index = attempts++;

        if (action) {
            const auto status = action(index);

            if (status != r::ObserverResult::OK) {
                return status;
            }
        }

        records.push_back({view.record, view.timing, view.frameIndex, view.recordIndex, view.senderId, clock.current});

        return r::ObserverResult::OK;
    }

    /** Complete test output exactly once when asked.
     * @return Injected completion outcome or OK.
     */
    r::ObserverResult finish() override {
        ++finishes;

        return completion ? completion() : r::ObserverResult::OK;
    }
};

/** Split fixture cells without losing empty values.
 * @param text Borrowed fixture row.
 * @param separator Exact delimiter.
 * @return Owned string cells.
 */
std::vector<std::string> split(std::string_view text, char separator) {
    std::vector<std::string> cells;

    while (true) {
        const auto end = text.find(separator);
        cells.emplace_back(text.substr(0, end));

        if (end == std::string_view::npos) {
            return cells;
        }

        text.remove_prefix(end + 1);
    }
}

/** Parse exact signed fixture decimals.
 * @param text Complete fixture integer.
 * @return Signed value or malformed fixture exception.
 */
std::int64_t decimal(std::string_view text) {
    std::int64_t result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);

    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw std::runtime_error("fixture decimal");
    }

    return result;
}

/** Render a stable result for comparison with independent TSV acceptance rows.
 * @param outcome Terminal outcome.
 * @return Static enum spelling.
 */
std::string_view name(r::ReplayOutcome outcome) {
    switch (outcome) {
    case r::ReplayOutcome::COMPLETED:
        return "COMPLETED";
    case r::ReplayOutcome::CANCELLED:
        return "CANCELLED";
    case r::ReplayOutcome::INPUT_FAILURE:
        return "INPUT_FAILURE";
    case r::ReplayOutcome::OBSERVER_FAILURE:
        return "OBSERVER_FAILURE";
    case r::ReplayOutcome::CLOCK_FAILURE:
        return "CLOCK_FAILURE";
    case r::ReplayOutcome::WAIT_FAILURE:
        return "WAIT_FAILURE";
    case r::ReplayOutcome::TIMING_RANGE:
        return "TIMING_RANGE";
    case r::ReplayOutcome::CANCELLATION_FAILURE:
        return "CANCELLATION_FAILURE";
    case r::ReplayOutcome::RESOURCE_LIMIT:
        return "RESOURCE_LIMIT";
    case r::ReplayOutcome::ALLOCATION_FAILURE:
        return "ALLOCATION_FAILURE";
    case r::ReplayOutcome::INVALID_CONFIGURATION:
        return "INVALID_CONFIGURATION";
    }

    return "invalid";
}

/** Assert independently exposed counters sum to the public work ledger.
 * @param result Actual terminal result.
 */
void ledger(const r::ReplayResult &result) {
    const auto &a = result.counters;
    CHECK(a.workUnits == a.frameAttempts + a.recordAttempts + a.layoutVisits + a.cancellationPolls + a.clockCalls +
                             a.waitCalls + a.callbackCalls);
}
} // namespace

TEST_CASE("all reviewed replay policy rows execute through native writer and admission") {
    std::ifstream file(root / "tests/fixtures/protocol/tape-write/contracts/replay.tsv");
    REQUIRE(file.good());
    std::string row;
    std::getline(file, row);
    std::size_t tested = 0;

    while (std::getline(file, row)) {
        if (!row.empty() && row.back() == '\r') {
            row.pop_back();
        }

        const auto cells = split(row, '\t');
        REQUIRE(cells.size() == 13);
        CAPTURE(cells[0]);
        std::vector<std::optional<std::int64_t>> times;

        for (const auto &cell : split(cells[2], ',')) {
            times.push_back(cell == "absent" ? std::nullopt : std::optional(decimal(cell)));
        }

        std::vector<std::size_t> counts;

        for (const auto &cell : split(cells[3], ',')) {
            counts.push_back(static_cast<std::size_t>(decimal(cell)));
        }

        auto source = tape(times, counts);
        Clock clock;
        clock.current = decimal(cells[4]);
        Cancellation cancel;
        Observer observer(clock);
        auto limits = replayLimits();
        limits.maxRelativeMs = static_cast<std::uint64_t>(decimal(cells[5]));
        limits.maxWaitSliceNs = decimal(cells[6]);
        const auto &mutation = cells[7];

        if (mutation == "wait-ready-before-deadline") {
            clock.wait = [](auto) {
                return r::WaitResult::READY;
            };
        }

        if (mutation == "clock-decreases") {
            clock.sample = [](auto n) {
                return std::expected<std::int64_t, r::ClockError>(n == 1 ? 1 : 0);
            };
        }

        if (mutation == "cancel-before-parse") {
            cancel.action = [](auto) {
                return true;
            };
        }

        if (mutation == "cancel-before-record-1") {
            cancel.action = [&](auto) {
                return observer.records.size() == 1;
            };
        }

        if (mutation == "wait-cancelled") {
            clock.wait = [](auto) {
                return r::WaitResult::CANCELLED;
            };
        }

        if (mutation == "cancel-final-after-eof") {
            cancel.action = [](auto n) {
                return n == 8;
            };
        }

        if (mutation == "observer-record-1-fails") {
            observer.action = [](auto n) {
                return n == 1 ? r::ObserverResult::FAILED : r::ObserverResult::OK;
            };
        }

        if (mutation == "observer-finish-flush-fails") {
            observer.completion = []() {
                return r::ObserverResult::FAILED;
            };
        }

        if (mutation == "reader-processedRecords-1") {
            limits.reader.data.processedRecords = 1;
        }

        if (mutation == "maxWaitCalls-1") {
            limits.maxWaitCalls = 1;
        }

        if (mutation == "cancel-poll-throws") {
            cancel.action = [](auto) -> bool {
                throw std::bad_alloc();
            };
        }

        if (mutation == "wait-failed") {
            clock.wait = [](auto) {
                return r::WaitResult::FAILED;
            };
        }

        const auto policy = cells[1] == "IMMEDIATE" ? r::ReplayPolicy::IMMEDIATE : r::ReplayPolicy::RELATIVE;
        const auto result = r::replayTape(source, observer, clock, cancel, limits, policy);
        CHECK(name(result.outcome) == cells[8]);
        CHECK(result.counters.deliveredRecords == static_cast<std::size_t>(decimal(cells[10])));
        CHECK(result.cleanEof == (cells[11] == "true"));
        CHECK(result.observerFinished == (cells[12] == "true"));
        CHECK(observer.finishes == (result.observerFinished || result.stage == "observer-finish" ? 1 : 0));
        ledger(result);
        std::size_t delivered = 0;
        std::size_t frame = 2;
        const auto targets = split(cells[9], ',');

        for (std::size_t i = 0; i < times.size(); ++i) {
            if (times[i]) {
                ++frame;
            }

            for (std::size_t j = 0; j < counts[i] && delivered < observer.records.size(); ++j, ++delivered) {
                const auto &value = observer.records[delivered];
                CHECK(value.frame == frame);
                CHECK(value.index == j);
                CHECK(value.sender == 37);
                CHECK(value.timing.timestampMs == times[i]);
                CHECK(value.record.context.matches(source.schema().recordContext(0)));
                CHECK(value.record.typedValues == values(static_cast<std::int32_t>(delivered)));
                CHECK(value.record.symbol.units == u"IBM");

                if (policy == r::ReplayPolicy::RELATIVE && i < targets.size() && targets[i] != "immediate" &&
                    targets[i] != "error" && targets[i] != "excluded") {
                    CHECK(value.now == decimal(targets[i]));
                }
            }

            ++frame;
        }

        if (policy == r::ReplayPolicy::IMMEDIATE) {
            CHECK(clock.samples == 0);
            CHECK(clock.deadlines.empty());
        }

        ++tested;
    }

    CHECK(tested == 24);
}

TEST_CASE("exact ledger and finite count boundaries are independently derived") {
    const std::array<std::optional<std::int64_t>, 2> times{0, 1};
    const std::array<std::size_t, 2> counts{1, 1};
    auto source = tape(times, counts);
    const auto run = [&](r::ReplayLimits limits) {
        Clock clock;
        Cancellation cancel;
        Observer observer(clock);

        return r::replayTape(source, observer, clock, cancel, limits, r::ReplayPolicy::RELATIVE);
    };
    auto limits = replayLimits();
    const auto actual = run(limits);
    REQUIRE(actual.outcome == r::ReplayOutcome::COMPLETED);
    // 7 frames; 2 records/layout visits; 1 initial+7 frame+2 record+1 final+2 wait polls; 3 clock; 1 wait; 3 callbacks.
    CHECK(actual.counters.workUnits == 31);
    CHECK(actual.counters.frameAttempts == 7);
    CHECK(actual.counters.cancellationPolls == 13);
    CHECK(actual.counters.layoutVisits == 2);
    CHECK(actual.counters.clockCalls == 3);
    CHECK(actual.counters.waitCalls == 1);
    CHECK(actual.counters.parsedBytes == source.bytes().size());
    CHECK(actual.counters.parsedRecords == 2);
    CHECK(actual.counters.parsedFields == 16);
    using Budget = std::size_t r::ReplayLimits::*;
    const std::array<std::pair<Budget, std::size_t>, 5> budgets{{{&r::ReplayLimits::workUnits, 31},
                                                                 {&r::ReplayLimits::callbackCalls, 3},
                                                                 {&r::ReplayLimits::deliveredRecords, 2},
                                                                 {&r::ReplayLimits::clockCalls, 3},
                                                                 {&r::ReplayLimits::maxWaitCalls, 1}}};

    for (const auto &[member, exact] : budgets) {
        for (const auto bound : std::array<std::size_t, 4>{0, exact - 1, exact, exact + 1}) {
            auto changed = limits;
            changed.*member = bound;
            const auto result = run(changed);
            CHECK(result.outcome == (bound < exact ? r::ReplayOutcome::RESOURCE_LIMIT : r::ReplayOutcome::COMPLETED));
            ledger(result);
            CHECK(result.counters.workUnits <= changed.workUnits);
            CHECK(result.counters.callbackCalls <= changed.callbackCalls);
            CHECK(result.counters.deliveredRecords <= changed.deliveredRecords);
            CHECK(result.counters.clockCalls <= changed.clockCalls);
            CHECK(result.counters.waitCalls <= changed.maxWaitCalls);
        }
    }

    for (auto bound : std::array<std::uint64_t, 3>{0, 1, 2}) {
        auto changed = limits;
        changed.maxRelativeMs = bound;
        CHECK(run(changed).outcome == (bound == 0 ? r::ReplayOutcome::TIMING_RANGE : r::ReplayOutcome::COMPLETED));
    }

    for (auto slice : std::array<std::int64_t, 4>{-1, 0, 500000, 1000001}) {
        auto changed = limits;
        changed.maxWaitSliceNs = slice;
        const auto result = run(changed);
        CHECK(result.outcome == (slice <= 0 ? r::ReplayOutcome::INVALID_CONFIGURATION : r::ReplayOutcome::COMPLETED));
    }
}

TEST_CASE("callbacks errors exceptions and cancellation precedence are explicit") {
    const std::array<std::optional<std::int64_t>, 2> times{0, 1};
    const std::array<std::size_t, 2> counts{1, 2};
    auto source = tape(times, counts);

    for (int mutation = 0; mutation < 10; ++mutation) {
        Clock clock;
        Cancellation cancel;
        Observer observer(clock);
        r::ReplayOutcome expected = r::ReplayOutcome::CLOCK_FAILURE;

        if (mutation == 0) {
            clock.sample = [](auto) -> std::expected<std::int64_t, r::ClockError> {
                return std::unexpected(r::ClockError::FAILED);
            };
        }

        if (mutation == 1) {
            clock.sample = [](auto) -> std::expected<std::int64_t, r::ClockError> {
                throw std::bad_alloc();
            };
        }

        if (mutation == 2) {
            clock.wait = [](auto) -> r::WaitResult {
                throw std::length_error("caller");
            };
        }

        if (mutation == 3) {
            observer.action = [](auto) -> r::ObserverResult {
                throw std::bad_alloc();
            };
            expected = r::ReplayOutcome::OBSERVER_FAILURE;
        }

        if (mutation == 4) {
            observer.completion = []() -> r::ObserverResult {
                throw std::length_error("caller");
            };
            expected = r::ReplayOutcome::OBSERVER_FAILURE;
        }

        if (mutation == 5) {
            cancel.action = [](auto) -> bool {
                throw std::runtime_error("caller");
            };
            expected = r::ReplayOutcome::CANCELLATION_FAILURE;
        }

        if (mutation == 6) {
            clock.wait = [&](auto deadline) {
                clock.current = deadline;

                return r::WaitResult::CANCELLED;
            };
            expected = r::ReplayOutcome::CANCELLED;
        }

        if (mutation == 7) {
            clock.wait = [&](auto deadline) {
                clock.current = deadline;
                cancel.action = [](auto) {
                    return true;
                };

                return r::WaitResult::READY;
            };
            expected = r::ReplayOutcome::CANCELLED;
        }

        if (mutation == 8) {
            observer.action = [&](auto) {
                cancel.action = [](auto) {
                    return true;
                };

                return r::ObserverResult::FAILED;
            };
            expected = r::ReplayOutcome::OBSERVER_FAILURE;
        }

        if (mutation == 9) {
            observer.completion = [&]() {
                cancel.action = [](auto) {
                    return true;
                };

                return r::ObserverResult::FAILED;
            };
            expected = r::ReplayOutcome::OBSERVER_FAILURE;
        }

        const auto result = r::replayTape(source, observer, clock, cancel, replayLimits(), r::ReplayPolicy::RELATIVE);
        CAPTURE(mutation);
        CHECK(result.outcome == expected);
        CHECK_FALSE(result.observerFinished);
        ledger(result);

        if (mutation == 3 || mutation == 8) {
            CHECK(result.counters.failedRecordCallbacks == 1);
            CHECK(result.counters.deliveredRecords == 0);
        }

        if (mutation == 4 || mutation == 9) {
            CHECK(result.cleanEof);
            CHECK(observer.finishes == 1);
        }
    }
}

TEST_CASE("stricter fresh reader limits preserve original input diagnostics and discard failed prefixes") {
    const std::array<std::optional<std::int64_t>, 1> times{0};
    const std::array<std::size_t, 1> counts{2};
    auto source = tape(times, counts);

    for (auto byteLimit : std::array<std::size_t, 3>{1, 2, source.bytes().size() - 2}) {
        Clock clock;
        Cancellation cancel;
        Observer observer(clock);
        auto limits = replayLimits();
        limits.reader.inputBytes = byteLimit;
        const auto result = r::replayTape(source, observer, clock, cancel, limits, r::ReplayPolicy::IMMEDIATE);
        CHECK(result.outcome == r::ReplayOutcome::INPUT_FAILURE);
        REQUIRE(result.reader);
        CHECK(result.reader->error == p::TapeError::RESOURCE_LIMIT);
        CHECK(result.reader->stage == "input");
        CHECK(result.counters.parsedBytes == byteLimit);
        CHECK_FALSE(result.cleanEof);
        CHECK(observer.finishes == 0);
        ledger(result);

        if (byteLimit <= 2) {
            CHECK(observer.records.empty());
            CHECK(result.frameIndex == 0);
            CHECK(result.counters.frameAttempts == 2);
        }
    }

    for (auto recordLimit : std::array<std::size_t, 2>{0, 1}) {
        Clock clock;
        Cancellation cancel;
        Observer observer(clock);
        auto limits = replayLimits();
        limits.reader.data.processedRecords = recordLimit;
        const auto result = r::replayTape(source, observer, clock, cancel, limits, r::ReplayPolicy::IMMEDIATE);
        CHECK(result.outcome == r::ReplayOutcome::INPUT_FAILURE);
        REQUIRE(result.reader);
        CHECK(result.reader->error == p::TapeError::RESOURCE_LIMIT);
        CHECK(observer.records.empty());
        CHECK(result.counters.deliveredRecords == 0);
        CHECK(result.counters.parsedRecords == recordLimit);
        CHECK(observer.finishes == 0);
        CHECK_FALSE(result.cleanEof);
        ledger(result);
    }

    // A callback allowance failure wins over a later per-record cancellation poll that would throw.
    Clock clock;
    Cancellation cancel;
    Observer observer(clock);
    auto limits = replayLimits();
    limits.callbackCalls = 0;
    cancel.action = [](auto n) -> bool {
        if (n == 6) {
            throw std::runtime_error("late poll");
        }

        return false;
    };
    const auto limited = r::replayTape(source, observer, clock, cancel, limits, r::ReplayPolicy::IMMEDIATE);
    CHECK(limited.outcome == r::ReplayOutcome::RESOURCE_LIMIT);
    CHECK(limited.stage == "callback-calls");
    CHECK(cancel.polls == 5);
}

TEST_CASE("borrowed copies sender lookup late clock and independent sessions retain exact values") {
    const std::array<std::optional<std::int64_t>, 3> times{0, 2, 1};
    const std::array<std::size_t, 3> counts{2, 1, 1};
    auto source = tape(times, counts);

    for (int session = 0; session < 2; ++session) {
        Clock clock;
        Cancellation cancel;
        Observer observer(clock);
        observer.action = [&](auto) {
            clock.current += 3000000;

            return r::ObserverResult::OK;
        };
        const auto result = r::replayTape(source, observer, clock, cancel, replayLimits(), r::ReplayPolicy::RELATIVE);
        REQUIRE(result.outcome == r::ReplayOutcome::COMPLETED);
        CHECK(clock.deadlines.empty());
        CHECK(observer.records.size() == 4);
        CHECK(observer.records.back().record.typedValues == values(3));
        CHECK(observer.records.back().record.symbol.units == u"IBM");
        CHECK(observer.records[0].timing.timestampMs == 0);
        CHECK(observer.records.back().timing.timestampMs == 1);
        ledger(result);
    }

    auto second = tape(std::span(times).first(1), std::span(counts).first(1), true);
    Clock clock;
    Cancellation cancel;
    Observer observer(clock);
    const auto result = r::replayTape(second, observer, clock, cancel, replayLimits(), r::ReplayPolicy::IMMEDIATE);
    REQUIRE(result.outcome == r::ReplayOutcome::COMPLETED);
    CHECK(result.counters.layoutVisits == 4);
    CHECK(observer.records[0].sender == 127);
    CHECK(observer.records[0].record.localId == 1);
    auto moved = std::move(second);
    Observer inactive(clock);
    const auto invalid = r::replayTape(second, inactive, clock, cancel, replayLimits(), r::ReplayPolicy::IMMEDIATE);
    CHECK(invalid.outcome == r::ReplayOutcome::INVALID_CONFIGURATION);
    CHECK(invalid.counters.workUnits == 0);
    CHECK(moved.valid());
    const auto badPolicy =
        r::replayTape(moved, inactive, clock, cancel, replayLimits(), static_cast<r::ReplayPolicy>(-1));
    CHECK(badPolicy.outcome == r::ReplayOutcome::INVALID_CONFIGURATION);
    CHECK(badPolicy.counters.workUnits == 0);
}

TEST_CASE("nested wait polls remain caller work and final cancellation precedes completion budget") {
    const std::array<std::optional<std::int64_t>, 2> times{0, 1};
    const std::array<std::size_t, 2> counts{1, 1};
    auto source = tape(times, counts);

    for (bool throwing : std::array<bool, 2>{false, true}) {
        Clock clock;
        Cancellation cancel;
        Observer observer(clock);
        clock.wait = [&](auto deadline) {
            if (throwing) {
                cancel.action = [](auto) -> bool {
                    throw std::runtime_error("nested caller poll");
                };
            }

            static_cast<void>(cancel.isCancelled());
            clock.current = deadline;

            return r::WaitResult::READY;
        };
        const auto result = r::replayTape(source, observer, clock, cancel, replayLimits(), r::ReplayPolicy::RELATIVE);
        CHECK(result.outcome == (throwing ? r::ReplayOutcome::CLOCK_FAILURE : r::ReplayOutcome::COMPLETED));
        CHECK(cancel.polls == result.counters.cancellationPolls + 1);
        ledger(result);
    }

    const std::array<std::optional<std::int64_t>, 1> untimed{std::nullopt};
    const std::array<std::size_t, 1> empty{0};
    auto header = tape(untimed, empty);
    Clock clock;
    Cancellation cancel;
    Observer observer(clock);
    auto limits = replayLimits();
    limits.callbackCalls = 0;
    cancel.action = [](auto poll) {
        return poll == 6;
    };
    const auto cancelled = r::replayTape(header, observer, clock, cancel, limits, r::ReplayPolicy::RELATIVE);
    CHECK(cancelled.outcome == r::ReplayOutcome::CANCELLED);
    CHECK(cancelled.cleanEof);
    CHECK(observer.finishes == 0);
    CHECK(cancelled.counters.callbackCalls == 0);
    CHECK(clock.samples == 0);
}

TEST_CASE("relative multiplication addition and slice deadlines have exact representable boundaries") {
    constexpr auto largest = std::numeric_limits<std::int64_t>::max();
    constexpr auto elapsed = largest / 1000000;

    for (auto delta : std::array<std::int64_t, 3>{elapsed - 1, elapsed, elapsed + 1}) {
        const std::array<std::optional<std::int64_t>, 2> times{0, delta};
        const std::array<std::size_t, 2> counts{1, 1};
        auto source = tape(times, counts);
        Clock clock;
        Cancellation cancel;
        Observer observer(clock);
        auto limits = replayLimits();
        limits.maxRelativeMs = std::numeric_limits<std::uint64_t>::max();
        limits.maxWaitSliceNs = largest;
        const auto result = r::replayTape(source, observer, clock, cancel, limits, r::ReplayPolicy::RELATIVE);
        CHECK(result.outcome == (delta > elapsed ? r::ReplayOutcome::TIMING_RANGE : r::ReplayOutcome::COMPLETED));

        if (delta <= elapsed) {
            REQUIRE(clock.deadlines.size() == 1);
            CHECK(clock.deadlines[0] == delta * 1000000);
        } else {
            CHECK(clock.deadlines.empty());
            CHECK(result.counters.deliveredRecords == 1);
        }

        ledger(result);
    }

    const std::array<std::optional<std::int64_t>, 2> times{0, 1};
    const std::array<std::size_t, 2> counts{1, 1};
    auto source = tape(times, counts);

    for (auto start : std::array<std::int64_t, 3>{largest - 1000001, largest - 1000000, largest - 999999}) {
        Clock clock;
        clock.current = start;
        Cancellation cancel;
        Observer observer(clock);
        const auto result = r::replayTape(source, observer, clock, cancel, replayLimits(), r::ReplayPolicy::RELATIVE);
        CHECK(result.outcome ==
              (start > largest - 1000000 ? r::ReplayOutcome::TIMING_RANGE : r::ReplayOutcome::COMPLETED));

        if (result.outcome == r::ReplayOutcome::COMPLETED) {
            REQUIRE(clock.deadlines.size() == 1);
            CHECK(clock.deadlines[0] == start + 1000000);
        }

        ledger(result);
    }
}

TEST_CASE("raw cursor sequence matches locked production Java packing observations") {
    std::ifstream file(root / "tests/fixtures/protocol/raw-tape/expected/packing.tsv");
    REQUIRE(file.good());
    std::vector<std::optional<std::int64_t>> times;
    std::vector<std::int64_t> expected;
    std::string row;

    while (std::getline(file, row)) {
        if (!row.empty() && row.back() == '\r') {
            row.pop_back();
        }

        const auto cells = split(row, '\t');

        if (cells[0] == "packing") {
            REQUIRE(cells.size() == 3);
            times.push_back(decimal(cells[1]));
            expected.push_back(decimal(cells[2]));
        }
    }

    REQUIRE(times.size() == 9);
    const std::vector<std::size_t> counts(times.size(), 1);
    auto source = tape(times, counts);
    Clock clock;
    Cancellation cancel;
    Observer observer(clock);
    const auto result = r::replayTape(source, observer, clock, cancel, replayLimits(), r::ReplayPolicy::IMMEDIATE);
    REQUIRE(result.outcome == r::ReplayOutcome::COMPLETED);
    REQUIRE(observer.records.size() == expected.size());

    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(observer.records[i].timing.timestampMs == times[i]);
        CHECK(observer.records[i].timing.eventTimeSequence == expected[i]);
        CHECK(observer.records[i].record.typedValues == values(static_cast<std::int32_t>(i)));
    }

    CHECK(clock.samples == 0);
}

TEST_CASE("engine allocation faults fail closed while callback allocation exceptions retain their category") {
    const std::array<std::optional<std::int64_t>, 1> times{0};
    const std::array<std::size_t, 1> counts{2};
    auto source = tape(times, counts);
    std::size_t failures = 0;

    for (std::size_t allowed = 0; allowed < 12; ++allowed) {
        Clock clock;
        Cancellation cancel;
        Observer observer(clock);
        allocation_test::remaining = allowed;
        allocation_test::enabled = true;
        const auto result = r::replayTape(source, observer, clock, cancel, replayLimits(), r::ReplayPolicy::IMMEDIATE);
        allocation_test::enabled = false;
        CHECK((result.outcome == r::ReplayOutcome::ALLOCATION_FAILURE ||
               result.outcome == r::ReplayOutcome::INPUT_FAILURE));
        CHECK(result.counters.deliveredRecords == 0);
        CHECK(observer.finishes == 0);
        CHECK(observer.records.empty());

        if (result.outcome == r::ReplayOutcome::INPUT_FAILURE) {
            REQUIRE(result.reader);
            CHECK(result.reader->error == p::TapeError::ALLOCATION_FAILURE);
        }

        ++failures;
    }

    CHECK(failures == 12);
}

/** Run deterministic conformance with one explicit source root.
 * @param argc Argument count.
 * @param argv Arguments; argv[1] is the source root.
 * @return doctest status, or 2 for missing explicit fixtures.
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }

    root = std::filesystem::path(argv[1]);
    doctest::Context context;

    return context.run();
}
