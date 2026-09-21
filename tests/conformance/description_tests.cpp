#include "qd/protocol/describe_records.hpp"
#include "qd/protocol/frame_reader.hpp"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace {
using qd::protocol::DescriptionLimits;
using qd::protocol::DescriptionResult;
using qd::protocol::DescriptionStage;
using qd::protocol::DescriptionStatus;
using qd::protocol::parseDescribeRecords;

constexpr DescriptionLimits LIMITS{256, 32, 16, 64, {64, 64}, 256}; ///< Bounded Java fixture profile.
constexpr std::size_t CASE_COUNT = 81; ///< Exact selected P08 and historical payload observations.
std::filesystem::path evidenceFile;    ///< Immutable Java output selected by main.

/** Split exact fixture text, retaining empty trailing fields.
 * @param text Input text.
 * @param delimiter Separator character.
 * @return Ordered fields, including empty fields.
 */
std::vector<std::string> split(const std::string &text, char delimiter) {
    std::vector<std::string> out;
    std::size_t begin = 0;

    for (;;) {
        const auto end = text.find(delimiter, begin);
        out.push_back(text.substr(begin, end == std::string::npos ? end : end - begin));

        if (end == std::string::npos) {
            return out;
        }

        begin = end + 1;
    }
}

/** Validate a complete nonnegative decimal or hexadecimal integer.
 * @param text Exact nonempty digits.
 * @param base Numeric radix.
 * @return Parsed size_t, failing the test on malformed or overflowing input.
 */
std::size_t number(const std::string &text, int base = 10) {
    std::size_t out{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), out, base);
    REQUIRE_FALSE(text.empty());
    REQUIRE(parsed.ec == std::errc{});
    REQUIRE(parsed.ptr == text.data() + text.size());

    return out;
}

/** Decode exact fixture bytes with no permissive fallback.
 * @param text Hex octets or dash for empty.
 * @return Owned input bytes.
 */
std::vector<std::byte> unhex(const std::string &text) {
    if (text == "-") {
        return {};
    }

    REQUIRE_FALSE(text.empty());
    REQUIRE(text.size() % 2 == 0);
    std::vector<std::byte> out;

    for (std::size_t i = 0; i < text.size(); i += 2) {
        out.push_back(static_cast<std::byte>(number(text.substr(i, 2), 16)));
    }

    return out;
}

/** Format owned UTF-16 units for comparison with independently observed Java units.
 * @param value Exact decoded name.
 * @return Four lowercase hex digits per code unit, with no Unicode normalization.
 */
std::string units(const std::u16string &value) {
    constexpr char HEX[] = "0123456789abcdef"; // Four nibbles form one UTF-16 unit.
    std::string out;

    for (const auto unit : value) {
        for (int shift = 12; shift >= 0; shift -= 4) {
            out += HEX[(unit >> shift) & 15];
        }
    }

    return out;
}

/** Serialize actual ordered descriptions in the diagnostic evidence format.
 * @param result Parser result; no expectation is produced by this function.
 * @return Comparable actual value including duplicates and full type IDs.
 */
std::string descriptions(const DescriptionResult &result) {
    std::string out;

    for (const auto &record : result.descriptions) {
        if (!out.empty()) {
            out += ';';
        }

        out += std::to_string(record.senderId) + '/' + units(record.name) + '/';
        bool first = true;

        for (const auto &field : record.fields) {
            if (!first) {
                out += ',';
            }

            first = false;
            out += units(field.name) + ':' + std::to_string(field.serialTypeId);
        }
    }

    return out.empty() ? "-" : out;
}

