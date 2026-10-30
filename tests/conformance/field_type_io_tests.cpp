// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "../../tools/tape-inspect/inspect.hpp"
#include "qd/events/quote.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace view = org::ttldtor::qdcxx::inspect;
namespace core = org::ttldtor::qdcxx::core;
namespace protocol = org::ttldtor::qdcxx::protocol;
std::filesystem::path fixtures; ///< Immutable Java expected-artifact directory supplied by CTest.
constexpr auto PROFILE = view::DisplayProfile::ORACLE_FIELD_TYPES_V1; ///< Explicit field-only presentation.

/** Read an exact binary Java artifact.
 * @param name Fixture basename, including extension.
 * @return Owned contents, with a test failure if the file cannot be read.
 */
std::string readFixture(std::string_view name) {
    std::ifstream input(fixtures / name, std::ios::binary);
    REQUIRE(input.good());

    return {std::istreambuf_iterator<char>(input), {}};
}

/** Find the completion grammar for a selected representation.
 * @param format Selected output representation.
 * @return Static completion prefix.
 */
std::string_view completion(view::OutputFormat format) {
    return format == view::OutputFormat::TSV ? "complete\t" : "Complete:";
}

/** Exact mixed tags containing null, empty, signed scalars and unsafe UTF-16.
 * @return Owned record for direct formatter-boundary checks.
 */
protocol::StreamDataRecord record() {
    protocol::StreamDataRecord result;
    result.localId = 0;
    result.symbol.units = u"IBM";
    result.typedValues = {{core::SerialFieldType::UTF_CHAR_ARRAY, 0},
                          {core::SerialFieldType::SHORT_STRING, -2147483648LL},
                          {core::SerialFieldType::SEQUENCE, 2147483647LL},
                          {core::SerialFieldType::DATE, -1},
                          {core::SerialFieldType::COMPACT_INT, 0},
                          {core::SerialFieldType::UTF_CHAR, 0x10ffff},
                          {core::SerialFieldType::TIME_SECONDS, -7},
                          {core::SerialFieldType::WIDE_DECIMAL, std::numeric_limits<std::int64_t>::min()},
                          {core::SerialFieldType::UTF_CHAR_ARRAY, 0, std::u16string{}}};

    return result;
}

/** Count exact line prefixes without treating escaped contents as rows.
 * @param text LF-delimited output.
 * @param prefix Prefix to count only at line starts.
 * @return Matching row count.
 */
std::size_t countRows(std::string_view text, std::string_view prefix) {
    std::size_t count = 0;

    while (!text.empty()) {
        count += text.starts_with(prefix) ? 1 : 0;
        const auto end = text.find('\n');

        if (end == std::string_view::npos) {
            break;
        }

        text.remove_prefix(end + 1);
    }

    return count;
}

/** Captures physically accepted bytes and detects operations after an injected failure. */
class FaultBuffer : public std::streambuf {
    std::size_t byteLimit_; ///< First rejected byte, or the maximum representable size.
    unsigned failedFlush_;  ///< One-based failing flush, zero for none.
    bool throwing_;         ///< Throw instead of returning a short count.
    bool failed_ = false;   ///< First failure was already delivered to the stream.

    public:
    std::string bytes;         ///< Accepted physical prefix, including a possible partial row.
    unsigned flushes = 0;      ///< Number of requested flushes.
    unsigned afterFailure = 0; ///< Buffer operations attempted after detecting the first failure.

    /** Configure a sink failure independently of the inspector's output budget.
     * @param byteLimit Number of bytes accepted before a short or throwing write.
     * @param failedFlush One-based failing flush, zero disables flush failure.
     * @param throwing Whether a write rejection throws ios_base::failure.
     */
    FaultBuffer(std::size_t byteLimit, unsigned failedFlush, bool throwing)
        : byteLimit_(byteLimit), failedFlush_(failedFlush), throwing_(throwing) {
    }

