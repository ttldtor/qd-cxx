#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "qd/protocol/static_binding.hpp"

#include <array>
#include <charconv>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
using org::ttldtor::qdcxx::core::FieldLookup;
using org::ttldtor::qdcxx::core::LocalRecord;
using org::ttldtor::qdcxx::core::LocalSchema;
using org::ttldtor::qdcxx::core::SchemaError;
using org::ttldtor::qdcxx::protocol::BindingError;
using org::ttldtor::qdcxx::protocol::bindRecord;
using org::ttldtor::qdcxx::protocol::WireRecordDescription;

std::string fixturePath; ///< Explicit fixture location supplied by the test runner.
constexpr org::ttldtor::qdcxx::core::SchemaLimits SCHEMA_LIMITS{16, 64, 128, 128, 1024}; ///< Small test schema budgets.
constexpr org::ttldtor::qdcxx::protocol::BindingLimits BINDING_LIMITS{32, 128, 1024}; ///< Small test planner budgets.

/** Supply the explicitly resolved local subset used by the Java probe.
 * @return Owned S/R/E records with explicit full/local/property alias keys.
 */
std::vector<LocalRecord> localRecords() {
    return {{0, u"S", false, {{8}}, {{u"v", 0}, {u"S.v", 0}}},
            {1,
             u"R",
             false,
             {{8}, {8}, {8}},
             {{u"y", 0}, {u"R.y", 0}, {u"x", 1}, {u"R.x", 1}, {u"xdot", 1}, {u"z", 2}, {u"R.z", 2}}},
            {2, u"E", false, {}, {}}};
}

/** Split fixture cells while retaining empty final cells.
 * @param text Borrowed input.
 * @param delimiter Single separator.
 * @return Ordered owned cells, including empty cells.
 */
std::vector<std::string> split(std::string_view text, char delimiter) {
    std::vector<std::string> result;
    std::size_t begin = 0;

    for (;;) {
        const auto end = text.find(delimiter, begin);
        result.emplace_back(text.substr(begin, end == std::string_view::npos ? end : end - begin));

        if (end == std::string_view::npos) {
            return result;
        }

        begin = end + 1;
    }
}

/** Strict decimal or hexadecimal unsigned fixture number.
 * @param text Nonempty digits without signs or trailing text.
 * @param base Numeric base, 10 or 16.
 * @return Parsed count; throws on malformed or overflowing input.
 */
std::size_t number(std::string_view text, int base = 10) {
    std::size_t result{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result, base);

    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw std::runtime_error("Invalid fixture number");
    }

    return result;
}

/** Decode strict fixture hex into raw description bytes.
 * @param text Nonempty even-sized hexadecimal payload.
 * @return Owned bytes; throws on malformed input.
 */
std::vector<std::byte> bytes(std::string_view text) {
    if (text.empty() || text.size() % 2 != 0) {
        throw std::runtime_error("Invalid fixture hex");
    }

    std::vector<std::byte> result;

    for (std::size_t i = 0; i < text.size(); i += 2) {
        result.push_back(static_cast<std::byte>(number(text.substr(i, 2), 16)));
    }

    return result;
}

