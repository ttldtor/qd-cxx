// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
/** Internal loopback peer for the installed Quote console consumer.
 * Invocation: qdcxx_quote_example_peer PORT_FILE REPLY_GATE STOP_FILE [DNS_PORT_FILE].
 * Inputs are three or four fresh caller-owned paths with distinct normalized spellings, ignoring case on Windows.
 * Only their existence is read; gate contents
 * are never inspected. PORT_FILE is created without replacement and contains the ephemeral IPv4
 * loopback port. REPLY_GATE permits one canonical Quote DATA reply; STOP_FILE stops/joins only this
 * helper's peer. Optional DNS_PORT_FILE publishes the existing five-second loopback UDP DNS fixture's port;
 * route.invalid resolves through alias.route.invalid to 127.0.0.1, with no AAAA address. No process is launched
 * and no external endpoint or ambient DNS is used.
 * The complete controller deadline is twenty seconds. Created files are retained for the caller.
 * Exit0 means explicit stop or a permitted post-reply peer close with complete requested bytes;
 * exit1 means bootstrap/I/O/socket/deadline/incomplete-exchange failure; exit2 means invalid/stale paths.
 * Standalone --help and no arguments produce identical help without schema/bootstrap/file/socket work.
 */
#include "recovery_test_peer.hpp"
#include "transport/tls_dns_peer.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/penta_symbol.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/events/quote.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace e = root::encoding;
namespace q = root::events;
namespace t = root::transport;
namespace peer = root::test;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
constexpr auto LIFETIME = std::chrono::seconds{20};          ///< Whole controller/peer deadline from local setup.
constexpr auto POLL_INTERVAL = std::chrono::milliseconds{1}; ///< Caller-selected local file-gate cadence.
constexpr std::int32_t SENDER_ID = 41; ///< Explicit directional source identity, independent of local ID zero.
constexpr c::SchemaLimits SCHEMA_LIMITS{1, 8, 32, 128, 4096}; ///< Full unchanged eight-field Quote schema bounds.

/** Show the internal invocation contract without starting owners or inspecting files.
 * @return Zero on successful stdout output; one on output failure.
 */
int help() {
    std::cout << "Usage: qdcxx_quote_example_peer PORT_FILE REPLY_GATE STOP_FILE [DNS_PORT_FILE]\n"
                 "Internal IPv4 loopback peer for the installed Quote subscription example.\n"
                 "All supplied paths must be distinct and absent before startup.\n"
                 "Normalized path comparison ignores case on Windows and is exact on POSIX.\n"
                 "Creates PORT_FILE once with the ephemeral port; retains all caller-owned files.\n"
                 "REPLY_GATE permits canonical OTHER/IBM full Quote DATA; STOP_FILE stops this peer.\n"
                 "Optional DNS_PORT_FILE creates a loopback UDP DNS port receipt; no TLS dependency.\n"
                 "Its five-second fixture answers route.invalid via CNAME with IPv4 loopback and no AAAA address.\n"
                 "Deadline: 20 seconds. Output: one numeric exchange report after stop.\n"
                 "Exit: 0 controlled complete exchange/stop, 1 failure, 2 invalid or stale paths.\n";
    std::cout.flush();

    return std::cout ? 0 : 1;
}

/** Observe path presence without following a dangling symlink or opening its contents.
 * @param path Explicit caller-owned port or controller path.
 * @return True for any existing filesystem object; false only for a missing path.
 * @throws fs::filesystem_error If inspection fails for another reason.
 */
bool present(const fs::path &path) {
    std::error_code error;
    const auto status = fs::symlink_status(path, error);

    if (error == std::errc::no_such_file_or_directory) {
        return false;
    }

    if (error) {
        throw fs::filesystem_error("Cannot inspect a controller path", path, error);
    }

    return fs::exists(status);
}

/** Compare normalized controller spellings without changing filesystem or process settings.
 * Windows comparison is conservatively case-insensitive even for a case-sensitive directory.
 * Parent junction/symlink aliases are outside this lexical comparison.
 * @param first Absolute normalized controller path.
 * @param second Absolute normalized controller path.
 * @return Whether the spellings alias under the selected platform comparison.
 * @throws std::length_error If a Windows comparison length cannot be represented.
 * @throws std::system_error If the Windows ordinal comparison fails.
 */
