// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "event_oracle.hpp"
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

/** Decode canonical UTF-16 hexadecimal evidence.
 * @param text Hex cell; dash denotes empty.
 * @return Exact owned units.
 */
std::u16string units(const std::string &text) {
    if (text == "-") {
        return {};
    }

    need(!text.empty() && text.size() % 4 == 0 && text.find_first_not_of("0123456789abcdef") == std::string::npos,
         "unit grammar");
    std::u16string out;

    for (std::size_t i = 0; i < text.size(); i += 4) {
        unsigned value{};
        const auto parsed = std::from_chars(text.data() + i, text.data() + i + 4, value, 16);
        need(parsed.ec == std::errc{}, "hex parse");
        out.push_back(static_cast<char16_t>(value));
    }

    return out;
}

/** Encode exact UTF-16 for the TSV expectation.
 * @param text Borrowed units.
 * @return Canonical hex or dash.
 */
std::string hex(std::u16string_view text) {
    if (text.empty()) {
        return "-";
    }

    std::string out;

    for (auto unit : text) {
        for (int shift : {12, 8, 4, 0}) {
            out += "0123456789abcdef"[(unit >> shift) & 15];
        }
    }

    return out;
}

/** Render only the measured fixed Profile-fixture text domain, independently of Formatter.
 * @param text Exact observed units.
 * @return Expected safely quoted bytes; unknown text fails rather than guessing output.
 */
