// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../tools/tape-inspect/diagnostics.hpp"
#include "../../tools/tape-inspect/inspect.hpp"
#include "qd/events/quote.hpp"
#include <algorithm>
#include <doctest/doctest.h>
#include <sstream>
#include <streambuf>

namespace {
namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace presentation = org::ttldtor::qdcxx::inspect;
namespace core = org::ttldtor::qdcxx::core;
namespace protocol = org::ttldtor::qdcxx::protocol;
namespace events = org::ttldtor::qdcxx::events;

/** Finite explicit schema limits for isolated presentation checks. */
constexpr core::SchemaLimits SCHEMA_LIMITS{1, 8, 32, 64, 2048};

/** IO fault at a selected operation; no filesystem dependency. */
class FaultBuffer : public std::streambuf {
    int operation_; ///< Zero read, one write, two flush.
    public:
    /** Select the failing operation.
     * @param operation Zero read, one write, two flush.
     */
    explicit FaultBuffer(int operation) : operation_(operation) {
    }

    protected:
    /** Inject read failure.
     * @return Never returns normally.
     */
    int_type underflow() override {
        throw std::ios_base::failure("injected read");
    }

    /** Inject write failure or accept bytes.
     * @param data Borrowed output bytes.
     * @param count Requested count.
     * @return Zero for write failure, otherwise count.
     */
    std::streamsize xsputn(const char *data, std::streamsize count) override {
        static_cast<void>(data);

        return operation_ == 1 ? 0 : count;
    }

    /** Inject flush failure.
     * @return Minus one for flush failure, otherwise zero.
     */
    int sync() override {
        return operation_ == 2 ? -1 : 0;
    }
};

/** Build an accepted minimal tape header without version/send entries.
 * @return Exact binary bytes from the descriptor grammar.
 */
std::string header() {
    const unsigned char bytes[]{23, 1,   68, 88,  80,  51, 2,   4,   116, 121, 112, 101,
                                4,  116, 97, 112, 101, 3,  111, 112, 116, 0,   0,   0};

    return {reinterpret_cast<const char *>(bytes), sizeof(bytes)};
}

/** Create a local record with unsafe exact symbols and decimal special values.
 * @return Owned tagged Quote with supplementary raw exchange and an isolated surrogate exchange.
 */
protocol::StreamDataRecord record() {
    protocol::StreamDataRecord value;
    value.localId = 0;
    value.symbol.units = std::u16string{u'"', u'\\', u'\n', u'\t', u'\x1b', u'\x202e', char16_t(0xd800)};

    for (const auto type : events::QUOTE_FIELD_TYPES) {
        value.typedValues.push_back({type, 0});
    }

    value.typedValues[0].raw = -2147483648LL;
    value.typedValues[1].raw = 0x1f600;
    value.typedValues[3].raw = 256;
    value.typedValues[4].raw = 2147483647;
    value.typedValues[5].raw = 0xd800;
    value.typedValues[6].raw = -256;
    value.typedValues[7].raw = 128;

    return value;
}
} // namespace

TEST_CASE("Quote output preserves exact tags and values with canonical views and bounded safe escaping") {
    const auto value = record();

    for (const auto format : {presentation::OutputFormat::TSV, presentation::OutputFormat::HUMAN}) {
        presentation::Formatter formatter(format, presentation::DisplayProfile::DEFAULT_QUOTE_V1);
        const auto row = formatter.record(value, u"Quote", 7, 3, 4096);
        CHECK(std::count(row.begin(), row.end(), '\n') == 2);
        CHECK(row.find('\x1b') == std::string::npos);
        CHECK(row.find("7ff8000000000000") != std::string::npos);
        CHECK(row.find("7ff0000000000000") != std::string::npos);
        CHECK(row.find("fff0000000000000") != std::string::npos);
        CHECK(row.find("0000000000000000") != std::string::npos);
        CHECK(formatter.record(value, u"Quote", 7, 3, row.size()) == row);
        CHECK_THROWS_AS(formatter.record(value, u"Quote", 7, 3, row.size() - 1), presentation::OutputLimit);

        if (format == presentation::OutputFormat::TSV) {
            CHECK(row.find("56:-2147483648,2:128512,120:0,120:256,56:2147483647,2:55296,120:-256,120:128") !=
                  std::string::npos);
            CHECK(row.starts_with("typed-record\t7\t3\t0\t00510075006f00740065\t"));
        } else {
            CHECK(row.find("\\uD800") != std::string::npos);
            CHECK(row.find("Bid.Time=-2147483648000 ms") != std::string::npos);
            CHECK(row.find("Bid.Price=NaN (type:120 raw:0)") != std::string::npos);
            CHECK(row.find("Ask.Size=0 (type:120 raw:128)") != std::string::npos);
        }
    }
}