/** Validated Java observation plus its explicitly raw-layer projection. */
struct Observation {
    std::string id;                 ///< Unique evidence scenario.
    std::vector<std::byte> payload; ///< Exact Java-observed payload bytes.
    std::vector<std::byte> frame;   ///< Newly completed Java-framed input.
    DescriptionStatus status;       ///< Raw diagnostic category, not parser callback category.
    DescriptionStage stage;         ///< Source-checked diagnostic stage.
    std::size_t consumed;           ///< Diagnostic primitive examination count.
    std::size_t offset;             ///< Primitive start or validation checkpoint.
    std::string records;            ///< Ordered diagnostic completed records in UTF-16 hex.
    qd::encoding::Status cause;     ///< Projection of the Java primitive exception class.
};

/** Validate encoded descriptor/name/map grammar, including unused constructor observations.
 * @param text Diagnostic records or constructed map entries.
 * @param raw Whether record entries include raw record names.
 */
void validateRecords(const std::string &text, bool raw) {
    if (text == "-") {
        return;
    }

    for (const auto &item : split(text, ';')) {
        const auto cells = split(item, '/');
        REQUIRE(cells.size() == (raw ? 3 : 2));
        REQUIRE(number(cells[0]) <= 1024);

        if (raw) {
            REQUIRE_FALSE(cells[1].empty());
            REQUIRE(cells[1].size() % 4 == 0);
            unhex(cells[1]);
        }

        if (!cells.back().empty()) {
            for (const auto &field : split(cells.back(), ',')) {
                const auto pair = split(field, ':');
                REQUIRE(pair.size() == 2);
                REQUIRE_FALSE(pair[0].empty());
                REQUIRE(pair[0].size() % 4 == 0);
                unhex(pair[0]);
                REQUIRE(number(pair[1]) <= 255);
            }
        }
    }
}

/** Load all selected evidence with strict row/value and nonempty coverage validation.
 * @return Ordered, validated observations; missing/unknown/duplicate rows fail the test.
 */
std::vector<Observation> load() {
    const std::map<std::string, DescriptionStatus> statuses{{"OK", DescriptionStatus::OK},
                                                            {"TRUNCATED_PAYLOAD", DescriptionStatus::TRUNCATED_PAYLOAD},
                                                            {"INVALID_RECORD", DescriptionStatus::INVALID_RECORD},
                                                            {"INVALID_FIELD", DescriptionStatus::INVALID_FIELD},
                                                            {"INVALID_STRING", DescriptionStatus::INVALID_STRING}};
    const std::map<std::string, DescriptionStage> stages{{"END", DescriptionStage::END},
                                                         {"SENDER_ID", DescriptionStage::SENDER_ID},
                                                         {"RECORD_NAME", DescriptionStage::RECORD_NAME},
                                                         {"FIELD_COUNT", DescriptionStage::FIELD_COUNT},
                                                         {"RECORD_HEADER", DescriptionStage::RECORD_HEADER},
                                                         {"FIELD_NAME", DescriptionStage::FIELD_NAME},
                                                         {"FIELD_TYPE", DescriptionStage::FIELD_TYPE},
                                                         {"FIELD_HEADER", DescriptionStage::FIELD_HEADER}};
    const std::map<std::string, qd::encoding::Status> causes{
        {"-", qd::encoding::Status::OK},
        {"java.io.EOFException", qd::encoding::Status::INSUFFICIENT_INPUT},
        {"java.io.UTFDataFormatException", qd::encoding::Status::INVALID_ENCODING},
        {"java.io.IOException", qd::encoding::Status::INVALID_LENGTH}};
    std::ifstream input(evidenceFile);
    REQUIRE(input.good());
    std::vector<Observation> out;
    std::set<std::string> ids;
    bool complete = false;
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        REQUIRE_FALSE(complete);
        const auto c = split(line, '\t');

        if (c[0] == "complete") {
            REQUIRE(c.size() == 2);
            REQUIRE(c[1] == "descriptions");
            complete = true;
            continue;
        }

        REQUIRE(c[0] == "case");
        REQUIRE(c.size() == 14);
        REQUIRE_FALSE(c[1].empty());
        REQUIRE(ids.insert(c[1]).second);
        REQUIRE(statuses.contains(c[4]));
        REQUIRE(stages.contains(c[5]));
        REQUIRE(causes.contains(c[9]));
        validateRecords(c[8], true);
        validateRecords(c[11], false);

        if (c[10] != "-") {
            for (const auto &id : split(c[10], ';')) {
                REQUIRE(number(id) <= 1024);
            }
        }

        REQUIRE((c[12] == "-" || c[12] == "message:2"));
        auto payload = unhex(c[2]);
        auto frame = unhex(c[3]);
        REQUIRE(number(c[6]) <= payload.size());
        REQUIRE(number(c[7]) <= number(c[6]));
        REQUIRE(number(c[13]) == frame.size());
        out.push_back({c[1], std::move(payload), std::move(frame), statuses.at(c[4]), stages.at(c[5]), number(c[6]),
                       number(c[7]), c[8], causes.at(c[9])});
    }

    REQUIRE(complete);
    REQUIRE(out.size() == CASE_COUNT);

    return out;
}

