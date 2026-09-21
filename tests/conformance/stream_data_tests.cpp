#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/stream_data.hpp"
#include <algorithm>
#include <charconv>
#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace {
using namespace qd::protocol;
std::string fixturePath; ///< Immutable oracle TSV path supplied by CTest.
constexpr StreamDataLimits LIMITS{4096, 128, 512, 128, 512, 1024, {4096, 128, 128}};    ///< Bounded corpus profile.
constexpr ContextLimits CONTEXT_LIMITS{{16, 64, 128, 128, 1024}, 32, {256, 128, 2048}}; ///< Snapshot corpus bounds.

/** Parse exact signed decimal or positive hex fixture numbers.
 * @param text Entire numeric cell.
 * @param base Radix, decimal by default.
 * @return Parsed value, or throw on malformed input.
 */
std::int64_t number(std::string_view text, int base = 10) {
    // Hex cells cannot use a sign: narrowing a parsed negative would hide malformed fixture data.
    auto digits = text;

    if (base == 10 && digits.starts_with('-')) {
        digits.remove_prefix(1);
    }

    if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [base](char c) {
            return (c >= '0' && c <= '9') || (base == 16 && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')));
        })) {
        throw std::runtime_error("Bad number syntax");
    }

    std::int64_t result{};
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), result, base);

    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw std::runtime_error("Bad number");
    }

    return result;
}

/** Split a TSV row into owned cells.
 * @param line Borrowed TSV row.
 * @return Ordered cells, preserving empty and trailing cells for column-count validation.
 */
std::vector<std::string> cells(const std::string &line) {
    std::vector<std::string> result;
    std::size_t begin = 0;

    for (;;) {
        const auto end = line.find('\t', begin);
        result.push_back(line.substr(begin, end == std::string::npos ? end : end - begin));

        if (end == std::string::npos) {
            return result;
        }

        begin = end + 1;
    }
}

/** Read exact byte hex, with '-' for empty.
 * @param hex Strict fixture cell.
 * @return Owned bytes; throws on invalid hex.
 */
std::vector<std::byte> bytes(std::string_view hex) {
    std::vector<std::byte> result;

    if (hex == "-") {
        return result;
    }

    if (hex.empty() || hex.size() % 2) {
        throw std::runtime_error("Bad byte hex");
    }

    for (std::size_t i = 0; i < hex.size(); i += 2) {
        result.push_back(static_cast<std::byte>(number(hex.substr(i, 2), 16)));
    }

    return result;
}

/** Read exact UTF-16 units without transcoding.
 * @param hex Strict unit cell, '-' for empty; null is not a successful symbol.
 * @return Owned UTF-16 units.
 */
std::u16string units(std::string_view hex) {
    std::u16string result;

    if (hex == "-") {
        return result;
    }

    if (hex.empty() || hex.size() % 4) {
        throw std::runtime_error("Bad unit hex");
    }

    for (std::size_t i = 0; i < hex.size(); i += 4) {
        result.push_back(static_cast<char16_t>(number(hex.substr(i, 4), 16)));
    }

    return result;
}

/** Construct the same explicit lookup subset as the production Java scheme.
 * @return Owned validated local schema; throw on a broken test setup.
 */
qd::core::LocalSchema schema() {
    std::vector<qd::core::LocalRecord> records{
        {0, u"S", false, {{8}}, {{u"v", 0}, {u"S.v", 0}}},
        {1,
         u"R",
         false,
         {{8}, {8}, {8}},
         {{u"y", 0}, {u"R.y", 0}, {u"x", 1}, {u"R.x", 1}, {u"xdot", 1}, {u"z", 2}, {u"R.z", 2}}},
        {2, u"E", false, {}, {}}};
    auto result = qd::core::LocalSchema::create(records, CONTEXT_LIMITS.schema);

    if (!result) {
        throw std::runtime_error("schema setup");
    }

    return std::move(*result);
}

/** Compare owned outputs, independent of Java callback grouping.
 * @param a Actual record prefix.
 * @param b Expected record prefix.
 */
