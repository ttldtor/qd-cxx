// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/protocol/tape_reader.hpp"
#include <algorithm>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <sstream>
#include <string>

namespace {
namespace p = org::ttldtor::qdcxx::protocol;
namespace c = org::ttldtor::qdcxx::core;
std::filesystem::path fixtures; ///< Explicit repository fixture root from CTest.

/** Construct a bounded diagnostic scheme.
 * @return Validated owned S/R/E schema.
 */
c::LocalSchema schema() {
    auto s = c::LocalSchema::create(
        std::vector<c::LocalRecord>{{0, u"S", false, {{8}}, {{u"v", 0}}},
                                    {1, u"R", false, {{8}, {8}, {8}}, {{u"y", 0}, {u"x", 1}, {u"z", 2}}},
                                    {2, u"E", false, {}, {}}},
        {10, 20, 20, 20, 200});
    REQUIRE(s);

    return std::move(*s);
}

/** Supply deliberately small finite test budgets.
 * @return Required reader configuration.
 */
p::TapeLimits limits() {
    return {{4096, 4105},
            65536,
            128,
            {16, 8, {1024, 256}, 2048},
            {{10, 20, 20, 20, 200}, 20, {100, 100, 1000}},
            10000,
            {4096, 100, 200, 100, 200, 1000, {4096, 100, 100}}};
}

/** Read one immutable test input.
 * @param name Case filename stem.
 * @return Owned bytes, including intentional empty input.
 */
std::vector<std::byte> input(std::string_view name) {
    std::ifstream in(fixtures / (std::string(name) + ".qtp"), std::ios::binary);
    REQUIRE(in.good());
    std::vector<char> chars((std::istreambuf_iterator<char>(in)), {});
    auto span = std::as_bytes(std::span(chars));

    return {span.begin(), span.end()};
}

/** Captured owning reader result across any schedule. */
struct Observation {
    p::TapeStatus status;                     ///< EOF/terminal state.
    p::TapeDiagnostic diagnostic;             ///< First error positions.
    std::vector<p::StreamDataRecord> records; ///< Complete owned prefix.
    std::size_t construction;                 ///< Full rebuild charges.
    std::size_t fields;                       ///< Charged field work.
    std::size_t processed;                    ///< Charged record work.
    std::size_t markers;                      ///< Empty envelope count.
};

/** Feed arbitrary bounded chunks through the public API.
 * @param bytes Owned caller input, borrowed during feeds.
 * @param budget Required limits.
 * @param chunk Maximum schedule chunk, positive.
 * @param first Optional first-chunk split, zero means ordinary schedule.
 * @return Independent completed results after reader destruction.
 */
Observation run(const std::vector<std::byte> &bytes, p::TapeLimits budget, std::size_t chunk = 65536,
                std::size_t first = 0) {
    auto local = schema();
    p::TapeReader reader(local, budget);
    Observation out{};
    std::size_t offset = 0;

    while (offset < bytes.size()) {
        auto size = std::min((offset == 0 && first > 0) ? first : chunk, bytes.size() - offset);
        auto r = reader.feed(std::span(bytes).subspan(offset, size));
        offset += r.accepted;

        for (auto &record : r.records) {
            out.records.push_back(std::move(record));
        }

        if (r.status == p::TapeStatus::FAILED) {
            break;
        }

        REQUIRE(r.accepted > 0);
        CHECK(reader.feed({}).status == p::TapeStatus::NEED_INPUT);
    }

    out.status = reader.finish();
    out.diagnostic = reader.diagnostic();
    out.construction = reader.constructionUnits();
    out.fields = reader.wireFields();
    out.processed = reader.processedRecords();
    out.markers = reader.markerCount();
    CHECK(reader.finish() == out.status);
    CHECK(reader.feed(bytes).accepted == 0);

    return out;
}

/** Compare independent exact local observations.
 * @param left First owned list.
 * @param right Second owned list.
 */
void same(const std::vector<p::StreamDataRecord> &left, const std::vector<p::StreamDataRecord> &right) {
    REQUIRE(left.size() == right.size());

    for (std::size_t i = 0; i < left.size(); ++i) {
        CHECK(left[i].localId == right[i].localId);
        CHECK(left[i].symbol.units == right[i].symbol.units);
        CHECK(left[i].values == right[i].values);
    }
}
} // namespace

