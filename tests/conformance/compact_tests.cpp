#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using qd::encoding::Status;

/// Fixture directory supplied by main before doctest executes any test case.
std::filesystem::path fixtureDirectory;

/** Require a condition through doctest in every configuration, including Release.
 * @param condition Predicate which must hold for this assertion.
 * @param message Failure diagnostic, including expected/actual details where applicable.
 * @throws doctest::detail::TestFailureException When the fatal assertion fails.
 */
void require(bool condition, const std::string &message) {
    REQUIRE_MESSAGE(condition, message);
}

/** Parse a whole decimal field without rounding or accepting trailing characters.
 * @tparam T Integral destination type; overflow is rejected.
 * @param text Complete decimal fixture field.
 * @return The exact parsed integer; malformed input fails the active doctest case.
 */
template <typename T> T decimal(const std::string &text) {
    T value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    require(result.ec == std::errc{} && result.ptr == text.data() + text.size(), "invalid decimal: " + text);

    return value;
}

/** Split TSV while retaining empty inputs and a possible final empty field.
 * @param line One TSV line without its line ending.
 * @return Ordered field strings, including empty fields.
 */
std::vector<std::string> split(const std::string &line) {
    std::vector<std::string> fields;
    std::size_t start = 0;

    for (;;) {
        const auto end = line.find('\t', start);
        fields.push_back(line.substr(start, end == std::string::npos ? end : end - start));

        if (end == std::string::npos) {
            return fields;
        }

        start = end + 1;
    }
}

/** Decode fixture hex only; this is not a second compact codec.
 * @param text Hexadecimal fixture bytes; an empty string represents empty input.
 * @return Decoded bytes; malformed hex fails the active doctest case.
 */
std::vector<std::byte> unhex(const std::string &text) {
    require(text.size() % 2 == 0, "odd hex length");
    std::vector<std::byte> bytes;

    for (std::size_t index = 0; index < text.size(); index += 2) {
        unsigned int value{};
        const auto result = std::from_chars(text.data() + index, text.data() + index + 2, value, 16);
        require(result.ec == std::errc{} && result.ptr == text.data() + index + 2, "invalid hex: " + text);
        bytes.push_back(static_cast<std::byte>(value));
    }

    return bytes;
}

/** Render actual output bytes in the same exact representation as the oracle.
 * @param bytes Borrowed bytes to render, including an empty span.
 * @return Lowercase hex with exactly two characters per byte.
 */
std::string hex(std::span<const std::byte> bytes) {
    constexpr char DIGITS[] = "0123456789abcdef";
    std::string text;

    for (const auto byte : bytes) {
        const auto value = std::to_integer<unsigned int>(byte);
        text += DIGITS[value >> 4];
        text += DIGITS[value & 15];
    }

    return text;
}

/// Explicit corpus budgets, larger than every captured body and UTF-16 value.
constexpr qd::encoding::UtfStringLimits CORPUS_LIMITS{1024, 1024};

/** Parse exact UTF-16 hex, preserving null, empty and isolated surrogates.
 * @param text Oracle field containing four hex digits per unit or the null marker.
 * @return Owned units, or disengaged for Java null; malformed fields fail the test.
 */
std::optional<std::u16string> parseUnits(const std::string &text) {
    if (text == "\\N") {
        return std::nullopt;
    }

    require(text.size() % 4 == 0, "invalid UTF-16 hex length");
    const auto bytes = unhex(text);
    std::u16string units;

    for (std::size_t index = 0; index < bytes.size(); index += 2) {
        units.push_back(static_cast<char16_t>((std::to_integer<unsigned int>(bytes[index]) << 8) |
                                              std::to_integer<unsigned int>(bytes[index + 1])));
    }

    return units;
}

/** Render an owned result for exact oracle diagnostics.
 * @param value Optional UTF-16 units; disengaged means null.
 * @return Oracle UTF-16 hex or null marker.
 */
std::string unitsHex(const std::optional<std::u16string> &value) {
    if (!value) {
        return "\\N";
    }

    std::vector<std::byte> bytes;

    for (const auto unit : *value) {
        bytes.push_back(static_cast<std::byte>(unit >> 8));
        bytes.push_back(static_cast<std::byte>(unit & 255));
    }

    return hex(bytes);
}

/** Compare UTFString reader category, exact units/null and Java consumed/remaining bytes.
 * @param row Selected unchanged oracle observation; malformed fields fail the test.
 */