void equalRecords(const std::vector<StreamDataRecord> &a, const std::vector<StreamDataRecord> &b) {
    REQUIRE(a.size() == b.size());

    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].localId == b[i].localId);
        CHECK(a[i].symbol.units == b[i].symbol.units);
        CHECK(a[i].symbol.eventFlags == b[i].symbol.eventFlags);
        CHECK(a[i].values == b[i].values);
    }
}

/** One fully validated Java case, with callback observations separate from decoder policy. */
struct Observation {
    std::string name;                             ///< Unique scenario identity.
    std::vector<std::byte> descriptions;          ///< Complete preceding type-2 payload.
    std::vector<std::vector<std::byte>> payloads; ///< Complete type-15 payloads.
    std::vector<std::byte> wire;                  ///< Whole framed stream.
    std::vector<StreamDataRecord> records;        ///< Production delivered records.
    std::size_t errors = 0;                       ///< Returning-consumer error callbacks.
    std::size_t callbacks = 0;                    ///< Actual data callback count.
    std::size_t callbackRecords = 0;              ///< Sum of callback record counts.
};

/** Read int32 values with strict separators and range checks.
 * @param cell Comma-separated signed decimal values or '-'.
 * @return Owned local values; malformed data throws.
 */
std::vector<std::int32_t> values(std::string cell) {
    if (cell == "-") {
        return {};
    }

    std::replace(cell.begin(), cell.end(), ',', '\t');
    std::vector<std::int32_t> out;

    for (const auto &part : cells(cell)) {
        auto n = number(part);

        if (n < std::numeric_limits<std::int32_t>::min() || n > std::numeric_limits<std::int32_t>::max()) {
            throw std::runtime_error("int32 range");
        }

        out.push_back(static_cast<std::int32_t>(n));
    }

    return out;
}

/** Parse the entire strict observation schema, including completion and required scenario coverage.
 * @return Fully validated observations; unknown rows/arity/trailing content throw.
 */
