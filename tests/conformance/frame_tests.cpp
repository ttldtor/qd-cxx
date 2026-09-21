#include "qd/protocol/frame_reader.hpp"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using qd::protocol::Frame;
using qd::protocol::FrameError;
using qd::protocol::FrameKind;
using qd::protocol::FrameReader;
using qd::protocol::FrameStatus;

constexpr std::size_t MAX_COMPACT_PREFIX_BYTES = 9; ///< Full-width long prefix storage requirement.
constexpr std::size_t SELECTED_CASE_COUNT = 18;     ///< One historical P01 plus 17 new P02 subcases.
constexpr std::size_t MAX_TEST_BODY_BYTES = 64;     ///< Small fixture profile including P01 descriptions.

std::filesystem::path fixtureDirectory; ///< Java evidence location supplied by main.
std::filesystem::path projectionFile;   ///< Explicitly derived, separately reviewed frame expectations.

/** Split a fixture field, retaining empty trailing fields.
 * @param text Exact fixture text.
 * @param separator Field delimiter.
 * @return Ordered fields, including empty fields.
 */
std::vector<std::string> split(const std::string &text, char separator) {
    std::vector<std::string> fields;
    std::size_t start = 0;

    for (;;) {
        const auto end = text.find(separator, start);
        fields.push_back(text.substr(start, end == std::string::npos ? end : end - start));

        if (end == std::string::npos) {
            return fields;
        }

        start = end + 1;
    }
}

/** Parse a complete unsigned decimal fixture field, failing on overflow/trailing bytes.
 * @param text Decimal field to validate.
 * @return Exact uint64_t value.
 */
std::uint64_t number(const std::string &text) {
    std::uint64_t value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    REQUIRE(result.ec == std::errc{});
    REQUIRE(result.ptr == text.data() + text.size());

    return value;
}

/** Decode exact fixture hex; a dash denotes an explicitly empty byte sequence.
 * @param text Hex field (or dash), never silently accepting malformed input.
 * @return Owned bytes for replay or comparison.
 */
std::vector<std::byte> unhex(const std::string &text) {
    if (text == "-") {
        return {};
    }

    REQUIRE_FALSE(text.empty());
    REQUIRE(text.size() % 2 == 0);
    std::vector<std::byte> result;

    for (std::size_t i = 0; i < text.size(); i += 2) {
        unsigned int value{};
        const auto parsed = std::from_chars(text.data() + i, text.data() + i + 2, value, 16);
        REQUIRE(parsed.ec == std::errc{});
        REQUIRE(parsed.ptr == text.data() + i + 2);
        result.push_back(static_cast<std::byte>(value));
    }

    return result;
}

/** Compare exact ordered bytes without relying on span equality availability.
 * @param actual Borrowed actual byte sequence.
 * @param expected Borrowed expected byte sequence.
 */
void checkBytes(std::span<const std::byte> actual, std::span<const std::byte> expected) {
    REQUIRE(actual.size() == expected.size());
    CHECK(std::equal(actual.begin(), actual.end(), expected.begin()));
}

/** Load and validate the selected Java inputs, rejecting empty or partial selected coverage.
 * @return Map containing P01 and every separately captured P02 subcase.
 */
std::map<std::string, std::vector<std::byte>> loadInputs() {
    std::map<std::string, std::vector<std::byte>> inputs;

    for (const auto *name : {"P01", "P02"}) {
        std::ifstream file(fixtureDirectory / (std::string(name) + ".tsv"));
        REQUIRE(file.good());
        bool complete = false;
        std::string line;

        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            const auto fields = split(line, '\t');

            if (fields[0] == "complete") {
                REQUIRE(fields.size() == 2);
                REQUIRE(fields[1] == name);
                REQUIRE_FALSE(complete);
                complete = true;
            }

            if ((std::string(name) == "P01" && fields[0] == "wire") ||
                (std::string(name) == "P02" && fields[0] == "p02.input")) {
                REQUIRE_FALSE(complete);
                REQUIRE(fields.size() == 3);
                const auto id = std::string(name) == "P01" ? "P01" : fields[1];
                REQUIRE(inputs.emplace(id, unhex(fields[2])).second);
            }
        }

        REQUIRE(complete);
    }

    REQUIRE(inputs.size() == SELECTED_CASE_COUNT);

    return inputs;
}