TEST_CASE("Quote IO failures preserve nonzero exits and selected diagnostic grammar") {
    const auto schema = events::defaultQuoteSchema(SCHEMA_LIMITS);
    REQUIRE(schema);

    for (int mode = 0; mode < 3; ++mode) {
        FaultBuffer bad(mode);
        std::istream failedInput(&bad);
        std::ostream failedOutput(&bad);
        std::istringstream goodInput(header());
        std::ostringstream goodOutput, diagnostic;
        const auto code =
            ti::inspect(mode == 0 ? failedInput : goodInput, mode == 0 ? goodOutput : failedOutput, diagnostic, *schema,
                        {}, presentation::OutputFormat::TSV, presentation::DisplayProfile::DEFAULT_QUOTE_V1, 2);
        CHECK(code == 3);
        CHECK(diagnostic.str().starts_with("incomplete-v2\t3\t"));
        CHECK(diagnostic.str().ends_with("\tNONE\n"));
        const auto text = diagnostic.str();
        CHECK(std::count(text.begin(), text.end(), '\t') == 9);
    }
}

TEST_CASE("Schema and display profiles must agree before reading or emitting output") {
    const auto typed = events::defaultQuoteSchema(SCHEMA_LIMITS);
    const auto legacy = core::LocalSchema::create({}, SCHEMA_LIMITS);
    REQUIRE(typed);
    REQUIRE(legacy);

    for (bool typedSchema : {false, true}) {
        std::istringstream input(header());
        std::ostringstream output, diagnostic;
        const auto profile =
            typedSchema ? presentation::DisplayProfile::LOCAL_SCHEMA : presentation::DisplayProfile::DEFAULT_QUOTE_V1;
        CHECK(ti::inspect(input, output, diagnostic, typedSchema ? *typed : *legacy, {},
                          presentation::OutputFormat::TSV, profile, 2) == 4);
        CHECK(input.tellg() == 0);
        CHECK(output.str().empty());
        CHECK(diagnostic.str() == "incomplete-v2\t4\tprofile\t0\t-\t-\t-\t-\t-\tLOCAL_PROFILE\n");
    }

    for (bool wrongName : {false, true}) {
        std::vector<core::LocalRecord> records(typed->records().begin(), typed->records().end());

        if (wrongName) {
            records.front().name = u"Other";
        } else {
            records.front().fields.front().serialTypeId = static_cast<std::int32_t>(core::SerialFieldType::COMPACT_INT);
        }

        const auto schema = core::LocalSchema::create(records, SCHEMA_LIMITS, core::LocalProfile::QUOTE_FIELDS_V1);
        REQUIRE(schema);
        std::istringstream input(header());
        std::ostringstream output, diagnostic;
        CHECK(ti::inspect(input, output, diagnostic, *schema, {}, presentation::OutputFormat::TSV,
                          presentation::DisplayProfile::DEFAULT_QUOTE_V1, 2) == 4);
        CHECK(input.tellg() == 0);
        CHECK(output.str().empty());
        CHECK(diagnostic.str().ends_with("\tLOCAL_PROFILE\n"));
    }
}

TEST_CASE("Version two reasons are static and legacy diagnostic rows remain exact") {
    std::ostringstream legacy, extended;
    ti::report(legacy, 2, "usage");
    CHECK(legacy.str() == "incomplete\t2\tusage\t0\t-\t-\t-\t-\t-\n");
    ti::report(extended, 2, "usage", 2, protocol::TapeRejection::LOCAL_PROFILE, presentation::OutputFormat::HUMAN);
    CHECK(extended.str().starts_with("incomplete-v2\t2\tusage\t0\t-\t-\t-\t-\t-\tLOCAL_PROFILE\nReason: "));

    for (const auto reason :
         {protocol::TapeRejection::TIME, protocol::TapeRejection::OPTIONS, protocol::TapeRejection::MESSAGE,
          protocol::TapeRejection::TYPE, protocol::TapeRejection::LOCAL_PROFILE, protocol::TapeRejection::DESCRIPTOR,
          protocol::TapeRejection::SEQUENCE}) {
        const auto text = ti::tapeRejectionExplanation(reason);
        CHECK_FALSE(text.empty());
        CHECK(text.size() < 256);
        CHECK(text.find('\n') == std::string_view::npos);
    }
}