std::vector<Observation> observations() {
    std::ifstream input(fixturePath);

    if (!input) {
        throw std::runtime_error("missing fixture");
    }

    std::vector<Observation> result;
    std::set<std::string> names;
    bool profile = false, complete = false, active = false, desc = false, wire = false;
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (complete) {
            throw std::runtime_error("observations after complete prohibited");
        }

        auto c = cells(line);
        auto arity = [&](std::size_t n) {
            if (c.size() != n) {
                throw std::runtime_error("Wrong TSV column count");
            }
        };

        if (c[0] == "profile") {
            arity(7);

            if (profile || active ||
                c != cells("profile\tQD-3.355\tordinary-type-8\tmetadata=false\treplacers=null\tevent-time=0\tcomplete-"
                           "frames")) {
                throw std::runtime_error("profile");
            }

            profile = true;
        } else if (c[0] == "case") {
            arity(2);

            if (!profile || active || !names.insert(c[1]).second) {
                throw std::runtime_error("case order");
            }

            result.push_back({c[1]});
            active = true;
            desc = false;
            wire = false;
        } else if (c[0] == "complete") {
            arity(2);

            if (!profile || active || c[1] != "stream-data") {
                throw std::runtime_error("completion");
            }

            complete = true;
        } else {
            if (!active) {
                throw std::runtime_error("row outside case");
            }

            auto &o = result.back();

            if (c[0] == "descriptions") {
                arity(2);

                if (desc || wire) {
                    throw std::runtime_error("description order");
                }

                o.descriptions = bytes(c[1]);
                desc = true;
            } else if (c[0] == "payload") {
                arity(3);

                if (!desc || wire || number(c[1]) != static_cast<std::int64_t>(o.payloads.size())) {
                    throw std::runtime_error("payload order");
                }

                o.payloads.push_back(bytes(c[2]));
            } else if (c[0] == "wire") {
                arity(2);

                if (wire || o.payloads.empty()) {
                    throw std::runtime_error("wire order");
                }

                o.wire = bytes(c[1]);
                wire = true;
            } else if (c[0] == "record") {
                arity(6);
                auto id = number(c[2]);
                auto flags = number(c[5]);

                if (!wire || number(c[1]) != static_cast<std::int64_t>(o.records.size()) || id < 0 || id > 2 ||
                    flags < 0 || flags > 2) {
                    throw std::runtime_error("record schema");
                }

                auto v = values(c[4]);

                if (v.size() != (id == 0 ? 1 : id == 1 ? 3 : 0)) {
                    throw std::runtime_error("record values arity");
                }

                o.records.push_back({static_cast<std::int32_t>(id),
                                     {units(c[3]), 0, static_cast<std::int32_t>(flags), 0},
                                     std::move(v)});
            } else if (c[0] == "callback") {
                arity(4);
                auto n = number(c[3]);

                if (!wire || number(c[1]) != static_cast<std::int64_t>(o.callbacks) || c[2] != "15" || n <= 0 ||
                    n > 128) {
                    throw std::runtime_error("callback schema");
                }

                ++o.callbacks;
                o.callbackRecords += static_cast<std::size_t>(n);

                if (o.callbackRecords != o.records.size()) {
                    throw std::runtime_error("callback order");
                }
            } else if (c[0] == "error") {
                arity(3);

                if (!wire || c[1] != "message" || c[2] != "15") {
                    throw std::runtime_error("error schema");
                }

                ++o.errors;
            } else if (c[0] == "end") {
                arity(6);

                if (!wire || number(c[1]) != static_cast<std::int64_t>(o.wire.size()) || c[2] != "0" ||
                    number(c[3]) != static_cast<std::int64_t>(o.records.size()) ||
                    number(c[4]) != static_cast<std::int64_t>(o.callbacks) ||
                    number(c[5]) != static_cast<std::int64_t>(o.errors) || o.callbackRecords != o.records.size()) {
                    throw std::runtime_error("end schema");
                }

                active = false;
            } else {
                throw std::runtime_error("Unknown Java TSV row type");
            }
        }
    }

    if (!complete) {
        throw std::runtime_error("missing completion");
    }

    std::set<std::string> required{"mixed",        "empty",       "small",          "skip-prefix",  "late-failure",
                                   "undescribed",  "negative-id", "invalid-symbol", "truncated-id", "nonzero-flags",
                                   "remove-event", "zero-flags",  "reset-valid",    "reset-repeat", "string-forms",
                                   "boundaries",   "overlong"};

    for (int i = 0; i < 5; ++i) {
        required.insert("small/prefix-" + std::to_string(i));
    }

    for (int i = 0; i < 13; ++i) {
        required.insert("skip-prefix/prefix-" + std::to_string(i));
    }

    if (names != required) {
        throw std::runtime_error("required coverage");
    }

    return result;
}

/** Replay immutable frames with arbitrary chunks, using caller-provided static context only.
 * @param o Java stream and its payloads.
 * @param context Preconstructed static context, never changed by frame dispatch.
 * @param chunks Contiguous chunk sizes covering the wire.
 * @param expected Direct complete-payload outcomes.
 */
void replay(const Observation &o, const StreamDataContext &context, const std::vector<std::size_t> &chunks,
            const std::vector<StreamDataResult> &expected) {
    FrameReader reader({4096, 4105});
    std::size_t offset = 0, index = 0;

    for (auto n : chunks) {
        auto part = std::span(o.wire).subspan(offset, n);
        offset += n;

        while (!part.empty()) {
            auto fed = reader.feed(part);
            REQUIRE(fed.accepted > 0);
            part = part.subspan(fed.accepted);
            REQUIRE(fed.status != FrameStatus::FAILED);

            if (fed.frame) {
                CHECK(fed.frame->kind == FrameKind::MESSAGE);

                if (index == 0) {
                    CHECK(fed.frame->type == 2);
                    CHECK(fed.frame->payload == o.descriptions);
                } else {
                    REQUIRE(index <= o.payloads.size());
                    CHECK(fed.frame->type == 15);
                    CHECK(fed.frame->payload == o.payloads[index - 1]);
                    const auto decoded = decodeStreamData(fed.frame->payload, context, LIMITS);
                    const auto &direct = expected[index - 1];
                    CHECK(decoded.status == direct.status);
                    CHECK(decoded.stage == direct.stage);
                    CHECK(decoded.consumed == direct.consumed);
                    CHECK(decoded.errorOffset == direct.errorOffset);
                    equalRecords(decoded.records, direct.records);
                }

                ++index;
            }
        }
    }

    CHECK(offset == o.wire.size());
    CHECK(index == o.payloads.size() + 1);
    CHECK(reader.finish() == FrameStatus::FINISHED);
}
} // namespace