/** Reviewed frame-only projection, distinct from Java callbacks and diagnostic positions. */
struct Projection {
    std::string id;                 ///< Selected source case/subcase identifier.
    std::size_t limit;              ///< Small finite body budget.
    std::vector<Frame> frames;      ///< Ordered owned frame expectations.
    FrameError error;               ///< First structural error, or NONE before finish.
    std::uint64_t accepted;         ///< C++ projected accepted position before finish.
    std::vector<std::byte> pending; ///< Exact incomplete/failed bytes before finish.
    std::uint64_t errorOffset;      ///< Failed frame start, interpreted only on error.
};

/** Parse strict derived expectations; unknown classifications must fail selection.
 * @return All reviewed P01/P02 projections; no silently omitted or duplicate row.
 */
std::vector<Projection> loadProjections() {
    const std::map<std::string, FrameError> errors{{"NONE", FrameError::NONE},
                                                   {"INVALID_LENGTH", FrameError::INVALID_LENGTH},
                                                   {"BODY_LIMIT", FrameError::BODY_LIMIT},
                                                   {"TRUNCATED_TYPE", FrameError::TRUNCATED_TYPE},
                                                   {"INVALID_TYPE", FrameError::INVALID_TYPE}};
    std::ifstream file(projectionFile);
    REQUIRE(file.good());
    std::vector<Projection> result;
    std::set<std::string> names;
    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto fields = split(line, '\t');
        REQUIRE(fields.size() == 7);
        REQUIRE(names.insert(fields[0]).second);
        REQUIRE(errors.contains(fields[3]));
        REQUIRE(number(fields[1]) <= MAX_TEST_BODY_BYTES);
        Projection projection{fields[0],
                              static_cast<std::size_t>(number(fields[1])),
                              {},
                              errors.at(fields[3]),
                              number(fields[4]),
                              unhex(fields[5]),
                              number(fields[6])};

        if (fields[2] != "-") {
            for (const auto &entry : split(fields[2], ';')) {
                const auto parts = split(entry, ':');
                REQUIRE(parts.size() == 5);
                REQUIRE((parts[0] == "E" || parts[0] == "M"));
                Frame frame{parts[0] == "E" ? FrameKind::EMPTY_HEARTBEAT : FrameKind::MESSAGE, std::nullopt,
                            unhex(parts[2]), number(parts[3]), number(parts[4])};

                if (frame.kind == FrameKind::MESSAGE) {
                    const auto type = number(parts[1]);
                    REQUIRE(type <= std::numeric_limits<std::int32_t>::max());
                    frame.type = static_cast<std::int32_t>(type);
                } else {
                    REQUIRE(parts[1] == "-");
                    REQUIRE(frame.payload.empty());
                }

                REQUIRE(frame.startOffset < frame.endOffset);
                REQUIRE(frame.startOffset == (projection.frames.empty() ? 0 : projection.frames.back().endOffset));
                projection.frames.push_back(std::move(frame));
            }
        }

        result.push_back(std::move(projection));
    }

    REQUIRE(result.size() == SELECTED_CASE_COUNT);

    return result;
}

/** Compare every externally observable field, including owned opaque payload bytes.
 * @param actual Extracted frame.
 * @param expected Reviewed frame projection.
 */
void checkFrame(const Frame &actual, const Frame &expected) {
    CHECK(actual.kind == expected.kind);
    CHECK(actual.type == expected.type);
    CHECK(actual.startOffset == expected.startOffset);
    CHECK(actual.endOffset == expected.endOffset);
    checkBytes(actual.payload, expected.payload);
}

/** Replay a partition and check exact per-call accounting, retention and absence of early output.
 * @param bytes Complete source bytes, including any suffix after terminal framing error.
 * @param projection Independent reviewed expected result.
 * @param chunks Chunk sizes whose sum must equal the input length; zero is allowed.
 */
