#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "inspect.hpp"

#include <doctest/doctest.h>
#include <sstream>
#include <stdexcept>

namespace {
/** Stream buffer that deterministically fails the first read rather than reporting EOF. */
class ReadFailure : public std::streambuf {
    std::string prefix_; ///< Readable bytes before the injected failure.

    public:
    /** Provide a complete readable prefix before failure.
     * @param prefix Owned bytes, empty by default.
     */
    explicit ReadFailure(std::string prefix = {}) : prefix_(std::move(prefix)) {
        setg(prefix_.data(), prefix_.data(), prefix_.data() + prefix_.size());
    }

    private:
    /** Inject an I/O error when the caller attempts to read.
     * @return Never returns; the owning stream translates the exception to badbit.
     */
    int_type underflow() override {
        throw std::runtime_error("injected read failure");
    }
};

/** Writable sink with an explicit flush failure, independent of OS devices. */
class FlushFailure : public std::stringbuf {
    int successfulFlushes_; ///< Number of flushes to allow before failing.

    public:
    /** Choose whether the final completion flush or an earlier flush fails.
     * @param successfulFlushes Count of flush calls accepted before failure.
     */
    explicit FlushFailure(int successfulFlushes = 0) : successfulFlushes_(successfulFlushes) {
    }

    private:
    /** Reject every flush without pretending bytes can be retracted.
     * @return Negative to make the owning stream bad.
     */
    int sync() override {
        return successfulFlushes_-- > 0 ? 0 : -1;
    }
};

/** Sink whose first write returns zero, independently of buffering or OS permissions. */
class WriteFailure : public std::streambuf {
    /** Reject a buffered write.
     * @param data Unused caller bytes.
     * @param count Requested byte count.
     * @return Zero bytes accepted.
     */
    std::streamsize xsputn(const char *data, std::streamsize count) override {
        static_cast<void>(data);
        static_cast<void>(count);

        return 0;
    }
};
} // namespace

TEST_CASE("injected input failure differs from EOF including exact input budget") {
    auto schema = org::ttldtor::qdcxx::inspect::oracleSchema();
    REQUIRE(schema);

    for (const auto inputLimit : {0, 100}) {
        ReadFailure buffer;
        std::istream input(&buffer);
        std::ostringstream output;
        std::ostringstream diagnostic;
        org::ttldtor::qdcxx::inspect::Limits limits;
        limits.input = inputLimit;
        CHECK(org::ttldtor::qdcxx::inspect::inspect(input, output, diagnostic, *schema, limits) == 3);
        CHECK(output.str().find("complete\t") == std::string::npos);
        CHECK(diagnostic.str() == "incomplete\t3\tread\t-\t-\t-\t-\t-\t-\t-\t-\n");
    }
}

TEST_CASE("injected writes and flushes return I/O failure even if stderr is broken") {
    auto schema = org::ttldtor::qdcxx::inspect::oracleSchema();
    REQUIRE(schema);
    std::istringstream input;
    WriteFailure broken;
    std::ostream output(&broken);
    std::ostringstream diagnostic;
    CHECK(org::ttldtor::qdcxx::inspect::inspect(input, output, diagnostic, *schema, {}) == 3);
    CHECK(diagnostic.str() == "incomplete\t3\twrite\t-\t-\t-\t-\t-\t-\t-\t-\n");
    std::istringstream empty;
    FlushFailure buffered;
    std::ostream flushing(&buffered);
    std::ostringstream flushDiagnostic;
    CHECK(org::ttldtor::qdcxx::inspect::inspect(empty, flushing, flushDiagnostic, *schema, {}) == 3);
    CHECK(buffered.str().find("complete\t") == std::string::npos);
    CHECK(flushDiagnostic.str() == "incomplete\t3\tflush\t-\t-\t-\t-\t-\t-\t-\t-\n");
    std::istringstream another;
    output.clear();
    std::ostream deadDiagnostic(&broken);
    deadDiagnostic.exceptions(std::ios::badbit | std::ios::failbit);
    CHECK(org::ttldtor::qdcxx::inspect::inspect(another, output, deadDiagnostic, *schema, {}) == 3);
}

