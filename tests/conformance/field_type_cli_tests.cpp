// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "evidence_cells.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace e = org::ttldtor::qdcxx::encoding;
using org::ttldtor::qdcxx::test_support::hexUnits;
using org::ttldtor::qdcxx::test_support::split;
using Cells = std::vector<std::string>; ///< Exact ordered observation cells.

/** Require an independent expectation invariant.
 * @param condition Required fact.
 * @param message Failure detail.
 */
void need(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Read immutable binary evidence.
 * @param path Artifact path.
 * @return Exact bytes, or throw on IO failure.
 */
std::string read(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    need(input.good(), "open " + path.string());

    return {std::istreambuf_iterator<char>(input), {}};
}

/** Parse canonical signed decimal.
 * @param text Whole decimal cell.
 * @return Exact int64 value or throw.
 */
std::int64_t number(const std::string &text) {
    std::int64_t value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    need(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::to_string(value) == text,
         "decimal grammar");

    return value;
}

/** Decode canonical UTF-16 hexadecimal evidence.
 * @param text Hex cell; dash denotes empty.
 * @return Exact owned units.
 */
std::u16string units(const std::string &text) {
    return org::ttldtor::qdcxx::test_support::decodeUnits(text, "unit grammar");
}

/** Pin checker grammar before sharing its mechanical TSV and UTF-16 operations. */
void checkCellGrammar() {
    need(split("", '\t') == Cells{""}, "empty TSV row");
    need(split("a\t\t", '\t') == Cells{"a", "", ""}, "trailing empty TSV cells");
    const std::u16string exact{0, 0xd800, 0xdfff};
    need(units("-").empty() && hexUnits(u"") == "-", "empty UTF-16 sentinel");
    need(units("0000d800dfff") == exact && hexUnits(exact) == "0000d800dfff", "exact UTF-16 units");

    for (const std::string malformed : {"", "000", "00000", "D800", "000g", "null"}) {
        bool rejected = false;

        try {
            static_cast<void>(units(malformed));
        } catch (const std::runtime_error &error) {
            rejected = std::string_view(error.what()) == "unit grammar";
        }

        need(rejected, "malformed UTF-16 cell accepted or diagnostic changed");
    }

    need(number("0") == 0 && number("-1") == -1, "canonical decimal cells");

    for (const std::string malformed : {"01", "-0", "+1", " 1", "1 ", "1x", "", "9223372036854775808"}) {
        bool rejected = false;

        try {
            static_cast<void>(number(malformed));
        } catch (const std::runtime_error &error) {
            rejected = std::string_view(error.what()) == "decimal grammar";
        }

        need(rejected, "noncanonical decimal cell accepted or diagnostic changed");
    }
}

/** Render only the measured fixed mixed-fixture text domain, independently of Formatter.
 * @param text Exact observed units.
 * @return Expected safely quoted bytes; unknown text fails rather than guessing output.
 */
std::string expectedQuote(std::u16string_view text) {
    if (text == std::u16string{u'A', 0, 0xd83d, 0xde00, 0xd800, u'\n'}) {
        return "\"A\\u0000\xf0\x9f\x98\x80\\uD800\\n\"";
    }

    std::string out = "\"";

    for (auto unit : text) {
        need(unit >= 0x20 && unit <= 0x7e && unit != u'"' && unit != u'\\', "unexpected fixture text domain");
        out += static_cast<char>(unit);
    }

    return out + '"';
}

/** Java STREAM projection of a delivered logical record. */
struct Record {
    Cells header;              ///< Exact Java identity, symbol, flags and timing.
    std::vector<Cells> fields; ///< Exact complete logical values.
};

/** Parse all Java observation rows with strict identity/order/shape checks.
 * @param path Original Java output.
 * @param stem Required case identity.
 * @return STREAM records; RAW callback fanout is validated but not multiplied.
 */
std::vector<Record> observations(const std::filesystem::path &path, const std::string &stem) {
    std::ifstream input(path);
    need(input.good(), "Java observation open");
    std::vector<Record> all;
    bool ended = false, complete = false;
    std::size_t row = 0;
    std::string line;
    constexpr std::array<int, 9> TYPES{10, 40, 72, 88, 8, 2, 56, 120, 10};
    constexpr std::array<std::u16string_view, 9> NAMES{u"Text", u"Short",   u"Sequence", u"Date", u"Count",
                                                       u"Char", u"Seconds", u"Wide",     u"Tail"};

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        need(!complete, "row after completion");
        auto cells = split(line, '\t');

        if (row++ == 0) {
            need(cells == Cells{"case", stem}, "Java header");
        } else if (cells[0] == "record") {
            need(!ended && cells.size() == 8 && number(cells[1]) == static_cast<std::int64_t>(all.size()),
                 "Java record order");
            need(all.empty() || all.back().fields.size() == TYPES.size(), "missing Java field");
            const auto callback =
                stem == "mixed-raw" ? std::array<int, 9>{10, 15, 20, 10, 10, 15, 15, 20, 20}.at(all.size()) : 15;
            need(number(cells[2]) == callback && cells[3] == "0" && units(cells[4]) == u"FieldTypes" &&
                     units(cells[5]) == u"IBM" && cells[6] == "0",
                 "Java record identity");
            all.push_back({std::move(cells), {}});
        } else if (cells[0] == "field") {
            need(!ended && !all.empty() && cells.size() == 7, "Java field grammar");
            auto &record = all.back();
            const auto i = record.fields.size();
            need(i < TYPES.size() && cells[1] == record.header[1] && number(cells[2]) == static_cast<std::int64_t>(i) &&
                     units(cells[3]) == NAMES[i] && number(cells[4]) == TYPES[i],
                 "Java field identity");
            const auto raw = number(cells[5]);

            if (TYPES[i] == 10) {
                need(raw == 0, "string raw");

                if (cells[6] != "null") {
                    static_cast<void>(units(cells[6]));
                }
            } else {
                need(cells[6] == "-", "scalar payload");
            }

            record.fields.push_back(std::move(cells));
        } else if (cells[0] == "end") {
            need(!ended && cells.size() == 3 && number(cells[1]) == static_cast<std::int64_t>(all.size()) &&
                     cells[2] == "0" && !all.empty() && all.back().fields.size() == TYPES.size(),
                 "Java end");
            ended = true;
        } else {
            need(ended && cells == Cells{"complete", stem}, "unknown Java row/completion");
            complete = true;
        }
    }

    need(complete && all.size() == (stem == "mixed-raw" ? 9U : 3U), "Java completion count");
    std::vector<Record> out;

    for (auto &record : all) {
        if (record.header[2] == "15") {
            out.push_back(std::move(record));
        }
    }

    return out;
}