bool sameControllerPath(const fs::path &first, const fs::path &second) {
#ifdef _WIN32
    const auto left = first.generic_wstring();
    const auto right = second.generic_wstring();
    const auto maximum = static_cast<std::size_t>(std::numeric_limits<int>::max());

    if (left.size() > maximum || right.size() > maximum) {
        throw std::length_error("Controller path comparison length exceeds the Windows API domain");
    }

    const auto compared = ::CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                                                 static_cast<int>(right.size()), TRUE);

    if (compared == 0) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                "Cannot compare controller path spellings");
    }

    return compared == CSTR_EQUAL;
#else

    return first == second;
#endif
}

/** Validate every supplied controller path before bootstrap, including an optional DNS port receipt.
 * @param paths Absolute normalized input spellings; main supplies exactly three or four.
 * @return False for any existing object or conflicting pair under the platform comparison.
 */
bool freshPaths(std::span<const fs::path> paths) {
    for (std::size_t index = 0; index < paths.size(); ++index) {
        if (present(paths[index])) {
            return false;
        }

        for (std::size_t previous = 0; previous < index; ++previous) {
            if (sameControllerPath(paths[previous], paths[index])) {
                return false;
            }
        }
    }

    return true;
}

/** Publish a controller-owned numeric port once, without replacing another receipt.
 * @param path Previously validated fresh TCP or DNS port path.
 * @param port Actual bound loopback port, never a guessed fixed port.
 * @throws std::runtime_error If create-only opening or flushed output fails.
 */
void publishPort(const fs::path &path, std::uint16_t port) {
    std::ofstream receipt(path, std::ios::out | std::ios::noreplace);
    receipt << port << '\n';
    receipt.flush();

    if (!receipt) {
        throw std::runtime_error("Cannot create a port receipt without replacement");
    }
}

/** Append a canonical compact scalar using the public production encoder.
 * @param bytes Owned synthetic fixture payload.
 * @param value Exact scalar, message ID or bounded length.
 */
void number(std::vector<std::byte> &bytes, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    const auto encoded = e::writeCompactLong(value, buffer);

    if (encoded.status != e::Status::OK) {
        throw std::runtime_error("Fixture compact encoding failed");
    }

    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(encoded.written));
}

/** Append an exact non-null UTFString using the public production encoder.
 * @param bytes Owned synthetic fixture payload.
 * @param value Bounded record, field or protocol name.
 */
void text(std::vector<std::byte> &bytes, std::u16string_view value) {
    std::array<std::byte, 1024> buffer{};
    const auto encoded = e::writeUtfString(value, buffer, {1024, 128});

    if (encoded.status != e::Status::OK) {
        throw std::runtime_error("Fixture text encoding failed");
    }

    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(encoded.written));
}

/** Wrap one complete native envelope without implementing a parser or socket layer.
 * @param type Exact QTP message identity.
 * @param payload Complete synthetic body, excluding the type.
 * @return Owned complete frame bytes.
 */
std::vector<std::byte> frame(std::int32_t type, std::span<const std::byte> payload) {
    std::vector<std::byte> body;
    number(body, type);
    body.insert(body.end(), payload.begin(), payload.end());
    std::vector<std::byte> result;
    number(result, static_cast<std::int64_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());

    return result;
}

/** Advertise actual STREAM input/output readiness to the unchanged production recovery owner.
 * @return DXP3 descriptor with SEND STREAM_DATA and RECEIVE STREAM add/remove.
 */