/** Compare every raw-parser surface with Java diagnostic evidence.
 * @param actual Owned C++ result.
 * @param expected Independent Java observation.
 */
void compare(const DescriptionResult &actual, const Observation &expected) {
    CHECK(actual.status == expected.status);
    CHECK(actual.stage == expected.stage);
    CHECK(actual.consumed == expected.consumed);
    CHECK(actual.errorOffset == expected.offset);
    CHECK(actual.primitiveStatus == expected.cause);
    CHECK(descriptions(actual) == expected.records);
}

/** Compose independent frame extraction and payload parsing at one arbitrary schedule.
 * @param expected Java-framed input and raw projection.
 * @param chunks Feed lengths partitioning the frame plus a following sentinel frame.
 */
void replay(const Observation &expected, const std::vector<std::size_t> &chunks) {
    auto stream = expected.frame;
    stream.push_back(std::byte{1}); // Complete following heartbeat, independent of payload validity.
    stream.push_back(std::byte{0});
    qd::protocol::FrameReader reader({256, 265});
    std::size_t offset = 0;
    std::size_t count = 0;
    DescriptionResult saved{};

    for (const auto chunk : chunks) {
        const auto end = offset + chunk;
        REQUIRE(end <= stream.size());

        do {
            const auto feed = reader.feed(std::span(stream).subspan(offset, end - offset));
            REQUIRE(feed.status != qd::protocol::FrameStatus::FAILED);
            REQUIRE(feed.accepted <= end - offset);
            offset += feed.accepted;

            if (feed.frame) {
                if (count == 0) {
                    REQUIRE(feed.frame->type == 2);
                    CHECK(feed.frame->payload == expected.payload);
                    const auto accepted = reader.totalAccepted();
                    saved = parseDescribeRecords(feed.frame->payload, LIMITS);
                    compare(saved, expected);
                    CHECK(reader.totalAccepted() == accepted);
                    CHECK(reader.error() == qd::protocol::FrameError::NONE);
                    CHECK(offset == expected.frame.size());
                } else {
                    REQUIRE(count == 1);
                    CHECK(feed.frame->type == 0);
                    CHECK(feed.frame->payload.empty());
                }

                ++count;
            } else {
                REQUIRE(offset == end);
            }
        } while (offset < end);
    }

    CHECK(offset == stream.size());
    CHECK(count == 2);
    CHECK(reader.finish() == qd::protocol::FrameStatus::FINISHED);
    reader.reset();
    compare(saved, expected); // Frame/input buffers have already been destroyed or reset.
}
} // namespace

TEST_CASE("all Java raw projections and frame composition") {
    for (const auto &expected : load()) {
        CAPTURE(expected.id);
        compare(parseDescribeRecords(expected.payload, LIMITS), expected);
        const auto size = expected.frame.size() + 2;
        replay(expected, {size});

        for (std::size_t splitAt = 0; splitAt <= size; ++splitAt) {
            replay(expected, {splitAt, size - splitAt});
        }

        replay(expected, std::vector<std::size_t>(size, 1));
    }
}