/** Original immutable envelope boundaries, independent of actual CLI output. */
struct Frame {
    std::size_t start, end, payload; ///< Original byte positions.
    std::int64_t type;               ///< Minus one for marker.
};

/** Scan only original framing with bounded primitive reads.
 * @param binary Original complete tape.
 * @return All exact envelope positions.
 */
std::vector<Frame> frames(const std::string &binary) {
    const auto bytes = std::as_bytes(std::span(binary));
    std::vector<Frame> result;
    std::size_t at = 0;

    while (at < bytes.size()) {
        const auto start = at;
        const auto length = e::readCompactLong(bytes.subspan(at));
        need(length.status == e::Status::OK && length.value >= 0, "frame length");
        at += length.consumed;
        need(static_cast<std::uint64_t>(length.value) <= bytes.size() - at, "frame extent");
        const auto end = at + static_cast<std::size_t>(length.value);
        std::int64_t type = -1;

        if (length.value != 0) {
            const auto id = e::readCompactInt(bytes.subspan(at, end - at));
            need(id.status == e::Status::OK, "frame type");
            type = id.value;
            at += id.consumed;
        }

        result.push_back({start, end, at, type});
        at = end;
    }

    return result;
}

/** Read independently bounded descriptor primitives.
 * @param bytes Complete descriptor payload.
 * @return Exact text properties; validates required send/receive shape separately.
 */