void checkUtfReader(const std::vector<std::string> &row) {
    require(row.size() >= 5, "missing reader status");
    const bool success = row[4] == "ok";
    require(success || row[4] == "error", "unexpected status: " + row[4]);
    require(row.size() == (success ? 8 : 9), "invalid reader field count");
    const auto input = unhex(row[3]);
    const auto consumed = decimal<std::size_t>(row[6]);
    const auto remaining = decimal<std::size_t>(row[7]);
    require(consumed <= input.size() && remaining == input.size() - consumed, "inconsistent fixture position");
    auto status = Status::OK;

    if (!success) {
        if (row[5] == "java.io.EOFException") {
            status = Status::INSUFFICIENT_INPUT;
        } else if (row[5] == "java.io.UTFDataFormatException") {
            status = Status::INVALID_ENCODING;
        } else {
            require(row[5] == "java.io.IOException", "unexpected exception category: " + row[5]);
            status = Status::INVALID_LENGTH;
        }
    }

    const auto expected = success ? parseUnits(row[5]) : std::nullopt;
    const auto result = qd::encoding::readUtfString(input, CORPUS_LIMITS);
    require(result.status == status, "UTF status expected " + row[4] + "/" + row[5]);
    require(result.value == expected,
            "value expected " + (success ? row[5] : "\\N") + " actual " + unitsHex(result.value));
    require(result.consumed == consumed, "consumed expected " + row[6] + " actual " + std::to_string(result.consumed));
    require(result.consumed <= input.size() && input.size() - result.consumed == remaining, "UTF remaining bytes");
}

/** Compare canonical UTFString bytes at every capacity with canaries and atomic failures.
 * @param row Selected writer observation with exact raw UTF-16 input and Java output.
 */
void checkUtfWriter(const std::vector<std::string> &row) {
    require(row.size() == 5, "invalid writer field count");
    const auto expected = unhex(row[4]);
    require(!expected.empty(), "empty writer bytes");
    const auto units = parseUnits(row[3]);
    const auto view = units ? std::optional<std::u16string_view>(*units) : std::nullopt;

    for (std::size_t capacity = 0; capacity <= expected.size() + 2; ++capacity) {
        std::vector<std::byte> storage(capacity + 2, std::byte{0x5a});
        auto output = std::span(storage).subspan(1, capacity);
        const auto result = qd::encoding::writeUtfString(view, output, CORPUS_LIMITS);
        const bool fits = capacity >= expected.size();
        require(result.status == (fits ? Status::OK : Status::INSUFFICIENT_OUTPUT), "UTF writer capacity status");
        require(result.written == (fits ? expected.size() : 0), "UTF writer byte count");

        if (fits) {
            require(hex(output.first(result.written)) == row[4],
                    "bytes expected " + row[4] + " actual " + hex(output.first(result.written)));
        }

        require(storage.front() == std::byte{0x5a}, "UTF leading guard");

        for (std::size_t index = 1 + result.written; index < storage.size(); ++index) {
            require(storage[index] == std::byte{0x5a}, "UTF changed bytes beyond written");
        }
    }
}

/** Compare reader value/category and position, then check trailing sentinel isolation.
 * @param row Selected reader TSV fields; malformed rows fail the active doctest case.
 */
void checkReader(const std::vector<std::string> &row) {
    require(row.size() >= 5, "missing reader status");
    const bool success = row[4] == "ok";
    require(success || row[4] == "error", "unexpected status: " + row[4]);
    require(row.size() == (success ? 8 : 9), "invalid reader field count");
    auto input = unhex(row[3]);
    const auto expectedConsumed = decimal<std::size_t>(row[6]);
    const auto expectedRemaining = decimal<std::size_t>(row[7]);
    require(expectedConsumed <= input.size() && expectedRemaining == input.size() - expectedConsumed,
            "inconsistent fixture position");
    const auto expectedValue = success ? decimal<std::int64_t>(row[5]) : 0;
    require(success || row[5] == "java.io.EOFException", "unexpected exception category: " + row[5]);
    const auto read = [&row](std::span<const std::byte> bytes) {
        if (row[2] == "readCompactInt") {
            const auto result = qd::encoding::readCompactInt(bytes);

            return qd::encoding::ReadResult<std::int64_t>{result.value, result.consumed, result.status};
        }

        return qd::encoding::readCompactLong(bytes);
    };
    const auto result = read(input);
    require(result.status == (success ? Status::OK : Status::INSUFFICIENT_INPUT),
            "status expected " + row[4] + " actual " + std::to_string(static_cast<int>(result.status)));
    require(result.value == expectedValue,
            "value expected " + std::to_string(expectedValue) + " actual " + std::to_string(result.value));
    require(result.consumed == expectedConsumed,
            "consumed expected " + row[6] + " actual " + std::to_string(result.consumed));
    require(result.consumed <= input.size() && input.size() - result.consumed == expectedRemaining,
            "remaining expected " + row[7] + " actual " + std::to_string(input.size() - result.consumed));

    if (success) {
        input.push_back(std::byte{0x25});
        const auto followed = read(input);
        require(followed.status == Status::OK && followed.value == result.value && followed.consumed == result.consumed,
                "sentinel changed read result");
        require(input[followed.consumed] == std::byte{0x25}, "sentinel consumed");
    }
}

