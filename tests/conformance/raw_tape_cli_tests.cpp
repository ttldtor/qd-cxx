// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/encoding/compact.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace e = org::ttldtor::qdcxx::encoding;
using Cells = std::vector<std::string>; ///< Exact TSV cells, retaining empty cells.

/** Reject an invalid observation without accepting a partial comparison.
 * @param condition Required invariant.
 * @param message Failure explanation.
 */
void need(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Read bytes without newline translation.
 * @param path Existing artifact.
 * @return Owned bytes; throws on IO failure.
 */
std::string read(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    need(input.good(), "open " + path.string());

    return {std::istreambuf_iterator<char>(input), {}};
}

/** Split an exact delimiter-separated sequence.
 * @param text Complete input.
 * @param delimiter Separator.
 * @return Ordered cells including empty cells.
 */
Cells split(const std::string &text, char delimiter = '\t') {
    Cells result;
    std::size_t start = 0;

    for (;;) {
        const auto end = text.find(delimiter, start);
        result.push_back(text.substr(start, end - start));

        if (end == std::string::npos) {
            return result;
        }

        start = end + 1;
    }
}

/** Require canonical signed decimal.
 * @param text Entire cell.
 * @return Exact int64 value or an exception.
 */
std::int64_t number(const std::string &text) {
    std::int64_t value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    need(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::to_string(value) == text,
         "noncanonical decimal: " + text);

    return value;
}

/** Parse a LF TSV artifact with no blank/partial rows or BOM.
 * @param text Complete artifact.
 * @return Ordered rows; throws for invalid framing.
 */
std::vector<Cells> rows(const std::string &text) {
    need(!text.empty() && text.back() == '\n' && text.find('\r') == std::string::npos, "TSV newline grammar");
    auto lines = split(text.substr(0, text.size() - 1), '\n');
    std::vector<Cells> result;

    for (const auto &line : lines) {
        need(!line.empty(), "blank TSV row");
        result.push_back(split(line));
    }

    return result;
}

/** Read platform-native Java line endings without weakening CLI LF validation.
 * @param text Captured Java TSV bytes.
 * @return Parsed evidence rows.
 */
std::vector<Cells> artifactRows(std::string text) {
    std::string normalized;

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
            continue;
        }

        normalized += text[i];
    }

    return rows(normalized);
}

/** Exact row comparison includes arity, order, canonical values and sentinels.
 * @param actual Parsed observed row.
 * @param expected Required row.
 */
void equal(const Cells &actual, const Cells &expected) {
    need(actual == expected, "row mismatch: " + actual.front() + " expected " + expected.front());
}

/** Captured STREAM callback record, independent of the production decoder. */
struct Record {
    Cells row;                 ///< Original Java record cells.
    std::vector<Cells> fields; ///< All fields in Java local order.
    Cells mapped;              ///< Optional QuoteDelegate mapping.
};

/** Checked exact output cursor. */
class Cursor {
    const std::vector<Cells> &rows_; ///< Borrowed parsed output.
    std::size_t position_ = 0;       ///< Next unconsumed row.
    public:
    /** Borrow output for one comparison.
     * @param value Parsed output alive throughout comparison.
     */
    explicit Cursor(const std::vector<Cells> &value) : rows_(value) {
    }

    /** Consume one exact required row.
     * @param expected Exact cells.
     */
    void take(const Cells &expected) {
        need(position_ < rows_.size(), "missing " + expected.front());
        equal(rows_[position_++], expected);
    }

    /** Require full consumption, rejecting extra completion or unknown rows. */
    void finish() const {
        need(position_ == rows_.size(), "unexpected trailing rows");
    }
};

/** Compare v3 output against Java production semantic projection and exact envelope evidence.
 * @param directory Immutable Java fixture directory.
 * @param scenario Fixture stem.
 * @param actual Complete CLI stdout.
 */