void replay(std::span<const std::byte> bytes, const Projection &projection, const std::vector<std::size_t> &chunks) {
    FrameReader reader({projection.limit, projection.limit + MAX_COMPACT_PREFIX_BYTES});
    std::vector<Frame> retained;
    std::size_t supplied = 0;
    std::uint64_t lastEnd = 0;

    for (const auto chunk : chunks) {
        REQUIRE(chunk <= bytes.size() - supplied);
        const auto end = supplied + chunk;
        auto offset = supplied;

        do {
            const auto before = reader.totalAccepted();
            auto result = reader.feed(bytes.subspan(offset, end - offset));
            CHECK(result.accepted <= end - offset);
            CHECK(reader.totalAccepted() == before + result.accepted);
            const auto nextBoundary = retained.size() < projection.frames.size()
                                          ? projection.frames[retained.size()].endOffset
                                          : projection.accepted;
            const auto expectedPosition = std::min<std::uint64_t>(end, nextBoundary);
            CHECK(reader.totalAccepted() == expectedPosition);
            offset += result.accepted;

            if (result.status == FrameStatus::FRAME_READY) {
                REQUIRE(result.frame.has_value());
                REQUIRE(retained.size() < projection.frames.size());
                checkFrame(*result.frame, projection.frames[retained.size()]);
                CHECK(result.frame->endOffset <= end);
                lastEnd = result.frame->endOffset;
                retained.push_back(std::move(*result.frame));
            } else {
                CHECK_FALSE(result.frame.has_value());
                CHECK((result.status == FrameStatus::NEED_INPUT || result.status == FrameStatus::FAILED));

                if (result.status == FrameStatus::NEED_INPUT) {
                    CHECK(offset == end);
                }
            }

            checkBytes(reader.pending(), bytes.subspan(static_cast<std::size_t>(lastEnd),
                                                       static_cast<std::size_t>(reader.totalAccepted() - lastEnd)));
            CHECK(reader.pending().size() <= reader.storageBytes());
            CHECK(reader.storageBytes() == projection.limit + MAX_COMPACT_PREFIX_BYTES);

            if (result.status == FrameStatus::FAILED) {
                CHECK(reader.error() == projection.error);
                break;
            }

            if (offset == end) {
                break;
            }

            REQUIRE(result.accepted > 0);
        } while (true);

        supplied = end;
    }

    REQUIRE(supplied == bytes.size());
    CHECK(reader.totalAccepted() == projection.accepted);
    CHECK(reader.error() == projection.error);
    checkBytes(reader.pending(), projection.pending);
    REQUIRE(retained.size() == projection.frames.size());
    const auto beforeFinish = reader.totalAccepted();
    const auto expectedFinish = projection.error != FrameError::NONE || !projection.pending.empty()
                                    ? FrameStatus::FAILED
                                    : FrameStatus::FINISHED;
    CHECK(reader.finish() == expectedFinish);
    CHECK(reader.finish() == expectedFinish);

    if (projection.error != FrameError::NONE) {
        CHECK(reader.errorOffset() == projection.errorOffset);
    } else if (!projection.pending.empty()) {
        CHECK(reader.error() == FrameError::TRUNCATED_INPUT);
        CHECK(reader.errorOffset() == lastEnd);
    }

    auto after = reader.feed(bytes);
    CHECK(after.accepted == 0);
    CHECK(after.status == expectedFinish);
    CHECK(reader.totalAccepted() == beforeFinish);
    checkBytes(reader.pending(), projection.pending);
    reader.reset();
    CHECK(reader.pending().empty());
    CHECK(reader.totalAccepted() == 0);
    CHECK(reader.error() == FrameError::NONE);
    CHECK(reader.feed({}).status == FrameStatus::NEED_INPUT);

    for (std::size_t i = 0; i < retained.size(); ++i) {
        checkFrame(retained[i], projection.frames[i]);
    }
}

TEST_CASE("P01/P02 reviewed frames are independent of stream partition") {
    const auto inputs = loadInputs();
    const auto projections = loadProjections();

    for (const auto &projection : projections) {
        INFO(projection.id);
        REQUIRE(inputs.contains(projection.id));
        const auto &bytes = inputs.at(projection.id);
        replay(bytes, projection, {bytes.size()});

        for (std::size_t splitAt = 0; splitAt <= bytes.size(); ++splitAt) {
            replay(bytes, projection, {splitAt, bytes.size() - splitAt});
        }

        std::vector<std::size_t> bytewise{0};

        for (std::size_t i = 0; i < bytes.size(); ++i) {
            bytewise.push_back(1);
            bytewise.push_back(0);
        }

        replay(bytes, projection, bytewise);

        // All three-chunk partitions include empty feeds and boundaries inside both compact fields.
        for (std::size_t first = 0; first <= bytes.size(); ++first) {
            for (std::size_t second = first; second <= bytes.size(); ++second) {
                replay(bytes, projection, {first, 0, second - first, 0, bytes.size() - second});
            }
        }
    }
}