/** Compare every writer with Java bytes and all undersized/exact/oversized output spans.
 * @param row Selected writer TSV fields with an exact decimal value and golden bytes.
 */
void checkWriter(const std::vector<std::string> &row) {
    require(row.size() == 5, "invalid writer field count");
    const auto expected = unhex(row[4]);
    require(!expected.empty() && expected.size() <= 9, "invalid expected writer length");
    const auto value = decimal<std::int64_t>(row[3]);

    if (row[2] == "writeCompactInt") {
        require(value >= std::numeric_limits<std::int32_t>::min() && value <= std::numeric_limits<std::int32_t>::max(),
                "int input outside range");
    }

    for (std::size_t capacity = 0; capacity <= 10; ++capacity) {
        std::array<std::byte, 12> storage;
        storage.fill(std::byte{0x5a});
        const auto output = std::span(storage).subspan(1, capacity);
        const auto result = row[2] == "writeCompactInt"
                                ? qd::encoding::writeCompactInt(static_cast<std::int32_t>(value), output)
                                : qd::encoding::writeCompactLong(value, output);
        const bool fits = capacity >= expected.size();
        require(result.status == (fits ? Status::OK : Status::INSUFFICIENT_OUTPUT), "writer capacity status");
        require(result.written == (fits ? expected.size() : 0), "writer byte count");

        if (fits) {
            require(hex(output.first(result.written)) == row[4],
                    "bytes expected " + row[4] + " actual " + hex(output.first(result.written)));
        }

        require(storage.front() == std::byte{0x5a}, "leading guard overwritten");

        for (std::size_t index = 1 + result.written; index < storage.size(); ++index) {
            require(storage[index] == std::byte{0x5a}, "output changed outside written range");
        }
    }
}

/** Run one unchanged TSV; reject unknown methods, malformed rows and empty selected coverage.
 * @param path Read-only Java fixture file to execute; missing or malformed files fail the test.
 */
void checkFile(const std::filesystem::path &path) {
    std::ifstream stream(path);
    require(stream.is_open(), "cannot open " + path.string());
    std::string line;
    std::size_t writers = 0;
    std::size_t readers = 0;
    std::size_t excluded = 0;
    bool complete = false;
    bool profile = false;
    std::set<std::string> identities;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const auto row = split(line);
        require(!complete, "rows after complete marker");

        if (row[0] == "profile") {
            require(!profile && identities.empty() && row.size() == 5 && row[1] == "QD-3.355", "invalid profile");
            profile = true;
            continue;
        }

        if (row[0] == "complete") {
            require(row.size() == 2 && row[1] == path.stem().string(), "invalid complete marker");
            complete = true;
            continue;
        }

        require(profile && row.size() >= 3 && !row[1].empty(), "malformed observation");
        const auto identity = row[0] + "/" + row[1] + "/" + row[2];
        require(identities.insert(identity).second, "duplicate " + identity);
        INFO(path.filename().string(), ": ", identity);
        {
            if (row[0] == "reader" && (row[2] == "readCompactInt" || row[2] == "readCompactLong")) {
                checkReader(row);
                ++readers;
            } else if (row[0] == "writer" && (row[2] == "writeCompactInt" || row[2] == "writeCompactLong")) {
                checkWriter(row);
                ++writers;
            } else if (row[0] == "reader" && row[2] == "readUTFString") {
                checkUtfReader(row);
                ++readers;
            } else if (row[0] == "writer" && row[2] == "writeUTFString") {
                checkUtfWriter(row);
                ++writers;
            } else {
                require(false, "unknown kind/method");
            }

            std::cout << "PASS\t" << path.filename().string() << '\t' << identity << '\n';
        }
    }

    require(stream.eof() && profile && complete && readers > 0, "incomplete/empty selected coverage");
    require(path.stem() == "forms" || writers > 0, "empty writer coverage");
    std::cout << "TOTAL\t" << path.filename().string() << "\twriters=" << writers << "\treaders=" << readers
              << "\texcluded=" << excluded << '\n';
}

TEST_CASE("Java compact canonical and cross-width vectors") {
    checkFile(fixtureDirectory / "integers.tsv");
}

TEST_CASE("Java compact overlong forms and prefixes") {
    checkFile(fixtureDirectory / "forms.tsv");
}

