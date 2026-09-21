#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "qd/encoding/compact.hpp"
#include "qd/encoding/symbol_reader.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
using qd::encoding::SymbolReader;
using qd::encoding::SymbolStatus;
std::string fixturePath;                                       ///< Explicit immutable Java observation path.
constexpr qd::encoding::SymbolLimits LIMITS{4096, 2048, 2048}; ///< Bounded corpus profile.

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

/** Project Java exception family/message to the separately documented C++ classification.
 * @param text Java captured status cell.
 * @return Expected C++ status; unknown exceptions fail the test.
 */
SymbolStatus status(std::string_view text) {
    if (text == "OK") {
        return SymbolStatus::OK;
    }

    if (text == "java.io.EOFException:End of buffer.") {
        return SymbolStatus::TRUNCATED_INPUT;
    }

    if (text == "java.io.IOException:Illegal length") {
        return SymbolStatus::INVALID_LENGTH;
    }

    if ((text == "java.io.UTFDataFormatException:null" ||
         text == "java.io.UTFDataFormatException:Code point is beyond BMP") ||
        text == "java.io.IOException:Reserved bit sequence" || text == "java.io.IOException:Symbol is undefined" ||
        text == "java.io.IOException:Duplicated event flags prefix") {
        return SymbolStatus::INVALID_ENCODING;
    }

    throw std::runtime_error("Unrecognized Java result");
}

/** Parse a nonnegative position/count without signed-to-size narrowing.
 * @param text Decimal cell with no sign.
 * @return Representable size_t count; throws on negative, overflow or malformed text.
 */
std::size_t count(std::string_view text) {
    const auto value = number(text);

    if (text.starts_with('-') || value < 0 ||
        static_cast<std::uint64_t>(value) > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Bad count");
    }

    return static_cast<std::size_t>(value);
}

/** Validate a raw Java int without changing its signed value.
 * @param text Decimal cipher or flags cell.
 */
void validateJavaInt(std::string_view text) {
    const auto value = number(text);

    if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error("Java int out of range");
    }
}

/** Validate a scenario identifier, excluding empty cells and escaped control text.
 * @param text Identifier from the bounded oracle vocabulary.
 */
void validateId(std::string_view text) {
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '/' ||
                   c == '_';
        })) {
        throw std::runtime_error("Bad scenario identifier");
    }
}

/** Check the complete supported Java TSV schema before any C++ comparison or diagnostic-only handling.
 * Numeric columns are decimal counts or signed Java ints; hex columns retain exact bytes/UTF-16.
 * @param c Ordered cells including the row type and any empty trailing cells.
 * @throws std::runtime_error Unknown row type, wrong arity, invalid cell syntax or invalid range.
 */