TEST_CASE("integration accepts an independent validated local schema") {
    auto schema = org::ttldtor::qdcxx::core::LocalSchema::create(
        std::vector<org::ttldtor::qdcxx::core::LocalRecord>{{0, u"S", false, {}, {}}}, {1, 0, 0, 1, 1});
    REQUIRE(schema);
    // Same sender S.v, independent local S has no fields: consume the value and emit no values.
    const char wire[]{0x08, 0x02, 0x09, 0x01, 0x53, 0x01, 0x01, 0x76, 0x08, 0x04, 0x0f, static_cast<char>(0xfe),
                      0x09, 0x01};
    std::istringstream input(std::string(wire, sizeof(wire)));
    std::ostringstream output;
    std::ostringstream diagnostic;
    CHECK(org::ttldtor::qdcxx::inspect::inspect(input, output, diagnostic, *schema, {}) == 0);
    CHECK(diagnostic.str().empty());
    CHECK(output.str().find("record\t1\t0\t0\t0053\t-\t-\n") != std::string::npos);
    CHECK(output.str().ends_with("complete\t14\t2\t1\t1\t1\n"));
}

TEST_CASE("read failure after a completed record retains output but has no completion") {
    auto schema = org::ttldtor::qdcxx::inspect::oracleSchema();
    REQUIRE(schema);
    const char wire[]{0x08, 0x02, 0x09, 0x01, 0x53, 0x01, 0x01, 0x76, 0x08, 0x04, 0x0f, static_cast<char>(0xfe),
                      0x09, 0x01};
    ReadFailure buffer(std::string(wire, sizeof(wire)));
    std::istream input(&buffer);
    std::ostringstream output;
    std::ostringstream diagnostic;
    org::ttldtor::qdcxx::inspect::Limits limits;
    limits.chunkSize = 1;
    CHECK(org::ttldtor::qdcxx::inspect::inspect(input, output, diagnostic, *schema, limits) == 3);
    CHECK(output.str().find("record\t1\t0\t0\t0053\t-\t1\n") != std::string::npos);
    CHECK(output.str().find("complete\t") == std::string::npos);
    CHECK(diagnostic.str() == "incomplete\t3\tread\t1\t15\t9\t11\t-\t-\t-\t-\n");
}

TEST_CASE("final completion flush failure never reports success") {
    auto schema = org::ttldtor::qdcxx::inspect::oracleSchema();
    REQUIRE(schema);
    std::istringstream input;
    FlushFailure buffer(1);
    std::ostream output(&buffer);
    output.exceptions(std::ios::badbit);
    std::ostringstream diagnostic;
    CHECK(org::ttldtor::qdcxx::inspect::inspect(input, output, diagnostic, *schema, {}) == 3);
    CHECK(diagnostic.str() == "incomplete\t3\tflush\t-\t-\t-\t-\t-\t-\t-\t-\n");
    // Physical delivery is irreversible; exit status invalidates a marker already in the sink.
    CHECK(buffer.str().find("complete\t") != std::string::npos);
}

TEST_CASE("read failure inside a new envelope identifies that envelope") {
    auto schema = org::ttldtor::qdcxx::inspect::oracleSchema();
    REQUIRE(schema);
    // One complete heartbeat followed by an incomplete two-byte length prefix.
    ReadFailure buffer(std::string("\x00\x80", 2));
    std::istream input(&buffer);
    std::ostringstream output;
    std::ostringstream diagnostic;
    org::ttldtor::qdcxx::inspect::Limits limits;
    limits.chunkSize = 1;
    CHECK(org::ttldtor::qdcxx::inspect::inspect(input, output, diagnostic, *schema, limits) == 3);
    CHECK(output.str() == "format\tqd-qtp-inspect\t1\nframe\t0\t0\t1\t-\t-\t0\n");
    CHECK(diagnostic.str() == "incomplete\t3\tread\t1\t-\t1\t-\t-\t-\t-\t-\n");
}