TEST_CASE("small budget boundaries and completed prefix") {
    const auto payload = unhex("00015202017808017908");

    // Each row controls one budget, with the exact successful boundary independently counted.
    for (int budget = 0; budget < 7; ++budget) {
        const std::size_t boundary = budget == 0 ? 10 : budget == 1 ? 1 : budget < 4 ? 2 : budget < 6 ? 1 : 3;

        for (std::size_t limit = 0; limit <= boundary + 1; ++limit) {
            auto limits = LIMITS;

            switch (budget) {
            case 0:
                limits.payloadBytes = limit;
                break;
            case 1:
                limits.descriptions = limit;
                break;
            case 2:
                limits.fieldsPerDescription = limit;
                break;
            case 3:
                limits.totalFields = limit;
                break;
            case 4:
                limits.name.bodyBytes = limit;
                break;
            case 5:
                limits.name.codeUnits = limit;
                break;
            default:
                limits.totalNameUnits = limit;
                break;
            }

            const auto result = parseDescribeRecords(payload, limits);
            CAPTURE(budget);
            CAPTURE(limit);
            CHECK(result.status == (limit < boundary ? DescriptionStatus::RESOURCE_LIMIT : DescriptionStatus::OK));
            CHECK(result.descriptions.size() == (limit < boundary ? 0 : 1));

            if (limit >= boundary) {
                CHECK(result.consumed == 10);
                CHECK(result.errorOffset == 10);
                CHECK(result.stage == DescriptionStage::END);
                CHECK(result.primitiveStatus == qd::encoding::Status::OK);
            } else {
                const std::size_t positions[] = {0, 0, 4, 4, 2, 3, 3 + limit * 3};
                const std::size_t offsets[] = {0, 0, 4, 4, 1, 1, 1 + limit * 3};
                const DescriptionStage stages[] = {DescriptionStage::PAYLOAD,
                                                   DescriptionStage::DESCRIPTION,
                                                   DescriptionStage::RECORD_HEADER,
                                                   DescriptionStage::RECORD_HEADER,
                                                   DescriptionStage::RECORD_NAME,
                                                   DescriptionStage::RECORD_NAME,
                                                   limit == 0 ? DescriptionStage::RECORD_NAME
                                                              : DescriptionStage::FIELD_NAME};
                CHECK(result.consumed == positions[budget]);
                CHECK(result.errorOffset == offsets[budget]);
                CHECK(result.stage == stages[budget]);
                CHECK(result.primitiveStatus ==
                      (budget < 4 ? qd::encoding::Status::OK : qd::encoding::Status::RESOURCE_LIMIT));
            }
        }
    }

    const auto two = unhex("0001520101780801015301017908");

    for (int budget = 0; budget < 4; ++budget) {
        auto limits = LIMITS;

        if (budget == 0) {
            limits.descriptions = 1;
        }

        if (budget == 1) {
            limits.totalFields = 1;
        }

        if (budget == 2) {
            limits.totalNameUnits = 2;
        }

        if (budget == 3) {
            limits.totalNameUnits = 3;
        }

        const auto result = parseDescribeRecords(two, limits);
        REQUIRE(result.status == DescriptionStatus::RESOURCE_LIMIT);
        CHECK(descriptions(result) == "0/0052/0078:8");
        const std::size_t positions[] = {7, 11, 10, 13};
        const std::size_t offsets[] = {7, 11, 8, 11};
        CHECK(result.consumed == positions[budget]);
        CHECK(result.errorOffset == offsets[budget]);
    }

    CHECK(parseDescribeRecords({}, {0, 0, 0, 0, {0, 0}, 0}).status == DescriptionStatus::OK);
    const auto zeroFields = parseDescribeRecords(unhex("00015200"), {4, 1, 0, 0, {1, 1}, 1});
    CHECK(zeroFields.status == DescriptionStatus::OK);
    CHECK(descriptions(zeroFields) == "0/0052/");
}