void validateRow(const std::vector<std::string> &c) {
    /** Reject missing and extra cells before indexing row-specific columns.
     * @param expected Exact column count including the row type.
     */
    const auto arity = [&](std::size_t expected) {
        if (c.size() != expected) {
            throw std::runtime_error("Wrong TSV column count");
        }
    };

    if (c.empty()) {
        throw std::runtime_error("Missing row type");
    }

    if (c[0] == "profile") {
        arity(5);

        if (c[1] != "QD-3.355" || c[2] != "EMPTY_SET" || c[3] != "resolver=null" || c[4] != "bounded<=2048bytes") {
            throw std::runtime_error("Unknown oracle profile");
        }
    } else if (c[0] == "writer-error") {
        arity(6);
        validateId(c[1]);

        if (c[2] != "null") {
            (void)units(c[2]);
        }

        validateJavaInt(c[3]);

        if (c[4] != "java.lang.IllegalArgumentException") {
            throw std::runtime_error("Unknown writer error");
        }

        (void)count(c[5]);
    } else if (c[0] == "writer") {
        arity(5);
        validateId(c[1]);
        (void)units(c[2]);
        validateJavaInt(c[3]);
        (void)bytes(c[4]);
    } else if (c[0] == "writer-sequence") {
        arity(3);
        validateId(c[1]);
        (void)bytes(c[2]);
    } else if (c[0] == "sequence") {
        arity(6);
        validateId(c[1]);

        if (count(c[3]) <= count(c[2])) {
            throw std::runtime_error("Invalid sequence positions");
        }

        (void)units(c[4]);
        validateJavaInt(c[5]);
    } else if (c[0] == "read") {
        arity(12);
        validateId(c[1]);

        if (c[2] != "main" && c[2] != "after-repeat" && c[2] != "after-mru" && c[2] != "after-reset" &&
            c[2] != "sentinel") {
            throw std::runtime_error("Unknown read phase");
        }

        (void)bytes(c[3]);
        const auto input = bytes(c[4]);
        const auto outcome = status(c[5]);

        if (count(c[6]) > input.size()) {
            throw std::runtime_error("Consumption exceeds input");
        }

        validateJavaInt(c[7]);

        if (c[8] != "null") {
            (void)units(c[8]);
        }

        if (c[9] != "null" || outcome == SymbolStatus::OK) {
            (void)units(c[9]);
        }

        validateJavaInt(c[10]);

        if (count(c[11]) > count(c[6])) {
            throw std::runtime_error("Flag byte count exceeds consumption");
        }
    } else if (c[0] == "integration") {
        arity(2);
        (void)bytes(c[1]);
    } else if (c[0] == "replay") {
        arity(3);

        // The symbol oracle uses one whole-input chunk, printed by Arrays.toString as [count].
        if (c[1] != "production" || c[2].size() < 3 || c[2].front() != '[' || c[2].back() != ']' ||
            count(std::string_view(c[2]).substr(1, c[2].size() - 2)) == 0) {
            throw std::runtime_error("Invalid production replay schedule");
        }
    } else if (c[0] == "parse.begin") {
        arity(2);
        (void)count(c[1]);
    } else if (c[0] == "record") {
        arity(3);
        (void)count(c[1]);
        constexpr std::string_view PREFIX = "STREAM_DATA|0|R|"; // Only empty R records in this integration.

        if (!c[2].starts_with(PREFIX)) {
            throw std::runtime_error("Unknown record shape");
        }

        const auto symbol = std::string_view(c[2]).substr(PREFIX.size());

        if (symbol.empty() || !std::all_of(symbol.begin(), symbol.end(), [](char ch) {
                return ch >= 'A' && ch <= 'Z';
            })) {
            throw std::runtime_error("Invalid integration symbol text");
        }
    } else if (c[0] == "callback") {
        arity(4);
        (void)count(c[1]);

        if (c[2] != "STREAM_DATA") {
            throw std::runtime_error("Unknown callback type");
        }

        (void)count(c[3]);
    } else if (c[0] == "parse.end") {
        arity(7);

        for (std::size_t i = 1; i < c.size(); ++i) {
            (void)count(c[i]);
        }
    } else if (c[0] == "complete") {
        arity(3);

        if (c[1] != "symbols") {
            throw std::runtime_error("Unknown completion profile");
        }

        (void)count(c[2]);
    } else {
        throw std::runtime_error("Unknown Java TSV row type: " + c[0]);
    }
}
} // namespace

