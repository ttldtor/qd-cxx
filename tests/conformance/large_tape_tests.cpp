// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "source_fixture.hpp"
#include "tape_sequence.hpp"
#include <charconv>
#include <cstdio>
#include <iostream>
#include <set>
#include <sstream>
#include <streambuf>
#include <type_traits>
#ifdef _WIN32
#    include <corecrt_startup.h>
#    include <cstdlib>
#endif

namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace source_fixture = org::ttldtor::qdcxx::test_support::source_fixture;

namespace {
constexpr std::size_t DEFAULT_RECORDS = 3000000;
constexpr std::size_t MAX_LINE_BYTES = 8192;
constexpr std::size_t SOURCE_OUTPUT_BYTES = 1500000000;
constexpr std::size_t GLOBAL_OUTPUT_BYTES = 1600000000;

/** Fail the internal runner with a fixed bounded explanation.
 * @param condition Expected invariant; false throws runtime_error.
 * @param message Static bounded failure explanation.
 */
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Encode native identity independently of the production row builder.
 * @param path Borrowed explicit native path, never normalized.
 * @return Owning lowercase native-byte hex, with UTF-16LE units on Windows.
 */
std::string nativeHex(const std::filesystem::path &path) {
    constexpr char HEX[] = "0123456789abcdef";
    std::string result;

    for (const auto unit : path.native()) {
        const auto unsignedUnit = static_cast<std::make_unsigned_t<decltype(unit)>>(unit);

        for (std::size_t byte = 0; byte < sizeof(unit); ++byte) {
            const auto value = static_cast<unsigned>((unsignedUnit >> (byte * 8)) & 0xff);
            result += HEX[value >> 4];
            result += HEX[value & 15];
        }
    }

    return result;
}

/** Independent complete canonical projection validator, retaining only one bounded line.
 * Resource rows are checked separately because observed work/calls may depend on feed chunking.
 * Every frame/schema/record/source/time/complete byte is compared against an on-demand expected row.
 */
class CanonicalSink : public std::streambuf {
    std::array<char, MAX_LINE_BYTES> line_{};
    std::size_t lineSize_ = 0;
    std::size_t records_;
    std::uint64_t sourceBytes_;
    std::size_t pathUnits_ = 0;
    std::size_t pathHexBytes_ = 0;
    std::vector<std::string> preamble_;
    std::size_t preambleIndex_ = 0;
    std::size_t emitted_ = 0;
    std::size_t dataFrames_ = 0;
    std::size_t frameRecords_ = 0;
    std::size_t frameRemaining_ = 0;
    std::uint64_t offset_ = 0;
    bool timing_ = false;
    bool ended_ = false;
    bool complete_ = false;
    bool valid_ = true;
    std::size_t maximumLine_ = 0;
    std::uint64_t encodedBytes_ = 0;
    std::set<std::string> resourceNames_;
    std::string failure_;

    /** Compare a single complete row and retain only its first bounded mismatch.
     * @param actual Borrowed complete row without LF.
     * @param expected Independently reconstructed complete row without LF.
     */
    void equal(std::string_view actual, std::string_view expected) {
        if (actual != expected) {
            valid_ = false;

            if (failure_.empty()) {
                failure_ = "expected=" + std::string(expected.substr(0, 200)) +
                           " actual=" + std::string(actual.substr(0, 200));
            }
        }
    }

    /** Validate the fixed NONE per-source and sequence resource inventory independently.
     * @param row Complete resource row without LF, borrowed only for this call.
     */
    void resource(std::string_view row) {
        require(preambleIndex_ == preamble_.size() && emitted_ == records_ && frameRemaining_ == 0 && !timing_ &&
                    !complete_,
                "resource publication phase");
        std::array<std::string_view, 6> cells{};
        std::size_t first = 0;

        for (std::size_t i = 0; i < cells.size(); ++i) {
            const auto next = row.find('\t', first);
            require((i + 1 == cells.size()) == (next == row.npos), "resource cell count");
            cells[i] = row.substr(first, next == row.npos ? row.size() - first : next - first);
            first = next + 1;
        }

        const auto name = cells[3];
        require(
            resourceNames_.insert(std::string(cells[1]) + "." + std::string(cells[2]) + "." + std::string(name)).second,
            "duplicate resource");

        const auto number = [](std::string_view text) {
            std::uint64_t value = 0;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);

            require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size(), "resource integer");

