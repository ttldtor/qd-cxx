// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: PROGRAM RUN; only exact owned children/peers and RUN files are touched.
#include "connect_peer.hpp"
#include "connect_process_support.hpp"

#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace e = root::events;
namespace t = root::transport;
namespace s = root::connect_tests;
namespace process = s::process;
using process::Child;
using process::read;
using process::require;
using process::rows;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
using Table = std::vector<std::vector<std::string_view>>;
constexpr c::SchemaLimits SCHEMA_LIMITS{16, 128, 256, 128, 16384}; ///< Same complete ordinary fixture schema scale.
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON) && ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON
constexpr std::int32_t SNAPSHOT_BEGIN = 4; ///< Actual QTP binary source flag.
constexpr std::int32_t SNAPSHOT_END = 8;   ///< Actual QTP complete source snapshot flag.
constexpr std::int32_t SNAPSHOT_SNIP = 16; ///< Actual QTP truncated source snapshot flag.
constexpr std::int64_t HISTORY_FROM = -2;  ///< Explicit signed raw bound, deliberately not a Unix conversion.
#endif
#ifndef _WIN32
constexpr auto CAPTURE_DEADLINE = 5s; ///< Finite observation bound for an owned running child's flushed evidence.
constexpr std::size_t CAPTURE_BYTES = 65536; ///< Same finite synthetic capture ceiling as the shared harness.
#endif

/** Owned exact child result, inspected only after ordinary owned process retirement. */
struct Capture {
    int code;
    std::string output;
    std::string error;
};

/** Parse exact complete TSV rows while retaining every empty cell and native capture bytes.
 * @param output Borrowed capture that outlives returned views.
 * @param name Exact row-name cell.
 * @return Only complete matching rows; LF/CRLF are recognized without modifying captures.
 */
Table table(std::string_view output, std::string_view name) {
    Table result;

    while (!output.empty()) {
        const auto end = output.find('\n');
        require(end != output.npos, "complete captured TSV row");
        auto line = output.substr(0, end);

        if (line.ends_with('\r'))
            line.remove_suffix(1);

        output.remove_prefix(end + 1);
        std::vector<std::string_view> cells;

        while (true) {
            const auto tab = line.find('\t');
            cells.push_back(line.substr(0, tab));

            if (tab == line.npos)
                break;

            line.remove_prefix(tab + 1);
        }

        if (!cells.empty() && cells[0] == name)
            result.push_back(std::move(cells));
    }

    return result;
}

/** Read one exact decimal observation without partial/sign/locale ambiguity.
 * @param value Exact numeric TSV cell.
 * @return Signed64 value for the bounded fixture observations.
 */
std::int64_t integer(std::string_view value) {
    std::int64_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    require(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size(), "exact numeric TSV cell");

    return result;
}

#ifndef _WIN32
/** Observe a bounded prefix from this test's still-running child's explicitly shared capture.
 * @param path Exact owned diagnostic file.
 * @return Current bytes; a final partial row is deliberately allowed during observation.
 */
std::string runningCapture(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);

    if (!input)
        return {};

    std::string value(CAPTURE_BYTES, '\0');
    input.read(value.data(), static_cast<std::streamsize>(value.size()));
    require(!input.bad(), "owned live capture read");
    value.resize(static_cast<std::size_t>(input.gcount()));

    return value;
}

/** Wait only for complete actual ACTIVE output from one owned child, never from a sleep guess.
 * @param path Owned child's stderr capture.
 */
void waitForActive(const std::filesystem::path &path) {
    const auto deadline = Clock::now() + CAPTURE_DEADLINE;

    while (Clock::now() < deadline) {
        auto current = runningCapture(path);
        const auto complete = current.rfind('\n');

        if (complete != current.npos) {
            current.resize(complete + 1);

            for (const auto &row : table(current, "recovery-status"))
                if (row.size() == 7 && row[1] == "5")
                    return;
        }

        std::this_thread::sleep_for(1ms);
    }

    require(false, "owned child actual ACTIVE deadline");
}

/** Independently observe actual complete peer ADD after local ACTIVE, which is not wire receipt.
 * @param peer Exact owned native peer.
 */
void waitForAdd(const s::Peer &peer) {
    const auto deadline = Clock::now() + CAPTURE_DEADLINE;

    while (Clock::now() < deadline && peer.adds() == 0 && !peer.failed()) {
        std::this_thread::sleep_for(1ms);
    }

    require(peer.adds() == 1 && !peer.failed(), "owned peer actual full ADD deadline");
}
#endif

