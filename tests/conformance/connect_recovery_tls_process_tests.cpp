// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: PROGRAM RUN, with explicit TLS peer/fixture macros. Only owned loopback
// children and assigned RUN captures are touched; failures retire children without foreign process lookup.
#include "connect_peer.hpp"
#include "transport/tls_test_peer.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/events/quote.hpp"
#include "qd/transport/tls_runtime.hpp"

#include "connect_process_support.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace e = root::encoding;
namespace s = root::connect_tests;
namespace h = root::test;
namespace t = root::transport;
namespace fs = std::filesystem;
using s::process::Child;
using s::process::read;
using s::process::require;
using namespace std::chrono_literals;
using Bytes = std::vector<std::uint8_t>;
using Table = std::vector<std::vector<std::string_view>>;
constexpr c::SchemaLimits SCHEMA_LIMITS{16, 128, 256, 128, 16384}; ///< Complete standard fixture declarations.
constexpr std::string_view TOKEN = "Basic cWRjeHg6dGVzdA==";       ///< Public synthetic repository credential only.
constexpr std::u16string_view CHALLENGE = u"LOGIN synthetic-cli-realm"; ///< Never echoed by the product.
constexpr std::size_t MAXIMUM_TOKEN_UNITS = 4094; ///< Complete prefix plus maximum complete quartets.
std::size_t failedCases = 0; ///< Serial harness failures; independent cases still execute after owned cleanup.

/** Exact completed child observations, never borrowed from a running process. */
struct Capture {
    int code;
    std::string output;
    std::string error;
};

/** Preserve exact UTF-8 path bytes for the shared strict Windows native argument encoder.
 * @param path Explicit owned fixture/capture path.
 * @return Owned argument bytes, without process codepage changes.
 */