    protected:
    /** Deliberately fail any source read.
     * @return Never returns normally.
     */
    int_type underflow() override {
        throw std::ios_base::failure("injected input failure");
    }

    /** Accept a physical prefix, then fail the selected write.
     * @param data Borrowed output bytes.
     * @param count Requested byte count.
     * @return Actual accepted bytes, unless the configured error throws.
     */
    std::streamsize xsputn(const char *data, std::streamsize count) override {
        afterFailure += failed_ ? 1 : 0;
        const auto requested = static_cast<std::size_t>(count);
        const auto accepted = std::min(requested, byteLimit_ - bytes.size());
        bytes.append(data, accepted);

        if (accepted != requested) {
            failed_ = true;

            if (throwing_) {
                throw std::ios_base::failure("injected output failure");
            }
        }

        return static_cast<std::streamsize>(accepted);
    }

    /** Fail the first or final flush independently of writes.
     * @return Minus one for the selected failure; zero otherwise.
     */
    int sync() override {
        afterFailure += failed_ ? 1 : 0;
        ++flushes;

        if (flushes == failedFlush_) {
            failed_ = true;

            return -1;
        }

        return 0;
    }
};
} // namespace

TEST_CASE("Field-only formatter preserves exact identity null empty and scalar raw grammar") {
    const auto value = record();

    for (auto format : {view::OutputFormat::TSV, view::OutputFormat::HUMAN}) {
        view::Formatter formatter(format, PROFILE);
        const auto row = formatter.record(value, u"FieldTypes", 7, 3, 4096);
        CHECK(std::count(row.begin(), row.end(), '\n') == 1);

        if (format == view::OutputFormat::TSV) {
            CHECK(row == "field-record\t7\t3\t0\t004600690065006c006400540079007000650073\t00490042004d\t"
                         "10:0:null,40:-2147483648:-,72:2147483647:-,88:-1:-,8:0:-,2:1114111:-,"
                         "56:-7:-,120:-9223372036854775808:-,10:0:empty\n");
            CHECK(std::count(row.begin(), row.end(), '\t') == 6);
        } else {
            CHECK(row == "Fields 3: frame=7 local=0 name=\"FieldTypes\" symbol=\"IBM\" fields={"
                         "field[0]=(type:10 raw:0 units:null), field[1]=(type:40 raw:-2147483648), "
                         "field[2]=(type:72 raw:2147483647), field[3]=(type:88 raw:-1), "
                         "field[4]=(type:8 raw:0), field[5]=(type:2 raw:1114111), "
                         "field[6]=(type:56 raw:-7), field[7]=(type:120 raw:-9223372036854775808), "
                         "field[8]=(type:10 raw:0 units:\"\")}\n");
        }

        for (std::size_t budget = 0; budget < row.size(); ++budget) {
            CAPTURE(budget);
            CHECK_THROWS_AS(formatter.record(value, u"FieldTypes", 7, 3, budget), view::OutputLimit);
        }

        CHECK(formatter.record(value, u"FieldTypes", 7, 3, row.size()) == row);
    }
}