void compare(const std::filesystem::path &directory, const std::string &scenario, const std::string &actual) {
    const auto oracle = artifactRows(read(directory / (scenario + ".tsv")));
    const auto frames = artifactRows(read(directory / (scenario + "-frames.tsv")));
    const auto binary = read(directory / (scenario + ".qtp"));
    const auto output = rows(actual);
    Cursor cursor(output);
    std::vector<Record> records;
    Cells timestamps;
    std::string version = "-", time = "", options = "absent", send = "-";
    bool selected = false;

    for (const auto &row : oracle) {
        if (row[0] == "descriptor") {
            need(row.size() == 6, "oracle descriptor arity");
            send = row[2] == "[RAW_DATA]" ? "5" : "-";
        } else if (row[0] == "property") {
            need(row.size() == 3, "oracle property arity");

            if (row[1] == "00760065007200730069006f006e") {
                version = row[2];
            }

            if (row[1] == "00740069006d0065") {
                time = row[2] == "006d006500730073006100670065" ? "message" : "MESSAGE";
            }

            if (row[1] == "006f00700074") {
                options = row[2] == "-" ? "empty" : "hs";
            }
        } else if (row[0] == "heartbeat") {
            need(row.size() == 7, "oracle heartbeat arity");
            timestamps.push_back(row[2]);
        } else if (row[0] == "record") {
            need(row.size() == 8, "oracle record arity");
            selected = row[2] == "15";

            if (selected) {
                records.push_back({row, {}, {}});
            }
        } else if (row[0] == "field" && selected) {
            need(row.size() == 5 && row[1] == records.back().row[1] &&
                     number(row[2]) == static_cast<std::int64_t>(records.back().fields.size()),
                 "oracle field ordering");
            records.back().fields.push_back(row);
        } else if (row[0] == "mapped" && selected) {
            need(row.size() == 13 && row[1] == records.back().row[1], "oracle mapped arity/index");
            records.back().mapped = row;
        }
    }

    need(!records.empty(), "scenario must contain production records");
    // RawTapeOracle.production/connector publish singletons; focused copies those singleton frames.
    // This fixture restriction must come from the producer, never from rows in the actual CLI output.
    const auto rawFrames = std::count_if(frames.begin(), frames.end(), [](const Cells &frame) {
        return frame.size() == 5 && frame[0] == "frame" && frame[4] == "5";
    });
    need(static_cast<std::size_t>(rawFrames) == records.size(), "fixture requires one record per RAW frame");
    const bool quote = records.front().row[4] == "00510075006f00740065";
    cursor.take({"format", "qdcxx-tape-inspect", "3", quote ? "default-quote-v1" : "oracle-sre-v1", "raw-message-v1"});
    std::size_t recordIndex = 0, heartbeatIndex = 0, markerCount = 0, fieldCount = 0, frameCount = 0;
    std::optional<std::int64_t> timestamp;
    std::int64_t sequence = 0, eventTime = 0;

    for (const auto &frame : frames) {
        if (frame[0] == "complete") {
            break;
        }

        need(frame.size() == 5 && number(frame[1]) == static_cast<std::int64_t>(frameCount), "oracle frame order");
        ++frameCount;
        const auto start = static_cast<std::size_t>(number(frame[2]));
        const auto end = static_cast<std::size_t>(number(frame[3]));
        need(start < end && end <= binary.size(), "oracle frame range");
        auto bytes = std::as_bytes(std::span(binary)).subspan(start, end - start);
        const auto length = e::readCompactInt(bytes);
        need(length.status == e::Status::OK, "envelope length");
        auto payloadStart = start + length.consumed;
        const bool marker = frame[4] == "-1";

        if (!marker) {
            const auto type =
                e::readCompactInt(std::as_bytes(std::span(binary)).subspan(payloadStart, end - payloadStart));
            need(type.status == e::Status::OK && std::to_string(type.value) == frame[4], "envelope type");
            payloadStart += type.consumed;
        }

        cursor.take({"frame", frame[1], frame[2], frame[3], marker ? "-" : frame[4],
                     marker ? "-" : std::to_string(payloadStart), std::to_string(end - payloadStart)});

        if (frameCount == 1) {
            cursor.take({"descriptor-v3", version == "-" ? "0" : "1", version, time, send, "-", options});
        } else if (marker) {
            ++markerCount;
        } else if (frame[4] == "0") {
            std::string content = "-", raw = "-";

            if (payloadStart != end) {
                const auto bits =
                    e::readCompactInt(std::as_bytes(std::span(binary)).subspan(payloadStart, end - payloadStart));
                need(bits.status == e::Status::OK && (bits.value == 0 || bits.value == 1), "heartbeat bits");
                content = std::to_string(bits.value);
                need(heartbeatIndex < timestamps.size(), "missing Java heartbeat");
                raw = timestamps[heartbeatIndex++];

                if (bits.value == 1) {
                    timestamp = number(raw);
                    auto seconds = *timestamp / 1000;
                    auto millis = *timestamp % 1000;

                    if (millis < 0) {
                        --seconds;
                        millis += 1000;
                    }

                    // Java saturates signed seconds, retaining the original millisecond remainder.
                    seconds = std::clamp<std::int64_t>(seconds, INT32_MIN, INT32_MAX);
                    sequence = std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(seconds) << 32) |
                                                           (static_cast<std::uint64_t>(millis) << 22));
                    eventTime = seconds * 1000 + millis;
                }
            }

            cursor.take({"heartbeat", frame[1], content, raw});
        } else if (frame[4] == "5") {
            cursor.take({"timing", frame[1], timestamp ? std::to_string(*timestamp) : "-", std::to_string(sequence),
                         std::to_string(eventTime)});

            need(recordIndex < records.size(), "extra record");
            const auto &record = records[recordIndex];
            need(number(record.row[7]) == sequence && record.row[6] == "0", "Java cursor timing/flags");
            std::string values;

            for (const auto &field : record.fields) {
                if (!values.empty()) {
                    values += ',';
                }

                values += quote ? field[3] + ":" + field[4] : field[4];
            }

            cursor.take({quote ? "typed-record" : "record", frame[1], std::to_string(recordIndex), record.row[3],
                         record.row[4], record.row[5], values.empty() ? "-" : values});

            if (quote) {
                const auto &m = record.mapped;
                need(m.size() == 13 && number(m[10]) == eventTime, "Java Quote event time");
                cursor.take({"quote", std::to_string(recordIndex), m[2], m[4], m[6], m[7], m[3], m[5], m[8], m[9],
                             std::to_string(std::max(number(m[2]), number(m[3]))), m[11], m[12], m[10]});
            }

            fieldCount += record.fields.size();
            ++recordIndex;
        }
    }

    need(recordIndex == records.size() && heartbeatIndex == timestamps.size(), "missing Java records/heartbeats");
    cursor.take({"markers", std::to_string(markerCount)});
    cursor.take({"complete", std::to_string(binary.size()), std::to_string(frameCount), std::to_string(recordIndex),
                 std::to_string(recordIndex), std::to_string(fieldCount)});
    cursor.finish();
}
} // namespace

/** Run one strict artifact comparison; this is an internal test executable.
 * @param argc Argument count.
 * @param argv Fixture directory, scenario stem and CLI stdout file.
 * @return Zero only for a fully consumed exact comparison.
 */
int main(int argc, char **argv) {
    try {
        need(argc == 4, "usage: comparator fixture-directory scenario stdout-file");
        compare(argv[1], argv[2], read(argv[3]));

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
