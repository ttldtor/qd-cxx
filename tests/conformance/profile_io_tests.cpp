// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "../../tools/tape-inspect/inspect.hpp"
#include "event_oracle.hpp"
#include <doctest/doctest.h>
#include <sstream>
#include <streambuf>

namespace {
namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace display = org::ttldtor::qdcxx::inspect;
namespace events = org::ttldtor::qdcxx::events;
namespace core = org::ttldtor::qdcxx::core;
namespace protocol = org::ttldtor::qdcxx::protocol;
std::filesystem::path fixtures;                                   ///< Explicit immutable Java evidence directory.
constexpr core::SchemaLimits SCHEMA_LIMITS{1, 16, 64, 128, 4096}; ///< Finite complete default Profile allowances.

/** Inject one selected IO fault. */
class FaultBuffer : public std::streambuf {
    int operation_;       ///< Read zero, write one, first flush two, second flush three.
    int flushes_ = 0;     ///< Completed flush attempts.
    std::string written_; ///< Bytes accepted before the injected failure.
    public:
    /** Select a fault.
     * @param operation Selected operation.
     */
    explicit FaultBuffer(int operation) : operation_(operation) {
    }

    /** Inspect accepted bytes, including completion before a final-flush failure.
     * @return Borrowed accepted prefix.
     */
    const std::string &written() const noexcept {
        return written_;
    }

    protected:
    /** Fail input.
     * @return Never returns normally.
     */
    int_type underflow() override {
        throw std::ios_base::failure("read fault");
    }

    /** Accept or reject a complete write.
     * @param data Borrowed bytes.
     * @param count Requested byte count.
     * @return Accepted bytes.
     */
    std::streamsize xsputn(const char *data, std::streamsize count) override {
        if (operation_ == 1) {
            return 0;
        }

        written_.append(data, static_cast<std::size_t>(count));

        return count;
    }