/** Convert an actual filesystem path to exact UTF-8 argv bytes for the shared native child encoder.
 * @param path Owned fixture path, including non-ASCII filename.
 * @return Exact UTF-8 argument, without changing process codepages.
 */
std::string pathArgument(const std::filesystem::path &path) {
    const auto utf8 = path.u8string();

    return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}

/** Prepare an unchanged full standard fixture schema with explicit binary field support.
 * @param profile Select complete Profile instead of complete Quote.
 * @return Owning schema used only by this actual independent peer.
 */
c::LocalSchema schema(bool profile = false) {
    auto builtin = profile ? e::defaultProfileSchema(SCHEMA_LIMITS) : e::defaultQuoteSchema(SCHEMA_LIMITS);
    require(builtin.has_value(), "full standard process schema");
    auto result = c::LocalSchema::create(builtin->records(), SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    require(result.has_value(), "standard binary process schema");

    return std::move(*result);
}

/** Fill every actual standard field without reducing a schema for the decoder.
 * @param local Complete ordinary schema.
 * @return Exact independently owned fixture values.
 */
std::vector<c::FieldValue> fields(const c::LocalSchema &local) {
    std::vector<c::FieldValue> result;

    for (const auto &field : local.records()[0].fields) {
        const auto type = static_cast<c::SerialFieldType>(field.serialTypeId);
        c::FieldValue value{type, 0};

        if (type == c::SerialFieldType::UTF_CHAR_ARRAY)
            value.units = u"process";

        result.push_back(std::move(value));
    }

    return result;
}

/** Supply explicit native recovery argv without introducing a different command parser.
 * @param peer Actual owned peer supplying the port.
 * @param symbol Exact selected ASCII key.
 * @param family Explicit public family spelling.
 * @param duration Independent finite admission interval in milliseconds.
 * @return Owned child arguments; individual cases add only their tested options.
 */
std::vector<std::string> arguments(const s::Peer &peer, std::string symbol = "IBM", std::string family = "stream",
                                   unsigned duration = 650) {
    return {"--mode",        "recovery",
            "--host",        "127.0.0.1",
            "--port",        std::to_string(peer.port()),
            "--symbol",      std::move(symbol),
            "--family",      std::move(family),
            "--duration-ms", std::to_string(duration),
            "--drain-ms",    "1000"};
}

/** Retire this exact child and retain exact already bounded captures for later assertions.
 * @param child Sole owned native child.
 * @param output Owned stdout capture; unused for a deliberate pipe-output case.
 * @param error Owned stderr capture.
 * @param capturedOutput Whether stdout was a file rather than an injected broken pipe.
 * @return Actual exit and exact owned evidence.
 */
Capture finish(Child &child, const std::filesystem::path &output, const std::filesystem::path &error,
               bool capturedOutput = true) {
    const auto code = child.wait();

    return {code, capturedOutput ? read(output) : std::string{}, read(error)};
}

/** Check actual complete owned retirement without deriving it from a timeout or empty output.
 * @param capture Actual completed child evidence.
 * @param outcome Required public local terminal enum code.
 */
void terminal(const Capture &capture, std::string_view outcome) {
    const auto results = table(capture.error, "recovery-terminal");
    require(results.size() == 1 && results[0].size() == 6, "one actual Recovery terminal row");
    require(results[0][1] == outcome && results[0][2] == "0", "actual unexpired local terminal outcome");
    require(capture.error.find("retirement is not certified") == capture.error.npos,
            "actual owned retirement completed");
    const auto queues = table(capture.error, "queue-terminal");
    require(queues.size() == 1 && queues[0].size() == 15, "one actual queue terminal row");
}

/** Separate actual local command application from independent peer wire observations.
 * @param capture Completed child evidence.
 * @param command Exact frontend command token.
 */
void applied(const Capture &capture, std::string_view command) {
    bool found = false;

    for (const auto &row : table(capture.error, "command-result")) {
        require(row.size() == 7, "complete command result row");

        if (row[1] == command && row[3] == "0" && integer(row[2]) > 0 && integer(row[4]) > 0)
            found = true;
    }

    require(found, "actual authoritative command APPLIED");
}

/** Check all standard generic field identities alongside explicit prepared output.
 * @param output Completed owned data capture.
 */
void completeQuote(std::string_view output) {
    const auto records = table(output, "record");
    require(records.size() == 1 && records[0].size() == 11 && records[0][10] == "8", "one complete eight-field record");
    const auto values = table(output, "field");
    require(values.size() == e::QUOTE_FIELD_TYPES.size(), "all generic Quote fields retained");

    for (std::size_t index = 0; index < values.size(); ++index) {
        require(values[index].size() == 7 && integer(values[index][2]) == static_cast<std::int64_t>(index),
                "exact Quote local field order");
        require(integer(values[index][3]) == static_cast<std::int64_t>(e::QUOTE_FIELD_TYPES[index]),
                "exact full Quote serial type");
    }

    require(table(output, "quote").size() == 1, "explicit prepared Quote row");
}

#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON) && ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON
/** Construct an independent complete raw indexed R schema, matching the explicit JSON fixture.
 * @return One exact pair-int32 index and ordinary value under the real binary profile.
 */
c::LocalSchema historySchema() {
    const c::LocalRecord record{
        0, u"R", true, {{8}, {8}, {8}}, {{u"TimeHigh", 0}, {u"TimeLow", 1}, {u"V", 2}}, c::IndexedTimePairInt32{0, 1}};
    auto result = c::LocalSchema::create(std::span(&record, 1), SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    require(result.has_value(), "independent indexed R schema");

    return std::move(*result);
}

/** Build actual signed64 index words; no timestamp or named-field inference is used.
 * @param time Exact raw signed index.
 * @param value Actual ordinary payload.
 * @param flags Actual QTP source flags.
 * @return Complete owning source tuple for the native peer.
 */
s::Record point(std::int64_t time, std::int32_t value, std::int32_t flags) {
    const auto bits = std::bit_cast<std::uint64_t>(time);

    return {u"A",
            {{c::SerialFieldType::COMPACT_INT, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32))},
             {c::SerialFieldType::COMPACT_INT, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits))},
             {c::SerialFieldType::COMPACT_INT, value}},
            flags};
}