std::map<std::u16string, std::u16string> descriptor(std::span<const std::byte> bytes) {
    need(bytes.size() >= 4, "descriptor magic");
    need(std::string(reinterpret_cast<const char *>(bytes.data()), 4) == "DXP3", "descriptor magic");
    bytes = bytes.subspan(4);
    const auto integer = [&]() {
        const auto value = e::readCompactInt(bytes);
        need(value.status == e::Status::OK, "descriptor int");
        bytes = bytes.subspan(value.consumed);

        return value.value;
    };
    const auto text = [&]() {
        const auto value = e::readUtfString(bytes, {4096, 1024});
        need(value.status == e::Status::OK && value.value.has_value(), "descriptor text");
        bytes = bytes.subspan(value.consumed);

        return *value.value;
    };
    std::map<std::u16string, std::u16string> props;
    const auto count = integer();
    need(count >= 0 && count <= 16, "descriptor count");

    for (int i = 0; i < count; ++i) {
        auto key = text();
        auto value = text();
        need(props.emplace(key, value).second, "duplicate property");
    }

    need(integer() == 1, "fixture send count");
    const bool raw = props.contains(u"time") && props.at(u"time") == u"message";
    need(integer() == (raw ? 5 : 15), "fixture send id");
    need(text() == (raw ? u"RAW_DATA" : u"STREAM_DATA"), "fixture send name");
    need(integer() == 0 && integer() == 0 && bytes.empty(), "fixture send properties/receive shape");
    need(props.at(u"type") == u"tape" && props.at(u"opt").empty(), "fixture header policy");

    return props;
}

/** Construct the exact specified output from Java fields and original frames, never actual output grouping.
 * @param root Immutable fixture directory.
 * @param stem mixed-stream or mixed-raw singleton producer.
 * @param human Whether to build the independent human expectation.
 * @return Entire required stdout, including exact completion/order.
 */