TEST_CASE("policy precedence sparse identities and ownership") {
    const auto sparse = parseDescribeRecords(unhex("f07fffffff015200"), LIMITS);
    CHECK(sparse.status == DescriptionStatus::OK);
    CHECK(descriptions(sparse) == "2147483647/0052/");
    // Temporary input above is already destroyed: the result must retain owned names.
    const auto hugeCount = parseDescribeRecords(unhex("000152f07fffffff"), LIMITS);
    CHECK(hugeCount.status == DescriptionStatus::RESOURCE_LIMIT);
    CHECK(hugeCount.consumed == 8);
    CHECK(hugeCount.stage == DescriptionStage::RECORD_HEADER);
    auto limits = LIMITS;
    limits.fieldsPerDescription = 0;
    CHECK(parseDescribeRecords(unhex("7f015201"), limits).status == DescriptionStatus::INVALID_RECORD);
    limits = LIMITS;
    limits.name.bodyBytes = 0;
    auto result = parseDescribeRecords(unhex("0001ff"), limits);
    CHECK(result.status == DescriptionStatus::RESOURCE_LIMIT);
    CHECK(result.consumed == 2);
    CHECK(result.errorOffset == 1);
    limits = LIMITS;
    limits.totalNameUnits = 0;
    result = parseDescribeRecords(unhex("0001ff"), limits);
    CHECK(result.status == DescriptionStatus::INVALID_STRING);
    result = parseDescribeRecords(unhex("0001c280"), limits);
    CHECK(result.status == DescriptionStatus::RESOURCE_LIMIT); // Budget precedes completed-body overrun.
    CHECK(result.consumed == 4);
    result = parseDescribeRecords(unhex("0004f09f988000"), {256, 1, 1, 1, {4, 2}, 1});
    CHECK(result.status == DescriptionStatus::RESOURCE_LIMIT); // A surrogate pair is charged atomically.
    CHECK(result.consumed == 6);
    result = parseDescribeRecords(unhex("0004f09f988000"), {256, 1, 1, 1, {4, 2}, 2});
    CHECK(result.status == DescriptionStatus::OK);
    CHECK(descriptions(result) == "0/d83dde00/");
    limits = LIMITS;
    limits.totalFields = std::numeric_limits<std::size_t>::max();
    limits.descriptions = std::numeric_limits<std::size_t>::max();
    limits.totalNameUnits = std::numeric_limits<std::size_t>::max();
    CHECK(parseDescribeRecords(unhex("00015200"), limits).status == DescriptionStatus::OK);

    // Full type range is structural, not an assertion that event decoders support these codes.
    for (std::int32_t type = 0; type <= 255; ++type) {
        auto bytes = unhex("000152010178");
        std::byte encoded[5]{};
        const auto written = qd::encoding::writeCompactInt(type, encoded);
        bytes.insert(bytes.end(), encoded, encoded + written.written);
        const auto parsed = parseDescribeRecords(bytes, LIMITS);
        REQUIRE(parsed.status == DescriptionStatus::OK);
        REQUIRE(parsed.descriptions.size() == 1);
        REQUIRE(parsed.descriptions[0].fields.size() == 1);
        CHECK(parsed.descriptions[0].fields[0].serialTypeId == type);
    }
}

/** Run doctest using a mandatory explicit evidence path.
 * @param argc Count of command-line arguments.
 * @param argv Executable and immutable Java fixture path.
 * @return Zero only when all selected checks pass.
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: description_tests descriptions.tsv\n";

        return 2;
    }

    evidenceFile = argv[1];
    doctest::Context context;

    return context.run();
}