TEST_CASE("Field-only formatter escapes controls and lone surrogates before writing a bounded row") {
    auto value = record();
    const std::u16string unsafe{u'"',
                                u'\\',
                                u'\n',
                                u'\t',
                                u'\r',
                                u'\0',
                                u'\x1b',
                                u'\x202e',
                                char16_t(0xd800),
                                char16_t(0xdc00),
                                char16_t(0xd800)};
    value.typedValues.front().units = unsafe;
    value.symbol.units = unsafe;

    for (auto format : {view::OutputFormat::TSV, view::OutputFormat::HUMAN}) {
        view::Formatter formatter(format, PROFILE);
        const auto row = formatter.record(value, unsafe, 0, 0, 8192);
        CHECK(std::count(row.begin(), row.end(), '\n') == 1);
        CHECK(row.find('\r') == std::string::npos);
        CHECK(row.find('\0') == std::string::npos);
        CHECK(row.find('\x1b') == std::string::npos);
        CHECK(std::count(row.begin(), row.end(), '\t') == (format == view::OutputFormat::TSV ? 6 : 0));

        if (format == view::OutputFormat::TSV) {
            CHECK(row.find("10:0:0022005c000a0009000d0000001b202ed800dc00d800") != std::string::npos);
        } else {
            CHECK(row.find("\\uD800") != std::string::npos);
            CHECK(row.find("\\u202E") != std::string::npos);
            CHECK(row.find("\\n") != std::string::npos);
            CHECK(row.find("\\t") != std::string::npos);
        }

        for (std::size_t budget = 0; budget < row.size(); ++budget) {
            CHECK_THROWS_AS(formatter.record(value, unsafe, 0, 0, budget), view::OutputLimit);
        }

        CHECK(formatter.record(value, unsafe, 0, 0, row.size()) == row);
    }
}

TEST_CASE("Typed formatter refuses invalid manually constructed tags ranges and payloads") {
    std::vector<core::FieldValue> invalid{{static_cast<core::SerialFieldType>(25), 0},
                                          {core::SerialFieldType::UTF_CHAR_ARRAY, 1},
                                          {core::SerialFieldType::UTF_CHAR_ARRAY, -1, std::u16string{}},
                                          {core::SerialFieldType::UTF_CHAR, -1},
                                          {core::SerialFieldType::UTF_CHAR, 0x110000}};

    for (auto type :
         {core::SerialFieldType::COMPACT_INT, core::SerialFieldType::SHORT_STRING, core::SerialFieldType::SEQUENCE,
          core::SerialFieldType::DATE, core::SerialFieldType::TIME_SECONDS}) {
        invalid.push_back({type, 2147483648LL});
        invalid.push_back({type, -2147483649LL});
    }

    for (auto type : {core::SerialFieldType::UTF_CHAR, core::SerialFieldType::COMPACT_INT,
                      core::SerialFieldType::SHORT_STRING, core::SerialFieldType::SEQUENCE, core::SerialFieldType::DATE,
                      core::SerialFieldType::TIME_SECONDS, core::SerialFieldType::WIDE_DECIMAL}) {
        invalid.push_back({type, 0, std::u16string{}});
        invalid.push_back({type, 0, std::u16string{u'x'}});
    }

    for (const auto &field : invalid) {
        CAPTURE(static_cast<std::int32_t>(field.type));
        CAPTURE(field.raw);
        CHECK_FALSE(core::validFieldValue(field));
        auto value = record();
        value.typedValues.front() = field;

        for (auto format : {view::OutputFormat::TSV, view::OutputFormat::HUMAN}) {
            view::Formatter formatter(format, PROFILE);
            CHECK_THROWS_AS(formatter.record(value, u"FieldTypes", 0, 0, 8192), std::invalid_argument);
        }
    }
}