            return value;
        };
        const auto reserved = number(cells[4]);
        std::optional<std::uint64_t> observed;

        if (cells[5] != "-") {
            observed = number(cells[5]);
            require(*observed <= reserved, "resource finite observation");
        }

        if (cells[1] == "data") {
            require(!ended_ && cells[2] == "0", "data resource scope");

            if (name == "physicalFetched" || name == "compressedConsumed" || name == "logicalProduced") {
                require(observed == sourceBytes_, "NONE source byte observation");
            } else if (name == "workBytes") {
                require(observed == sourceBytes_ * 2, "NONE irreversible byte work");
            } else if (name == "headerBytes" || name == "inflateCalls" || name == "physicalProbes") {
                require(observed == 0, "NONE envelope work");
            } else {
                require(name == "inflatedProbes" && !observed, "NONE unknown inflated probe observation");
            }

            return;
        }

        if (cells[1] == "parser") {
            require(!ended_ && cells[2] == "0", "parser resource scope");

            if (name == "parserAccepted") {
                require(observed == sourceBytes_, "parser accepted observation");
            } else if (name == "frames") {
                require(observed == dataFrames_ + 2, "parser frame observation");
            } else if (name == "markers") {
                require(observed == 0, "parser marker observation");
            } else {
                require((name == "emittedRecords" || name == "processedRecords" || name == "wireFields") &&
                            observed == records_,
                        "parser record observation");
            }

            return;
        }

        require(ended_ && cells[1] == "sequence" && cells[2] == "-", "sequence resource scope");

        constexpr std::array<std::string_view, 92> NAMES{
            "tape-frame-maximumBodyBytes",
            "tape-frame-maximumPendingBytes",
            "tape-inputBytes",
            "tape-frames",
            "tape-descriptor-properties",
            "tape-descriptor-messages",
            "tape-descriptor-text-bodyBytes",
            "tape-descriptor-text-codeUnits",
            "tape-descriptor-textUnits",
            "tape-context-schema-records",
            "tape-context-schema-fields",
            "tape-context-schema-lookups",
            "tape-context-schema-nameUnits",
            "tape-context-schema-totalNameUnits",
            "tape-context-descriptions",
            "tape-context-bindings-fields",
            "tape-context-bindings-nameUnits",
            "tape-context-bindings-totalNameUnits",
            "tape-constructionUnits",
            "tape-data-payloadBytes",
            "tape-data-processedRecords",
            "tape-data-wireFields",
            "tape-data-emittedRecords",
            "tape-data-emittedValues",
            "tape-data-ownedSymbolUnits",
            "tape-data-symbol-inputBytes",
            "tape-data-symbol-codeUnits",
            "tape-data-symbol-stateUnits",
            "tape-data-fieldString-encodedBytes",
            "tape-data-fieldString-codeUnits",
            "tape-data-fieldBytes",
            "tape-data-fieldUnits",
            "tape-data-ownedFieldUnits",
            "tape-data-fieldPayloadEncodedBytes",
            "tape-data-fieldPayloadBytes",
            "tape-data-payloadEncodedBytes",
            "tape-data-payloadWorkBytes",
            "tape-data-ownedPayloadBytes",
            "tape-data-ownedFieldTimeRecords",
            "tape-timingEvents",
            "tape-snapshotUnits",
            "dataContainer-compressedBytes",
            "companionContainer-compressedBytes",
            "dataContainer-inflatedBytes",
            "companionContainer-inflatedBytes",
            "dataContainer-expansionRatio",
            "companionContainer-expansionRatio",
            "dataContainer-ratioSlack",
            "companionContainer-ratioSlack",
            "dataContainer-gzipMembers",
            "companionContainer-gzipMembers",
            "dataContainer-zipEntries",
            "companionContainer-zipEntries",
            "dataContainer-headerBytes",
            "companionContainer-headerBytes",
            "dataContainer-totalHeaderBytes",
            "companionContainer-totalHeaderBytes",
            "dataContainer-workBytes",
            "companionContainer-workBytes",
            "dataContainer-inflateCalls",
            "companionContainer-inflateCalls",
            "dataContainer-retainedBytes",
            "companionContainer-retainedBytes",
            "dataContainer-inflaterBytes",
            "companionContainer-inflaterBytes",
            "dataContainer-chunkBytes",
            "companionContainer-chunkBytes",
            "scanner-inputBytes",
            "scanner-lineBytes",
            "scanner-lines",
            "scanner-marks",
            "scanner-workBytes",
            "scanner-retainedBytes",
            "chunkBytes",
            "outputBytes",
            "eventUnits",
            "physicalProbes",
            "inflatedProbes",
            "path-native-units",
            "path-encoded-bytes",
            "path-storage-bytes",
            "occurrences",
            "source-handles",
            "diagnostic-bytes",
            "terminal-bytes",
            "output-charged-bytes",
            "closed-sources",
            "accepted-bytes",
            "frames",
            "emitted-records",
            "processed-records",
            "wire-fields",
        };
        require(std::find(NAMES.begin(), NAMES.end(), name) != NAMES.end(), "sequence resource inventory name");