TEST_CASE("production Java delivery and complete-frame integration") {
    for (const auto &o : observations()) {
        INFO(o.name);
        auto parsed = parseDescribeRecords(o.descriptions, {4096, 32, 64, 256, {128, 128}, 2048});
        REQUIRE(parsed.status == DescriptionStatus::OK);
        auto context = StreamDataContext::create(schema(), parsed.descriptions, CONTEXT_LIMITS);
        REQUIRE(context);
        std::vector<StreamDataResult> outcomes;
        std::vector<StreamDataRecord> delivered;
        std::size_t errors = 0;
        const bool unsupported = o.name == "nonzero-flags" || o.name == "remove-event";

        for (const auto &p : o.payloads) {
            auto out = decodeStreamData(p, *context, LIMITS);
            CHECK(out.consumed <= p.size());
            CHECK(out.errorOffset <= out.consumed);

            if (unsupported) {
                CHECK(out.status == StreamDataStatus::UNSUPPORTED_PROFILE);
                CHECK(out.stage == StreamDataStage::FLAGS);
                CHECK(out.consumed == 3);
                CHECK(out.errorOffset == 3);
                CHECK(out.records.empty());
            } else if (out.status != StreamDataStatus::OK) {
                ++errors;
            } else {
                delivered.insert(delivered.end(), out.records.begin(), out.records.end());
            }

            if (o.name == "late-failure") {
                REQUIRE(out.records.size() == 1);
                CHECK(out.records[0].values == std::vector<std::int32_t>{3});
            }

            outcomes.push_back(std::move(out));
        }

        if (!unsupported) {
            CHECK(errors == o.errors);
            equalRecords(delivered, o.records);
        } else {
            REQUIRE(o.records.size() == 1);
            CHECK(o.errors == 0);
            CHECK(o.records[0].localId == 1);
            CHECK(o.records[0].symbol.units.empty());
            CHECK(o.records[0].symbol.eventFlags == (o.name == "remove-event" ? 2 : 1));
            CHECK(o.records[0].values == std::vector<std::int32_t>{0, o.name == "remove-event" ? 0 : 7, 0});
        }

        replay(o, *context, {o.wire.size()}, outcomes);

        for (std::size_t split = 0; split <= o.wire.size(); ++split) {
            replay(o, *context, {split, o.wire.size() - split}, outcomes);
        }

        replay(o, *context, std::vector<std::size_t>(o.wire.size(), 1), outcomes);
    }
}