TEST_CASE("Field inspection checks every output-byte cutoff before writing the next whole row") {
    const auto schema = ti::oracleFieldTypesSchema(ti::Limits{}.tape.context.schema);
    REQUIRE(schema);

    for (bool raw : {false, true}) {
        const auto mode = raw ? protocol::TapeMode::RAW_MESSAGE_V1 : protocol::TapeMode::LEGACY;
        const auto bytes = readFixture(raw ? "mixed-raw.qtp" : "mixed-stream.qtp");

        for (auto format : {view::OutputFormat::TSV, view::OutputFormat::HUMAN}) {
            std::istringstream input(bytes);
            std::ostringstream output, diagnostic;
            REQUIRE(ti::inspect(input, output, diagnostic, *schema, {}, format, PROFILE, 2, mode) == 0);
            REQUIRE(diagnostic.str().empty());
            const auto baseline = output.str();
            REQUIRE(countRows(baseline, completion(format)) == 1);
            CHECK(countRows(baseline, format == view::OutputFormat::TSV ? "field-record\t" : "Fields ") == 3);
            const auto modeName = raw ? "raw-message-v1" : "legacy";
            const auto header =
                format == view::OutputFormat::TSV
                    ? std::string("format\tqdcxx-tape-inspect\t4\toracle-field-types-v1\t") + modeName + "\n"
                    : std::string("qdcxx-tape-inspect human 4 profile=oracle-field-types-v1 tape-mode=") + modeName +
                          " identities=local\n";
            CHECK(baseline.starts_with(header));

            for (std::size_t bound = 0; bound <= baseline.size() + 1; ++bound) {
                CAPTURE(raw);
                CAPTURE(static_cast<int>(format));
                CAPTURE(bound);
                ti::Limits limits;
                limits.output = bound;
                std::istringstream again(bytes);
                std::ostringstream actual, errors;
                const auto code = ti::inspect(again, actual, errors, *schema, limits, format, PROFILE, 2, mode);

                if (bound >= baseline.size()) {
                    CHECK(code == 0);
                    CHECK(actual.str() == baseline);
                    CHECK(errors.str().empty());
                } else {
                    CHECK(code == 6);
                    const auto partial = baseline.substr(0, bound);
                    const auto newline = partial.rfind('\n');
                    const auto prefix = newline == std::string::npos ? std::string{} : partial.substr(0, newline + 1);
                    CHECK(actual.str() == prefix);
                    CHECK(countRows(actual.str(), completion(format)) == 0);
                    CHECK(errors.str().starts_with("incomplete-v2\t6\twrite\t"));
                }
            }
        }
    }
}

TEST_CASE("Field string budgets reach inspection without weakening scalar or completed-prefix behavior") {
    const auto schema = ti::oracleFieldTypesSchema(ti::Limits{}.tape.context.schema);
    REQUIRE(schema);

    for (bool raw : {false, true}) {
        const auto mode = raw ? protocol::TapeMode::RAW_MESSAGE_V1 : protocol::TapeMode::LEGACY;
        const auto bytes = readFixture(raw ? "mixed-raw.qtp" : "mixed-stream.qtp");

        for (unsigned bound = 0; bound < 5; ++bound) {
            ti::Limits limits;

            switch (bound) {
            case 0:
                limits.tape.data.fieldString.encodedBytes = 0;
                break;
            case 1:
                limits.tape.data.fieldString.codeUnits = 0;
                break;
            case 2:
                limits.tape.data.fieldBytes = 0;
                break;
            case 3:
                limits.tape.data.fieldUnits = 0;
                break;
            case 4:
                limits.tape.data.ownedFieldUnits = 0;
                break;
            }

            std::istringstream input(bytes);
            std::ostringstream output, diagnostic;
            CHECK(ti::inspect(input, output, diagnostic, *schema, limits, view::OutputFormat::TSV, PROFILE, 2, mode) ==
                  6);
            CHECK(diagnostic.str().starts_with("incomplete-v2\t6\tdata\t"));
            CHECK(countRows(output.str(), "field-record\t") == 0);
            CHECK(countRows(output.str(), "complete\t") == 0);
        }

        ti::Limits limits;
        limits.tape.data.fieldUnits = 4; // The first tail owns four units; the third record needs six more.
        std::istringstream input(bytes);
        std::ostringstream output, diagnostic;
        CHECK(ti::inspect(input, output, diagnostic, *schema, limits, view::OutputFormat::TSV, PROFILE, 2, mode) == 6);
        CHECK(countRows(output.str(), "field-record\t") == 2);
        CHECK(countRows(output.str(), "complete\t") == 0);
    }
}