std::string pathArgument(const fs::path &path) {
    const auto bytes = path.u8string();

    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

/** Convert independently encoded primitive bytes to the existing child script representation.
 * @param bytes Complete source bytes.
 * @return Exact owning copy, with no protocol writer involved.
 */
Bytes wire(std::span<const std::byte> bytes) {
    Bytes result;
    result.reserve(bytes.size());

    for (const auto byte : bytes)
        result.push_back(std::to_integer<std::uint8_t>(byte));

    return result;
}

/** Append one canonical signed compact primitive.
 * @param bytes Owned fixture destination.
 * @param value Exact signed raw value.
 */
void number(Bytes &bytes, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    const auto encoded = e::writeCompactLong(value, buffer);
    require(encoded.status == e::Status::OK, "fixture compact primitive");

    for (std::size_t index = 0; index < encoded.written; ++index)
        bytes.push_back(std::to_integer<std::uint8_t>(buffer[index]));
}

/** Append an exact UTFString through primitive codecs, supporting the maximum synthetic token.
 * @param bytes Owned fixture destination.
 * @param value Exact bounded UTF-16 input.
 */
void text(Bytes &bytes, std::u16string_view value) {
    std::array<std::byte, 65536> buffer{};
    const auto encoded = e::writeUtfString(value, buffer, {buffer.size(), 4096});
    require(encoded.status == e::Status::OK, "fixture UTFString primitive");

    for (std::size_t index = 0; index < encoded.written; ++index)
        bytes.push_back(std::to_integer<std::uint8_t>(buffer[index]));
}

/** Wrap a complete bounded fixture payload; this is not the production output composer.
 * @param type Actual single-byte message identity.
 * @param payload Complete body after that identity.
 * @return Complete owning envelope.
 */
Bytes frame(std::int32_t type, std::span<const std::uint8_t> payload) {
    Bytes result;
    number(result, static_cast<std::int64_t>(payload.size() + 1));
    number(result, type);
    result.insert(result.end(), payload.begin(), payload.end());

    return result;
}

/** Append whole independent frames without interpreting them.
 * @param target Owned destination.
 * @param source Complete source bytes.
 */
void append(Bytes &target, const Bytes &source) {
    target.insert(target.end(), source.begin(), source.end());
}

/** Widen public synthetic ASCII without decoding username/password or base64 content.
 * @param value Complete synthetic ASCII fixture text.
 * @return Exact widened units.
 */
std::u16string widen(std::string_view value) {
    std::u16string result;

    for (const unsigned char byte : value) {
        require(byte < 128, "synthetic ASCII token");
        result.push_back(static_cast<char16_t>(byte));
    }

    return result;
}

/** Build one complete independent DXP3 property message.
 * @param key Exact property name.
 * @param value Exact synthetic value; empty means an actual empty authentication acknowledgement.
 * @return Exact descriptor bytes, without using LiveSession or a production writer.
 */
Bytes property(std::u16string_view key, std::u16string_view value) {
    Bytes body{'D', 'X', 'P', '3'};
    number(body, 1);
    text(body, key);
    text(body, value);
    number(body, 0);
    number(body, 0);

    return frame(1, body);
}

/** Build the full expected initial STREAM protocol and local declarations independently.
 * @param schema Complete selected default Quote declarations, with local sender ID zero.
 * @return Exact complete client prefix, including all eight fields; STREAM does not advertise hs.
 */
Bytes clientPrelude(const c::LocalSchema &schema) {
    require(schema.records().size() == 1 && schema.records()[0].id == 0, "one default client fixture record");
    Bytes body{'D', 'X', 'P', '3'};
    number(body, 0);
    number(body, 5);
    constexpr std::array<std::int32_t, 5> send{0, 1, 2, 16, 17};
    constexpr std::array<std::u16string_view, 5> sendNames{u"HEARTBEAT", u"DESCRIBE_PROTOCOL", u"DESCRIBE_RECORDS",
                                                           u"STREAM_ADD_SUBSCRIPTION", u"STREAM_REMOVE_SUBSCRIPTION"};

    for (std::size_t index = 0; index < send.size(); ++index) {
        number(body, send[index]);
        text(body, sendNames[index]);
        number(body, 0);
    }

    number(body, 4);
    constexpr std::array<std::int32_t, 4> receive{0, 1, 2, 15};
    constexpr std::array<std::u16string_view, 4> receiveNames{u"HEARTBEAT", u"DESCRIBE_PROTOCOL", u"DESCRIBE_RECORDS",
                                                              u"STREAM_DATA"};

    for (std::size_t index = 0; index < receive.size(); ++index) {
        number(body, receive[index]);
        text(body, receiveNames[index]);
        number(body, 0);
    }

    auto result = frame(1, body);
    body.clear();
    const auto &record = schema.records()[0];
    number(body, record.id);
    text(body, record.name);
    number(body, static_cast<std::int64_t>(record.fields.size()));

    for (std::size_t index = 0; index < record.fields.size(); ++index) {
        const auto lookup = std::ranges::find_if(record.lookups, [index](const auto &value) {
            return value.fieldId == index;
        });
        require(lookup != record.lookups.end(), "complete client fixture lookup");
        text(body, lookup->name);
        number(body, record.fields[index].serialTypeId);
    }

    append(result, frame(2, body));

    return result;
}

/** Encode the actual default exact-only raw-symbol client subscription independently.
 * @return Complete type16 ADD for IBM and local sender zero, without a history bound.
 */
Bytes clientAdd() {
    Bytes body{0xfd};
    number(body, 3);
    body.insert(body.end(), {'I', 'B', 'M'});
    number(body, 0);

    return frame(16, body);
}

/** Parse complete actual TSV lines while preserving native captured bytes and empty cells.
 * @param bytes Completed bounded capture, outliving all returned views.
 * @param name Exact row name.
 * @return Matching complete rows, accepting only LF/CRLF line endings.
 */
Table table(std::string_view bytes, std::string_view name) {
    Table result;

    while (!bytes.empty()) {
        const auto end = bytes.find('\n');
        require(end != bytes.npos, "complete captured evidence row");
        auto line = bytes.substr(0, end);
        bytes.remove_prefix(end + 1);

        if (line.ends_with('\r'))
            line.remove_suffix(1);

        std::vector<std::string_view> cells;

        while (true) {
            const auto tab = line.find('\t');
            cells.push_back(line.substr(0, tab));

            if (tab == line.npos)
                break;

            line.remove_prefix(tab + 1);
        }

        if (cells[0] == name)
            result.push_back(std::move(cells));
    }

    return result;
}

/** Require a complete numeric observation without locale or partial parse acceptance.
 * @param value Exact bounded TSV numeric cell.
 * @return Signed observation.
 */
std::int64_t integer(std::string_view value) {
    std::int64_t result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    require(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size(), "complete numeric evidence");

    return result;
}

/** Observe actual local convergence separately from actual peer input comparison.
 * @param capture Completed CLI evidence.
 * @return True only for an actual ACTIVE status row.
 */
bool active(const Capture &capture) {
    for (const auto &row : table(capture.error, "recovery-status")) {
        require(row.size() == 7, "complete actual recovery status");

        if (row[1] == "5")
            return true;
    }

    return false;
}

/** Check exact actual local terminal without claiming TLS close_notify or server acknowledgement.
 * @param capture Completed CLI evidence.
 * @param outcome Required local terminal code, or empty to require any actual non-DRAINED outcome.
 */
void terminal(const Capture &capture, std::string_view outcome) {
    const auto rows = table(capture.error, "recovery-terminal");
    require(rows.size() == 1 && rows[0].size() == 6, "one actual local terminal");
    require(rows[0][2] == "0", "actual local retirement before drain deadline");
    require(outcome.empty() ? rows[0][1] != "0" : rows[0][1] == outcome, "actual required local terminal outcome");
    require(capture.error.find("retirement is not certified") == capture.error.npos, "owned retirement certified");
}

/** Find a typed TLS cause in actual published evidence.
 * @param capture Completed CLI evidence.
 * @param expected Required public TLS category.
 * @return True only if the actual numeric TLS diagnostic contains that category.
 */
bool tlsCause(const Capture &capture, t::TlsError expected) {
    for (const auto &row : table(capture.error, "tls-diagnostic")) {
        require(row.size() == 8, "complete typed TLS diagnostic");

        if (integer(row[2]) == static_cast<int>(expected))
            return true;
    }

    return false;
}

/** Verify complete default generic values and explicit existing prepared Quote rows.
 * @param capture Completed CLI data/evidence.
 * @param count Required whole delivered record count.
 */
void quote(const Capture &capture, std::size_t count = 1) {
    require(capture.output.starts_with("qdcxx-subscriptions\t1"), "recovery data format");
    const auto records = table(capture.output, "record");
    const auto fields = table(capture.output, "field");
    require(records.size() == count && fields.size() == 8 * count, "complete generic Quote fields retained");
    require(table(capture.output, "quote").size() == count, "explicit prepared Quote delivered");

    for (const auto &row : records)
        require(row.size() == 11 && row[7] == "00490042004D" && row[10] == "8", "actual full selected Quote record");

    for (std::size_t index = 0; index < fields.size(); ++index)
        require(fields[index].size() == 7 && integer(fields[index][2]) == static_cast<std::int64_t>(index % 8) &&
                    integer(fields[index][3]) == static_cast<std::int64_t>(root::events::QUOTE_FIELD_TYPES[index % 8]),
                "actual exact local Quote field order and serial identity");
}

/** Refuse product echo of synthetic credential, challenge, trust or secret-file paths.
 * @param capture Completed product stdout/stderr.
 * @param trust Explicit owned trust path.
 * @param token Explicit owned token path.
 */
void redacted(const Capture &capture, const fs::path &trust, const fs::path &token) {
    for (const auto &bytes : {std::cref(capture.output), std::cref(capture.error)}) {
        require(bytes.get().find(TOKEN) == std::string::npos, "synthetic token never echoed by product");
        require(bytes.get().find("synthetic-cli-realm") == std::string::npos, "challenge never echoed by product");
        require(bytes.get().find("synthetic-refused") == std::string::npos, "denial never echoed by product");
        require(bytes.get().find(pathArgument(trust)) == std::string::npos, "trust path never echoed by product");
        require(bytes.get().find(pathArgument(token)) == std::string::npos, "token path never echoed by product");
    }
}

/** Write explicit bounded owned evidence or synthetic input; no stdout content echo.
 * @param path Exact file inside this run's owned directory.
 * @param bytes Exact data, including public synthetic peer captures.
 */
void write(const fs::path &path, std::string_view bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(output.good(), "owned fixture open");
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    require(!output.fail(), "owned fixture write and close");
}

/** Persist captured plaintext only for explicit public synthetic fixtures.
 * @param path Exact owned capture filename.
 * @param bytes Actual post-retirement child-reported plaintext.
 */
void write(const fs::path &path, const Bytes &bytes) {
    write(path, {bytes.empty() ? "" : reinterpret_cast<const char *>(bytes.data()), bytes.size()});
}

/** Retire the exact server helper and retain numeric/raw evidence before its owned directory disappears.
 * @param peer Sole owned server process.
 * @param run Owned capture directory.
 * @param name Fixed scenario token.
 * @param completed Whether the script is expected to finish itself before stop.
 * @return Actual final peer report, retaining exact captured bytes independently of helper lifetime.
 */
h::PeerSnapshot retire(h::TlsTestPeer &peer, const fs::path &run, std::string_view name, bool completed = true) {
    if (completed) {
        const auto deadline = std::chrono::steady_clock::now() + 3s;

        while (peer.snapshot().childRunning && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(1ms);
    }

    auto report = peer.stop();
    write(run / (std::string(name) + ".client.bin"), report.firstClientBytes);
    write(run / (std::string(name) + ".successor.bin"), report.secondClientBytes);
    std::string numeric;

    for (const auto value :
         {report.processId, static_cast<std::uint64_t>(report.tlsVersion), report.bytesRead, report.bytesWritten,
          static_cast<std::uint64_t>(report.handshakeComplete), static_cast<std::uint64_t>(report.expectedBytesMatched),
          static_cast<std::uint64_t>(report.completedSessions), static_cast<std::uint64_t>(report.successfulHandshakes),
          static_cast<std::uint64_t>(report.matchedSessions), static_cast<std::uint64_t>(report.firstCaptureEnd),
          static_cast<std::uint64_t>(report.secondCaptureEnd), static_cast<std::uint64_t>(report.failed),
          static_cast<std::uint64_t>(report.timedOut), static_cast<std::uint64_t>(report.exitObserved),
          static_cast<std::uint64_t>(report.exitCode), static_cast<std::uint64_t>(report.parentTerminationRequested),
          static_cast<std::uint64_t>(report.sniPresent), static_cast<std::uint64_t>(report.sniLocalhost)})
        numeric += std::to_string(value) + '\t';

    numeric += '\n';
    write(run / (std::string(name) + ".peer.tsv"), numeric);
    require(report.exitObserved && !report.childRunning && !report.parentTerminationRequested,
            "owned peer reaped without forced termination");

    return report;
}

/** Prepare ordinary server/helper settings, never constructing parent-side TLS state.
 * @param control Existing owned directory for exact child control directories.
 * @param version Actual version the child server must negotiate.
 * @return Explicit finite helper script inputs.
 */
h::PeerOptions peerOptions(const fs::path &control, h::PeerTlsVersion version) {
    h::PeerOptions result;
    result.childExecutable = fs::path(ORG_TTLDTOR_QDCXX_TLS_TEST_PEER);
    result.certificateFile = fs::path(ORG_TTLDTOR_QDCXX_TLS_TEST_FIXTURES) / "server.pem";
    result.privateKeyFile = fs::path(ORG_TTLDTOR_QDCXX_TLS_TEST_FIXTURES) / "server.key";
    result.controlRoot = control;
    result.tlsVersion = version;
    result.closeMode = h::PeerCloseMode::HOLD;
    result.captureUntilClose = true;
    result.readTimeout = 4s;
    result.overallTimeout = 10s;

    return result;
}

/** Configure real challenge/ACK/ADD ordering with the existing helper's two exact read barriers.
 * @param options Explicit ordinary server script to fill.
 * @param local Complete selected schema.
 * @param token Exact public synthetic credential.
 * @param data Complete independent DATA frame.
 */
void authenticated(h::PeerOptions &options, const c::LocalSchema &local, std::string_view token, const Bytes &data) {
    options.initialBytes = property(u"authentication", CHALLENGE);
    options.expectedClientBytes = clientPrelude(local);
    append(*options.expectedClientBytes, property(u"authorization", widen(token)));
    options.replyBytes = property(u"authentication", u"");
    append(options.replyBytes, wire(s::fixturePeerPrefix(local)));
    options.expectedReplyClientBytes = clientAdd();
    options.finalBytes = data;
}

/** Form actual CLI arguments with independent numeric route and DNS SAN reference identity.
 * @param peer Owned listening server.
 * @param trust Explicit ordinary CA path.
 * @param token Optional exact Basic token path; empty selects no provider.
 * @return Owned finite CLI input; test cases append only their selected policy overrides.
 */
std::vector<std::string> arguments(const h::TlsTestPeer &peer, const fs::path &trust, const fs::path &token = {}) {
    std::vector<std::string> result{"--mode",
                                    "recovery",
                                    "--host",
                                    "127.0.0.1",
                                    "--port",
                                    std::to_string(peer.port()),
                                    "--symbol",
                                    "IBM",
                                    "--security",
                                    "tls",
                                    "--tls-reference",
                                    "localhost",
                                    "--tls-trust-pem",
                                    pathArgument(trust),
                                    "--output",
                                    "quote",
                                    "--duration-ms",
                                    "2000",
                                    "--drain-ms",
                                    "1000",
                                    "--read-timeout-ms",
                                    "1500"};

    if (!token.empty())
        result.insert(result.end(), {"--auth-basic-token-file", pathArgument(token)});

    return result;
}

/** Run and reap one actual CLI using the existing strict native Child harness.
 * @param program Actual frozen CLI executable.
 * @param args Exact working argv.
 * @param run Owned captures.
 * @param name Fixed scenario name.
 * @param brokenOutput Deliberately close stdout after observing its actual version header.
 * @return Exact exit code and completed bounded captures.
 */
Capture execute(const fs::path &program, const std::vector<std::string> &args, const fs::path &run,
                std::string_view name, bool brokenOutput = false) {
    const auto out = run / (std::string(name) + ".out");
    const auto err = run / (std::string(name) + ".err");
    Child child(program, args, out, err, brokenOutput);

    if (brokenOutput)
        child.closeOutput(true);

    const auto code = child.wait();

    return {code, brokenOutput ? std::string{} : read(out), read(err)};
}

/** Preserve actual full expected client prefix for independent post-retirement capture equality.
 * @param local Complete fixture schema.
 * @param token Optional public synthetic token, empty when no challenge/provider is configured.
 * @param includeAdd Whether an actual ACK/readiness permits the initial subscription.
 * @return Full independently encoded actual client conversation expected from this finite scenario.
 */
Bytes expectedClient(const c::LocalSchema &local, std::string_view token = {}, bool includeAdd = true) {
    auto result = clientPrelude(local);

    if (!token.empty())
        append(result, property(u"authorization", widen(token)));

    if (includeAdd)
        append(result, clientAdd());

    return result;
}

/** Announce only a fixed case name and assertions result, never script or input bytes.
 * @tparam Action Synchronous body owning all its returning child operations.
 * @param name Fixed test case token.
 * @param action Real scenario to execute before PASS.
 */
template <class Action> void scenario(std::string_view name, Action &&action) {
    std::cout << "TLS_CLI_CASE " << name << " BEGIN\n" << std::flush;

    try {
        action();
        std::cout << "TLS_CLI_CASE " << name << " PASS\n" << std::flush;
    } catch (const std::exception &error) {
        ++failedCases;
        std::cerr << "TLS_CLI_CASE " << name << " FAIL " << error.what() << '\n' << std::flush;
    }
}
} // namespace

/** Execute real native TLS CLI cases, with automatic exact-owner failure cleanup.
 * The caller supplies a suitable test-owned RUN: known capture/input files are overwritten on rerun,
 * while the server control directory must be empty before any child starts. No existing child tree
 * is adopted or recursively removed.
 * @param argc Must be three: runner, actual CLI program, test-owned reusable RUN directory.
 * @param argv Explicit native paths; no environment discovery or shell subprocess execution.
 * @return Zero only after all actual assertions; otherwise one with static harness diagnostics.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 3, "TLS process runner PROGRAM RUN arguments");
        const fs::path program = fs::absolute(argv[1]);
        const fs::path run = fs::absolute(argv[2]);
        std::error_code error;
        fs::create_directories(run, error);
        require(!error && fs::is_directory(run, error) && !error, "owned TLS process RUN directory");
        const auto control = run / "controls";
        fs::create_directories(control, error);
        require(!error && fs::is_directory(control, error) && !error && fs::is_empty(control, error) && !error,
                "empty owned TLS control root");
        const auto trust = run / fs::path(u8"доверие-CA.pem");
        const auto tokenFile = run / fs::path(u8"секрет-Basic.token");
        write(trust, read(fs::path(ORG_TTLDTOR_QDCXX_TLS_TEST_FIXTURES) / "ca.pem"));
        write(tokenFile, TOKEN);
        auto builtin = root::events::defaultQuoteSchema(SCHEMA_LIMITS);
        require(builtin.has_value(), "full default Quote schema");
        auto local = c::LocalSchema::create(builtin->records(), SCHEMA_LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
        require(local.has_value(), "full binary default Quote schema");
        std::vector<c::FieldValue> fields;

        for (const auto &field : local->records()[0].fields)
            fields.push_back({static_cast<c::SerialFieldType>(field.serialTypeId), 0});

        const std::array<s::Record, 1> records{{{u"IBM", fields}}};
        const auto data = wire(s::fixturePeerData(records));

        for (const auto version : {h::PeerTlsVersion::TLS_1_2, h::PeerTlsVersion::TLS_1_3}) {
            const auto name = version == h::PeerTlsVersion::TLS_1_2 ? "verified-tls12" : "verified-tls13";
            scenario(name, [&] {
                auto options = peerOptions(control, version);
                options.initialBytes = wire(s::fixturePeerPrefix(*local));
                options.expectedClientBytes = expectedClient(*local);
                options.replyBytes = data;
                auto peer = h::TlsTestPeer::start(std::move(options));
                require(peer.has_value(), "owned TLS peer start");
                auto args = arguments(*peer, trust);
                args.insert(args.end(), {"--max-records", "1"});
                const auto observed = execute(program, args, run, name);
                const auto report = retire(*peer, run, name);
                require(observed.code == 0 && active(observed), "verified TLS actual ACTIVE and target exit");
                terminal(observed, "0");
                quote(observed);
                require(report.tlsVersion == static_cast<unsigned>(version) && report.handshakeComplete &&
                            report.expectedBytesMatched && !report.failed && report.exitCode == 0 &&
                            report.firstClientBytes == expectedClient(*local),
                        "actual full client protocol descriptions ADD before Quote DATA");
                require(report.sniPresent && report.sniLocalhost,
                        "explicit localhost reference independent of numeric route");
                redacted(observed, trust, tokenFile);
            });
        }

        for (const bool maximum : {false, true}) {
            const auto name = maximum ? "basic-maximum-4094" : "basic-challenge-ack-add";
            scenario(name, [&] {
                const auto token =
                    maximum ? std::string("Basic ") + std::string(MAXIMUM_TOKEN_UNITS - 6, 'A') : std::string(TOKEN);
                const auto path = maximum ? run / fs::path(u8"максимальный.token") : tokenFile;

                if (maximum)
                    write(path, token);

                auto options = peerOptions(control, h::PeerTlsVersion::TLS_1_3);
                authenticated(options, *local, token, data);
                auto peer = h::TlsTestPeer::start(std::move(options));
                require(peer.has_value(), "owned authentication peer start");
                auto args = arguments(*peer, trust, path);
                args.insert(args.end(), {"--max-records", "1"});
                const auto observed = execute(program, args, run, name);
                const auto report = retire(*peer, run, name);
                require(observed.code == 0 && active(observed), "actual verified Basic ACK eligibility and delivery");
                terminal(observed, "0");
                quote(observed);
                require(report.expectedBytesMatched && !report.failed && report.exitCode == 0 &&
                            report.firstClientBytes == expectedClient(*local, token),
                        "actual complete authorization followed by actual full ADD after empty ACK");
                require(observed.error.find(token) == std::string::npos &&
                            observed.output.find(token) == std::string::npos,
                        "actual supplied token never echoed");
                redacted(observed, trust, path);
            });
        }

        scenario("wrong-reference-no-authorization", [&] {
            auto options = peerOptions(control, h::PeerTlsVersion::TLS_1_3);
            options.initialBytes = property(u"authentication", CHALLENGE);
            auto peer = h::TlsTestPeer::start(std::move(options));
            require(peer.has_value(), "owned identity peer start");
            auto args = arguments(*peer, trust, tokenFile);
            const auto reference = std::ranges::find(args, "--tls-reference");
            require(reference != args.end(), "explicit reference argument");
            *(reference + 1) = "wrong-reference.invalid";
            const auto observed = execute(program, args, run, "wrong-reference");
            const auto report = retire(*peer, run, "wrong-reference");
            require(observed.code == 1 && !active(observed) && table(observed.output, "record").empty(),
                    "wrong reference never activates application delivery");
            require(tlsCause(observed, t::TlsError::IDENTITY_FAILURE), "typed reference verification failure");
            require(report.bytesRead == 0 && report.firstClientBytes.empty(),
                    "no authorization before actual identity verification");
            terminal(observed, {});
            redacted(observed, trust, tokenFile);
        });

        scenario("untrusted-chain-no-authorization", [&] {
            const auto otherTrust = run / fs::path(u8"другая-CA.pem");
            write(otherTrust, read(fs::path(ORG_TTLDTOR_QDCXX_TLS_TEST_FIXTURES) / "other-ca.pem"));
            auto options = peerOptions(control, h::PeerTlsVersion::TLS_1_3);
            options.initialBytes = property(u"authentication", CHALLENGE);
            auto peer = h::TlsTestPeer::start(std::move(options));
            require(peer.has_value(), "owned trust peer start");
            const auto observed = execute(program, arguments(*peer, otherTrust, tokenFile), run, "untrusted-chain");
            const auto report = retire(*peer, run, "untrusted-chain");
            require(observed.code == 1 && !active(observed) && table(observed.output, "record").empty(),
                    "untrusted chain never activates application delivery");
            require(tlsCause(observed, t::TlsError::TRUST_FAILURE), "typed actual chain verification failure");
            require(report.bytesRead == 0 && report.firstClientBytes.empty(),
                    "no authorization before actual chain verification");
            terminal(observed, {});
            redacted(observed, otherTrust, tokenFile);
        });

        for (const bool timeout : {false, true}) {
            const auto name = timeout ? "authentication-timeout" : "authentication-denied";
            scenario(name, [&] {
                auto options = peerOptions(control, h::PeerTlsVersion::TLS_1_3);
                options.initialBytes = property(u"authentication", CHALLENGE);
                options.expectedClientBytes = expectedClient(*local, TOKEN, false);

                if (!timeout)
                    options.replyBytes = property(u"authentication", u"synthetic-refused");

                auto peer = h::TlsTestPeer::start(std::move(options));
                require(peer.has_value(), "owned authentication failure peer start");
                auto args = arguments(*peer, trust, tokenFile);
                args.insert(args.end(), {"--auth-timeout-ms", "150"});
                const auto observed = execute(program, args, run, name);
                const auto report = retire(*peer, run, name);
                require(observed.code == 1 && !active(observed) && table(observed.output, "record").empty(),
                        "authentication refusal never fabricates ACTIVE or delivery");
                require(tlsCause(observed, timeout ? t::TlsError::DEADLINE : t::TlsError::AUTHENTICATION_DENIED),
                        "actual typed authentication failure retained");
                require(report.expectedBytesMatched && !report.failed &&
                            report.firstClientBytes == expectedClient(*local, TOKEN, false),
                        "actual one authorization and no ADD before actual ACK");
                terminal(observed, {});
                redacted(observed, trust, tokenFile);
            });
        }

        scenario("partial-protocol-clean-tls-eof", [&] {
            auto options = peerOptions(control, h::PeerTlsVersion::TLS_1_3);
            options.captureUntilClose = false;
            options.closeMode = h::PeerCloseMode::CLEAN;
            options.initialBytes = wire(s::fixturePeerPrefix(*local));
            options.expectedClientBytes = expectedClient(*local);
            options.replyBytes = data;
            options.replyBytes.push_back(0x81);
            auto peer = h::TlsTestPeer::start(std::move(options));
            require(peer.has_value(), "owned partial protocol peer start");
            const auto observed = execute(program, arguments(*peer, trust), run, "partial-protocol");
            const auto report = retire(*peer, run, "partial-protocol");
            require(observed.code == 1 && report.expectedBytesMatched,
                    "actual protocol partial EOF fails after full ADD");
            require(tlsCause(observed, t::TlsError::PROTOCOL_FAILURE), "typed partial protocol failure");
            terminal(observed, {});
            redacted(observed, trust, tokenFile);
        });

        scenario("abrupt-native-eof", [&] {
            auto options = peerOptions(control, h::PeerTlsVersion::TLS_1_3);
            options.captureUntilClose = false;
            options.closeMode = h::PeerCloseMode::ABRUPT;
            options.initialBytes = wire(s::fixturePeerPrefix(*local));
            options.expectedClientBytes = expectedClient(*local);
            options.replyBytes = data;
            auto peer = h::TlsTestPeer::start(std::move(options));
            require(peer.has_value(), "owned abrupt native peer start");
            const auto observed = execute(program, arguments(*peer, trust), run, "abrupt-native");
            const auto report = retire(*peer, run, "abrupt-native");
            require(observed.code == 1 && report.expectedBytesMatched, "actual abrupt encrypted EOF fails visibly");
            require(tlsCause(observed, t::TlsError::TRANSPORT_FAILURE), "typed native TLS truncation failure");
            terminal(observed, {});
            redacted(observed, trust, tokenFile);
        });

        scenario("callback-broken-stdout", [&] {
            auto options = peerOptions(control, h::PeerTlsVersion::TLS_1_3);
            options.initialBytes = wire(s::fixturePeerPrefix(*local));
            options.expectedClientBytes = expectedClient(*local);
            options.replyBytes = data;
            options.readDelay = 750ms;
            auto peer = h::TlsTestPeer::start(std::move(options));
            require(peer.has_value(), "owned broken stdout peer start");
            const auto observed = execute(program, arguments(*peer, trust), run, "broken-stdout", true);
            const auto report = retire(*peer, run, "broken-stdout");
            require(observed.code == 1 && report.expectedBytesMatched && !report.failed,
                    "actual TLS callback broken sink failure");
            const auto counts = table(observed.error, "queue-counts");
            require(counts.size() == 1 && counts[0].size() == 19 && counts[0][13] == "1" && counts[0][14] == "1",
                    "actual failed callback batch and record, rather than initial-header failure");
            terminal(observed, "2");
            redacted(observed, trust, tokenFile);
        });

        scenario("fresh-successor-authentication", [&] {
            auto options = peerOptions(control, h::PeerTlsVersion::TLS_1_3);
            authenticated(options, *local, TOKEN, data);
            options.successor.emplace();
            options.successor->initialBytes = options.initialBytes;
            options.successor->expectedClientBytes = options.expectedClientBytes;
            options.successor->replyBytes = options.replyBytes;
            options.successor->expectedReplyClientBytes = options.expectedReplyClientBytes;
            options.successor->finalBytes = options.finalBytes;
            auto peer = h::TlsTestPeer::start(std::move(options));
            require(peer.has_value(), "owned successor peer start");
            auto args = arguments(*peer, trust, tokenFile);
            const auto timeout = std::ranges::find(args, "--read-timeout-ms");
            require(timeout != args.end(), "explicit inactivity input");
            *(timeout + 1) = "200";
            args.insert(args.end(), {"--max-records", "2", "--attempts", "2", "--retry-delay-ms", "25"});
            const auto observed = execute(program, args, run, "successor");
            const auto report = retire(*peer, run, "successor");
            require(observed.code == 1 && active(observed),
                    "fresh successor delivers target while retaining STREAM gap refusal");
            terminal(observed, "1");
            quote(observed, 2);
            const auto queues = table(observed.error, "queue-terminal");
            require(queues.size() == 1 && queues[0].size() == 15 && queues[0][3] == "1" && queues[0][5] == "1" &&
                        queues[0][7] == "1",
                    "actual unresolved STREAM reset gap cannot fabricate DRAINED");
            const auto counts = table(observed.error, "queue-counts");
            require(counts.size() == 1 && counts[0].size() == 19 && counts[0][12] == "2" && counts[0][13] == "0" &&
                        counts[0][14] == "0",
                    "both actual callbacks completed normally despite upstream gap");
            const auto records = table(observed.output, "record");
            require(records[0][4] != records[1][4], "distinct actual run-local epochs after fresh attempt");
            require(report.successfulHandshakes == 2 && report.completedSessions == 2 && report.matchedSessions == 2 &&
                        !report.failed && report.firstClientBytes == expectedClient(*local, TOKEN) &&
                        report.secondClientBytes == expectedClient(*local, TOKEN),
                    "actual two fresh handshakes authorizations and full ADD prefixes independently captured");
            redacted(observed, trust, tokenFile);
        });

        require(fs::is_empty(control, error) && !error, "all owned peer control directories retired");
        std::cout << "TLS_CLI_OWNERS retired=all\n";

        return failedCases == 0 ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << "TLS CLI process test failed: " << error.what() << '\n';

        return 1;
    }
}