TEST_CASE("snapshot validation ownership and sparse lookup") {
    auto local = schema();
    std::vector<WireRecordDescription> descriptions{{2147483647, u"R", {{u"xdot", 8}, {u"missing", 8}}}, {9, u"E", {}}};
    auto limits = CONTEXT_LIMITS;
    auto context = StreamDataContext::create(local, descriptions, limits);
    REQUIRE(context);
    descriptions[0].name = u"destroyed";
    descriptions[0].fields.clear();
    const auto *plan = context->find(2147483647);
    REQUIRE(plan);
    CHECK(plan->localFieldCount == 3);
    CHECK(plan->binding.localRecordId == 1);
    REQUIRE(plan->binding.fields.size() == 2);
    CHECK(plan->binding.fields[0].targetFieldId == 1);
    CHECK_FALSE(plan->binding.fields[1].targetFieldId);
    CHECK(context->find(0) == nullptr);
    CHECK(context->find(-1) == nullptr);
    descriptions = {{7, u"R", {{u"x", 8}}}, {7, u"E", {}}};
    CHECK(StreamDataContext::create(local, descriptions, limits).error() == BindingError::INVALID_DESCRIPTION);
    descriptions.pop_back();
    descriptions[0].senderId = -1;
    CHECK(StreamDataContext::create(local, descriptions, limits).error() == BindingError::INVALID_DESCRIPTION);
    descriptions[0].senderId = 7;
    descriptions[0].fields[0].serialTypeId = 9;
    CHECK(StreamDataContext::create(local, descriptions, limits).error() == BindingError::UNSUPPORTED_PROFILE);
    descriptions[0].fields[0].serialTypeId = 256;
    CHECK(StreamDataContext::create(local, descriptions, limits).error() == BindingError::INVALID_DESCRIPTION);
    descriptions[0].fields[0] = {u"", 8};
    CHECK(StreamDataContext::create(local, descriptions, limits).error() == BindingError::INVALID_DESCRIPTION);
    descriptions[0].fields[0] = {u"x", 8};
    descriptions[0].name.clear();
    CHECK(StreamDataContext::create(local, descriptions, limits).error() == BindingError::INVALID_DESCRIPTION);
    descriptions[0].name = u"R";
    limits.descriptions = 0;
    CHECK(StreamDataContext::create(local, descriptions, limits).error() == BindingError::RESOURCE_LIMIT);
    limits = CONTEXT_LIMITS;
    limits.bindings = {1, 1, 2};
    CHECK(StreamDataContext::create(local, descriptions, limits));

    for (int which = 0; which < 3; ++which) {
        auto small = limits;

        if (which == 0) {
            small.bindings.fields = 0;
        } else if (which == 1) {
            small.bindings.nameUnits = 0;
        } else {
            small.bindings.totalNameUnits = 1;
        }

        CHECK(StreamDataContext::create(local, descriptions, small).error() == BindingError::RESOURCE_LIMIT);
    }

    descriptions.push_back({8, u"R", {{u"x", 8}}});
    limits.bindings = {1, 8, 128};
    CHECK(StreamDataContext::create(local, descriptions, limits).error() == BindingError::RESOURCE_LIMIT);
    limits.bindings = {2, 8, 3};
    CHECK(StreamDataContext::create(local, descriptions, limits).error() == BindingError::RESOURCE_LIMIT);
    limits.bindings.totalNameUnits = 4;
    CHECK(StreamDataContext::create(local, descriptions, limits));
    // Derive exact logical schema counts, then reduce each budget independently.
    auto schemaLimits = CONTEXT_LIMITS;
    std::size_t fields = 0, lookups = 0, names = 0, longest = 0;

    for (const auto &r : local.records()) {
        fields += r.fields.size();
        lookups += r.lookups.size();
        names += r.name.size();
        longest = std::max(longest, r.name.size());

        for (const auto &key : r.lookups) {
            names += key.name.size();
            longest = std::max(longest, key.name.size());
        }
    }

    schemaLimits.schema = {3, fields, lookups, longest, names};
    CHECK(StreamDataContext::create(local, descriptions, schemaLimits));

    for (int which = 0; which < 5; ++which) {
        auto small = schemaLimits;

        switch (which) {
        case 0:
            --small.schema.records;
            break;
        case 1:
            --small.schema.fields;
            break;
        case 2:
            --small.schema.lookups;
            break;
        case 3:
            --small.schema.nameUnits;
            break;
        default:
            --small.schema.totalNameUnits;
            break;
        }

        CHECK(StreamDataContext::create(local, descriptions, small).error() == BindingError::RESOURCE_LIMIT);
    }

    auto invalid = qd::core::LocalSchema::create(
        std::vector<qd::core::LocalRecord>{{0, u"R", false, {{8}}, {{u"x", 1}}}}, CONTEXT_LIMITS.schema);
    CHECK_FALSE(invalid);
    auto empty = StreamDataContext::create(local, {}, CONTEXT_LIMITS);
    REQUIRE(empty);
    CHECK(decodeStreamData({}, *empty, LIMITS).status == StreamDataStatus::OK);
    CHECK(decodeStreamData(bytes("fe00"), *empty, LIMITS).status == StreamDataStatus::UNDESCRIBED_ID);
}