std::vector<std::byte> readiness() {
    std::vector<std::byte> bytes{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    number(bytes, 0);
    number(bytes, 1);
    number(bytes, 15);
    text(bytes, u"STREAM_DATA");
    number(bytes, 0);
    number(bytes, 2);

    for (const auto &[id, name] :
         std::array{std::pair{16, u"STREAM_ADD_SUBSCRIPTION"}, std::pair{17, u"STREAM_REMOVE_SUBSCRIPTION"}}) {
        number(bytes, id);
        text(bytes, name);
        number(bytes, 0);
    }

    return frame(1, bytes);
}

/** Describe every actual default Quote field, using its explicit schema lookups and exact full types.
 * @param schema Independently constructed complete eight-field default Quote schema.
 * @return DESCRIBE_RECORDS with directional sender41 and unchanged Quote identity.
 */
std::vector<std::byte> description(const c::LocalSchema &schema) {
    if (schema.records().size() != 1 || schema.records()[0].fields.size() != q::QUOTE_FIELD_TYPES.size()) {
        throw std::runtime_error("The complete default Quote schema is required");
    }

    const auto &record = schema.records()[0];
    std::vector<std::byte> bytes;
    number(bytes, SENDER_ID);
    text(bytes, record.name);
    number(bytes, static_cast<std::int64_t>(record.fields.size()));

    for (std::size_t field = 0; field < record.fields.size(); ++field) {
        const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
            return entry.fieldId == field;
        });

        if (lookup == record.lookups.end()) {
            throw std::runtime_error("A default Quote field has no explicit lookup");
        }

        text(bytes, lookup->name);
        number(bytes, record.fields[field].serialTypeId);
    }

    return frame(2, bytes);
}

/** Append one complete Quote row with canonical symbol identity and deterministic exact raw values.
 * @param bytes Owned DATA payload.
 * @param symbol Known representable Penta symbol, either OTHER or IBM.
 */
void quote(std::vector<std::byte> &bytes, std::u16string_view symbol) {
    const auto encoded = e::encodePentaSymbol(symbol);

    if (!encoded || encoded->size == 0 || encoded->size > encoded->bytes.size()) {
        throw std::runtime_error("The fixture symbol must have a canonical Penta encoding");
    }

    bytes.insert(bytes.end(), encoded->bytes.begin(),
                 encoded->bytes.begin() + static_cast<std::ptrdiff_t>(encoded->size));
    number(bytes, SENDER_ID);
    constexpr std::array<std::int64_t, 8> RAW_VALUES{1, 'Q', 0, 0, 2, 'N', 0, 0};

    for (std::size_t field = 0; field < RAW_VALUES.size(); ++field) {
        if (q::QUOTE_FIELD_TYPES[field] == c::SerialFieldType::UTF_CHAR) {
            std::array<std::byte, 4> buffer{};
            const auto result = e::writeUtfChar(static_cast<std::int32_t>(RAW_VALUES[field]), buffer);

            if (result.status != e::Status::OK) {
                throw std::runtime_error("Fixture exchange-code encoding failed");
            }

            bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
        } else {
            // Decimal raw zero is the existing NaN representation, not a fabricated business price.
            number(bytes, RAW_VALUES[field]);
        }
    }
}

/** Build one controlled script without inventing incoming source views or matching client protocol bytes.
 * @param schema Complete immutable default Quote schema.
 * @return Byte-only exchange with explicit controller reply/close gates and one captured client byte.
 */
peer::TcpPeerScript script(const c::LocalSchema &schema) {
    peer::TcpPeerScript result;
    result.initialBytes = readiness();
    const auto described = description(schema);
    result.initialBytes.insert(result.initialBytes.end(), described.begin(), described.end());
    std::vector<std::byte> data;
    quote(data, u"OTHER");
    quote(data, u"IBM");
    result.replyBytes = frame(15, data);
    result.clientBytesToRead = 1;
    result.waitForControllerReply = true;
    result.waitForControllerClose = true;

    return result;
}

/** Run only the owned loopback peer under explicit file gates and one finite deadline.
 * @param paths Normalized distinct absent PORT_FILE, REPLY_GATE, STOP_FILE and optional DNS_PORT_FILE.
 * @return Documented success/failure status; the numeric report is emitted after owned peer retirement.
 */
