#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
/** Largest nonnegative full field/type identifier and positive int32 value. */
constexpr std::uint64_t MAX_INT32 = std::numeric_limits<std::int32_t>::max();
/** Raw STREAM_DATA message identifier in the retained TSV contract. */
constexpr std::uint64_t STREAM_DATA_TYPE = 15;

/** Fail independently of NDEBUG. @param condition Required condition. @param message Failure context. */
void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

/** Read exact binary bytes. @param path Input file. @return Owned bytes, or throws on I/O failure. */
std::string read(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    require(stream.is_open(), "input open failure");
    std::string data{std::istreambuf_iterator<char>(stream), {}};
    require(!stream.bad(), "input read failure");

    return data;
}

/** Write disposable evidence. @param path Output file. @param data Exact bytes. */
void write(const std::filesystem::path &path, std::string_view data) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    stream.close();
    require(!stream.fail(), "output write failure");
}

/** Split without dropping empty cells. @param data Text. @param separator Separator. @return Borrowed cells. */
std::vector<std::string_view> split(std::string_view data, char separator) {
    std::vector<std::string_view> result;

    for (;;) {
        auto end = data.find(separator);
        result.push_back(data.substr(0, end));

        if (end == data.npos) {
            return result;
        }

        data.remove_prefix(end + 1);
    }
}

/** Parse canonical uint64. @param cell Numeric cell. @return Value, or throws on syntax/overflow. */
std::uint64_t number(std::string_view cell) {
    require(!cell.empty() && (cell == "0" || (cell[0] >= '1' && cell[0] <= '9')), "unsigned syntax");
    std::uint64_t value{};
    auto result = std::from_chars(cell.data(), cell.data() + cell.size(), value);
    require(result.ec == std::errc{} && result.ptr == cell.data() + cell.size(), "unsigned range/syntax");

    return value;
}