        if (name == "path-native-units") {
            require(observed == pathUnits_, "native path units observation");
        } else if (name == "path-encoded-bytes") {
            require(observed == pathHexBytes_, "encoded path bytes observation");
        } else if (name == "path-storage-bytes") {
            require(observed ==
                        sizeof(ti::TapeSegmentSpec) + (pathUnits_ + 1) * sizeof(std::filesystem::path::value_type),
                    "path storage observation");
        } else if (name == "occurrences" || name == "source-handles" || name == "closed-sources") {
            require(observed == 1, "sequence ownership observation");
        } else if (name == "output-charged-bytes") {
            require(observed == encodedBytes_ - row.size() - 1, "actual encoded bytes observation");
        } else if (name == "accepted-bytes") {
            require(observed == sourceBytes_, "sequence accepted observation");
        } else if (name == "frames") {
            require(observed == dataFrames_ + 2, "sequence frame observation");
        } else if (name == "emitted-records" || name == "processed-records" || name == "wire-fields") {
            require(observed == records_, "sequence record observation");
        } else {
            require(!observed, "reserved capacity is not a measurement");
        }
    }

    /** Reconstruct canonical rows from fixture grammar and scalar ordinals, never formatter output.
     * @param row Borrowed complete bounded row, released before the next write.
     */
    void accept(std::string_view row) {
        if (!valid_) {
            return;
        }

        if (row.starts_with("resource\t")) {
            resource(row);

            return;
        }

        if (preambleIndex_ < preamble_.size()) {
            equal(row, preamble_[preambleIndex_++]);

            return;
        }

        if (timing_) {
            equal(row, "timing\t" + std::to_string(dataFrames_ + 1) + "\t-\t0\t0");
            timing_ = false;

            return;
        }

        if (frameRemaining_ != 0) {
            std::array<char, 512> expected{};
            const auto length = std::snprintf(expected.data(), expected.size(),
                                              "wire-data-v9\t0\t%zu\t%zu\t%zu\t1\t0\t0\t0052\t-\t8:%zu:-:-\t-"
                                              "\t0\t0\t0\t-\t0\t-\t-\t-\t-\t-\tnone\tnone\tzero\t0\t0",
                                              dataFrames_ + 1, emitted_, emitted_, emitted_ % 17);
            require(length > 0 && static_cast<std::size_t>(length) < expected.size(), "expected row bound");
            equal(row, {expected.data(), static_cast<std::size_t>(length)});
            ++emitted_;
            --frameRemaining_;

            return;
        }

        if (emitted_ < records_) {
            frameRecords_ = std::min(source_fixture::RECORDS_PER_FRAME, records_ - emitted_);
            const auto payload = frameRecords_ * 3;
            const auto physical = payload + (payload + 1 <= 63 ? 2 : 3);
            ++dataFrames_;
            equal(row, "frame\t" + std::to_string(dataFrames_ + 1) + "\t" + std::to_string(offset_) + "\t" +
                           std::to_string(offset_ + physical) + "\t15\t" +
                           std::to_string(offset_ + physical - payload) + "\t" + std::to_string(payload));
            offset_ += physical;
            frameRemaining_ = frameRecords_;
            timing_ = true;

            return;
        }

        const auto counts = std::to_string(sourceBytes_) + "\t" + std::to_string(dataFrames_ + 2) + "\t" +
                            std::to_string(records_) + "\t" + std::to_string(records_) + "\t" +
                            std::to_string(records_);

        if (!ended_) {
            equal(row, "source-end\t0\t" + counts + "\t" + std::to_string(sourceBytes_) + "\t" +
                           std::to_string(sourceBytes_));
            ended_ = true;

            return;
        }

        require(!complete_, "duplicate completion");
        equal(row, "complete-v9\t1\t" + counts);
        complete_ = true;
    }

    protected:
    /** Consume only bounded line storage; no complete output or input is retained.
     * @param bytes Borrowed output bytes for this write only.
     * @param size Nonnegative count of available bytes.
     * @return Consumed count; a short result reports the fixed line-capacity failure.
     */
    std::streamsize xsputn(const char *bytes, std::streamsize size) override {
        for (std::streamsize i = 0; i < size; ++i) {
            ++encodedBytes_;

            if (bytes[i] == '\n') {
                maximumLine_ = std::max(maximumLine_, lineSize_);
                accept({line_.data(), lineSize_});
                lineSize_ = 0;
            } else {
                if (lineSize_ == line_.size()) {
                    valid_ = false;

                    return i;
                }

                line_[lineSize_++] = bytes[i];
            }
        }

        return size;
    }

    /** Consume one optional output byte.
     * @param value One byte or the streambuf EOF sentinel.
     * @return Accepted byte/non-EOF sentinel, or EOF on a short write.
     */
    int_type overflow(int_type value) override {
        if (traits_type::eq_int_type(value, traits_type::eof())) {
            return traits_type::not_eof(value);
        }

        const auto byte = traits_type::to_char_type(value);

        return xsputn(&byte, 1) == 1 ? value : traits_type::eof();
    }

    public:
    /** Borrow only fixture identity and independently known physical source size.
     * @param path Explicit fixture identity, encoded and then released.
     * @param records Exact independently generated record count.
     * @param bytes Exact independently measured physical source bytes.
     */
    CanonicalSink(const std::filesystem::path &path, std::size_t records, std::uint64_t bytes)
        : records_(records), sourceBytes_(bytes) {
        pathUnits_ = path.native().size();
        pathHexBytes_ = nativeHex(path).size();

        const auto header = source_fixture::header();
        const auto description = source_fixture::description();
        offset_ = header.size() + description.size();
        preamble_ = {"format\tqdcxx-tape-inspect\t9\tbinary-catalog-v1\tself-contained-segments-v1",
#ifdef _WIN32
                     "source\t0\tutf16le\t" + nativeHex(path) + "\t-",
#else
                     "source\t0\tbytes\t" + nativeHex(path) + "\t-",
#endif
                     "source-begin\t0",
                     "source-policy\tnone\tnone\t-",
                     "frame\t0\t0\t" + std::to_string(header.size()) + "\t1\t2\t" + std::to_string(header.size() - 2),
                     "descriptor-v8\t0\t0\t-\t1\t-\t1\t004e004f004e0045\t32768",
                     "frame\t1\t" + std::to_string(header.size()) + "\t" + std::to_string(offset_) + "\t2\t" +
                         std::to_string(header.size() + 2) + "\t" + std::to_string(description.size() - 2),
                     "schema\t1\t1\t1\t1",
                     "schema-record\t1\t0\t0052\t0\t1",
                     "schema-field\t1\t0\t0\t0056\t8",
                     "wire-record\t1\t0\t0\t0\t0052\t1",
                     "wire-field\t1\t0\t0\t0056\t8\t0",
                     "schema-end\t1"};
    }

    /** Check complete byte equality and inventory after the owning call has returned.
     * @throws std::runtime_error If any row, final state, byte count or inventory differs.
     */
    void verify() const {
        if (!failure_.empty()) {
            std::cerr << failure_ << '\n';
        }

        require(valid_ && complete_ && lineSize_ == 0 && emitted_ == records_ && offset_ == sourceBytes_,
                "canonical equality");
        require(resourceNames_.size() == 106, "full resource inventory");
    }

    /** @return Actual largest complete output line, excluding LF, in bytes. */
    std::size_t maximumLine() const noexcept {
        return maximumLine_;
    }

    /** @return All encoded bytes actually examined, including every LF and resource row. */
    std::uint64_t encodedBytes() const noexcept {
        return encodedBytes_;
    }
};