TEST_CASE("Field IO failures cannot report success or append completion after failure detection") {
    const auto schema = ti::oracleFieldTypesSchema(ti::Limits{}.tape.context.schema);
    REQUIRE(schema);
    constexpr auto unlimited = std::numeric_limits<std::size_t>::max();

    for (bool raw : {false, true}) {
        const auto mode = raw ? protocol::TapeMode::RAW_MESSAGE_V1 : protocol::TapeMode::LEGACY;
        const auto bytes = readFixture(raw ? "mixed-raw.qtp" : "mixed-stream.qtp");

        for (auto format : {view::OutputFormat::TSV, view::OutputFormat::HUMAN}) {
            std::istringstream baselineInput(bytes);
            std::ostringstream baselineOutput, baselineErrors;
            REQUIRE(ti::inspect(baselineInput, baselineOutput, baselineErrors, *schema, {}, format, PROFILE, 2, mode) ==
                    0);
            const auto baseline = baselineOutput.str();
            const auto firstRecord = baseline.find(format == view::OutputFormat::TSV ? "field-record\t" : "Fields ");
            REQUIRE(firstRecord != std::string::npos);

            for (unsigned operation = 0; operation < 5; ++operation) {
                CAPTURE(operation);
                const bool readFailure = operation == 0;
                const bool writeFailure = operation == 1 || operation == 2;
                FaultBuffer fault(writeFailure ? firstRecord + 5 : unlimited,
                                  operation == 3   ? 1
                                  : operation == 4 ? 2
                                                   : 0,
                                  operation == 2);
                std::istream failedInput(&fault);
                std::ostream failedOutput(&fault);
                failedOutput.exceptions(std::ios::badbit | std::ios::failbit);
                std::istringstream input(bytes);
                std::ostringstream output, diagnostic;
                CHECK(ti::inspect(readFailure ? failedInput : input, readFailure ? output : failedOutput, diagnostic,
                                  *schema, {}, format, PROFILE, 2, mode) == 3);
                CHECK(diagnostic.str().starts_with("incomplete-v2\t3\t"));
                CHECK(fault.afterFailure == 0);
                const auto physical = readFailure ? output.str() : fault.bytes;

                if (operation == 4) {
                    CHECK(physical == baseline);
                    CHECK(countRows(physical, completion(format)) == 1);
                    CHECK(fault.flushes == 2);
                } else {
                    CHECK(countRows(physical, completion(format)) == 0);

                    if (writeFailure) {
                        CHECK(physical == baseline.substr(0, firstRecord + 5));
                    }
                }
            }
        }
    }
}

TEST_CASE("Field display and schema mismatches fail before any input or output work") {
    const auto limits = ti::Limits{}.tape.context.schema;
    const auto fields = ti::oracleFieldTypesSchema(limits);
    const auto legacy = core::LocalSchema::create({}, limits);
    const auto quote = org::ttldtor::qdcxx::events::defaultQuoteSchema(limits);
    REQUIRE(fields);
    REQUIRE(legacy);
    REQUIRE(quote);
    const std::array cases{std::pair{&*legacy, PROFILE}, std::pair{&*quote, PROFILE},
                           std::pair{&*fields, view::DisplayProfile::LOCAL_SCHEMA},
                           std::pair{&*fields, view::DisplayProfile::DEFAULT_QUOTE_V1}};

    for (const auto &[schema, profile] : cases) {
        std::istringstream input("not a tape");
        std::ostringstream output, diagnostic;
        CHECK(ti::inspect(input, output, diagnostic, *schema, {}, view::OutputFormat::TSV, profile, 2) == 4);
        CHECK(input.tellg() == 0);
        CHECK(output.str().empty());
        CHECK(diagnostic.str() == "incomplete-v2\t4\tprofile\t0\t-\t-\t-\t-\t-\tLOCAL_PROFILE\n");
    }
}

/** Run injected IO and exact field-output checks against a supplied immutable fixture root.
 * @param argc Argument count.
 * @param argv Executable, expected-artifact directory, then optional doctest arguments.
 * @return Doctest result or usage failure.
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