TEST_CASE("Java symbols, strict prefixes and observable successful state") {
    std::ifstream file(fixturePath);
    REQUIRE(file.good());
    SymbolReader reader(LIMITS, 0);
    std::vector<std::byte> sequence;
    std::vector<std::u16string> delivered;
    std::size_t deliveredCount = 0;
    std::size_t readCount = 0, mainCount = 0, sequenceCount = 0, writerCount = 0, terminalCount = 0;
    std::set<std::string> identities;
    bool failed = false, complete = false;
    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        REQUIRE_MESSAGE(!complete, "No observations, including another complete, may follow complete");
        auto c = cells(line);
        INFO("TSV row: ", line);
        validateRow(c);

        if (c[0] == "profile") {
            // Validated configuration metadata; no C++ reader observation to compare.
        } else if (c[0] == "writer-error") {
            // Validated Java writer rejection; no production C++ writer exists.
        } else if (c[0] == "replay") {
            // Validated Java parser chunk schedule; the C++ symbol API receives complete payloads.
        } else if (c[0] == "parse.begin") {
            // Validated Java parser input position; not a C++ symbol-consumption observation.
        } else if (c[0] == "callback") {
            // Validated Java callback grouping; C++ exposes individual owned symbol results.
        } else if (c[0] == "parse.end") {
            // Validated Java parser summary; full parser recovery/accounting is outside this API.
        } else if (c[0] == "integration") {
            REQUIRE(c.size() == 2);
            const auto wire = bytes(c[1]);
            std::size_t offset = 0;
            std::size_t messages = 0;

            while (offset < wire.size()) {
                const auto length = qd::encoding::readCompactLong(std::span(wire).subspan(offset));
                REQUIRE(length.status == qd::encoding::Status::OK);
                REQUIRE(length.value > 0);
                offset += length.consumed;
                REQUIRE(static_cast<std::size_t>(length.value) <= wire.size() - offset);
                const auto body = std::span(wire).subspan(offset, static_cast<std::size_t>(length.value));
                const auto type = qd::encoding::readCompactInt(body);
                REQUIRE(type.status == qd::encoding::Status::OK);

                if (type.value == 15) { // Captured STREAM_DATA with empty R records; test driver only.
                    REQUIRE(reader.beginMessage(0) == SymbolStatus::OK);
                    auto position = type.consumed;

                    while (position < body.size()) {
                        auto symbol = reader.read(body.subspan(position));
                        REQUIRE(symbol.status == SymbolStatus::OK);
                        REQUIRE(symbol.consumed > 0);
                        delivered.push_back(symbol.value->units);
                        position += symbol.consumed;
                        auto record = qd::encoding::readCompactInt(body.subspan(position));
                        REQUIRE(record.status == qd::encoding::Status::OK);
                        REQUIRE(record.value == 0);
                        position += record.consumed;
                    }

                    ++messages;
                } else {
                    REQUIRE(type.value == 2);
                }

                offset += body.size();
            }

            CHECK(messages == 2);
            CHECK(delivered.size() == 7);
        } else if (c[0] == "record") {
            REQUIRE(c.size() == 3);
            REQUIRE(deliveredCount < delivered.size());
            REQUIRE(c[2].starts_with("STREAM_DATA|0|R|"));
            const auto text = c[2].substr(std::string("STREAM_DATA|0|R|").size());
            CHECK(delivered[deliveredCount++] == std::u16string(text.begin(), text.end()));
        } else if (c[0] == "writer") {
            // Validated canonical writer evidence; corresponding main reads compare against C++.
            ++writerCount;
        } else if (c[0] == "writer-sequence") {
            REQUIRE(c.size() == 3);
            sequence = bytes(c[2]);
            REQUIRE(reader.beginMessage(0) == SymbolStatus::OK);
        } else if (c[0] == "sequence") {
            REQUIRE(c.size() == 6);
            const auto start = count(c[2]);
            REQUIRE(start < sequence.size());
            REQUIRE(count(c[3]) <= sequence.size());
            auto result = reader.read(std::span(sequence).subspan(start));
            REQUIRE(result.status == SymbolStatus::OK);
            REQUIRE(result.value.has_value());
            CHECK(result.consumed == number(c[3]) - number(c[2]));
            CHECK(result.value->units == units(c[4]));
            CHECK(result.value->eventFlags == number(c[5]));
            ++sequenceCount;
        } else if (c[0] == "read") {
            REQUIRE(c.size() == 12);
            INFO(c[1], " ", c[2]);
            REQUIRE(identities.insert(c[1] + "/" + c[2]).second);

            if (c[2] == "main") {
                REQUIRE(reader.beginMessage(0) == SymbolStatus::OK);

                if (c[3] != "-") {
                    REQUIRE(reader.read(bytes(c[3])).status == SymbolStatus::OK);
                }

                failed = false;
                ++mainCount;
            } else if (c[2] == "after-reset" || c[2] == "sentinel") {
                REQUIRE(reader.beginMessage(0) == SymbolStatus::OK);
                failed = false; // Sentinel reset is the explicit C++ terminal-policy projection.
            } else {
                REQUIRE((c[2] == "after-repeat" || c[2] == "after-mru"));
            }

            auto result = reader.read(bytes(c[4]));

            if (failed) {
                // Java post-error fields were validated above, but terminal C++ state is a deliberate projection.
                CHECK(result.status == SymbolStatus::FAILED);
                CHECK(result.consumed == 0);
                CHECK_FALSE(result.value.has_value());
                ++terminalCount;
            } else {
                CHECK(result.status == status(c[5]));
                CHECK(result.consumed == number(c[6]));

                if (result.status == SymbolStatus::OK) {
                    REQUIRE(result.value.has_value());
                    CHECK(result.value->cipher == number(c[7]));
                    CHECK(result.value->units == units(c[9]));
                    CHECK(result.value->eventFlags == number(c[10]));
                    CHECK(result.value->eventFlagsBytes == number(c[11]));
                } else {
                    CHECK_FALSE(result.value.has_value());
                    failed = true;
                }
            }

            ++readCount;
        } else if (c[0] == "complete") {
            REQUIRE(c.size() == 3);
            CHECK(c[1] == "symbols");
            CHECK(number(c[2]) == 156);
            complete = true;
        } else {
            FAIL("Validated row type has no explicit disposition");
        }
    }

    CHECK(file.eof());
    CHECK(complete);
    CHECK(deliveredCount == 7);
    CHECK(writerCount == 19);
    CHECK(sequenceCount == 114);
    CHECK(readCount == 3465);
    CHECK(mainCount == 693);
    CHECK(terminalCount > 500);
}