/** Write a complete explicit native-binary file at an owned non-ASCII path.
 * @param run Test-owned output directory; no source-tree fixture is created.
 * @return Actual ordinary fixture path, including Unicode on both platforms.
 */
std::filesystem::path writeHistorySchema(const std::filesystem::path &run) {
    const auto path = run / std::filesystem::path(u8"история-R.json");
    constexpr std::string_view document =
        R"({"formatVersion":1,"profile":{"id":"qdcxx-native-binary","version":1},"records":[{"id":0,"name":"R","hasTime":true,"fields":[{"serialTypeId":8},{"serialTypeId":8},{"serialTypeId":8}],"lookups":[{"name":"TimeHigh","fieldId":0},{"name":"TimeLow","fieldId":1},{"name":"V","fieldId":2}],"indexedTimeLayout":{"kind":"pair-int32","highFieldId":0,"lowFieldId":1}}]})";
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(output.good(), "owned Unicode schema open");
    output.write(document.data(), static_cast<std::streamsize>(document.size()));
    output.flush();
    require(output.good(), "owned Unicode schema write");
    output.close();
    require(!output.fail(), "owned Unicode schema close");

    return path;
}

/** Check actual scoped range/source evidence, preserving incomplete versus unavailable.
 * @param capture Completed HISTORY invocation.
 * @param snapshot Required actual source enum code in at least one observation.
 * @param complete Whether actual delivered/source completeness must be observed or remain false.
 */
void historyEvidence(const Capture &capture, std::string_view snapshot, bool complete) {
    bool delivered = false;
    bool actualSource = false;

    for (const auto &row : table(capture.error, "history")) {
        require(row.size() == 17 && row[7] == "-2", "actual requested signed HISTORY bound");

        if (row[15] == "1")
            delivered = true;
    }

    for (const auto &row : table(capture.error, "history-source")) {
        require(row.size() == 15, "complete actual HISTORY source row");

        if (row[10] == snapshot && row[14] == (complete ? "1" : "0"))
            actualSource = true;

        if (!complete)
            require(row[14] != "1", "incomplete source is never fabricated complete");
    }

    require(actualSource, "actual source snapshot/range observation");
    require(delivered == complete, "actual delivered completeness has truthful scope");
}
#endif

/** Record one named actual process scenario only after all independent observations passed.
 * @tparam Run Owning synchronous fixture action.
 * @param name Static scenario name.
 * @param run Complete case including exact child reap.
 */
template <class Run> void scenario(std::string_view name, Run run) {
    run();
    std::cout << "RECOVERY_PROCESS_CASE " << name << " PASS\n";
}
} // namespace