int run(std::span<const fs::path> paths) {
    const auto deadline = Clock::now() + LIFETIME;
    auto schema = q::defaultQuoteSchema(SCHEMA_LIMITS);

    if (!schema) {
        std::cerr << "Default Quote schema failed; error=" << static_cast<int>(schema.error()) << '\n';

        return 1;
    }

    auto exchange = script(*schema);
    const auto initialBytes = exchange.initialBytes.size();
    const auto replyBytes = exchange.replyBytes.size();
    auto bootstrap = t::RuntimeBootstrap::create();

    if (!bootstrap) {
        std::cerr << "Bootstrap failed; error=" << static_cast<int>(bootstrap.error().error) << '\n';

        return 1;
    }

    std::optional<peer::TlsDnsPeer> dns;

    if (paths.size() == 4) {
        dns.emplace();
    }

    peer::TcpPeerConfig config;
    config.sessions.push_back(std::move(exchange));
    config.maximumBytesPerSession = 4096;
    config.lifetime = LIFETIME;
    peer::OwnedTcpPeer listener(std::move(config));

    if (listener.port() == 0) {
        const auto failed = listener.snapshot();
        std::cerr << "Peer startup failed; error=" << static_cast<int>(failed.error) << " native=" << failed.nativeCode
                  << '\n';

        return 1;
    }

    publishPort(paths[0], listener.port());

    if (dns) {
        publishPort(paths[3], dns->port());
    }

    bool replied = false;
    bool stopped = false;
    bool finished = false;

    while (Clock::now() < deadline) {
        if (present(paths[2])) {
            stopped = true;
            break;
        }

        if (dns) {
            dns->step();
        }

        if (!replied && present(paths[1])) {
            listener.allowReply(0);
            replied = true;
        }

        if (listener.snapshot().finished) {
            finished = true;
            break;
        }

        std::this_thread::sleep_for(POLL_INTERVAL);
    }

    listener.stop();
    const auto result = listener.snapshot();

    if (result.sessions.size() != 1) {
        std::cerr << "The owned peer returned an invalid session count.\n";

        return 1;
    }

    const auto &session = result.sessions[0];
    const bool deadlineExpired = !stopped && !finished;
    std::cout << "accepted=" << session.accepted << " initialBytesWritten=" << session.initialBytesWritten
              << " replyBytesWritten=" << session.replyBytesWritten << " clientBytesRead=" << session.received.size()
              << " error=" << static_cast<int>(result.error) << " nativeCode=" << result.nativeCode
              << " replyGate=" << replied << " stopped=" << stopped << " deadlineExpired=" << deadlineExpired
              << " dnsQueries=" << (dns ? dns->queries() : 0) << '\n';
    std::cout.flush();

    if (deadlineExpired) {
        std::cerr << "The controller deadline expired before stop or peer completion.\n";
    }

    const bool allowedError =
        result.error == peer::TcpPeerError::NONE || result.error == peer::TcpPeerError::PEER_CLOSED;
    const bool completeReply = !replied || (session.accepted && session.initialBytesWritten == initialBytes &&
                                            session.replyBytesWritten == replyBytes && session.received.size() == 1);
    const bool completedControl = stopped || (replied && finished && result.error == peer::TcpPeerError::PEER_CLOSED);

    return std::cout && allowedError && completeReply && completedControl ? 0 : 1;
}
} // namespace

/** Dispatch side-effect-free help or one explicitly owned controlled loopback exchange.
 * @param argc Raw argument count, including executable name.
 * @param argv Borrowed filesystem path arguments, never modified.
 * @return Documented internal runner status.
 */
int main(int argc, char **argv) {
    try {
        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
            return help();
        }

        if (argc != 4 && argc != 5) {
            std::cerr << "Expected PORT_FILE REPLY_GATE STOP_FILE [DNS_PORT_FILE]; use --help.\n";

            return 2;
        }

        std::vector<fs::path> paths;
        paths.reserve(static_cast<std::size_t>(argc - 1));

        for (int index = 1; index < argc; ++index) {
            paths.push_back(fs::absolute(argv[index]).lexically_normal());
        }

        if (!freshPaths(paths)) {
            std::cerr << "Controller paths must be distinct and fresh before bootstrap.\n";

            return 2;
        }

        return run(paths);
    } catch (const std::exception &error) {
        std::cerr << "Peer helper failure: " << error.what() << '\n';

        return 1;
    }
}