TEST_CASE("exact budgets positions partial failure and message lifetime") {
    std::vector<WireRecordDescription> descriptions{
        {7, u"R", {{u"x", 8}, {u"skip", 8}}}, {9, u"E", {}}, {31, u"Unknown", {{u"a", 8}}}};
    auto context = StreamDataContext::create(schema(), descriptions, CONTEXT_LIMITS);
    REQUIRE(context);
    auto input = bytes("0443070102ff09ff1f01ff070304");
    StreamDataLimits exact{input.size(), 4, 5, 3, 6, 9, {input.size(), 3, 3}};
    auto result = decodeStreamData(input, *context, exact);
    REQUIRE(result.status == StreamDataStatus::OK);
    REQUIRE(result.records.size() == 3);
    CHECK(result.records[0].values == std::vector<std::int32_t>{0, 1, 0});
    CHECK(result.records[1].values.empty());
    CHECK(result.records[2].values == std::vector<std::int32_t>{0, 3, 0});

    for (int which = 0; which < 9; ++which) {
        auto small = exact;

        switch (which) {
        case 0:
            --small.payloadBytes;
            break;
        case 1:
            --small.processedRecords;
            break;
        case 2:
            --small.wireFields;
            break;
        case 3:
            --small.emittedRecords;
            break;
        case 4:
            --small.emittedValues;
            break;
        case 5:
            --small.ownedSymbolUnits;
            break;
        case 6:
            --small.symbol.inputBytes;
            break;
        case 7:
            --small.symbol.codeUnits;
            break;
        default:
            --small.symbol.stateUnits;
            break;
        }

        auto failed = decodeStreamData(input, *context, small);
        CHECK(failed.status == StreamDataStatus::RESOURCE_LIMIT);
        CHECK(failed.consumed <= input.size());
        CHECK(failed.errorOffset <= failed.consumed);
    }

    auto zero = StreamDataLimits{0, 0, 0, 0, 0, 0, {0, 0, 0}};
    CHECK(decodeStreamData({}, *context, zero).status == StreamDataStatus::OK);

    for (auto text : {"fe09ff09", "fe1f01ff1f02"}) {
        auto limited = LIMITS;
        limited.processedRecords = 1;
        auto failed = decodeStreamData(bytes(text), *context, limited);
        CHECK(failed.status == StreamDataStatus::RESOURCE_LIMIT);
        CHECK(failed.stage == StreamDataStage::RECORD);
    }

    // Primitive starts, validation checkpoints and successful record prefixes are explicit policy.
    /** Explicit C++ policy projection for one malformed complete payload. */
    struct Failure {
        const char *hex;         ///< Exact complete payload bytes.
        StreamDataStatus status; ///< Expected first-error classification.
        StreamDataStage stage;   ///< Expected active checkpoint.
        std::size_t consumed;    ///< Expected committed payload position.
        std::size_t offset;      ///< Expected primitive start or validation position.
        std::size_t records;     ///< Expected retained complete record count.
    };

    for (const auto &f :
         std::vector<Failure>{{"c0", StreamDataStatus::INVALID_SYMBOL, StreamDataStage::SYMBOL, 1, 0, 0},
                              {"fe80", StreamDataStatus::TRUNCATED_PAYLOAD, StreamDataStage::RECORD_ID, 2, 1, 0},
                              {"fe7f", StreamDataStatus::INVALID_ID, StreamDataStage::RECORD_ID, 2, 2, 0},
                              {"fe08", StreamDataStatus::UNDESCRIBED_ID, StreamDataStage::RECORD_ID, 2, 2, 0},
                              {"fe0780", StreamDataStatus::TRUNCATED_PAYLOAD, StreamDataStage::FIELD, 3, 2, 0},
                              {"fe070180", StreamDataStatus::TRUNCATED_PAYLOAD, StreamDataStage::FIELD, 4, 3, 0},
                              {"fe1f80", StreamDataStatus::TRUNCATED_PAYLOAD, StreamDataStage::FIELD, 3, 2, 0},
                              {"fe09ff0780", StreamDataStatus::TRUNCATED_PAYLOAD, StreamDataStage::FIELD, 5, 4, 1},
                              {"f901fe", StreamDataStatus::UNSUPPORTED_PROFILE, StreamDataStage::FLAGS, 3, 3, 0},
                              {"ff09", StreamDataStatus::INVALID_SYMBOL, StreamDataStage::SYMBOL, 1, 0, 0},
                              {"fc7f", StreamDataStatus::INVALID_SYMBOL, StreamDataStage::SYMBOL, 2, 0, 0}}) {
        auto failed = decodeStreamData(bytes(f.hex), *context, LIMITS);
        CHECK(failed.status == f.status);
        CHECK(failed.stage == f.stage);
        CHECK(failed.consumed == f.consumed);
        CHECK(failed.errorOffset == f.offset);
        CHECK(failed.records.size() == f.records);
    }

    auto tiny = LIMITS;
    tiny.emittedRecords = 0;
    auto rejected = decodeStreamData(bytes("fe0780"), *context, tiny);
    CHECK(rejected.status == StreamDataStatus::RESOURCE_LIMIT);
    CHECK(rejected.stage == StreamDataStage::OUTPUT);
    CHECK(rejected.consumed == 2);
    tiny = LIMITS;
    tiny.wireFields = 0;
    rejected = decodeStreamData(bytes("fe0780"), *context, tiny);
    CHECK(rejected.status == StreamDataStatus::RESOURCE_LIMIT);
    CHECK(rejected.stage == StreamDataStage::FIELD);
    CHECK(rejected.consumed == 2);
    auto owned = [] {
        auto local = schema();
        std::vector<WireRecordDescription> d{{7, u"R", {{u"x", 8}}}};
        auto c = StreamDataContext::create(local, d, CONTEXT_LIMITS);

        if (!c) {
            throw std::runtime_error("context");
        }

        auto p = bytes("fd034100420701");
        auto out = decodeStreamData(p, *c, LIMITS);
        p.clear();
        d.clear();

        return out;
    }();
    REQUIRE(owned.status == StreamDataStatus::OK);
    CHECK(owned.records[0].symbol.units == std::u16string{u'A', u'\0', u'B'});
    CHECK(decodeStreamData(bytes("ff09"), *context, LIMITS).status == StreamDataStatus::INVALID_SYMBOL);
    CHECK(decodeStreamData(bytes("fe09"), *context, LIMITS).status == StreamDataStatus::OK);
    input.clear();
    context = StreamDataContext::create(schema(), {}, CONTEXT_LIMITS);
    CHECK(result.records[0].symbol.units == u"ABC");
    CHECK(result.records[2].values[1] == 3);
}