/** Resolve one literal finite real-file occurrence; no aggregate inference/refill is used.
 * @param path Explicit owned temporary physical source.
 * @param records Exact generated count; cumulative capacities scale while peak buffers stay fixed.
 * @param bytes Independently measured physical source length.
 * @param chunk Positive parser produced-span capacity in bytes.
 * @return Fully resolved single-occurrence specification and its explicit aggregate vector.
 */
std::pair<ti::TapeSegmentSpec, ti::TapeSequenceOptions>
configuration(const std::filesystem::path &path, std::size_t records, std::uint64_t bytes, std::size_t chunk) {
    ti::TapeSegmentSpec segment;

    segment.path = path;
    segment.limits.chunkSize = chunk;
    segment.limits.output = SOURCE_OUTPUT_BYTES;
    auto &limits = segment.limits.tape;
    limits.inputBytes = static_cast<std::size_t>(bytes);
    limits.frame.maximumBodyBytes = 4096;
    limits.frames = (records + source_fixture::RECORDS_PER_FRAME - 1) / source_fixture::RECORDS_PER_FRAME + 2;
    limits.data.processedRecords = records;
    limits.data.wireFields = records;
    limits.data.emittedValues = records;
    limits.constructionUnits = records * 128 + 1048576;
    limits.timingEvents = limits.frames;
    limits.snapshotUnits = 65536;
    require(ti::prepareLimits(segment.limits), "prepare limits");
    segment.containerLimits.compressedBytes = bytes;
    segment.containerLimits.inflatedBytes = bytes;
    segment.containerLimits.workBytes = bytes * 3 + 4096;
    ti::TapeSequenceOptions options;
    options.time = ti::SourceTimePolicy::NONE;
    options.aggregate.tape = segment.limits.tape;
    options.aggregate.dataContainer = segment.containerLimits;
    options.aggregate.chunkBytes = chunk;
    options.aggregate.outputBytes = SOURCE_OUTPUT_BYTES;
    options.aggregate.eventUnits = segment.limits.eventUnits;
    options.aggregate.physicalProbes = 1;
    options.aggregate.inflatedProbes = 1;
    options.outputBytes = GLOBAL_OUTPUT_BYTES;

    return {std::move(segment), options};
}