/** Validate lowercase hex groups. @param cell Hex text. @param width Group width. */
void hex(std::string_view cell, std::size_t width) {
    require(!cell.empty() && cell.size() % width == 0, "hex width");

    for (char c : cell) {
        require((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'), "hex syntax");
    }
}

/** Validate nullable UTF-16 hex. @param cell Exact units or dash. */
void units(std::string_view cell) {
    if (cell != "-") {
        // Each exact UTF-16 code unit occupies four hexadecimal digits.
        hex(cell, 4);
    }
}

/** Validate canonical signed int32 values. @param cell Comma-separated values or dash. */
void values(std::string_view cell) {
    if (cell == "-") {
        return;
    }

    for (auto part : split(cell, ',')) {
        auto magnitude = part;
        bool negative = magnitude.starts_with('-');

        if (negative) {
            magnitude.remove_prefix(1);
        }

        auto value = number(magnitude);
        require((!negative || value != 0) && value <= (negative ? MAX_INT32 + 1 : MAX_INT32), "int32 range");
    }
}

/** Read strict LF-terminated ASCII rows. @param data Bytes. @return Borrowed rows. */
std::vector<std::string_view> lines(std::string_view data) {
    for (unsigned char c : data) {
        // ASCII has only seven payload bits; the retained format uses LF alone.
        require(c < 128 && c != '\r', "ASCII/LF required");
    }

    if (data.empty()) {
        return {};
    }

    require(data.back() == '\n', "missing final LF");
    data.remove_suffix(1);

    return split(data, '\n');
}

/** Semantic records retained as exact cells. */
using Records = std::vector<std::vector<std::string>>;

/** Validate all TSV rows and return record projection. @param data Bytes. @param success Expected completion. @return
 * Records. */
Records validateTsv(std::string_view data, bool success) {
    // Column positions and arities below follow the immutable version-1 TSV fixture contract.
    auto rows = lines(data);

    if (rows.empty()) {
        require(!success, "missing TSV");

        return {};
    }

    require(rows[0] == "format\tqd-qtp-inspect\t1", "TSV header");
    std::uint64_t frameCount{}, end{};
    std::uint64_t currentType = std::numeric_limits<std::uint64_t>::max();
    bool done = false;
    Records records;

    for (std::size_t i = 1; i < rows.size(); ++i) {
        auto row = split(rows[i], '\t');
        require(!done, "row after completion");

        for (auto cell : row) {
            require(!cell.empty(), "empty cell");
        }

        if (row[0] == "frame") {
            require(row.size() == 7, "frame arity");
            require(number(row[1]) == frameCount && number(row[2]) == end, "frame ordering");
            auto previousEnd = end;
            end = number(row[3]);
            require(end > previousEnd, "frame span");

            if (row[4] == "-") {
                require(row[5] == "-" && row[6] == "0", "marker fields");
                currentType = std::numeric_limits<std::uint64_t>::max();
            } else {
                currentType = number(row[4]);
                require(currentType <= MAX_INT32, "frame type range");
                auto base = number(row[5]);
                auto size = number(row[6]);
                require(previousEnd < base && base <= end && size == end - base, "payload span");
            }

            ++frameCount;
        } else if (row[0] == "record") {
            require(row.size() == 7 && currentType == STREAM_DATA_TYPE && frameCount > 0, "record arity/type");
            require(number(row[1]) == frameCount - 1 && number(row[2]) == records.size(), "record ordering");
            require(number(row[3]) <= MAX_INT32, "local id range");
            units(row[4]);
            units(row[5]);
            values(row[6]);
            require(row[4] != "-", "missing record name");
            records.emplace_back(row.begin() + 3, row.end());
        } else if (row[0] == "complete") {
            require(row.size() == 6 && success, "completion arity/status");
            require(number(row[1]) == end && number(row[2]) == frameCount && number(row[3]) == records.size(),
                    "completion counts");
            require(number(row[4]) >= records.size(), "charged records");
            number(row[5]);
            done = true;
        } else {
            throw std::runtime_error("unknown TSV row");
        }
    }

    require(done == success, "completion presence");

    return records;
}

/** Validate strict diagnostic grammar and offset relations. @param data Bytes. @param code Expected exit. */
void diagnostic(std::string_view data, int code) {
    // Eleven diagnostic cells include payload base (6), relative offsets (7/8), absolute offsets (9/10).
    if (code == 0) {
        require(data.empty(), "success diagnostic");

        return;
    }

    auto rows = lines(data);
    require(rows.size() == 1, "diagnostic row count");
    auto row = split(rows[0], '\t');
    require(row.size() == 11 && row[0] == "incomplete" && row[1] == std::to_string(code), "diagnostic header");
    bool known = false;

    for (auto word : {"usage", "open", "read", "write", "flush", "input-limit", "frame-limit", "sequence", "type",
                      "duplicate-id", "allocation"}) {
        known |= row[2] == word;
    }

    for (auto prefix : {"binding.", "data.", "description.", "framing."}) {
        if (row[2].starts_with(prefix)) {
            auto suffix = row[2].substr(std::string_view(prefix).size());
            known = !suffix.empty();

            for (char c : suffix) {
                known &= c >= '0' && c <= '9';
            }
        }
    }

    require(known, "diagnostic reason");

    for (std::size_t i = 3; i < row.size(); ++i) {
        if (row[i] != "-") {
            number(row[i]);
        }
    }

    for (std::size_t i : {7, 8}) {
        if (row[6] != "-" && row[i] != "-") {
            auto base = number(row[6]), relative = number(row[i]), absolute = number(row[i + 2]);
            require(base <= absolute && relative == absolute - base, "diagnostic offsets");
        }
    }
}

/** Validate canonical UTF-8, display safety and completion placement. @param data Bytes. @param success Completion
 * expectation. */
void human(std::string_view data, bool success) {
    require(!data.starts_with("\xef\xbb\xbf"), "human BOM");
    require(data.empty() || data.back() == '\n', "human final LF");

    for (std::size_t i = 0; i < data.size();) {
        auto first = static_cast<unsigned char>(data[i++]);
        std::uint32_t cp = first;
        unsigned remaining = 0;
        std::uint32_t minimum = 0;

        // UTF-8 lead ranges select 2/3/4-byte forms. The masks remove prefix bits;
        // minimum scalars reject overlong forms and F4 is the highest valid lead byte.
        if (first >= 0xc2 && first <= 0xdf) {
            cp = first & 0x1f;
            remaining = 1;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            cp = first & 0x0f;
            remaining = 2;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            cp = first & 7;
            remaining = 3;
            minimum = 0x10000;
        } else {
            require(first < 0x80, "UTF-8 lead");
        }

        for (unsigned j = 0; j < remaining; ++j) {
            require(i < data.size(), "UTF-8 truncation");
            auto next = static_cast<unsigned char>(data[i++]);
            // A continuation is 10xxxxxx and contributes exactly six scalar bits.
            require((next & 0xc0) == 0x80, "UTF-8 continuation");
            cp = (cp << 6) | (next & 0x3f);
        }

        // Unicode ends at U+10FFFF; UTF-16 surrogate code points are not Unicode scalars.
        require(cp >= minimum && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff), "UTF-8 scalar");
        // Permit LF but reject C0, DEL/C1, Arabic letter mark, direction marks,
        // line/paragraph separators, embedding/override controls and isolate controls.
        require((cp >= 32 || cp == '\n') && !(cp >= 127 && cp <= 159), "human control");
        require(cp != 0x61c && cp != 0x200e && cp != 0x200f && !(cp >= 0x2028 && cp <= 0x202e) &&
                    !(cp >= 0x2066 && cp <= 0x206f),
                "human direction control");
    }

    std::size_t completions = 0;
    std::string_view last;

    if (!data.empty()) {
        for (auto line : split(data.substr(0, data.size() - 1), '\n')) {
            completions += line.starts_with("Complete: ");
            last = line;
        }
    }

    require(completions == (success ? 1U : 0U), "human completion count");
    require(!success || last.starts_with("Complete: "), "human completion position");
}

/** Validate output pair. @param mode tsv/human. @param code Exit. @param out stdout. @param err stderr. */
void validate(std::string_view mode, int code, std::string_view out, std::string_view err) {
    require(mode == "tsv" || mode == "human", "unknown validation mode");

    if (mode == "tsv") {
        validateTsv(out, code == 0);
    } else {
        human(out, code == 0);
    }

    diagnostic(err, code);
}

/** Compare observations with strict independent expectations. @param mode Output format. @param code Exit. @param out
 * Actual stdout. @param err Actual stderr. @param expected Expected stdout. @param error Expected stderr. */
void exact(std::string_view mode, int code, std::string_view out, std::string_view err, std::string_view expected,
           std::string_view error) {
    if (mode == "tsv") {
        validate(mode, code, expected, error);
    }

    require(out == expected && err == error, "exact byte mismatch");
    validate(mode, code, out, err);
}

/** Validate complete Java capture and compare wire and record projection. @param data Capture. @param wire Binary tape.
 * @param actual Inspector TSV. */
void java(std::string_view data, std::string_view wire, std::string_view actual) {
    // Captured text was read with universal-newline handling by the historical checker.
    std::string normalized;

    for (std::size_t i = 0; i < data.size(); ++i) {
        if (data[i] != '\r') {
            normalized += data[i];
        } else if (i + 1 == data.size() || data[i + 1] != '\n') {
            normalized += '\n';
        }
    }

    if (!normalized.ends_with('\n')) {
        normalized += '\n';
    }

    auto rows = lines(normalized);
    require(rows.size() >= 4 && rows[0] == "profile\tQD-3.355\toracle-sre-v1\tEMPTY_SET\traw-QTP", "Java header");
    auto wireRow = split(rows[1], '\t');
    require(wireRow.size() == 2 && wireRow[0] == "wire", "Java wire row");
    hex(wireRow[1], 2);
    std::string captured;

    for (std::size_t i = 0; i < wireRow[1].size(); i += 2) {
        unsigned byte{};
        std::from_chars(wireRow[1].data() + i, wireRow[1].data() + i + 2, byte, 16);
        captured += static_cast<char>(byte);
    }

    require(captured == wire, "Java wire mismatch");
    Records records;

    for (std::size_t i = 2; i + 2 < rows.size(); ++i) {
        auto row = split(rows[i], '\t');
        require(row.size() == 7 && row[0] == "record" && number(row[1]) == records.size(), "Java record row");
        number(row[2]);
        units(row[3]);
        units(row[4]);
        values(row[5]);
        require(row[6] == "0", "Java flags");
        records.emplace_back(row.begin() + 2, row.begin() + 6);
    }

    auto end = split(rows[rows.size() - 2], '\t');
    require(end.size() == 4 && end[0] == "end", "Java end row");
    require(number(end[1]) == wire.size() && number(end[2]) == records.size() && !records.empty() && number(end[3]) > 0,
            "Java end counts");
    require(rows.back() == "complete\toffline-inspection", "Java completion");
    require(validateTsv(actual, true) == records, "Java record mismatch");
}

/** Replace a required first target. @param value Original. @param oldText Target. @param replacement Replacement.
 * @return Changed bytes. */
std::string changed(std::string value, std::string_view oldText, std::string_view replacement) {
    auto at = value.find(oldText);
    require(at != value.npos, "mutation target absent");
    value.replace(at, oldText.size(), replacement);

    return value;
}

/** Replace every required target, matching human expectation mutations.
 * @param value Original bytes. @param oldText Nonempty target. @param replacement Replacement.
 * @return Changed bytes. */
std::string changedAll(std::string value, std::string_view oldText, std::string_view replacement) {
    require(!oldText.empty() && value.find(oldText) != value.npos, "mutation target absent");
    std::size_t offset = 0;

    for (;;) {
        auto at = value.find(oldText, offset);

        if (at == value.npos) {
            return value;
        }

        value.replace(at, oldText.size(), replacement);
        offset = at + replacement.size();
    }
}

/** Require a specific validator rejection, never a child-process failure. @tparam Action Validation action. @param
 * action Action. */
template <class Action> void rejects(Action action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    require(rejected, "negative accepted");
}

/** Exercise malformed syntax directly in Release builds. */
void selftest() {
    human("Complete: records=0\n", true);

    for (auto text : {"", "Complete: a\nComplete: b\n", "Complete: a\ntrailing\n"}) {
        rejects([&] {
            human(text, true);
        });
    }

    rejects([] {
        human("Complete: records=0\n", false);
    });

    for (auto text : {"", "00", "+1", "-1", "1x", "18446744073709551616"}) {
        rejects([&] {
            number(text);
        });
    }

    for (auto text : {"-0", "2147483648", "-2147483649", "1,", "+1"}) {
        rejects([&] {
            values(text);
        });
    }

    for (auto text : {"zzzz", "000", "ABCD"}) {
        rejects([&] {
            units(text);
        });
    }

    for (auto text : {"\xef\xbb\xbf", "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x80\xae", "\xc2\x80", "\t",
                      "\r", "\xe2\x82"}) {
        rejects([&] {
            human(std::string(text) + '\n', false);
        });
    }

    rejects([] {
        diagnostic("incomplete\t6\twrite\t-\t-\t-\t5\t2\t-\t8\t-\n", 6);
    });
    rejects([] {
        diagnostic("incomplete\t6\tunknown\t-\t-\t-\t-\t-\t-\t-\t-\n", 6);
    });
    rejects([] {
        diagnostic("incomplete\t6\twrite\t-\t-\t-\t-\t-\t-\t-\t-\n", 3);
    });
    rejects([] {
        exact("tsv", 2, "", "changed", "", "incomplete\t2\tusage\t-\t-\t-\t-\t-\t-\t-\t-\n");
    });
    rejects([] {
        exact("tsv", 2, "changed", "incomplete\t2\tusage\t-\t-\t-\t-\t-\t-\t-\t-\n", "",
              "incomplete\t2\tusage\t-\t-\t-\t-\t-\t-\t-\t-\n");
    });
}

/** Run retained named mutations on disposable files. @param argv Paths supplied by the CMake adapter. */
void mutations(char **argv) {
    const auto offline = read(argv[2]), offlineError = read(argv[3]);
    const auto display = read(argv[4]), displayError = read(argv[5]);
    const auto observation = read(argv[6]), wire = read(argv[7]), javaActual = read(argv[8]);
    const std::filesystem::path directory(argv[9]);
    std::filesystem::create_directories(directory);
    auto prove = [&](std::string_view suite, std::string_view label, const std::string &original,
                     const std::string &mutation, auto validator) {
        require(original != mutation, "mutation did not change target");
        auto path = directory / (std::string(suite) + "-" + std::string(label) + ".txt");
        write(path, mutation);
        const auto disposable = read(path);
        std::string stage;
        try {
            validator(disposable);
        } catch (const std::runtime_error &error) {
            const std::string_view reason(error.what());
            stage = reason == "exact byte mismatch"    ? "exact-comparison"
                    : reason == "Java record mismatch" ? "java-record-comparison"
                                                       : "strict-validation";
        }
        const auto expectedStage = suite == "human" || label == "value" || label == "symbol" || label == "local-id" ||
                                           label == "name" || label == "complete-count"
                                       ? "exact-comparison"
                                   : label == "java-value" ? "java-record-comparison"
                                                           : "strict-validation";
        require(stage == expectedStage, "mutation rejected at wrong stage or accepted");
        require(read(path) == mutation, "temporary mutation modified");
        std::cout << suite << '\t' << label << "\trejected at " << stage << "; temporary expectation unchanged\n";
    };
    std::vector<std::pair<std::string, std::string>> offlineCases{
        {"value", changed(offline, "004100420043\t1\n", "004100420043\t9\n")},
        {"symbol", changed(offline, "004100420043", "004100420044")},
        {"local-id", changed(offline, "record\t1\t0\t0", "record\t1\t0\t1")},
        {"name", changed(offline, "0053", "0052")},
        {"offset", changed(offline, "frame\t0\t0\t9", "frame\t0\t1\t9")},
        {"row-type", changed(offline, "record\t", "event\t")},
        {"row-arity", changed(offline, "record\t1\t0\t0", "record\t1\t0")},
        {"number-syntax", changed(offline, "record\t1\t0\t0", "record\t01\t0\t0")},
        {"hex-syntax", changed(offline, "004100420043", "00410042zzzz")},
        {"complete-count", offline.substr(0, offline.rfind('\t')) + "\t999\n"},
        {"missing-complete", offline.substr(0, offline.rfind("complete\t"))},
        {"double-complete", offline + offline.substr(offline.rfind("complete\t"))},
        {"trailing-row", offline + "unknown\tx\n"}};

    for (const auto &[label, mutation] : offlineCases) {
        prove("offline", label, offline, mutation, [&](const auto &value) {
            exact("tsv", 0, offline, offlineError, value, offlineError);
        });
    }

    for (const auto &[label, mutation] : std::vector<std::pair<std::string, std::string>>{
             {"java-value", changed(observation, "22,44,0", "22,45,0")},
             {"java-unknown", changed(observation, "end\t", "unknown\t")},
             {"java-completion", changed(observation, "complete\toffline-inspection\n", "")}}) {
        prove("offline", label, observation, mutation, [&](const auto &value) {
            java(value, wire, javaActual);
        });
    }

    for (const auto &[label, mutation] : std::vector<std::pair<std::string, std::string>>{
             {"label", changed(display, "S.v=", "v=")},
             {"value", changed(display, "S.v=1", "S.v=2")},
             {"escape", changed(display, "\\uD800", "\\uD801")},
             {"escape-lookalike", changed(display, "\\uD800", "\\\\uD800")},
             {"completion-count", changedAll(display, "records=1", "records=2")},
             {"missing-completion", display.substr(0, display.rfind("Complete:"))},
             {"double-completion", display + display.substr(display.rfind("Complete:"))},
             {"trailing-output", display + "Record 999: injected\n"},
             {"frame-offset", changed(display, "bytes=[0,9)", "bytes=[1,9)")}}) {
        prove("human", label, display, mutation, [&](const auto &value) {
            exact("human", 0, display, displayError, value, displayError);
        });
    }

    exact("tsv", 0, offline, offlineError, offline, offlineError);
    exact("human", 0, display, displayError, display, displayError);
    java(observation, wire, javaActual);
    require(read(argv[2]) == offline && read(argv[3]) == offlineError && read(argv[4]) == display &&
                read(argv[5]) == displayError && read(argv[6]) == observation && read(argv[7]) == wire &&
                read(argv[8]) == javaActual,
            "baseline modified");
    std::cout << "baseline\tpassed; originals unchanged\n";
}
} // namespace

