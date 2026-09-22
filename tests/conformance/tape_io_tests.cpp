// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../tools/qtp-inspect/inspect.hpp"
#include "../../tools/tape-inspect/inspect.hpp"
#include <algorithm>
#include <doctest/doctest.h>
#include <sstream>
#include <streambuf>

namespace {
namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace presentation = org::ttldtor::qdcxx::inspect;

/** IO fault at a selected operation. */
class FaultBuffer : public std::streambuf {
    int operation_;

    public:
    /** Select read, write or flush failure.
     * @param operation Zero means read, one write, two flush.
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

/** Build a minimal complete tape header with no version/send entries.
 * @return Exact binary bytes from documented grammar.
 */
std::string header() {
    const unsigned char bytes[]{23, 1,   68, 88,  80,  51, 2,   4,   116, 121, 112, 101,
                                4,  116, 97, 112, 101, 3,  111, 112, 116, 0,   0,   0};

    return {reinterpret_cast<const char *>(bytes), sizeof(bytes)};
}
} // namespace

TEST_CASE("tape IO read write flush and encoded output boundaries") {
    auto schema = presentation::oracleSchema();
    REQUIRE(schema);

    for (int mode = 0; mode < 3; ++mode) {
        FaultBuffer bad(mode);
        std::istream failedInput(&bad);
        std::ostream failedOutput(&bad);
        std::istringstream goodInput(header());
        std::ostringstream goodOutput, diagnostic;
        auto code = ti::inspect(mode == 0 ? failedInput : goodInput, mode == 0 ? goodOutput : failedOutput, diagnostic,
                                *schema, {});
        CHECK(code == 3);
        CHECK(diagnostic.str().starts_with("incomplete\t3\t"));
    }

    for (auto format : {presentation::OutputFormat::TSV, presentation::OutputFormat::HUMAN}) {
        std::istringstream in(header());
        std::ostringstream out, err;
        REQUIRE(ti::inspect(in, out, err, *schema, {}, format) == 0);

        for (auto delta : {0U, 1U}) {
            ti::Limits limits;
            limits.output = out.str().size() - delta;
            std::istringstream next(header());
            std::ostringstream result, diagnostic;
            CHECK(ti::inspect(next, result, diagnostic, *schema, limits, format) == (delta == 0 ? 0 : 6));
            CHECK(result.str().size() <= limits.output);

            if (delta == 1) {
                CHECK(result.str().find("complete\t") == std::string::npos);
                CHECK(result.str().find("Complete:") == std::string::npos);
            }
        }
    }
}

TEST_CASE("tape version escaping is lossless and output budget checked before expansion") {
    presentation::Formatter formatter(presentation::OutputFormat::HUMAN, presentation::DisplayProfile::ORACLE_SRE_V1);
    org::ttldtor::qdcxx::protocol::TapeDescriptor d;
    d.version = std::u16string{u'"', u'\\', u'\n', u'\t', u'\x1b', u'\x202e', char16_t(0xd800)};
    auto row = formatter.tapeDescriptor(d, 4096);
    CHECK(std::count(row.begin(), row.end(), '\n') == 1);
    CHECK(row.find('\\') != std::string::npos);
    CHECK(row.find('\x1b') == std::string::npos);
    CHECK(row.find("\\uD800") != std::string::npos);
    CHECK_THROWS_AS(formatter.tapeDescriptor(d, row.size() - 1), presentation::OutputLimit);
    CHECK(formatter.tapeDescriptor(d, row.size()) == row);
}