TEST_CASE("Quote parser rejections preserve numeric positions while adding bounded explanations") {
    const auto schema = events::defaultQuoteSchema(SCHEMA_LIMITS);
    REQUIRE(schema);
    auto timeTape = header();
    timeTape[6] = 3; // Add a third descriptor property before the empty send/receive lists.
    const std::string timeProperty("\x04time\x07MESSAGE", 13);
    timeTape.insert(22, timeProperty);
    timeTape[0] = static_cast<char>(timeTape.size() - 1);
    auto optionsTape = header();
    optionsTape[21] = 1; // EMPTY_SET no longer holds.
    optionsTape.insert(22, "x");
    ++optionsTape[0];
    const auto messageTape = header() + std::string("\x01\x0a", 2);
    const auto sequenceTape = header() + header();
    // One exact local field name with a different supported full identity is excluded.
    const unsigned char description[]{20,  2,   0,   5,   'Q', 'u', 'o', 't', 'e', 1, 9,
                                      'B', 'i', 'd', '.', 'P', 'r', 'i', 'c', 'e', 8};
    const auto typeTape = header() + std::string(reinterpret_cast<const char *>(description), sizeof(description));
    const std::array cases{
        std::pair{timeTape, protocol::TapeRejection::TIME}, std::pair{optionsTape, protocol::TapeRejection::OPTIONS},
        std::pair{messageTape, protocol::TapeRejection::MESSAGE},
        std::pair{sequenceTape, protocol::TapeRejection::SEQUENCE}, std::pair{typeTape, protocol::TapeRejection::TYPE}};

    for (const auto &[input, reason] : cases) {
        CAPTURE(ti::tapeRejectionName(reason));
        std::string legacyDiagnostic;

        for (unsigned version : {1U, 2U}) {
            std::istringstream in(input);
            std::ostringstream out, err;
            const auto code = ti::inspect(in, out, err, *schema, {}, presentation::OutputFormat::TSV,
                                          presentation::DisplayProfile::DEFAULT_QUOTE_V1, version);
            CHECK(code == 4);
            CHECK(out.str().find("complete\t") == std::string::npos);
            auto text = err.str();

            if (version == 1) {
                legacyDiagnostic = text;
            } else {
                const auto suffix = std::string("\t") + std::string(ti::tapeRejectionName(reason)) + "\n";
                REQUIRE(text.ends_with(suffix));
                text.erase(text.size() - suffix.size());
                text.append("\n");
                text.erase(std::string_view("incomplete").size(), std::string_view("-v2").size());
                CHECK(text == legacyDiagnostic);
            }
        }
    }
}

TEST_CASE("Descriptor and raw event flag refusals retain exact CLI positions and both diagnostic versions") {
    const auto schema = events::defaultQuoteSchema(SCHEMA_LIMITS);
    REQUIRE(schema);
    auto descriptorTape = header();
    descriptorTape[13] = 'x'; // The first property becomes type=xape, outside the restricted descriptor.
    // Complete STREAM_DATA envelope: flags prefix 1, followed by an empty symbol; refusal precedes record ID.
    const unsigned char flaggedData[]{5, 15, 0xf9, 1, 0xfc, 0};
    const auto flagsTape = header() + std::string(reinterpret_cast<const char *>(flaggedData), sizeof(flaggedData));

    for (bool flags : {false, true}) {
        const auto &bytes = flags ? flagsTape : descriptorTape;
        const std::string cells = flags ? "4\tdata\t3\t24\t30\t26\t4\t4" : "4\tdescriptor\t0\t0\t24\t2\t15\t15";
        const std::string reason = flags ? "OPTIONS" : "DESCRIPTOR";
        const std::string explanation = flags ? "Explicit empty protocol options and zero raw event flags are required."
                                              : "The descriptor contains an unsupported property, value or extension.";

        for (const auto format : {presentation::OutputFormat::TSV, presentation::OutputFormat::HUMAN}) {
            std::string previousOutput;

            for (unsigned version : {1U, 2U}) {
                CAPTURE(flags);
                CAPTURE(version);
                CAPTURE(static_cast<int>(format));
                std::istringstream input(bytes);
                std::ostringstream output, diagnostic;
                CHECK(ti::inspect(input, output, diagnostic, *schema, {}, format,
                                  presentation::DisplayProfile::DEFAULT_QUOTE_V1, version) == 4);
                auto expected =
                    version == 1 ? "incomplete\t" + cells + "\n" : "incomplete-v2\t" + cells + "\t" + reason + "\n";

                if (version == 2 && format == presentation::OutputFormat::HUMAN) {
                    expected += "Reason: " + explanation + "\n";
                }

                CHECK(diagnostic.str() == expected);
                CHECK(output.str().find("complete\t") == std::string::npos);

                if (version == 1) {
                    previousOutput = output.str();
                } else {
                    CHECK(output.str() == previousOutput);
                }
            }
        }
    }
}