TEST_CASE("bounded delivery, output lifetime and explicit lifecycle") {
    FrameReader reader({2, 11});
    const auto sample = unhex("0200ab");
    std::vector<std::byte> many;

    for (int i = 0; i < 10000; ++i) {
        many.insert(many.end(), sample.begin(), sample.end());
    }

    std::optional<Frame> first;
    std::size_t offset = 0;
    std::size_t frames = 0;

    while (offset < many.size()) {
        auto result = reader.feed(std::span(many).subspan(offset));
        REQUIRE(result.status == FrameStatus::FRAME_READY);
        REQUIRE(result.frame.has_value());
        CHECK(result.accepted == 3);
        checkBytes(result.frame->payload, unhex("ab"));
        CHECK(reader.storageBytes() == 11);
        CHECK(reader.pending().empty());
        offset += result.accepted;
        ++frames;

        if (!first) {
            first = std::move(result.frame);
        }
    }

    CHECK(frames == 10000);
    CHECK(reader.feed({}).status == FrameStatus::NEED_INPUT);
    CHECK(reader.finish() == FrameStatus::FINISHED);
    CHECK(reader.feed(sample).accepted == 0);
    reader.reset();
    CHECK(reader.feed(sample).status == FrameStatus::FRAME_READY);
    checkBytes(first->payload, unhex("ab"));
    std::optional<Frame> survivor;
    {
        FrameReader temporary({2, 11});
        survivor = temporary.feed(sample).frame;
    }
    REQUIRE(survivor.has_value());
    checkBytes(survivor->payload, unhex("ab"));
}

TEST_CASE("configuration, full-width prefixes, type boundaries and exact retention") {
    CHECK_THROWS_AS(FrameReader({8, 16}), std::invalid_argument);
    CHECK_THROWS_AS(FrameReader({std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max()}),
                    std::invalid_argument);
    FrameReader zero({0, 9});
    CHECK(zero.feed(unhex("00")).status == FrameStatus::FRAME_READY);
    CHECK(zero.feed(unhex("01")).status == FrameStatus::FAILED);
    CHECK(zero.error() == FrameError::BODY_LIMIT);
    CHECK(zero.finish() == FrameStatus::FAILED);
    zero.reset();
    CHECK(zero.finish() == FrameStatus::FINISHED);
    FrameReader reader({9, 18});
    // Full-width overlong body length 9 and full-width overlong type 0 use the entire storage budget.
    const auto wide = unhex("ff0000000000000009ff0000000000000000");

    for (std::size_t count = 0; count < wide.size(); ++count) {
        reader.reset();
        const auto prefix = std::span(wide).first(count);
        CHECK(reader.feed(prefix).status == FrameStatus::NEED_INPUT);
        checkBytes(reader.pending(), prefix);
        CHECK(reader.feed({}).status == FrameStatus::NEED_INPUT);
        auto last = reader.feed(std::span(wide).subspan(count));
        REQUIRE(last.status == FrameStatus::FRAME_READY);
        REQUIRE(last.frame.has_value());
        CHECK(last.frame->type == 0);
        CHECK(last.frame->payload.empty());
        CHECK(reader.finish() == FrameStatus::FINISHED);
    }

    reader.reset();
    auto maximumType = reader.feed(unhex("05f07fffffff"));
    REQUIRE(maximumType.frame.has_value());
    CHECK(maximumType.frame->type == std::numeric_limits<std::int32_t>::max());
    reader.reset();
    CHECK(reader.feed(unhex("ff7fffffffffffffff")).status == FrameStatus::FAILED);
    CHECK(reader.error() == FrameError::BODY_LIMIT);
    CHECK(reader.totalAccepted() == 9);
    reader.reset();
    CHECK(reader.feed(unhex("ff8000000000000000")).status == FrameStatus::FAILED);
    CHECK(reader.error() == FrameError::INVALID_LENGTH);
    reader.reset();
    CHECK(reader.feed(unhex("0204ff")).status == FrameStatus::FRAME_READY); // PART is opaque.
}
} // namespace

/** Run conformance with explicit fixture locations for read-only negative verification.
 * @param argc Must be three, including executable name.
 * @param argv Executable, Java fixture directory and reviewed frame projection file.
 * @return Nonzero on missing arguments or any doctest failure in every build configuration.
 */
int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: frame_tests <Java fixture directory> <frames.tsv>\n";

        return 2;
    }

    fixtureDirectory = argv[1];
    projectionFile = argv[2];
    doctest::Context context;

    return context.run();
}