TEST_CASE("Complete payload boundaries, budgets, profiles and independent ownership") {
    SymbolReader reader(LIMITS, 0);
    auto saved = reader.read(bytes("fd03410042"));
    REQUIRE(saved.status == SymbolStatus::OK);
    REQUIRE(reader.read(bytes("f905ff")).status == SymbolStatus::OK);
    CHECK(reader.read(bytes("f8ff")).value->eventFlags == 5);
    CHECK(reader.read(bytes("ff")).value->eventFlags == 0);
    CHECK(reader.beginMessage(0) == SymbolStatus::OK);
    CHECK(reader.read(bytes("f8fe")).value->eventFlags == 1);
    CHECK(saved.value->units == std::u16string{u'A', 0, u'B'});
    CHECK(reader.beginMessage(1) == SymbolStatus::UNSUPPORTED_PROFILE);
    CHECK(reader.read(bytes("fe")).status == SymbolStatus::UNSUPPORTED_PROFILE);
    CHECK(reader.beginMessage(0) == SymbolStatus::OK);
    auto partial = reader.read(bytes("fd01ed"));
    CHECK(partial.status == SymbolStatus::TRUNCATED_INPUT);
    CHECK(partial.consumed == 3);
    CHECK(reader.read(bytes("a080")).status == SymbolStatus::FAILED);
    CHECK(reader.beginMessage(0) == SymbolStatus::OK);
    CHECK(reader.read(bytes("fd01eda080")).value->units == std::u16string(1, char16_t{0xd800}));

    for (const auto limits : {qd::encoding::SymbolLimits{1, 9, 9}, {9, 0, 9}, {9, 9, 0}}) {
        SymbolReader small(limits, 0);
        CHECK(small.read(bytes("0443")).status == SymbolStatus::RESOURCE_LIMIT);
        CHECK(small.read(bytes("fe")).status == SymbolStatus::FAILED);
        CHECK(small.beginMessage(0) == SymbolStatus::OK);
        CHECK(small.read(bytes("fe")).status == SymbolStatus::OK);
    }

    SymbolReader zero({0, 0, 0}, 0);
    CHECK(zero.read(bytes("fe")).consumed == 0);
    SymbolReader exact({5, 3, 3}, 0);
    CHECK(exact.read(bytes("fd03414243")).value->units == u"ABC");
    CHECK(exact.read(bytes("ff")).value->units == u"ABC");
    SymbolReader limited({9, 1, 1}, 0);
    CHECK(limited.read(bytes("fd024142")).consumed == 2);
    CHECK(limited.beginMessage(0) == SymbolStatus::OK);
    auto pair = limited.read(bytes("fc04f09f9880"));
    CHECK(pair.status == SymbolStatus::RESOURCE_LIMIT);
    CHECK(pair.consumed == 6);
    CHECK(limited.beginMessage(0) == SymbolStatus::OK);
    CHECK(limited.read(bytes("fc0a")).status == SymbolStatus::RESOURCE_LIMIT);
    CHECK(limited.beginMessage(0) == SymbolStatus::OK);
    CHECK(limited.read(bytes("fd7f")).status == SymbolStatus::INVALID_LENGTH);
    qd::encoding::SymbolResult owned;
    {
        SymbolReader temporary(LIMITS, 0);
        auto input = bytes("fd0141");
        owned = temporary.read(input);
        input.assign(input.size(), std::byte{});
        CHECK(temporary.beginMessage(0) == SymbolStatus::OK);
    }
    CHECK(owned.value->units == u"A");
}

/** Run doctest with an explicit immutable Java fixture path.
 * @param argc Argument count.
 * @param argv Fixture path followed by optional doctest arguments.
 * @return Nonzero on any assertion or missing fixture failure.
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