TEST_CASE("original unchanged tape and exact local ordered records across schedules") {
    auto bytes = input("original");
    auto baseline = run(bytes, limits());
    REQUIRE(baseline.status == p::TapeStatus::FINISHED);
    REQUIRE(baseline.records.size() == 7);
    const std::vector<std::int32_t> ids{0, 0, 1, 1, 2, 2, 0};
    const std::vector<std::u16string> symbols{u"ABC", u"ABC", u"ABC", u"\u2603", u"\u2603", u"", u"ABC"};
    const std::vector<std::vector<std::int32_t>> values{{7}, {-8}, {22, 11, 33}, {2147483647, (-2147483647 - 1), 0},
                                                        {},  {},   {42}};

    for (std::size_t i = 0; i < ids.size(); ++i) {
        CHECK(baseline.records[i].localId == ids[i]);
        CHECK(baseline.records[i].symbol.units == symbols[i]);
        CHECK(baseline.records[i].values == values[i]);
    }

    CHECK(baseline.fields == 9);
    CHECK(baseline.processed == 7);
    CHECK(baseline.markers == 1);

    for (std::size_t split = 1; split < bytes.size(); ++split) {
        auto r = run(bytes, limits(), 65536, split);
        CHECK(r.status == baseline.status);
        same(r.records, baseline.records);
    }

    for (auto chunk : {1U, 2U, 3U, 7U, 17U}) {
        auto r = run(bytes, limits(), chunk);
        CHECK(r.status == baseline.status);
        same(r.records, baseline.records);
    }
}

TEST_CASE("all Java stimuli have explicit separate C++ policy and stable failure positions") {
    const std::map<std::string, p::TapeError> special{{"original", p::TapeError::NONE},
                                                      {"header-only", p::TapeError::NONE},
                                                      {"no-footer", p::TapeError::NONE},
                                                      {"unknown-record", p::TapeError::NONE},
                                                      {"footer-then-data", p::TapeError::NONE},
                                                      {"send-empty", p::TapeError::NONE},
                                                      {"properties-2", p::TapeError::NONE},
                                                      {"properties-4", p::TapeError::NONE},
                                                      {"properties-11", p::TapeError::NONE},
                                                      {"missing-header", p::TapeError::UNSUPPORTED},
                                                      {"repeated-header", p::TapeError::UNSUPPORTED},
                                                      {"explicit-zero", p::TapeError::UNSUPPORTED},
                                                      {"extension", p::TapeError::UNSUPPORTED},
                                                      {"send-raw", p::TapeError::UNSUPPORTED},
                                                      {"receive-stream", p::TapeError::UNSUPPORTED},
                                                      {"properties-0", p::TapeError::UNSUPPORTED},
                                                      {"properties-1", p::TapeError::UNSUPPORTED},
                                                      {"properties-3", p::TapeError::UNSUPPORTED},
                                                      {"properties-5", p::TapeError::UNSUPPORTED},
                                                      {"properties-6", p::TapeError::UNSUPPORTED},
                                                      {"properties-7", p::TapeError::UNSUPPORTED},
                                                      {"properties-12", p::TapeError::UNSUPPORTED},
                                                      {"properties-8", p::TapeError::UNSUPPORTED}};
    std::size_t count = 0;

    for (const auto &entry : std::filesystem::directory_iterator(fixtures)) {
        if (entry.path().extension() != ".qtp") {
            continue;
        }

        ++count;
        auto name = entry.path().stem().string();
        INFO(name);
        auto expected = special.contains(name) ? special.at(name) : p::TapeError::MALFORMED;
        auto bytes = input(name);
        auto baseline = run(bytes, limits());
        CHECK(baseline.diagnostic.error == expected);
        CHECK(baseline.status == (expected == p::TapeError::NONE ? p::TapeStatus::FINISHED : p::TapeStatus::FAILED));

        for (auto chunk : {1U, 5U, 13U}) {
            auto r = run(bytes, limits(), chunk);
            CHECK(r.status == baseline.status);
            CHECK(r.diagnostic.error == baseline.diagnostic.error);
            CHECK(r.diagnostic.accepted == baseline.diagnostic.accepted);
            CHECK(r.diagnostic.frameStart == baseline.diagnostic.frameStart);
            CHECK(r.diagnostic.payloadStart == baseline.diagnostic.payloadStart);
            CHECK(r.diagnostic.consumed == baseline.diagnostic.consumed);
            CHECK(r.diagnostic.errorOffset == baseline.diagnostic.errorOffset);
            same(r.records, baseline.records);
        }
    }

    CHECK(count == 51);
}

