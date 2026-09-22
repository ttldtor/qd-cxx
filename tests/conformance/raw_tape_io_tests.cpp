// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "../../tools/qtp-inspect/inspect.hpp"
#include "../../tools/tape-inspect/inspect.hpp"
#include "qd/events/quote.hpp"
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <streambuf>

namespace {
namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace view = org::ttldtor::qdcxx::inspect;
namespace p = org::ttldtor::qdcxx::protocol;
namespace q = org::ttldtor::qdcxx::events;
std::filesystem::path fixtures; ///< Explicit immutable Java fixture directory.

/** Read one production artifact exactly.
 * @param name Relative filename.
 * @return Owned binary contents.
 */
std::string read(std::string_view name) {
    std::ifstream input(fixtures / name, std::ios::binary);
    REQUIRE(input.good());

    return {std::istreambuf_iterator<char>(input), {}};
}

/** Inject independent stream operation failures. */
class FaultBuffer : public std::streambuf {
    int operation_; ///< Read, write or flush failure.
    public:
    /** Select failure.
     * @param operation Zero read, one write, two flush.
     */
    explicit FaultBuffer(int operation) : operation_(operation) {
    }

    protected:
    /** Fail an input read.
     * @return Never returns normally.
     */
    int_type underflow() override {
        throw std::ios_base::failure("injected read");
    }

    /** Accept or fail a write.
     * @param data Borrowed bytes.
     * @param count Requested count.
     * @return Accepted count or zero for failure.
     */
    std::streamsize xsputn(const char *data, std::streamsize count) override {
        static_cast<void>(data);

        return operation_ == 1 ? 0 : count;
    }

    /** Inject flush error.
     * @return Minus one for failure, zero otherwise.
     */
    int sync() override {
        return operation_ == 2 ? -1 : 0;
    }
};
} // namespace

TEST_CASE("RAW output crosses profiles formats diagnostics and exact output budgets") {
    for (bool quote : {false, true}) {
        const auto schema = quote ? q::defaultQuoteSchema(ti::Limits{}.tape.context.schema) : view::oracleSchema();
        REQUIRE(schema);
        const auto bytes = read(quote ? "raw-quote.qtp" : "raw-synthetic.qtp");
        const auto profile = quote ? view::DisplayProfile::DEFAULT_QUOTE_V1 : view::DisplayProfile::ORACLE_SRE_V1;

        for (auto format : {view::OutputFormat::TSV, view::OutputFormat::HUMAN}) {
            for (unsigned version : {1U, 2U}) {
                std::istringstream input(bytes);
                std::ostringstream output, errors;
                CHECK(ti::inspect(input, output, errors, *schema, {}, format, profile, version,
                                  p::TapeMode::RAW_MESSAGE_V1) == 0);
                CHECK(errors.str().empty());
                const auto baseline = output.str();
                CHECK(baseline.find("raw-message-v1") != std::string::npos);
                CHECK(baseline.find(format == view::OutputFormat::TSV ? "timing\t" : "Timing: ") != std::string::npos);
                CHECK(baseline.find(format == view::OutputFormat::TSV ? "heartbeat\t" : "Heartbeat: ") !=
                      std::string::npos);

                for (std::size_t bound : {std::size_t{0}, baseline.size() - 1, baseline.size(), baseline.size() + 1}) {
                    ti::Limits limits;
                    limits.output = bound;
                    limits.chunkSize = 1;
                    std::istringstream again(bytes);
                    std::ostringstream actual, diagnostic;
                    const auto code = ti::inspect(again, actual, diagnostic, *schema, limits, format, profile, version,
                                                  p::TapeMode::RAW_MESSAGE_V1);

                    if (bound >= baseline.size()) {
                        CHECK(code == 0);
                        CHECK(actual.str() == baseline);
                        CHECK(diagnostic.str().empty());
                    } else {
                        CHECK(code == 6);
                        CHECK(actual.str().find(format == view::OutputFormat::TSV ? "complete\t" : "Complete:") ==
                              std::string::npos);
                    }
                }
            }
        }
    }
}

TEST_CASE("timed metadata has distinct absent zero and saturated representations and bounded rows") {
    for (auto format : {view::OutputFormat::TSV, view::OutputFormat::HUMAN}) {
        view::Formatter formatter(format, view::DisplayProfile::ORACLE_SRE_V1);
        const bool tsv = format == view::OutputFormat::TSV;
        CHECK(formatter.tapeHeartbeat({}, 7, 1024) ==
              (tsv ? "heartbeat\t7\t-\t-\n" : "Heartbeat: frame=7 content=- timestampMs=-\n"));
        CHECK(formatter.tapeHeartbeat({0, {}}, 7, 1024) ==
              (tsv ? "heartbeat\t7\t0\t-\n" : "Heartbeat: frame=7 content=0 timestampMs=-\n"));
        CHECK(formatter.tapeHeartbeat({1, 0}, 7, 1024) ==
              (tsv ? "heartbeat\t7\t1\t0\n" : "Heartbeat: frame=7 content=1 timestampMs=0\n"));
        CHECK(formatter.tapeTiming({}, 7, 1024) ==
              (tsv ? "timing\t7\t-\t0\t0\n" : "Timing: frame=7 timestampMs=- eventTimeSequence=0 eventTimeMs=0\n"));
        const p::TapeTiming timing{-1, -104857600};
        const auto row = formatter.tapeTiming(timing, 7, 1024);
        CHECK(row == (tsv ? "timing\t7\t-1\t-104857600\t-1\n"
                          : "Timing: frame=7 timestampMs=-1 eventTimeSequence=-104857600 eventTimeMs=-1\n"));
        CHECK(formatter.tapeTiming(timing, 7, row.size()) == row);
        CHECK_THROWS_AS(formatter.tapeTiming(timing, 7, row.size() - 1), view::OutputLimit);
        const auto heartbeat = formatter.tapeHeartbeat({1, 0}, 7, 1024);
        CHECK(formatter.tapeHeartbeat({1, 0}, 7, heartbeat.size()) == heartbeat);
        CHECK_THROWS_AS(formatter.tapeHeartbeat({1, 0}, 7, heartbeat.size() - 1), view::OutputLimit);
    }
}

TEST_CASE("RAW IO failures and invalid mode do not announce completion") {
    const auto schema = view::oracleSchema();
    REQUIRE(schema);
    const auto bytes = read("raw-synthetic.qtp");

    for (int operation = 0; operation < 3; ++operation) {
        FaultBuffer fault(operation);
        std::istream badInput(&fault);
        std::ostream badOutput(&fault);
        std::istringstream input(bytes);
        std::ostringstream output, diagnostic;
        CHECK(ti::inspect(operation == 0 ? badInput : input, operation == 0 ? output : badOutput, diagnostic, *schema,
                          {}, view::OutputFormat::TSV, view::DisplayProfile::ORACLE_SRE_V1, 2,
                          p::TapeMode::RAW_MESSAGE_V1) == 3);
        CHECK(diagnostic.str().starts_with("incomplete-v2\t3\t"));
    }

    std::istringstream input(bytes);
    std::ostringstream output, diagnostic;
    CHECK(ti::inspect(input, output, diagnostic, *schema, {}, view::OutputFormat::TSV,
                      view::DisplayProfile::ORACLE_SRE_V1, 1, static_cast<p::TapeMode>(99)) == 2);
    CHECK(input.tellg() == 0);
    CHECK(output.str().empty());
}

/** Run the fixture-backed IO suite.
 * @param argc Argument count.
 * @param argv Executable, fixture directory, then optional doctest arguments.
 * @return Doctest exit or usage failure.
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