TEST_CASE("production Java description projections preserve ordered binding decisions") {
    const std::array<std::string_view, 12> names{"reorder",      "alias",  "full",        "middle-skip",
                                                 "missing-wire", "zero",   "empty-local", "empty-both",
                                                 "duplicate",    "sparse", "alignment",   "case-sensitive"};
    auto schema = LocalSchema::create(localRecords(), SCHEMA_LIMITS);
    REQUIRE(schema.has_value());
    std::ifstream input(fixturePath);
    REQUIRE(input.is_open());
    std::string line;
    std::size_t caseIndex = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.starts_with('#')) {
            continue;
        }

        REQUIRE(caseIndex < names.size());
        const auto cells = split(line, '\t');
        REQUIRE(cells.size() == 3);
        REQUIRE(cells[0] == names[caseIndex++]);
        const auto payload = bytes(cells[1]);
        const auto raw =
            org::ttldtor::qdcxx::protocol::parseDescribeRecords(payload, {4096, 16, 32, 128, {128, 128}, 1024});
        REQUIRE(raw.status == org::ttldtor::qdcxx::protocol::DescriptionStatus::OK);
        const auto expectedRecords = split(cells[2], ';');
        REQUIRE(raw.descriptions.size() == expectedRecords.size());

        for (std::size_t i = 0; i < expectedRecords.size(); ++i) {
            const auto expected = split(expectedRecords[i], '/');
            REQUIRE(expected.size() == 3);
            const auto plan = bindRecord(raw.descriptions[i], *schema, BINDING_LIMITS);
            REQUIRE(plan.has_value());
            CHECK(plan->senderId == number(expected[0]));
            CHECK(plan->localRecordId.has_value() == (expected[1] != "-"));

            if (expected[1] != "-") {
                REQUIRE(plan->localRecordId.has_value());
                CHECK(*plan->localRecordId == number(expected[1]));
            }

            const auto targets = expected[2].empty() ? std::vector<std::string>{} : split(expected[2], ',');
            REQUIRE(plan->fields.size() == targets.size());

            for (std::size_t j = 0; j < targets.size(); ++j) {
                CHECK(plan->fields[j].targetFieldId.has_value() == (targets[j] != "-"));

                if (targets[j] != "-") {
                    REQUIRE(plan->fields[j].targetFieldId.has_value());
                    CHECK(*plan->fields[j].targetFieldId == number(targets[j]));
                }
            }
        }
    }

    CHECK(caseIndex == names.size());
}

TEST_CASE("local schema rejects unsupported and ambiguous input before ownership") {
    auto records = localRecords();
    CHECK(LocalSchema::create({}, {0, 0, 0, 0, 0}).has_value());

    for (std::size_t mutation = 0; mutation < 9; ++mutation) {
        auto changed = records;

        switch (mutation) {
        case 0:
            changed[1].id = 0;
            break;
        case 1:
            changed[1].name = u"S";
            break;
        case 2:
            changed[1].name.clear();
            break;
        case 3:
            changed[1].lookups[0].fieldId = 99;
            break;
        case 4:
            changed[1].lookups.push_back(changed[1].lookups[0]);
            break;
        case 5:
            changed[1].lookups[1].name = changed[1].lookups[2].name;
            break;
        case 6:
            changed[1].lookups.clear();
            break;
        case 7:
            changed[1].lookups[0].name.clear();
            break;
        case 8:
            changed[1].name = std::u16string(1, static_cast<char16_t>(0xd800));
            break;
        }

        const auto result = LocalSchema::create(changed, SCHEMA_LIMITS);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == SchemaError::INVALID_SCHEMA);
    }

    records[1].hasTime = true;
    CHECK(LocalSchema::create(records, SCHEMA_LIMITS).error() == SchemaError::UNSUPPORTED_PROFILE);
    records[1].hasTime = false;

    for (const auto type : {0, 1, 3, 9, 10, 104, 120, 264, -1}) {
        records[1].fields[0].serialTypeId = type;
        CHECK(LocalSchema::create(records, SCHEMA_LIMITS).error() == SchemaError::UNSUPPORTED_PROFILE);
    }

    // One record, two keys: exact finite budget boundaries for every dimension.
    records = {{0, u"R", false, {{8}}, {{u"x", 0}, {u"R.x", 0}}}};
    const std::array<std::size_t, 5> exact{1, 1, 2, 3, 5};

    for (std::size_t dimension = 0; dimension < exact.size(); ++dimension) {
        for (const auto count : {std::size_t{0}, exact[dimension] - 1, exact[dimension], exact[dimension] + 1}) {
            auto bound = exact;
            bound[dimension] = count;
            const auto result = LocalSchema::create(records, {bound[0], bound[1], bound[2], bound[3], bound[4]});
            CHECK(result.has_value() == (count >= exact[dimension]));

            if (!result) {
                CHECK(result.error() == SchemaError::RESOURCE_LIMIT);
            }
        }
    }
}