TEST_CASE("zero exact excess aggregate budgets and checked constructor arithmetic") {
    auto bytes = input("original");
    auto baseline = run(bytes, limits());
    using Setter = std::function<void(p::TapeLimits &, std::size_t)>;
    const std::vector<std::pair<Setter, std::size_t>> budgets{{[](auto &l, auto n) {
                                                                   l.inputBytes = n;
                                                               },
                                                               137},
                                                              {[](auto &l, auto n) {
                                                                   l.frames = n;
                                                               },
                                                               8},
                                                              {[](auto &l, auto n) {
                                                                   l.frame = {n, n + 9};
                                                               },
                                                               57},
                                                              {[](auto &l, auto n) {
                                                                   l.context.descriptions = n;
                                                               },
                                                               3},
                                                              {[](auto &l, auto n) {
                                                                   l.context.bindings.fields = n;
                                                               },
                                                               4},
                                                              {[](auto &l, auto n) {
                                                                   l.context.bindings.nameUnits = n;
                                                               },
                                                               1},
                                                              {[](auto &l, auto n) {
                                                                   l.context.bindings.totalNameUnits = n;
                                                               },
                                                               7},
                                                              {[](auto &l, auto n) {
                                                                   l.constructionUnits = n;
                                                               },
                                                               baseline.construction},
                                                              {[](auto &l, auto n) {
                                                                   l.data.processedRecords = n;
                                                               },
                                                               7},
                                                              {[](auto &l, auto n) {
                                                                   l.data.wireFields = n;
                                                               },
                                                               9},
                                                              {[](auto &l, auto n) {
                                                                   l.data.emittedRecords = n;
                                                               },
                                                               7},
                                                              {[](auto &l, auto n) {
                                                                   l.data.emittedValues = n;
                                                               },
                                                               9},
                                                              {[](auto &l, auto n) {
                                                                   l.data.ownedSymbolUnits = n;
                                                               },
                                                               14},
                                                              {[](auto &l, auto n) {
                                                                   l.descriptor.text.bodyBytes = n;
                                                               },
                                                               11},
                                                              {[](auto &l, auto n) {
                                                                   l.descriptor.text.codeUnits = n;
                                                               },
                                                               11},
                                                              {[](auto &l, auto n) {
                                                                   l.descriptor.textUnits = n;
                                                               },
                                                               40},
                                                              {[](auto &l, auto n) {
                                                                   l.data.payloadBytes = n;
                                                               },
                                                               23},
                                                              {[](auto &l, auto n) {
                                                                   l.data.symbol.inputBytes = n;
                                                               },
                                                               23},
                                                              {[](auto &l, auto n) {
                                                                   l.descriptor.properties = n;
                                                               },
                                                               3},
                                                              {[](auto &l, auto n) {
                                                                   l.descriptor.messages = n;
                                                               },
                                                               1},
                                                              {[](auto &l, auto n) {
                                                                   l.data.symbol.codeUnits = n;
                                                               },
                                                               3},
                                                              {[](auto &l, auto n) {
                                                                   l.data.symbol.stateUnits = n;
                                                               },
                                                               3}};

    for (std::size_t i = 0; i < budgets.size(); ++i) {
        INFO(i);
        auto [set, exact] = budgets[i];
        auto l = limits();
        set(l, exact);
        CHECK(run(bytes, l).status == p::TapeStatus::FINISHED);
        l = limits();
        set(l, exact - 1);
        CHECK(run(bytes, l).diagnostic.error == p::TapeError::RESOURCE_LIMIT);
        l = limits();
        set(l, 0);
        CHECK(run(bytes, l).diagnostic.error == p::TapeError::RESOURCE_LIMIT);
    }

    auto l = limits();
    l.frame = {std::numeric_limits<std::size_t>::max(), 0};
    auto s = schema();
    CHECK_THROWS_AS(p::TapeReader(s, l), std::invalid_argument);
}

TEST_CASE("failed description update publishes nothing and no subsequent frames are processed") {
    auto local = schema();
    p::TapeReader reader(local, limits());
    auto bytes = input("original");
    auto header = reader.feed(bytes);
    REQUIRE(header.accepted == 58);
    auto first = reader.feed(std::span(bytes).subspan(58));
    REQUIRE(first.accepted == 9);
    auto old = reader.context();
    REQUIRE(old);
    REQUIRE(old->find(1));
    CHECK(old->find(0) == nullptr);
    auto broken = input("partial-update");
    // First probe envelope is 52 bytes; take only its complete malformed description message.
    p::FrameReader split({4096, 4105});
    auto h = split.feed(broken);
    REQUIRE(h.frame);
    auto r = reader.feed(std::span(broken).subspan(h.accepted));
    CHECK(r.status == p::TapeStatus::FAILED);
    CHECK(reader.context() == old);
    CHECK(old->find(1));
    CHECK(old->find(2) == nullptr);
    CHECK(reader.feed(bytes).accepted == 0);
}