/** Exercise actual CLI processes against owned peers; all children self-retire or are reaped on failure.
 * @param argc Must contain exact PROGRAM and private RUN arguments.
 * @param argv Borrowed native arguments; supplied harness paths are controlled absolute build paths.
 * @return Zero only after all applicable real scenarios passed; Windows external signals are explicit NOT_CHECKED.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 3, "PROGRAM RUN required");
        const auto program = std::filesystem::absolute(argv[1]);
        const auto run = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(run);
        scenario("blocked-stdin-help", [&] {
            Child noargs(program, {}, run / "noargs.out", run / "noargs.err");
            require(noargs.wait() == 0, "noargs help without stdin consumption");
            Child help(program, {"--help"}, run / "help.out", run / "help.err");
            require(help.wait() == 0, "standalone help without stdin consumption");
            require(read(run / "noargs.out") == read(run / "help.out"), "noargs equals standalone help bytes");
            require(read(run / "noargs.err").empty() && read(run / "help.err").empty(), "help has empty stderr");
            require(read(run / "help.out").find("--mode recovery") != std::string::npos,
                    "recovery help is actually present");
        });
        auto bootstrap = t::RuntimeBootstrap::create();
        require(bootstrap.has_value(), "native peer bootstrap");
        scenario("legacy-symbol-mode-value", [&] {
            auto local = schema();
            const std::vector<std::u16string> selected{u"--mode"};
            const std::vector<s::Record> records{{selected[0], fields(local)}};
            s::Peer peer(local, records, selected);
            const auto out = run / "legacy-mode-value.out", err = run / "legacy-mode-value.err";
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "--mode",
                         "--max-records", "1", "--duration-ms", "2000"},
                        out, err);
            const auto observed = finish(child, out, err);
            require(observed.code == 0 && observed.output.starts_with("qdcxx-stream\t1"),
                    "--symbol --mode retains actual legacy route");
            require(peer.adds() == 1 && !peer.failed(), "legacy value actual full ADD observed");
        });
        scenario("stream-equal-remove-late", [&] {
            auto local = schema();
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}, {u"IBM", fields(local)}};
            s::PeerScript script;
            script.afterRemove = std::vector<s::Record>{{u"IBM", fields(local)}};
            s::Peer peer(local, records, selected, 8192, s::PeerMode::HOLD, std::move(script));
            auto args = arguments(peer);
            args.insert(args.end(), {"--remove-after-ms", "300"});
            const auto out = run / "stream-remove.out", err = run / "stream-remove.err";
            Child child(program, args, out, err);
            const auto observed = finish(child, out, err);
            require(observed.code == 0, "STREAM duration/remove actual drain exit");
            terminal(observed, "0");
            applied(observed, "initial-replace");
            applied(observed, "remove-initial");
            require(peer.adds() == 1 && peer.removes() == 1 && peer.initialDataSent() && peer.afterRemoveSent() &&
                        !peer.failed(),
                    "actual full ADD/REMOVE and both peer DATA write prefixes observed");
            require(observed.output.starts_with("qdcxx-subscriptions\t1"), "distinct recovery stdout header");
            const auto data = table(observed.output, "record");
            require(data.size() == 2 && data[0].size() == 11 && data[1].size() == 11 && data[0][7] == "00490042004D" &&
                        data[1][7] == data[0][7],
                    "two equal STREAM records survive; late removed-symbol record does not deliver");
            require(table(observed.output, "field").size() == 16, "both complete equal full Quote records retained");
            const auto counts = table(observed.error, "queue-counts");
            require(counts.size() == 1 && counts[0].size() == 19 && counts[0][2] == "2" && counts[0][3] == "3" &&
                        counts[0][5] == "2" && counts[0][12] == "2",
                    "client actually admits both DATA frames and filters only the late removed record");
        });
        scenario("whole-batch-target-crossing", [&] {
            auto local = schema();
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{
                {u"IBM", fields(local)}, {u"IBM", fields(local)}, {u"IBM", fields(local)}};
            s::PeerScript script;
            script.canonicalSymbols = true;
            s::Peer peer(local, records, selected, 8192, s::PeerMode::HOLD, std::move(script));
            auto args = arguments(peer, "IBM", "stream", 2000);
            args.insert(args.end(), {"--max-records", "2", "--selectors", "canonical-exact-v1"});
            const auto out = run / "whole-batch.out", err = run / "whole-batch.err";
            Child child(program, args, out, err);
            const auto observed = finish(child, out, err);
            require(observed.code == 0, "actual target cutoff after whole callback");
            terminal(observed, "0");
            require(table(observed.output, "record").size() == 3, "target2 preserves complete three-record callback");
            const auto counts = table(observed.error, "queue-counts");
            require(counts.size() == 1 && counts[0].size() == 19 && counts[0][9] == "1" && counts[0][10] == "3" &&
                        counts[0][11] == "1" && counts[0][12] == "3",
                    "actual single callback starts and completes all three records");
            require(peer.adds() == 1 && peer.initialDataSent() && !peer.failed(),
                    "canonical exact actual ADD/data observed");
        });
        scenario("ticker-default-full-quote", [&] {
            auto local = schema();
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::PeerScript script;
            script.family = c::SubscriptionFamily::TICKER;
            s::Peer peer(local, records, selected, 8192, s::PeerMode::HOLD, std::move(script));
            auto args = arguments(peer, "IBM", "ticker");
            args.insert(args.end(), {"--output", "quote"});
            const auto out = run / "ticker-quote.out", err = run / "ticker-quote.err";
            Child child(program, args, out, err);
            const auto observed = finish(child, out, err);
            require(observed.code == 0, "TICKER complete Quote actual duration drain");
            terminal(observed, "0");
            completeQuote(observed.output);
            require(peer.adds() == 1 && peer.initialDataSent() && !peer.failed(),
                    "actual complete TICKER ADD before DATA");
        });
        scenario("active-unmet-target", [&] {
            auto local = schema();
            const std::vector<std::u16string> selected{u"IBM"};
            s::Peer peer(local, {}, selected, 8192, s::PeerMode::SILENT);
            auto args = arguments(peer, "IBM", "stream", 400);
            args.insert(args.end(), {"--max-records", "1"});
            const auto out = run / "unmet.out", err = run / "unmet.err";
            Child child(program, args, out, err);
            const auto observed = finish(child, out, err);
            require(observed.code == 2, "active intentional interval with unmet target exits2");
            terminal(observed, "0");
            require(table(observed.output, "record").empty() && peer.adds() == 1 && !peer.failed(),
                    "actual ADD with no invented data");
        });
        scenario("no-readiness", [&] {
            auto local = schema();
            const std::vector<std::u16string> selected{u"IBM"};
            s::Peer peer(local, {}, selected, 8192, s::PeerMode::NO_READINESS);
            const auto out = run / "unready.out", err = run / "unready.err";
            Child child(program, arguments(peer, "IBM", "stream", 400), out, err);
            const auto observed = finish(child, out, err);
            require(observed.code == 2, "intentional interval without actual readiness exits2");
            terminal(observed, "0");
            require(peer.adds() == 0 && !peer.failed() && table(observed.output, "record").empty(),
                    "no premature ADD or data before readiness");
        });
        scenario("partial-peer-protocol-failure", [&] {
            auto local = schema();
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::Peer peer(local, records, selected, 8192, s::PeerMode::PARTIAL_END);
            const auto out = run / "partial.out", err = run / "partial.err";
            Child child(program, arguments(peer, "IBM", "stream", 2000), out, err);
            const auto observed = finish(child, out, err);
            require(observed.code == 1 && peer.adds() == 1 && peer.initialDataSent() && !peer.failed(),
                    "actual partial native peer fails visibly");
            const auto results = table(observed.error, "recovery-terminal");
            require(results.size() == 1 && results[0].size() == 6 && results[0][1] != "0",
                    "protocol failure cannot fabricate DRAINED");
            bool protocolFailure = false;

            for (const auto &row : table(observed.error, "recovery-diagnostic"))
                if (row.size() == 11 && row[3] == "14" && row[5] != "0")
                    protocolFailure = true;

            require(protocolFailure, "actual typed protocol cause remains visible after retirement");
        });
        scenario("callback-broken-stdout", [&] {
            auto local = schema();
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::PeerScript script;
            script.waitForDataRelease = true;
            s::Peer peer(local, records, selected, 8192, s::PeerMode::HOLD, std::move(script));
            const auto out = run / "unused-broken.out", err = run / "broken.err";
            Child child(program, arguments(peer, "IBM", "stream", 2000), out, err, true);
            child.closeOutput(true);
            peer.releaseData();
            const auto observed = finish(child, out, err, false);
            require(observed.code == 1, "actual callback stdout failure exits1");
            terminal(observed, "2");
            const auto counts = table(observed.error, "queue-counts");
            require(counts.size() == 1 && counts[0].size() == 19 && counts[0][13] == "1" && counts[0][14] == "1",
                    "actual failed whole callback counted without retry");
            require(peer.adds() == 1 && peer.initialDataSent() && !peer.failed(),
                    "broken sink follows actual ADD and DATA");
        });
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON) && ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON
        const auto json = writeHistorySchema(run);

        for (const auto kind : {0, 1, 2}) {
            const auto name = kind == 0 ? "history-complete" : (kind == 1 ? "history-snip" : "history-incomplete");
            scenario(name, [&] {
                auto local = historySchema();
                const std::vector<std::u16string> selected{u"A"};
                const std::vector<s::Record> points =
                    kind == 0 ? std::vector<s::Record>{point(-1, 11, SNAPSHOT_BEGIN), point(-2, 22, SNAPSHOT_END)}
                              : std::vector<s::Record>{point(-1, 11, SNAPSHOT_BEGIN | (kind == 1 ? SNAPSHOT_SNIP : 0))};
                s::PeerScript script;
                script.family = c::SubscriptionFamily::HISTORY;
                script.historyBound = HISTORY_FROM;
                s::Peer peer(local, points, selected, 8192, s::PeerMode::HOLD, std::move(script));
                auto args = arguments(peer, "A", "history");
                args.insert(args.end(),
                            {"--schema-json", pathArgument(json), "--record-id", "0", "--history-from-raw", "-2"});
                const auto out = run / (std::string(name) + ".out"), err = run / (std::string(name) + ".err");
                Child child(program, args, out, err);
                const auto observed = finish(child, out, err);
                require(observed.code == (kind == 0 ? 0 : 1),
                        "complete HISTORY drains; truncated/incomplete HISTORY refuses success");
                terminal(observed, kind == 0 ? "0" : "1");

                if (kind != 0) {
                    const auto queues = table(observed.error, "queue-terminal");
                    require(queues[0][3] == "1" && queues[0][5] == "1" && queues[0][7] == "1",
                            "incomplete HISTORY preserves actual ABORTED and unknown-loss gap");
                }

                applied(observed, "initial-replace");
                require(peer.adds() == 1 && peer.initialDataSent() && !peer.failed(),
                        "actual complete signed-bound HISTORY ADD precedes data");
                historyEvidence(observed, kind == 0 ? "2" : (kind == 1 ? "3" : "1"), kind == 0);
                const auto data = table(observed.output, "record");
                require(data.size() == points.size(), "actual indexed HISTORY source records delivered");

                for (const auto &row : data)
                    require(row.size() == 11 && row[2] == "history" && (row[8] == "-1" || row[8] == "-2"),
                            "raw signed time remains independent of business timestamp");

                require(observed.output.find(pathArgument(json)) == observed.output.npos &&
                            observed.error.find(pathArgument(json)) == observed.error.npos,
                        "Unicode schema path is not echoed into output or diagnostics");
            });
        }

#else
        scenario("json-off-refuses-before-file", [&] {
            const auto out = run / "json-off.out", err = run / "json-off.err";
            Child child(program,
                        {"--mode", "recovery", "--host", "127.0.0.1", "--port", "1", "--symbol", "A", "--schema-json",
                         pathArgument(run / "does-not-exist.json"), "--record-id", "0"},
                        out, err);
            const auto observed = finish(child, out, err);
            require(observed.code == 2 && observed.output.empty(), "explicit disabled JSON grammar refusal");
        });
        std::cout << "RECOVERY_PROCESS_CASE history-json-feature NOT_CHECKED build feature disabled\n";
#endif
#ifndef _WIN32
        scenario("owned-sigterm-aborted", [&] {
            auto local = schema();
            const std::vector<std::u16string> selected{u"IBM"};
            s::Peer peer(local, {}, selected, 8192, s::PeerMode::SILENT);
            const auto out = run / "sigterm.out", err = run / "sigterm.err";
            Child child(program, arguments(peer, "IBM", "stream", 5000), out, err);
            waitForActive(err);
            waitForAdd(peer);
            require(peer.adds() == 1 && !peer.failed(), "SIGTERM follows actual peer ADD and ACTIVE");
            child.terminate();
            const auto observed = finish(child, out, err);
            require(observed.code == 130, "owned SIGTERM cooperative exit130 rather than native signal death");
            terminal(observed, "1");
        });
#else
        std::cout << "RECOVERY_PROCESS_CASE windows-external-signal NOT_CHECKED CREATE_NO_WINDOW has no console\n";
#endif
        std::cout << "RECOVERY_PROCESS_OWNERS retired=all\n";

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