TEST_CASE("one failed complete payload cannot borrow the next frame") {
    const std::vector<WireRecordDescription> descriptions{{9, u"E", {}}};
    auto context = StreamDataContext::create(schema(), descriptions, CONTEXT_LIMITS);
    REQUIRE(context);
    const auto wire = bytes("020f04030ffe09");
    FrameReader reader({32, 41});
    auto first = reader.feed(wire);
    REQUIRE(first.frame);
    CHECK(first.accepted == 3);
    auto bad = decodeStreamData(first.frame->payload, *context, LIMITS);
    CHECK(bad.status == StreamDataStatus::TRUNCATED_PAYLOAD);
    CHECK(bad.consumed == 1);
    auto second = reader.feed(std::span(wire).subspan(first.accepted));
    REQUIRE(second.frame);
    auto good = decodeStreamData(second.frame->payload, *context, LIMITS);
    CHECK(good.status == StreamDataStatus::OK);
    REQUIRE(good.records.size() == 1);
    CHECK(good.records[0].localId == 2);
    CHECK(reader.finish() == FrameStatus::FINISHED);
}

/** Run the strict oracle suite with an explicit immutable input.
 * @param argc Argument count.
 * @param argv Fixture path and optional doctest arguments.
 * @return Nonzero on missing inputs or any failed assertion.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    fixturePath = argv[1];
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