std::string expected(const std::filesystem::path &root, const std::string &stem, bool human) {
    need(stem == "mixed-stream" || stem == "mixed-raw", "unsupported test case");
    const bool raw = stem == "mixed-raw";
    const std::string mode = raw ? "raw-message-v1" : "legacy";
    const auto records = observations(root / (stem + ".tsv"), stem);
    const auto binary = read(root / (stem + ".qtp"));
    const auto envelope = frames(binary);
    const auto bytes = std::as_bytes(std::span(binary));
    need(!envelope.empty() && envelope.front().type == 1, "initial descriptor");
    const auto props = descriptor(bytes.subspan(envelope[0].payload, envelope[0].end - envelope[0].payload));
    std::string out =
        human ? "qdcxx-tape-inspect human 4 profile=oracle-field-types-v1 tape-mode=" + mode + " identities=local\n"
              : "format\tqdcxx-tape-inspect\t4\toracle-field-types-v1\t" + mode + "\n";
    std::size_t delivered = 0, markers = 0;
    std::string timestamp = "-";
    std::int64_t sequence = 0;

    for (std::size_t i = 0; i < envelope.size(); ++i) {
        const auto &f = envelope[i];
        const auto id = f.type < 0 ? "-" : std::to_string(f.type);
        const auto payload = f.type < 0 ? "-" : std::to_string(f.payload);
        const auto size = std::to_string(f.end - f.payload);

        if (human) {
            const std::string label = f.type < 0    ? "EMPTY_HEARTBEAT"
                                      : f.type == 1 ? "DESCRIBE_PROTOCOL"
                                      : f.type == 2 ? "DESCRIBE_RECORDS"
                                      : f.type == 0 ? "HEARTBEAT"
                                      : raw         ? "RAW_DATA"
                                                    : "STREAM_DATA";
            out += "Frame " + std::to_string(i) + ": " + label + " (" + id + ") bytes=[" + std::to_string(f.start) +
                   "," + std::to_string(f.end) + ") payload=" + payload + " size=" + size + "\n";
        } else {
            out += "frame\t" + std::to_string(i) + "\t" + std::to_string(f.start) + "\t" + std::to_string(f.end) +
                   "\t" + id + "\t" + payload + "\t" + size + "\n";
        }

        if (i == 0) {
            const auto version = props.find(u"version");
            const auto v = version == props.end() ? std::u16string{} : version->second;
            const auto time = props.find(u"time");
            need(time == props.end() || time->second == u"message", "fixture time domain");
            const std::string t = time == props.end() ? "absent" : "message";

            if (human) {
                out += "Descriptor: type=tape opt=" + std::string(raw ? "empty" : "EMPTY_SET") +
                       " version=" + (version == props.end() ? "absent" : expectedQuote(v)) + " time=" + t +
                       " send=" + (raw ? "5" : "15") + " receive=-\n";
            } else {
                out += std::string(raw ? "descriptor-v3\t" : "descriptor\t") + (version == props.end() ? "0" : "1") +
                       "\t" + hexUnits(v) + "\t" + t + "\t" + (raw ? "5" : "15") + "\t-" + (raw ? "\tempty" : "") +
                       "\n";
            }
        } else if (f.type < 0) {
            ++markers;
        } else if (f.type == 0) {
            need(raw, "unexpected heartbeat");
            auto span = bytes.subspan(f.payload, f.end - f.payload);
            auto bits = e::readCompactInt(span);
            need(bits.status == e::Status::OK && bits.value == 1, "measured heartbeat content");
            auto time = e::readCompactLong(span.subspan(bits.consumed));
            need(time.status == e::Status::OK && bits.consumed + time.consumed == span.size() &&
                     time.value == 1600000000123LL,
                 "measured time");
            timestamp = "1600000000123";
            sequence = 6871947674115899392LL;
            out += human ? "Heartbeat: frame=" + std::to_string(i) + " content=1 timestampMs=" + timestamp + "\n"
                         : "heartbeat\t" + std::to_string(i) + "\t1\t" + timestamp + "\n";
        } else if (f.type == (raw ? 5 : 15)) {
            need(delivered < records.size(), "extra original data frame");
            const auto &r = records[delivered];
            need(number(r.header[7]) == sequence, "Java timing alignment");

            if (raw) {
                out += human ? "Timing: frame=" + std::to_string(i) + " timestampMs=" + timestamp +
                                   " eventTimeSequence=" + std::to_string(sequence) + " eventTimeMs=" + timestamp + "\n"
                             : "timing\t" + std::to_string(i) + "\t" + timestamp + "\t" + std::to_string(sequence) +
                                   "\t" + timestamp + "\n";
            }

            out += human ? "Fields " + std::to_string(delivered) + ": frame=" + std::to_string(i) +
                               " local=0 name=\"FieldTypes\" symbol=\"IBM\" fields={"
                         : "field-record\t" + std::to_string(i) + "\t" + std::to_string(delivered) + "\t0\t" +
                               r.header[4] + "\t" + r.header[5] + "\t";

            for (std::size_t j = 0; j < r.fields.size(); ++j) {
                const auto &v = r.fields[j];

                if (j) {
                    out += human ? ", " : ",";
                }

                if (human) {
                    out += "field[" + std::to_string(j) + "]=(type:" + v[4] + " raw:" + v[5];

                    if (v[4] == "10") {
                        out += " units:" + (v[6] == "null" ? "null" : expectedQuote(units(v[6])));
                    }

                    out += ")";
                } else {
                    out += v[4] + ":" + v[5] + ":" + (v[4] != "10" ? "-" : v[6] == "-" ? "empty" : v[6]);
                }
            }

            out += human ? "}\n" : "\n";
            ++delivered;
        } else {
            need(f.type == 2, "unexpected original frame");
        }
    }

    need(delivered == 3 && delivered == records.size(), "singleton frame producer contract");
    out += human ? "Markers: count=" + std::to_string(markers) + " authenticated-footer=no\n"
                 : "markers\t" + std::to_string(markers) + "\n";
    out += human
               ? "Complete: bytes=" + std::to_string(binary.size()) + " frames=" + std::to_string(envelope.size()) +
                     " records=3 charged-records=3 wire-fields=27\n"
               : "complete\t" + std::to_string(binary.size()) + "\t" + std::to_string(envelope.size()) + "\t3\t3\t27\n";

    return out;
}
} // namespace

/** Compare CLI bytes with an independently constructed oracle expectation.
 * @param argc Argument count.
 * @param argv Program, fixtures, stem, tsv/human, actual output file.
 * @return Zero only for exact semantic/grammar/byte equality; 1 failure, 2 usage.
 */
int main(int argc, char **argv) {
    if (argc != 5) {
        return 2;
    }

    try {
        checkCellGrammar();
        const std::string format = argv[3];
        need(format == "tsv" || format == "human", "format");
        const auto wanted = expected(argv[1], argv[2], format == "human");
        const auto actual = read(argv[4]);
        need(actual == wanted, "exact output differs from independent Java/frame expectation");

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