TEST_CASE("binding validates full types, budgets, empty and owned independent plans") {
    auto schema = LocalSchema::create(localRecords(), SCHEMA_LIMITS);
    REQUIRE(schema.has_value());
    WireRecordDescription raw{std::numeric_limits<std::int32_t>::max(), u"R", {{u"x", 8}, {u"xdot", 8}}};
    auto plan = bindRecord(raw, *schema, BINDING_LIMITS);
    REQUIRE(plan.has_value());
    CHECK(plan->senderId == std::numeric_limits<std::int32_t>::max());

    for (const auto &action : plan->fields) {
        CHECK(action.targetFieldId == 1);
    }

    for (const auto &name : {u"R", u"Unknown"}) {
        raw.name = name;

        for (std::int32_t type = 0; type <= 255; ++type) {
            raw.fields[1].serialTypeId = type;
            const auto result = bindRecord(raw, *schema, BINDING_LIMITS);
            CHECK(result.has_value() == (type == 8));

            if (!result) {
                CHECK(result.error() == BindingError::UNSUPPORTED_PROFILE);
            }
        }
    }

    raw = {0, u"R", {{u"x", 8}}};

    for (std::size_t mutation = 0; mutation < 5; ++mutation) {
        auto changed = raw;

        switch (mutation) {
        case 0:
            changed.senderId = -1;
            break;
        case 1:
            changed.name.clear();
            break;
        case 2:
            changed.fields[0].name.clear();
            break;
        case 3:
            changed.fields[0].serialTypeId = -1;
            break;
        case 4:
            changed.fields[0].serialTypeId = 256;
            break;
        }

        const auto result = bindRecord(changed, *schema, BINDING_LIMITS);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == BindingError::INVALID_DESCRIPTION);
    }

    raw.fields.push_back({u"xdot", 8});
    const std::array<std::size_t, 3> exact{2, 4, 6};

    for (std::size_t dimension = 0; dimension < exact.size(); ++dimension) {
        for (const auto count : {std::size_t{0}, exact[dimension] - 1, exact[dimension], exact[dimension] + 1}) {
            auto bound = exact;
            bound[dimension] = count;
            const auto result = bindRecord(raw, *schema, {bound[0], bound[1], bound[2]});
            CHECK(result.has_value() == (count >= exact[dimension]));

            if (!result) {
                CHECK(result.error() == BindingError::RESOURCE_LIMIT);
            }
        }
    }

    raw.fields.clear();
    CHECK(bindRecord(raw, *schema, {0, 1, 1})->fields.empty());
    raw.name = u"Unknown";
    const auto unknown = bindRecord(raw, *schema, {0, 7, 7});
    REQUIRE(unknown.has_value());
    CHECK_FALSE(unknown->localRecordId.has_value());
    CHECK(unknown->fields.empty());
    auto emptySchema = LocalSchema::create({}, {0, 0, 0, 0, 0});
    REQUIRE(emptySchema.has_value());
    CHECK_FALSE(bindRecord(raw, *emptySchema, BINDING_LIMITS)->localRecordId.has_value());
    // Raw isolated UTF-16 is structurally accepted, but cannot match well-formed local names.
    raw.name = std::u16string(1, static_cast<char16_t>(0xd800));
    CHECK_FALSE(bindRecord(raw, *schema, BINDING_LIMITS)->localRecordId.has_value());
    const auto owned = [] {
        auto inputs = localRecords();
        auto local = LocalSchema::create(inputs, SCHEMA_LIMITS);
        inputs.clear();
        WireRecordDescription wire{9, u"R", {{u"xdot", 8}, {u"R.x", 8}, {u"R.xdot", 8}}};

        return bindRecord(wire, *local, BINDING_LIMITS);
    }();
    REQUIRE(owned.has_value());
    CHECK(owned->senderId == 9);
    CHECK(owned->localRecordId == 1);
    REQUIRE(owned->fields.size() == 3);
    CHECK(owned->fields[0].targetFieldId == 1);
    CHECK(owned->fields[1].targetFieldId == 1);
    CHECK_FALSE(owned->fields[2].targetFieldId.has_value());
    const auto maximum = std::numeric_limits<std::size_t>::max();
    CHECK(LocalSchema::create(localRecords(), {maximum, maximum, maximum, maximum, maximum}).has_value());
    CHECK(bindRecord(raw, *schema, {maximum, maximum, maximum}).has_value());
}
} // namespace

/** Run mandatory fixture-backed and API-policy tests.
 * @param argc Argument count including fixture path.
 * @param argv Program and explicit fixture path.
 * @return Nonzero on test failure or missing fixture selection.
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }

    fixturePath = argv[1];
    doctest::Context context;

    return context.run();
}