/** Narrow test-only comparator entry point. @param argc Argument count. @param argv Arguments. @return Zero on success,
 * one on validation failure. */
int main(int argc, char **argv) {
    try {
        require(argc >= 2, "missing mode");
        std::string_view mode(argv[1]);

        if (mode == "selftest" && argc == 2) {
            selftest();
        } else if (mode == "materialize" && argc == 4) {
            const auto encoded = read(argv[2]);
            std::string decoded;

            if (!encoded.empty()) {
                hex(encoded, 2);

                for (std::size_t i = 0; i < encoded.size(); i += 2) {
                    unsigned byte{};
                    std::from_chars(encoded.data() + i, encoded.data() + i + 2, byte, 16);
                    decoded += static_cast<char>(byte);
                }
            }

            write(argv[3], decoded);
        } else if ((mode == "exact" && argc == 8) || (mode == "validate" && argc == 6)) {
            auto code = static_cast<int>(number(argv[3]));
            auto out = read(argv[4]), err = read(argv[5]);

            if (mode == "exact") {
                exact(argv[2], code, out, err, read(argv[6]), read(argv[7]));
            } else {
                validate(argv[2], code, out, err);
            }
        } else if (mode == "java" && argc == 5) {
            java(read(argv[2]), read(argv[3]), read(argv[4]));
        } else if (mode == "bytes" && argc == 4) {
            require(read(argv[2]) == read(argv[3]), "exact byte mismatch");
        } else if (mode == "mutations" && argc == 10) {
            mutations(argv);
        } else {
            throw std::runtime_error("invalid comparator arguments");
        }

        return 0;
    } catch (const std::exception &error) {
        std::cerr << "checker: " << error.what() << '\n';

        return 1;
    }
}