TEST_CASE("Java UTFString canonical vectors and strict prefixes") {
    checkFile(fixtureDirectory / "strings.tsv");
}

TEST_CASE("UTFString explicit budgets and precedence") {
    using qd::encoding::readUtfString;
    using qd::encoding::UtfStringLimits;
    using qd::encoding::writeUtfString;
    /** Check one local resource-policy outcome without manufacturing a Java observation.
     * @param bytes Hex input, including malformed or incomplete forms.
     * @param limits Explicit byte/unit budgets for this operation.
     * @param status Expected C++ operational or wire outcome.
     * @param consumed Expected number of examined bytes.
     */
    const auto check = [](const std::string &bytes, UtfStringLimits limits, Status status, std::size_t consumed) {
        const auto result = readUtfString(unhex(bytes), limits);
        CHECK(result.status == status);
        CHECK(result.consumed == consumed);

        if (status != Status::OK) {
            CHECK(!result.value);
        }
    };
    check("", {0, 0}, Status::INSUFFICIENT_INPUT, 0);
    check("ff0000", {0, 0}, Status::INSUFFICIENT_INPUT, 3);
    check("7e", {0, 0}, Status::INVALID_LENGTH, 1);
    check("7f", {0, 0}, Status::OK, 1);
    check("00", {0, 0}, Status::OK, 1);
    check("02", {1, 0}, Status::RESOURCE_LIMIT, 1);
    check("02", {2, 0}, Status::INSUFFICIENT_INPUT, 1);
    check("024142", {2, 1}, Status::RESOURCE_LIMIT, 3);
    check("04f09f9880", {4, 1}, Status::RESOURCE_LIMIT, 5);
    check("04f09f9880", {4, 2}, Status::OK, 5);
    check("03eda080", {3, 1}, Status::OK, 4);
    check("03e241", {3, 0}, Status::INSUFFICIENT_INPUT, 3);
    check("03e24180", {3, 0}, Status::INVALID_ENCODING, 4);
    check("01c280", {1, 1}, Status::INVALID_ENCODING, 3);
    check("01c280", {1, 0}, Status::RESOURCE_LIMIT, 3);
    check("01c2", {1, 1}, Status::INSUFFICIENT_INPUT, 2);
    check("014142", {1, 1}, Status::OK, 2);
    std::array<std::byte, 9> prefix{};

    for (const auto length : {8589934588LL, 8589934589LL, std::numeric_limits<long long>::max()}) {
        const auto written = qd::encoding::writeCompactLong(length, prefix).written;
        const auto input = std::span(prefix).first(written);
        const auto result = readUtfString(input, {0, 0});
        CHECK(result.status == (length == 8589934588LL ? Status::RESOURCE_LIMIT : Status::INVALID_LENGTH));
        CHECK(result.consumed == written);
        CHECK(!result.value);
    }

    const auto maximum = std::numeric_limits<std::size_t>::max();
    check("f1fffffffc", {maximum, maximum},
          sizeof(std::size_t) >= 8 ? Status::INSUFFICIENT_INPUT : Status::RESOURCE_LIMIT, 5);
    std::array<std::byte, 16> output;
    output.fill(std::byte{0x5a});
    const auto original = output;

    for (const auto limits : {UtfStringLimits{0, 1}, UtfStringLimits{3, 0}}) {
        const auto result = writeUtfString(u"\ud7ff", output, limits);
        CHECK(result.status == Status::RESOURCE_LIMIT);
        CHECK(result.written == 0);
        CHECK(output == original);
    }

    CHECK(writeUtfString(std::nullopt, output, {0, 0}).status == Status::OK);
    CHECK(writeUtfString(u"", output, {0, 0}).status == Status::OK);
    CHECK(writeUtfString(u"A", output, {maximum, maximum}).status == Status::OK);
    auto input = unhex("0141");
    const auto owned = readUtfString(input, {1, 1});
    input[1] = std::byte{0};
    CHECK(owned.value == std::optional<std::u16string>(u"A"));
}
} // namespace

/** Execute the Java-vector and resource-policy doctest cases with a caller-selected fixture directory.
 * @param argc Argument count, including executable, fixture directory and optional doctest flags.
 * @param argv Argument strings valid throughout this call; argv[1] identifies the fixture directory.
 * @return Zero if all selected tests pass, nonzero on usage or conformance failure.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: compact_tests <fixture directory> [doctest options]\n";

        return 1;
    }

    fixtureDirectory = argv[1];
    doctest::Context context(argc - 1, argv + 1);
    context.setOption("no-colors", true);
    context.setOption("no-breaks", true);

    return context.run();
}