TEST_CASE("accepted EOF does not authenticate markers or ignore trailing data") {
    CHECK(run(input("header-only"), limits()).status == p::TapeStatus::FINISHED);
    auto noFooter = run(input("no-footer"), limits());
    CHECK(noFooter.status == p::TapeStatus::FINISHED);
    CHECK(noFooter.markers == 0);
    auto trailing = run(input("footer-then-data"), limits());
    CHECK(trailing.status == p::TapeStatus::FINISHED);
    CHECK(trailing.records.size() == 9);
    auto broken = input("original");
    broken.push_back(std::byte{5});
    auto error = run(broken, limits());
    CHECK(error.status == p::TapeStatus::FAILED);
    CHECK(error.records.size() == 7);
}

TEST_CASE("noncanonical envelope boundaries and exact independent diagnostic positions") {
    auto original = input("original");
    std::vector<std::byte> overlong{std::byte{0x80}, std::byte{0x3a}, std::byte{0x80}, std::byte{1}};
    overlong.insert(overlong.end(), original.begin() + 2, original.end());
    auto local = schema();
    p::TapeReader reader(local, limits());
    auto h = reader.feed(overlong);
    REQUIRE(h.frame);
    CHECK(h.frame->startOffset == 0);
    CHECK(h.frame->endOffset == 60);
    CHECK(h.frame->payload.size() == 56);
    CHECK(*reader.diagnostic().payloadStart == 4);
    auto r = run(overlong, limits(), 1);
    CHECK(r.status == p::TapeStatus::FINISHED);
    same(r.records, run(original, limits()).records);
    auto empty = run(input("malformed-header-0"), limits());
    CHECK(empty.diagnostic.accepted == 2);
    CHECK(empty.diagnostic.payloadStart == 2);
    CHECK(empty.diagnostic.consumed == 0);
    CHECK(empty.diagnostic.errorOffset == 0);
    auto count = run(input("negative-properties"), limits());
    CHECK(count.diagnostic.accepted == 7);
    CHECK(count.diagnostic.payloadStart == 2);
    CHECK(count.diagnostic.consumed == 5);
    CHECK(count.diagnostic.errorOffset == 5);
    auto truncated = run(input("truncated-135"), limits());
    CHECK(truncated.diagnostic.frameStart == 122);
    CHECK(truncated.diagnostic.accepted == 135);
    CHECK_FALSE(truncated.diagnostic.payloadStart);
    CHECK(truncated.records.size() == 4);
}

TEST_CASE("message-local reset, completed payload prefix, skip charges and atomic late ID rejection") {
    auto bytes = input("original");
    // A complete data envelope has one valid empty-symbol S record then a truncated second field.
    bytes.resize(67);
    const std::vector<std::byte> data{std::byte{6}, std::byte{15},   std::byte{0xfe}, std::byte{1},
                                      std::byte{9}, std::byte{0xff}, std::byte{1}};
    bytes.insert(bytes.end(), data.begin(), data.end());
    auto partial = run(bytes, limits());
    REQUIRE(partial.records.size() == 1);
    CHECK(partial.records[0].values == std::vector<std::int32_t>{9});
    CHECK(partial.diagnostic.error == p::TapeError::MALFORMED);
    CHECK(partial.processed == 2);
    CHECK(partial.fields == 2);
    bytes = input("original");
    bytes.insert(bytes.end(), {std::byte{4}, std::byte{15}, std::byte{0xff}, std::byte{1}, std::byte{9}});
    auto reset = run(bytes, limits());
    CHECK(reset.records.size() == 7);
    CHECK(reset.diagnostic.error == p::TapeError::MALFORMED);
    auto skipped = run(input("unknown-record"), limits());
    CHECK(skipped.records.empty());
    CHECK(skipped.processed == 2);
    CHECK(skipped.fields == 2);
    auto budget = limits();
    budget.data.processedRecords = 1;
    CHECK(run(input("unknown-record"), budget).diagnostic.error == p::TapeError::RESOURCE_LIMIT);
    auto local = schema();
    p::TapeReader reader(local, limits());
    auto source = input("original");
    REQUIRE(reader.feed(source).accepted == 58);
    REQUIRE(reader.feed(std::span(source).subspan(58)).accepted == 9);
    auto old = reader.context();
    // New ID 2 precedes repeated ID 1 in one structurally complete update: nothing may commit.
    const std::vector<std::byte> duplicate{std::byte{9}, std::byte{2}, std::byte{2}, std::byte{1},   std::byte{'E'},
                                           std::byte{0}, std::byte{1}, std::byte{1}, std::byte{'E'}, std::byte{0}};
    auto update = reader.feed(duplicate);
    CHECK(update.status == p::TapeStatus::FAILED);
    CHECK(reader.context() == old);
    CHECK(old->find(2) == nullptr);
    CHECK(old->find(1) != nullptr);
}

/** Run doctest with explicit fixture ownership.
 * @param argc Process argument count.
 * @param argv Fixture directory followed by doctest options.
 * @return Nonzero on missing input or failed assertions.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    fixtures = argv[1];
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