    /** Fail the independently selected first or second flush.
     * @return Minus one for the selected flush fault.
     */
    int sync() override {
        ++flushes_;

        return (operation_ == 2 && flushes_ == 1) || (operation_ == 3 && flushes_ == 2) ? -1 : 0;
    }
};

/** Read original bytes without line conversion.
 * @param raw Select RAW/MESSAGE capture.
 * @return Owned original binary bytes.
 */
std::string tape(bool raw) {
    const auto bytes = event_oracle::bytes(fixtures / (raw ? "Profile-raw.qtp" : "Profile-stream.qtp"));

    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

/** Inspection result with exact streams. */
struct Result {
    int code;           ///< Native exit.
    std::string output; ///< Exact stdout.
    std::string error;  ///< Exact diagnostics.
};

/** Invoke one explicitly bounded Profile inspection.
 * @param bytes Original input bytes.
 * @param raw Select RAW/MESSAGE.
 * @param format Selected output.
 * @param limits Explicit limits.
 * @return Exit and captured streams.
 */
Result run(const std::string &bytes, bool raw, display::OutputFormat format, ti::Limits limits = {}) {
    const auto schema = events::defaultProfileSchema(SCHEMA_LIMITS);
    REQUIRE(schema);
    std::istringstream in(bytes);
    std::ostringstream out, err;
    const auto code = ti::inspect(in, out, err, *schema, limits, format, display::DisplayProfile::DEFAULT_PROFILE_V1, 2,
                                  raw ? protocol::TapeMode::RAW_MESSAGE_V1 : protocol::TapeMode::LEGACY);

    return {code, out.str(), err.str()};
}
} // namespace

TEST_CASE("Profile IO faults cannot report success, including final flush after completion") {
    const auto schema = events::defaultProfileSchema(SCHEMA_LIMITS);
    REQUIRE(schema);

    for (bool raw : {false, true}) {
        for (int operation = 0; operation < 4; ++operation) {
            FaultBuffer failed(operation);
            std::istream badInput(&failed);
            std::ostream badOutput(&failed);
            std::istringstream input(tape(raw));
            std::ostringstream output, error;
            CHECK(ti::inspect(operation == 0 ? badInput : input, operation == 0 ? output : badOutput, error, *schema,
                              {}, display::OutputFormat::TSV, display::DisplayProfile::DEFAULT_PROFILE_V1, 2,
                              raw ? protocol::TapeMode::RAW_MESSAGE_V1 : protocol::TapeMode::LEGACY) == 3);
            CHECK(error.str().starts_with("incomplete-v2\t3\t"));
            CHECK(error.str().ends_with("\tNONE\n"));

            if (operation == 2 || operation == 3) {
                CHECK(error.str().find("\tflush\t") != std::string::npos);
                CHECK((failed.written().find("\ncomplete\t") != std::string::npos) == (operation == 3));
                CHECK(failed.written().find("\nprofile\t2\t") != std::string::npos);
            }
        }
    }
}

TEST_CASE("Profile cumulative event ownership and encoded output use exact nonrefundable boundaries") {
    const ti::Limits legacyAggregate{4096, 67108864, {}};
    CHECK(legacyAggregate.eventUnits == 4194304);
    const auto oracle = event_oracle::read(fixtures / "observations.tsv");
    std::size_t total = 0, first = 0;

    for (const auto &r : oracle.observations) {
        if (r.phase != "Profile-stream-read") {
            continue;
        }

        std::size_t count = event_oracle::units(r.event[0]).size();

        for (const auto &v : r.values) {
            if (v.units) {
                count += v.units->size();
            }
        }

        total += count;

        if (r.index == 0) {
            first = count;
        }
    }

    REQUIRE(total > first);

    for (bool raw : {false, true}) {
        for (auto format : {display::OutputFormat::TSV, display::OutputFormat::HUMAN}) {
            const auto bytes = tape(raw);
            const auto baseline = run(bytes, raw, format);
            REQUIRE(baseline.code == 0);
            REQUIRE(baseline.error.empty());
            ti::Limits limits;
            limits.eventUnits = total;
            CHECK(run(bytes, raw, format, limits).output == baseline.output);
            limits.eventUnits = total - 1;
            const auto shortTotal = run(bytes, raw, format, limits);
            CHECK(shortTotal.code == 6);
            CHECK(shortTotal.error.find("\tmapping\t") != std::string::npos);
            CHECK(shortTotal.output.find(format == display::OutputFormat::TSV
                                             ? "\nprofile\t2\t"
                                             : "\nProfileValues 2:") == std::string::npos);
            CHECK(shortTotal.output.find(format == display::OutputFormat::TSV
                                             ? "\nprofile\t1\t"
                                             : "\nProfileValues 1:") != std::string::npos);
            limits.eventUnits = first;
            const auto one = run(bytes, raw, format, limits);
            CHECK(one.code == 6);
            CHECK(one.output.find(format == display::OutputFormat::TSV ? "\nprofile\t0\t" : "\nProfileValues 0:") !=
                  std::string::npos);
            CHECK(one.output.find(format == display::OutputFormat::TSV ? "\nprofile\t1\t" : "\nProfileValues 1:") ==
                  std::string::npos);
            limits.eventUnits = 0;
            CHECK(run(bytes, raw, format, limits).code == 6);
            limits = {};
            limits.output = baseline.output.size();
            CHECK(run(bytes, raw, format, limits).code == 0);
            --limits.output;
            const auto shortOutput = run(bytes, raw, format, limits);
            CHECK(shortOutput.code == 6);
            CHECK(shortOutput.output.find(format == display::OutputFormat::TSV ? "\ncomplete\t" : "\nComplete:") ==
                  std::string::npos);
            // A combined raw/event pair is atomic with respect to the output byte allowance.
            const auto eventStart =
                baseline.output.find(format == display::OutputFormat::TSV ? "\nprofile\t0\t" : "\nProfileValues 0:");
            REQUIRE(eventStart != std::string::npos);
            limits.output = eventStart + 1;
            const auto pair = run(bytes, raw, format, limits);
            CHECK(pair.code == 6);
            CHECK(pair.output.find(format == display::OutputFormat::TSV ? "\nprofile-record\t" : "\nProfile 0:") ==
                  std::string::npos);
            limits = {};
            limits.tape.inputBytes = bytes.size();
            CHECK(run(bytes, raw, format, limits).code == 0);
            --limits.tape.inputBytes;
            CHECK(run(bytes, raw, format, limits).code == 6);
            limits = {};
            limits.tape.data.fieldString.codeUnits = 7; // Exact measured longest nullable field.
            CHECK(run(bytes, raw, format, limits).code == 0);
            --limits.tape.data.fieldString.codeUnits;
            CHECK(run(bytes, raw, format, limits).code == 6);
            limits = {};
            limits.tape.data.fieldUnits = 14; // Both measured seven-unit strings charge cumulative work.
            CHECK(run(bytes, raw, format, limits).code == 0);
            --limits.tape.data.fieldUnits;
            CHECK(run(bytes, raw, format, limits).code == 6);
            limits = {};
            limits.tape.data.processedRecords = 3;
            CHECK(run(bytes, raw, format, limits).code == 0);
            --limits.tape.data.processedRecords;
            CHECK(run(bytes, raw, format, limits).code == 6);
            limits = {};
            limits.tape.data.wireFields = 48; // Three complete sixteen-field wire records.
            CHECK(run(bytes, raw, format, limits).code == 0);
            --limits.tape.data.wireFields;
            CHECK(run(bytes, raw, format, limits).code == 6);
            auto truncated = bytes;
            truncated.resize(truncated.size() - 2); // Remove the empty marker and truncate the preceding data envelope.
            const auto bad = run(truncated, raw, format);
            CHECK(bad.code != 0);
            CHECK(bad.output.find(format == display::OutputFormat::TSV ? "\ncomplete\t" : "\nComplete:") ==
                  std::string::npos);
        }
    }
}

TEST_CASE("Profile preset rejects mismatched names, types and selections before IO") {
    const auto original = events::defaultProfileSchema(SCHEMA_LIMITS);
    REQUIRE(original);

    for (int change = 0; change < 4; ++change) {
        std::vector<core::LocalRecord> records(original->records().begin(), original->records().end());

        if (change == 0) {
            records[0].name = u"Other";
        }

        if (change == 1) {
            records[0].fields[0].serialTypeId = 8;
        }

        if (change == 2) {
            std::swap(records[0].fields[4], records[0].fields[11]);
        }

        if (change == 3) {
            for (auto &lookup : records[0].lookups) {
                if (lookup.fieldId == 0) {
                    lookup.fieldId = 1;
                } else if (lookup.fieldId == 1) {
                    lookup.fieldId = 0;
                }
            }
        }

        const auto schema = core::LocalSchema::create(records, SCHEMA_LIMITS, core::LocalProfile::EXTENDED_FIELDS_V1);
        REQUIRE(schema);
        std::istringstream input(tape(false));
        std::ostringstream output, error;
        CHECK(ti::inspect(input, output, error, *schema, {}, display::OutputFormat::TSV,
                          display::DisplayProfile::DEFAULT_PROFILE_V1, 2) == 4);
        CHECK(input.tellg() == 0);
        CHECK(output.str().empty());
        CHECK(error.str() == "incomplete-v2\t4\tprofile\t0\t-\t-\t-\t-\t-\tLOCAL_PROFILE\n");
    }
}

TEST_CASE("Profile formatter rejects incomplete and noncanonical generic fields before rendering") {
    protocol::StreamDataRecord original{};
    original.localId = 0;

    for (const auto type : events::PROFILE_FIELD_TYPES) {
        original.typedValues.push_back({type, 0});
    }

    const events::Profile event{};

    for (const auto format : {display::OutputFormat::TSV, display::OutputFormat::HUMAN}) {
        const display::Formatter formatter(format, display::DisplayProfile::DEFAULT_PROFILE_V1);
        CHECK_NOTHROW(formatter.profileRecord(original, u"Profile", 0, 0, event, 4096));
        auto metadata = original;
        metadata.symbol.eventFlagsBytes = 5;
        CHECK(formatter.profileRecord(metadata, u"Profile", 0, 0, event, 4096) ==
              formatter.profileRecord(original, u"Profile", 0, 0, event, 4096));

        for (const auto flag : {1, 2, 4, 8, 16, 64, 95, -1}) {
            metadata.symbol.eventFlags = flag;
            CHECK_THROWS_WITH_AS(formatter.profileRecord(metadata, u"Profile", 0, 0, event, 0),
                                 "unsupported record metadata format", std::invalid_argument);
        }

        metadata.symbol.eventFlags = 0;
        metadata.removalTime = 0;
        CHECK_THROWS_WITH_AS(formatter.profileRecord(metadata, u"Profile", 0, 0, event, 0),
                             "unsupported record metadata format", std::invalid_argument);

        for (int change = 0; change < 6; ++change) {
            auto record = original;

            if (change == 0) {
                record.typedValues.pop_back();
            } else if (change == 1) {
                record.typedValues.push_back({core::SerialFieldType::COMPACT_INT, 0});
            } else if (change == 2) {
                record.values.push_back(0);
            } else if (change == 3) {
                record.typedValues[0].type = core::SerialFieldType::COMPACT_INT;
            } else if (change == 4) {
                record.typedValues[0].units = u"unexpected scalar payload";
            } else {
                record.typedValues[14].raw = 1; // String values require canonical raw zero.
            }

            CHECK_THROWS_AS(formatter.profileRecord(record, u"Profile", 0, 0, event, 4096), std::invalid_argument);
        }
    }
}

/** Run source-independent IO checks with an explicit fixture root.
 * @param argc Argument count.
 * @param argv Program and fixture directory.
 * @return Doctest status, or usage error.
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }

    fixtures = argv[1];
    doctest::Context context;

    return context.run();
}