/** Parse a positive internal-runner count with checked decimal conversion.
 * @param text Borrowed decimal count, allowed1..3000000.
 * @return Representable positive count; throws for invalid input.
 */
std::size_t count(std::string_view text) {
    std::size_t result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && result != 0 &&
                result <= DEFAULT_RECORDS,
            "runner count");

    return result;
}
} // namespace

/** Run bounded real-file acceptance or write a small fixture for the actual CLI process test.
 * Invocation: qdcxx_large_tape_tests RUN_DIRECTORY [RECORDS [CHUNK]], or --fixture PATH RECORDS.
 * Owns only a newly created named temporary file and removes it after a successful comparison.
 * A failed run retains its source for diagnosis; no child processes are created.
 * @param argc Internal test arguments.
 * @param argv Borrowed paths and positive decimal values.
 * @return Zero only after all canonical bytes, owning completion and physical cleanup pass.
 */
int main(int argc, char **argv) {
    try {
#ifdef _WIN32
        require(_configure_wide_argv(_crt_argv_unexpanded_arguments) == 0 && __wargv != nullptr && __argc == argc,
                "native arguments");
        const auto pathArgument = [](int index) {
            return std::filesystem::path(__wargv[index]);
        };
#else
        const auto pathArgument = [argv](int index) {
            return std::filesystem::path(argv[index]);
        };
#endif

        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout
                << "qdcxx_large_tape_tests RUN_DIRECTORY [RECORDS [CHUNK]]\n"
                   "qdcxx_large_tape_tests --fixture PATH RECORDS\n"
                   "qdcxx_large_tape_tests --existing-file OWNED_PATH RECORDS (consumes and removes this explicit "
                   "fixture)\n"
                   "Defaults: records3000000, chunks1/4096. Counts1..3000000; finite real temporary source.\n"
                   "Exit0 equality/fixture success;1 failure;2 usage. Failures retain the source; no child process.\n";

            return std::cout.flush() ? 0 : 1;
        }

        if (argc == 4 && std::string_view(argv[1]) == "--fixture") {
            const auto bytes = source_fixture::write(pathArgument(2), count(argv[3]));
            std::cout << "fixture-bytes=" << bytes << '\n';

            return 0;
        }

        if (argc == 5 && std::string_view(argv[1]) == "--verify") {
            const auto path = pathArgument(2);
            const auto bytes = std::filesystem::file_size(path);
            CanonicalSink validator(path, count(argv[4]), bytes);
            std::ifstream input(pathArgument(3), std::ios::binary);
            require(static_cast<bool>(input), "canonical input open");
            std::array<char, 4096> buffer{};

            while (input) {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const auto size = input.gcount();
                require(validator.sputn(buffer.data(), size) == size, "canonical input bound");
            }

            require(input.eof() && !input.bad(), "canonical input read");
            input.clear();
            input.close();
            require(static_cast<bool>(input), "canonical input close");
            validator.verify();

            return 0;
        }

        if (argc < 2 || argc > 4) {
            return 2;
        }

        const bool existing = argc == 4 && std::string_view(argv[1]) == "--existing-file";
        const auto records = existing ? count(argv[3]) : argc >= 3 ? count(argv[2]) : DEFAULT_RECORDS;
        const auto selectedChunk = !existing && argc == 4 ? count(argv[3]) : 0;
        auto path = existing ? pathArgument(2) : pathArgument(1) / "bounded-generated-records.qtp";

        if (!existing) {
            std::filesystem::create_directories(path.parent_path());
        }

        require(existing || !std::filesystem::exists(path), "temporary source already exists");
        const auto bytes = existing ? std::filesystem::file_size(path) : source_fixture::write(path, records);

        for (const auto chunk :
             selectedChunk == 0 ? std::vector<std::size_t>{1, 4096} : std::vector<std::size_t>{selectedChunk}) {
            CanonicalSink validator(path, records, bytes);
            std::ostream output(&validator);
            std::ostringstream diagnostic;
            auto [segment, options] = configuration(path, records, bytes, chunk);

            const auto result = ti::inspectSequence(std::span(&segment, 1), output, diagnostic, options);

            if (!diagnostic.str().empty()) {
                std::cerr << diagnostic.str();
            }

            require(result.status == ti::SequenceStatus::COMPLETE && result.primary.exitCode == 0 &&
                        diagnostic.str().empty(),
                    "owning completion");
            require(result.counters.emitted == records && result.counters.processed == records &&
                        result.counters.wireFields == records && result.counters.accepted == bytes &&
                        result.counters.sources == 1 && result.maximumOpenHandles == 1,
                    "semantic counters");
            validator.verify();
            require(result.reserved.tape.frame.maximumBodyBytes == 4096 &&
                        result.reserved.tape.snapshotUnits == 65536 && result.reserved.chunkBytes == chunk &&
                        segment.containerLimits.retainedBytes == 4194304,
                    "fixed peak capacity inventory");
            std::cout << "records=" << records << " chunk=" << chunk << " source-bytes=" << bytes
                      << " canonical-bytes=" << validator.encodedBytes()
                      << " maximum-line-bytes=" << validator.maximumLine()
                      << " frame-record-capacity=" << source_fixture::RECORDS_PER_FRAME
                      << " result=COMPLETE byte-equality=PASS\n";
            std::cout.flush();
            require(static_cast<bool>(std::cout), "runner output flush");
        }

        require(std::filesystem::remove(path), "owned source cleanup");

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
