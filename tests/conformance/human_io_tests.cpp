#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "inspect.hpp"

#include <doctest/doctest.h>
#include <limits>
#include <locale>
#include <sstream>

namespace {
/** Portable sink for deterministic write and pre/final-summary flush failures. */
class FailingSink : public std::stringbuf {
    bool writeFailure_; ///< Reject writes when true.
    int flushes_;       ///< Remaining successful flushes.
    public:
    /** Configure failure location.
     * @param writeFailure Reject the first write.
     * @param flushes Successful flush calls before failure.
     */
    FailingSink(bool writeFailure, int flushes) : writeFailure_(writeFailure), flushes_(flushes) {
    }

    private:
    /** Accept or reject a physical write.
     * @param data Borrowed encoded bytes.
     * @param count Requested bytes.
     * @return Accepted byte count.
     */
    std::streamsize xsputn(const char *data, std::streamsize count) override {
        return writeFailure_ ? 0 : std::stringbuf::xsputn(data, count);
    }

    /** Fail after the configured number of successful flushes.
     * @return Zero on success, negative on injected error.
     */
    int sync() override {
        return flushes_-- > 0 ? 0 : -1;
    }
};

/** Locale facet that would visibly group numbers if stream formatting were used. */
class GroupedNumbers : public std::numpunct<char> {
    /** Select a deliberately nonstandard grouping separator.
     * @return Underscore.
     */
    char do_thousands_sep() const override {
        return '_';
    }

    /** Group decimal digits individually.
     * @return One digit per group.
     */
    std::string do_grouping() const override {
        return "\1";
    }
};
} // namespace

TEST_CASE("human physical write and both flush failures preserve exact diagnostics") {
    auto schema = qd::inspect::oracleSchema();
    REQUIRE(schema);

    for (bool throws : {false, true}) {
        for (int stage : {0, 1, 2}) {
            std::istringstream input;
            FailingSink buffer(stage == 0, stage == 2 ? 1 : 0);
            std::ostream output(&buffer);

            if (throws) {
                output.exceptions(std::ios::badbit | std::ios::failbit);
            }

            std::ostringstream diagnostic;
            CHECK(qd::inspect::inspect(input, output, diagnostic, *schema, {}, qd::inspect::OutputFormat::HUMAN) == 3);
            const std::string checkpoint = stage == 0 ? "write" : "flush";
            CHECK(diagnostic.str() == "incomplete\t3\t" + checkpoint + "\t-\t-\t-\t-\t-\t-\t-\t-\n");
            CHECK((buffer.str().find("Complete: ") != std::string::npos) == (stage == 2));
        }
    }
}

TEST_CASE("independent schema uses local-order fallback labels regardless of alias order") {
    auto schema = qd::core::LocalSchema::create(
        std::vector<qd::core::LocalRecord>{{0, u"S", false, {{8}, {8}}, {{u"wrong-display", 1}, {u"v", 0}}}},
        {1, 2, 2, 32, 64});
    REQUIRE(schema);
    const char wire[]{0x08, 0x02, 0x09, 0x01, 0x53, 0x01, 0x01, 0x76, 0x08, 0x04, 0x0f, static_cast<char>(0xfe),
                      0x09, 0x01};
    std::istringstream input(std::string(wire, sizeof(wire)));
    std::ostringstream output, diagnostic;
    CHECK(qd::inspect::inspect(input, output, diagnostic, *schema, {}, qd::inspect::OutputFormat::HUMAN) == 0);
    CHECK(diagnostic.str().empty());
    CHECK(output.str() == "qd-qtp-inspect human 1 profile=local-schema identities=local\n"
                          "Frame 0: DESCRIBE_RECORDS (2) bytes=[0,9) payload=2 size=7\n"
                          "Frame 1: STREAM_DATA (15) bytes=[9,14) payload=11 size=3\n"
                          "Record 0: frame=1 local=0 name=\"S\" symbol=\"\" fields={field[0]=1, field[1]=0}\n"
                          "Complete: bytes=14 frames=2 records=1 charged-records=1 wire-fields=1\n");
}

TEST_CASE("formatter quotes independent local names and preserves empty zero-field records") {
    const qd::inspect::Formatter formatter(qd::inspect::OutputFormat::HUMAN, qd::inspect::DisplayProfile::LOCAL_SCHEMA);
    qd::protocol::StreamDataRecord record{};
    const std::u16string name{u'X', u'"', u'\\', u'\n', 0, 0xd800, 0x202e};
    const std::string expected =
        "Record 0: frame=1 local=0 name=\"X\\\"\\\\\\n\\u0000\\uD800\\u202E\" symbol=\"\" fields={}\n";
    CHECK(formatter.record(record, name, 1, 0, expected.size()) == expected);
    CHECK_THROWS_AS(formatter.record(record, name, 1, 0, expected.size() - 1), qd::inspect::OutputLimit);
    // Large escaped input with a tiny budget fails in the counting pass, without an expanded temporary.
    record.symbol.units.assign(100000, 0xd800);
    CHECK_THROWS_AS(formatter.record(record, u"S", 1, 0, 64), qd::inspect::OutputLimit);
}

TEST_CASE("formatter numbers ignore global locale and handle maximum counters") {
    const qd::inspect::Formatter formatter(qd::inspect::OutputFormat::HUMAN, qd::inspect::DisplayProfile::LOCAL_SCHEMA);
    const auto prior = std::locale();
    std::locale::global(std::locale(prior, new GroupedNumbers));
    const auto result = formatter.complete(std::numeric_limits<std::uint64_t>::max(), 12345, 12345, 12345, 12345, 256);
    std::locale::global(prior);
    CHECK(result ==
          "Complete: bytes=18446744073709551615 frames=12345 records=12345 charged-records=12345 wire-fields=12345\n");
    CHECK(formatter.complete(0, 0, 0, 0, 0, std::numeric_limits<std::size_t>::max()) ==
          "Complete: bytes=0 frames=0 records=0 charged-records=0 wire-fields=0\n");
}