std::string expectedQuote(std::u16string_view text) {
    if (text == std::u16string{u'A', 0, 0xd800, u'X', 0xdfff, 0xd83d, 0xde00}) {
        return "\"A\\u0000\\uD800X\\uDFFF\xf0\x9f\x98\x80\"";
    }

    if (text == u"\u0394TEST") {
        return "\"\xce\x94TEST\"";
    }

    std::string out = "\"";

    for (auto unit : text) {
        need(unit >= 0x20 && unit <= 0x7e && unit != u'"' && unit != u'\\', "unexpected fixture text domain");
        out += static_cast<char>(unit);
    }

    return out + '"';
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
 * @param stem Profile-stream or Profile-raw singleton producer.
 * @param human Whether to build the independent human expectation.
 * @return Entire required stdout, including exact completion/order.
 */
std::string expected(const std::filesystem::path &root, const std::string &stem, bool human) {
    need(stem == "Profile-stream" || stem == "Profile-raw" || stem == "Profile-wire-omitted", "unsupported test case");
    const bool raw = stem == "Profile-raw";
    const std::string mode = raw ? "raw-message-v1" : "legacy";
    const auto scenario = event_oracle::read(root / "observations.tsv");
    std::vector<event_oracle::Observation> records;

    for (const auto &observation : scenario.observations) {
        if (observation.phase == stem + "-read") {
            records.push_back(observation);
        }
    }

    const bool omitted = stem == "Profile-wire-omitted";
    need(records.size() == (omitted ? 1U : 3U), "Profile Java record count");
    const auto binary = read(root / (stem + ".qtp"));
    const auto envelope = frames(binary);
    const auto bytes = std::as_bytes(std::span(binary));
    need(!envelope.empty() && envelope.front().type == 1, "initial descriptor");
    const auto props = descriptor(bytes.subspan(envelope[0].payload, envelope[0].end - envelope[0].payload));
    std::string out =
        human ? "qdcxx-tape-inspect human 5 profile=default-profile-v1 tape-mode=" + mode + " identities=local\n"
              : "format\tqdcxx-tape-inspect\t5\tdefault-profile-v1\t" + mode + "\n";
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
                       "\t" + hex(v) + "\t" + t + "\t" + (raw ? "5" : "15") + "\t-" + (raw ? "\tempty" : "") + "\n";
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
            need(r.timeSequence == sequence, "Java timing alignment");

            if (raw) {
                out += human ? "Timing: frame=" + std::to_string(i) + " timestampMs=" + timestamp +
                                   " eventTimeSequence=" + std::to_string(sequence) + " eventTimeMs=" + timestamp + "\n"
                             : "timing\t" + std::to_string(i) + "\t" + timestamp + "\t" + std::to_string(sequence) +
                                   "\t" + timestamp + "\n";
            }

            const auto &values = r.values;
            const auto &event = r.event;
            need(values.size() == 16 && event.size() == 20, "Profile Java shape");
            constexpr std::array<std::string_view, 16> names{
                "Beta",         "Eps",    "DivFreq",     "ExdDiv.Amount",  "ExdDiv.Date",   "52High.Price",
                "52Low.Price",  "Shares", "FreeFloat",   "HighLimitPrice", "LowLimitPrice", "Halt.StartTime",
                "Halt.EndTime", "Flags",  "Description", "StatusReason"};
            out += human ? "Profile " + std::to_string(delivered) + ": frame=" + std::to_string(i) +
                               " local=0 name=\"Profile\" symbol=" + expectedQuote(units(event[0])) + " fields={"
                         : "profile-record\t" + std::to_string(i) + "\t" + std::to_string(delivered) +
                               "\t0\t00500072006f00660069006c0065\t" + event[0] + "\t";

            for (std::size_t j = 0; j < values.size(); ++j) {
                const auto &v = values[j];
                const auto type = std::to_string(std::to_underlying(v.type));
                const auto rawValue = std::to_string(v.raw);

                if (j) {
                    out += human ? ", " : ",";
                }

                if (human) {
                    out += std::string(names[j]) + "=(type:" + type + " raw:" + rawValue;

                    if (type == "10") {
                        out += " units:" + (v.units ? expectedQuote(*v.units) : "null");
                    }

                    out += ")";
                } else {
                    out += type + ":" + rawValue + ":" +
                           (type != "10"       ? "-"
                            : !v.units         ? "null"
                            : v.units->empty() ? "empty"
                                               : hex(*v.units));
                }
            }

            out += human ? "}\nProfileValues " : "\nprofile\t";
            out += std::to_string(delivered) + (human ? ":" : "");
            constexpr std::array<std::string_view, 19> labels{"beta",
                                                              "earningsPerShare",
                                                              "dividendFrequency",
                                                              "exDividendAmount",
                                                              "exDividendDayId",
                                                              "high52WeekPrice",
                                                              "low52WeekPrice",
                                                              "shares",
                                                              "freeFloat",
                                                              "highLimitPrice",
                                                              "lowLimitPrice",
                                                              "haltStartTimeMs",
                                                              "haltEndTimeMs",
                                                              "flags",
                                                              "description",
                                                              "statusReason",
                                                              "tradingStatus",
                                                              "shortSaleRestriction",
                                                              "eventTime"};
            // Literal decimal interpretations of the measured Java binary64 cells, independent of WideDecimal.
            const std::map<std::string, std::string> decimals{
                {"3ff4000000000000", "1.25"},      {"c004000000000000", "-2.5"},   {"400e000000000000", "3.75"},
                {"4010800000000000", "4.125"},     {"405a600000000000", "105.5"},  {"4019000000000000", "6.25"},
                {"4085e40000000000", "700.5"},     {"4089060000000000", "800.75"}, {"405b480000000000", "109.125"},
                {"4025c00000000000", "10.875"},    {"7ff8000000000000", "NaN"},    {"7ff0000000000000", "Infinity"},
                {"fff0000000000000", "-Infinity"}, {"0000000000000000", "0"}};

            for (std::size_t j = 1; j < event.size(); ++j) {
                std::string value = event[j];
                const bool decimal = j <= 4 || (j >= 6 && j <= 11);

                if (human && decimal) {
                    value = decimals.at(value);
                }

                if (j == 15 || j == 16) {
                    value = value == "null" ? "null"
                            : human         ? expectedQuote(units(value))
                            : value == "-"  ? "empty"
                                            : value;
                }

                out += human ? " " + std::string(labels[j - 1]) + "=" + value : "\t" + value;
            }

            out += "\n";
            ++delivered;
        } else {
            need(f.type == 2, "unexpected original frame");
        }
    }

    need(delivered == records.size(), "singleton frame producer contract");
    out += human ? "Markers: count=" + std::to_string(markers) + " authenticated-footer=no\n"
                 : "markers\t" + std::to_string(markers) + "\n";
    out += human ? "Complete: bytes=" + std::to_string(binary.size()) + " frames=" + std::to_string(envelope.size()) +
                       " records=" + std::to_string(delivered) + " charged-records=" + std::to_string(delivered) +
                       " wire-fields=" + (omitted ? "0" : "48") + "\n"
                 : "complete\t" + std::to_string(binary.size()) + "\t" + std::to_string(envelope.size()) + "\t" +
                       std::to_string(delivered) + "\t" + std::to_string(delivered) + "\t" + (omitted ? "0" : "48") +
                       "\n";

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
