// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: uses only finite owned loopback peers and reviewed synthetic fixtures.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/transport/tcp_runtime.hpp"

#include "deadline.hpp"
#include "dns_resolver.hpp"
#include "runtime_bootstrap.hpp"
#include "runtime_core.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/events/quote.hpp"

#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <cerrno>
#    include <fcntl.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace t = org::ttldtor::qdcxx::transport;
namespace td = org::ttldtor::qdcxx::transport::detail;
namespace p = org::ttldtor::qdcxx::protocol;
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace q = org::ttldtor::qdcxx::events;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket NO_SOCKET = INVALID_SOCKET; ///< Invalid owned Windows socket.
#else
using NativeSocket = int;
constexpr NativeSocket NO_SOCKET = -1; ///< Invalid owned POSIX socket.
#endif

/** Close exactly one owned native peer descriptor.
 * @param socket Descriptor; NO_SOCKET is ignored.
 */
void closeSocket(NativeSocket socket) noexcept {
    if (socket == NO_SOCKET) {
        return;
    }

#ifdef _WIN32
    closesocket(socket);
#else
    ::close(socket);
#endif
}

/** Configure a test-owned descriptor without changing any runtime-owned descriptor.
 * @param socket Owned peer descriptor.
 */
void nonblocking(NativeSocket socket) {
#ifdef _WIN32
    u_long enabled = 1;
    REQUIRE(ioctlsocket(socket, FIONBIO, &enabled) == 0);
#else
    REQUIRE(fcntl(socket, F_SETFL, O_NONBLOCK) == 0);
#endif
}

/** Single-threaded scripted loopback peer; each step attempts at most one accept/send/receive. */
struct Peer {
    NativeSocket listener = NO_SOCKET;   ///< Owned listening descriptor.
    NativeSocket connection = NO_SOCKET; ///< Owned accepted descriptor, never borrowed by the runtime.
    std::uint16_t port = 0;              ///< OS-assigned numeric loopback port.
    std::vector<std::byte> script;       ///< Finite exact peer-to-runtime bytes.
    std::vector<std::byte> received;     ///< Finite runtime-to-peer observation, bounded by the test workload.
    std::size_t sent = 0;                ///< Script cursor.
    std::size_t chunk = 1;               ///< Maximum peer bytes per native write attempt.
    bool read = true;                    ///< Whether the peer consumes runtime output.
    bool endInput = false;               ///< Whether to half-close after sending the complete script.
    bool ended = false;                  ///< Whether that half-close has been applied once.

    /** Bind a finite nonblocking IPv4 loopback listener after explicit process bootstrap.
     * @param receiveBuffer Optional per-peer SO_RCVBUF request; zero retains its OS default.
     */
    explicit Peer(int receiveBuffer = 0) {
        listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        REQUIRE(listener != NO_SOCKET);
        nonblocking(listener);

        if (receiveBuffer != 0) {
            REQUIRE(setsockopt(listener, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&receiveBuffer),
                               sizeof(receiveBuffer)) == 0);
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        REQUIRE(bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
        REQUIRE(listen(listener, 8) == 0);
#ifdef _WIN32
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        REQUIRE(getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length) == 0);
        port = ntohs(address.sin_port);
    }

    /** Close only this peer's accepted/listening descriptors. */
    ~Peer() {
        closeSocket(connection);
        closeSocket(listener);
    }

    /** Advance finite local peer behavior without waiting or touching runtime ownership. */
    void step() {
        if (connection == NO_SOCKET) {
            connection = accept(listener, nullptr, nullptr);

            if (connection == NO_SOCKET) {
                return;
            }

            nonblocking(connection);
        }

        if (sent != script.size()) {
            const auto count = (std::min)(chunk, script.size() - sent);
#ifdef _WIN32
            const auto result =
                send(connection, reinterpret_cast<const char *>(script.data() + sent), static_cast<int>(count), 0);
#else
            const auto result = send(connection, script.data() + sent, count, MSG_NOSIGNAL);
#endif

            if (result > 0) {
                sent += static_cast<std::size_t>(result);
            }
        } else if (endInput && !ended) {
#ifdef _WIN32
            REQUIRE(shutdown(connection, SD_SEND) == 0);
#else
            REQUIRE(shutdown(connection, SHUT_WR) == 0);
#endif
            ended = true;
        }

        if (read) {
            std::array<std::byte, 4096> bytes{};
#ifdef _WIN32
            const auto count =
                recv(connection, reinterpret_cast<char *>(bytes.data()), static_cast<int>(bytes.size()), 0);
#else
            const auto count = recv(connection, bytes.data(), bytes.size(), 0);
#endif

            if (count > 0) {
                received.insert(received.end(), bytes.begin(), bytes.begin() + count);
            }
        }
    }
};

/** Finite owned accept-backlog saturation; no connection is accepted or shared with another test. */
struct BacklogPeer {
    NativeSocket listener = NO_SOCKET; ///< Owned small-backlog listener.
    std::vector<NativeSocket> fillers; ///< Every pressure socket remains owned until teardown.
    std::uint16_t port = 0;            ///< Assigned local listener port.

    /** Create one loopback listener with a deliberately finite small accept backlog. */
    BacklogPeer() {
        listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        REQUIRE(listener != NO_SOCKET);
        nonblocking(listener);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        REQUIRE(bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
        REQUIRE(listen(listener, 1) == 0);
#ifdef _WIN32
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        REQUIRE(getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length) == 0);
        port = ntohs(address.sin_port);
    }

    /** Release exactly this fixture's pressure and listener descriptors. */
    ~BacklogPeer() {
        for (const auto socket : fillers) {
            closeSocket(socket);
        }

        closeSocket(listener);
    }

    /** Submit a finite512 nonblocking connection attempts without accepting any of them. */
    void saturate() {
        constexpr std::size_t ATTEMPTS = 512;
        fillers.reserve(ATTEMPTS);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);

        for (std::size_t i = 0; i < ATTEMPTS; ++i) {
            const auto socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            REQUIRE(socket != NO_SOCKET);
            fillers.push_back(socket);
            nonblocking(socket);
            (void)::connect(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address));
        }
    }
};

/** Append one DNS network-order short; wire constants remain explicit in the scripted peer.
 * @param bytes Owned packet.
 * @param value Unsigned16 value.
 */
void dnsShort(std::vector<std::byte> &bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::byte>(value >> 8));
    bytes.push_back(static_cast<std::byte>(value & 255));
}

/** Append one finite uncompressed DNS name, including arbitrary test octets inside a label.
 * @param bytes Owned packet.
 * @param name Dot-separated test name, without a final dot.
 */
void dnsName(std::vector<std::byte> &bytes, std::string_view name) {
    while (!name.empty()) {
        const auto dot = name.find('.');
        const auto count = dot == std::string_view::npos ? name.size() : dot;
        REQUIRE(count > 0);
        REQUIRE(count <= 63);
        bytes.push_back(static_cast<std::byte>(count));

        for (const unsigned char character : name.substr(0, count)) {
            bytes.push_back(static_cast<std::byte>(character));
        }

        if (dot == std::string_view::npos) {
            break;
        }

        name.remove_prefix(dot + 1);
    }

    bytes.push_back(std::byte{0});
}

/** Finite scripted DNS outcomes, using only valid locally generated DNS messages. */
enum class DnsMode { ADDRESS, HOLD, CNAME, CNAME_ONLY, LOOP, ESCAPED, UNRELATED, EXCESS, DUPLICATES, TCP };

/** Single-owner UDP/TCP resolver peer; no external DNS server, SDK parser or worker is used. */
struct DnsPeer {
    NativeSocket udp = NO_SOCKET;        ///< Owned datagram listener.
    NativeSocket tcp = NO_SOCKET;        ///< Owned stream listener on the same numeric port.
    NativeSocket accepted = NO_SOCKET;   ///< Owned accepted fallback stream.
    std::uint16_t port = 0;              ///< Shared OS-assigned DNS listener port.
    t::DnsServer server{{127, 0, 0, 1}}; ///< Stable explicit address storage borrowed only during factory calls.
    DnsMode mode;                        ///< Selected finite response script.
    std::vector<std::uint16_t> types;    ///< Observed wire question types, bounded by the test.
    std::vector<std::string> names;      ///< Observed exact original question names.
    std::vector<std::byte> tcpInput;     ///< Finite complete query-framing accumulator.
    std::vector<std::byte> tcpOutput;    ///< Finite exact response-framing bytes.
    std::size_t tcpSent = 0;             ///< Partial fallback response cursor.
    std::size_t tcpChunk = 7;            ///< Response fragment size, independent of runtime read capacities.

    /** Bind both local protocols without creating any background operation.
     * @param selected Finite response behavior.
     */
    explicit DnsPeer(DnsMode selected) : mode(selected) {
        try {
            constexpr std::size_t PAIR_ATTEMPTS = 64;
            int lastError = 0;

            for (std::size_t attempt = 0; attempt < PAIR_ATTEMPTS; ++attempt) {
                tcp = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                udp = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                REQUIRE(tcp != NO_SOCKET);
                REQUIRE(udp != NO_SOCKET);
                sockaddr_in address{};
                address.sin_family = AF_INET;
                address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

                // Reserve TCP first: a UDP ephemeral port need not be bindable in the separate TCP namespace.
                if (bind(tcp, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0) {
#ifdef _WIN32
                    int length = sizeof(address);
#else
                    socklen_t length = sizeof(address);
#endif
                    REQUIRE(getsockname(tcp, reinterpret_cast<sockaddr *>(&address), &length) == 0);

                    if (bind(udp, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0) {
                        port = ntohs(address.sin_port);
                        break;
                    }
                }

#ifdef _WIN32
                lastError = WSAGetLastError();
#else
                lastError = errno;
#endif
                closeSocket(tcp);
                closeSocket(udp);
                tcp = udp = NO_SOCKET;
            }

            INFO("dns-pair-bind-native=", lastError);
            REQUIRE(port != 0);
            nonblocking(tcp);
            nonblocking(udp);
            REQUIRE(listen(tcp, 4) == 0);
        } catch (...) {
            // A failed constructor has no destructor invocation; retire both owned descriptors here.
            closeSocket(tcp);
            closeSocket(udp);
            throw;
        }
    }

    /** Close only this peer's three owned descriptors. */
    ~DnsPeer() {
        closeSocket(accepted);
        closeSocket(tcp);
        closeSocket(udp);
    }

    /** Observe one finite exact question and construct its response; CNAME matching is tested independently.
     * @param query Complete unmodified query, without TCP length prefix.
     * @param truncated True for the UDP half of an explicit TCP-fallback script.
     * @return Exact finite answer, or empty for a silent peer.
     */
    std::vector<std::byte> answer(std::span<const std::byte> query, bool truncated) {
        REQUIRE(query.size() >= 17);
        REQUIRE(types.size() < 16);
        const auto type = static_cast<std::uint16_t>((std::to_integer<unsigned>(query[query.size() - 4]) << 8) |
                                                     std::to_integer<unsigned>(query[query.size() - 3]));
        types.push_back(type);
        std::string name;
        std::size_t position = 12;

        while (position < query.size() && query[position] != std::byte{0}) {
            const auto count = std::to_integer<std::size_t>(query[position++]);
            REQUIRE(count <= 63);
            REQUIRE(count <= query.size() - position);

            if (!name.empty()) {
                name.push_back('.');
            }

            for (std::size_t i = 0; i < count; ++i) {
                name.push_back(static_cast<char>(query[position++]));
            }
        }

        REQUIRE(position < query.size());
        names.push_back(name);

        if (mode == DnsMode::HOLD) {
            return {};
        }

        std::vector<std::byte> result(query.begin(), query.end());
        result[2] = truncated ? std::byte{0x83} : std::byte{0x81};
        result[3] = std::byte{0x80};
        result[6] = result[7] = result[8] = result[9] = result[10] = result[11] = std::byte{0};

        if (truncated || type == 28) {
            return result;
        }

        unsigned answers = 0;
        const auto appendRecord = [&](std::string_view owner, std::uint16_t recordType,
                                      std::span<const std::byte> payload) {
            dnsName(result, owner);
            dnsShort(result, recordType);
            dnsShort(result, 1);
            result.insert(result.end(), {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{60}});
            dnsShort(result, static_cast<std::uint16_t>(payload.size()));
            result.insert(result.end(), payload.begin(), payload.end());
            ++answers;
        };
        const auto cname = [&](std::string_view owner, std::string_view target) {
            std::vector<std::byte> payload;
            dnsName(payload, target);
            appendRecord(owner, 5, payload);
        };

        std::string_view owner = name;

        if (mode == DnsMode::CNAME || mode == DnsMode::CNAME_ONLY) {
            cname(name, "AlIaS.invalid");
            owner = "alias.INVALID";
        } else if (mode == DnsMode::LOOP) {
            cname(name, "alias.invalid");
            cname("alias.invalid", name);
        } else if (mode == DnsMode::ESCAPED) {
            cname(name, "bad\1.invalid");
        } else if (mode == DnsMode::UNRELATED) {
            owner = "unrelated.invalid";
        }

        if (mode != DnsMode::CNAME_ONLY && mode != DnsMode::LOOP && mode != DnsMode::ESCAPED) {
            const unsigned count = mode == DnsMode::EXCESS ? 65 : mode == DnsMode::DUPLICATES ? 128 : 1;

            for (unsigned i = 0; i < count; ++i) {
                const std::array<std::byte, 4> address{std::byte{127}, std::byte{0}, std::byte{0},
                                                       static_cast<std::byte>(mode == DnsMode::EXCESS ? i + 1 : 1)};
                appendRecord(owner, 1, address);
            }
        }

        result[6] = static_cast<std::byte>(answers >> 8);
        result[7] = static_cast<std::byte>(answers & 255);

        return result;
    }

    /** Advance at most one datagram and one finite TCP accept/read/write action without waiting. */
    void step() {
        std::array<std::byte, 4096> packet{};
        sockaddr_in source{};
#ifdef _WIN32
        int sourceLength = sizeof(source);
#else
        socklen_t sourceLength = sizeof(source);
#endif
        const auto received = recvfrom(udp, reinterpret_cast<char *>(packet.data()), static_cast<int>(packet.size()), 0,
                                       reinterpret_cast<sockaddr *>(&source), &sourceLength);

        if (received > 0) {
            const auto response =
                answer(std::span(packet).first(static_cast<std::size_t>(received)), mode == DnsMode::TCP);

            if (!response.empty()) {
                REQUIRE(sendto(udp, reinterpret_cast<const char *>(response.data()), static_cast<int>(response.size()),
                               0, reinterpret_cast<sockaddr *>(&source),
                               sourceLength) == static_cast<int>(response.size()));
            }
        }

        if (accepted == NO_SOCKET) {
            accepted = accept(tcp, nullptr, nullptr);

            if (accepted == NO_SOCKET) {
                return;
            }

            nonblocking(accepted);
            tcpInput.clear();
            tcpOutput.clear();
            tcpSent = 0;
        }

        const auto count = recv(accepted, reinterpret_cast<char *>(packet.data()), static_cast<int>(packet.size()), 0);

        if (count > 0) {
            REQUIRE(tcpInput.size() + static_cast<std::size_t>(count) <= packet.size());
            tcpInput.insert(tcpInput.end(), packet.begin(), packet.begin() + count);
        } else if (count == 0) {
            closeSocket(accepted);
            accepted = NO_SOCKET;

            return;
        }

        if (tcpInput.size() >= 2 && tcpOutput.empty()) {
            const auto length =
                (std::to_integer<std::size_t>(tcpInput[0]) << 8) | std::to_integer<std::size_t>(tcpInput[1]);

            if (tcpInput.size() >= length + 2) {
                const auto response = answer(std::span(tcpInput).subspan(2, length), false);
                dnsShort(tcpOutput, static_cast<std::uint16_t>(response.size()));
                tcpOutput.insert(tcpOutput.end(), response.begin(), response.end());
            }
        }

        if (tcpSent < tcpOutput.size()) {
            const auto bytes = (std::min)(tcpChunk, tcpOutput.size() - tcpSent);
#ifdef _WIN32
            const auto sent =
                send(accepted, reinterpret_cast<const char *>(tcpOutput.data() + tcpSent), static_cast<int>(bytes), 0);
#else
            const auto sent = send(accepted, tcpOutput.data() + tcpSent, bytes, MSG_NOSIGNAL);
#endif

            if (sent > 0) {
                tcpSent += static_cast<std::size_t>(sent);
            }
        }
    }
};

/** Supply a fresh explicit finite caller turn.
 * @return Test protocol credits, independent of the live run's lifetime.
 */
p::LiveWork liveTurn() {
    return {10000, 10000000, 10000000, 10000000};
}

/** Supply finite transport credits; DNS allowances are unused by numeric-only scenarios.
 * @return Independent caller-owned turn credits.
 */
t::TcpWork runtimeTurn() {
    return {128, 128, 10000000, 128, 2, 131070, 1000000000};
}

/** Configure the existing pure live protocol with explicit requested-capacity and logical limits.
 * @return Finite test profile.
 */
p::LiveLimits liveLimits() {
    p::LiveLimits result{};
    result.frame = {8192, 8201};
    result.protocol = {128, 64, {8192, 1024}, 16384};
    result.descriptions = {8192, 32, 32, 256, {8192, 1024}, 8192};
    result.context = {{16, 128, 256, 128, 16384}, 32, {512, 1024, 16384}};
    result.data = {8192, 64,   512,   64,    512,   8192, {8192, 128, 128}, {8192, 1024}, 8192, 8192, 8192,
                   8192, 8192, 16384, 16384, 16384, 64};
    result.residentBytes = 4 * 1024 * 1024;
    result.owners = 128;
    result.snapshotBytes = 65536;
    result.scratchBytes = 65536;
    result.eventBytes = 65536;
    result.outputBytes = 8192;
    result.credentials = p::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    result.fieldTime = p::FieldTimePolicy::DISABLED;
    result.events = p::DataEventPolicy::BINARY_CATALOG_V1;
    result.historySnapshot = true;

    return result;
}

/** Prepare explicit numeric-loopback runtime settings.
 * @param port Owned peer's numeric listener port.
 * @return Finite transport settings, with no resolver discovery or ambient input.
 */
t::TcpOptions options(std::uint16_t port) {
    return {"127.0.0.1", port, {}, 53, 1s, 1s, 5s, 5s, 5ms, {64, 7, 64, 16, 65536, 8192, 8 * 1024 * 1024}};
}

/** Matched ordinary default-Quote configuration with owning independent schema provenance. */
struct Configuration {
    c::LocalSchema schema;        ///< Immutable validated local configuration.
    p::SubscriptionWriter writer; ///< Exact matched outgoing layouts.
};

/** Build the real eight-field default Quote schema and exact complete outgoing declarations.
 * @return Matched owned configuration, suitable for transfer only after runtime preparation.
 */
Configuration configuration() {
    const c::SchemaLimits schemaBounds{16, 128, 256, 128, 16384};
    auto defaults = q::defaultQuoteSchema(schemaBounds);
    REQUIRE(defaults);
    auto schema = c::LocalSchema::create(defaults->records(), schemaBounds, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    p::OutgoingRecord declaration{17, 0, u"Quote", {}};
    const auto &record = schema->records()[0];

    for (std::size_t field = 0; field < record.fields.size(); ++field) {
        const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
            return entry.fieldId == field;
        });
        REQUIRE(lookup != record.lookups.end());
        declaration.fields.push_back(
            {lookup->name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
    }

    p::SubscriptionWriterWork work{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(*schema, std::span(&declaration, 1),
                                                {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, work);
    REQUIRE(writer);

    return {std::move(*schema), std::move(*writer)};
}

/** Prepare a fresh numeric run without activating network operations.
 * @param bootstrap Application-owned explicit process lifetime.
 * @param config Explicit finite socket settings.
 * @return Owned unactivated runtime.
 */
t::TcpRuntime runtime(t::RuntimeBootstrap &bootstrap, t::TcpOptions config) {
    auto source = configuration();
    auto work = liveTurn();

    if (!config.dnsServers.empty()) {
        const auto dns = t::TcpRuntime::dnsPreparationWorkRequirement(config.dnsServers.size());
        REQUIRE(dns);
        work.bytes += dns->bytes;
        work.visits += dns->visits;
    }

    auto result = t::TcpRuntime::create(bootstrap, std::move(source.schema), std::move(source.writer), liveLimits(),
                                        config, work);
    INFO("create error=", result ? 0 : static_cast<int>(result.error().error));
    REQUIRE(result);

    return std::move(*result);
}

/** Append one exact compact-long primitive to test-owned framing storage.
 * @param bytes Owned result buffer.
 * @param value Exact signed wire integer.
 */
void number(std::vector<std::byte> &bytes, std::int64_t value) {
    std::array<std::byte, 9> output{};
    const auto result = e::writeCompactLong(value, output);
    bytes.insert(bytes.end(), output.begin(), output.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Append one exact canonical UTFString to test-owned framing storage.
 * @param bytes Owned result buffer.
 * @param value Borrowed exact string.
 */
void text(std::vector<std::byte> &bytes, std::u16string_view value) {
    std::array<std::byte, 1024> output{};
    const auto result = e::writeUtfString(value, output, {1024, 256});
    REQUIRE(result.status == e::Status::OK);
    bytes.insert(bytes.end(), output.begin(), output.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Produce an independent selected peer readiness envelope using the documented DXP3 grammar.
 * @return Whole immutable type1 wire bytes.
 */
std::vector<std::byte> ready() {
    std::vector<std::byte> body;
    number(body, 1);
    body.insert(body.end(), {std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}});
    number(body, 0);
    number(body, 1);
    number(body, 15);
    text(body, u"STREAM_DATA");
    number(body, 0);
    number(body, 2);

    for (const auto &[id, name] :
         {std::pair{16, u"STREAM_ADD_SUBSCRIPTION"}, std::pair{17, u"STREAM_REMOVE_SUBSCRIPTION"}}) {
        number(body, id);
        text(body, name);
        number(body, 0);
    }

    std::vector<std::byte> result;
    number(result, static_cast<std::int64_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());

    return result;
}

/** Preserve exact original description/data frames from the reviewed standard Quote Java capture.
 * @return Readiness followed by unchanged whole description/data envelopes.
 */
std::vector<std::byte> quoteScript() {
    const auto path =
        std::filesystem::path(ORG_TTLDTOR_QDCXX_TCP_SOURCE_ROOT) / "tests/fixtures/protocol/quote/expected/time-1.qtp";
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    const std::vector<char> characters((std::istreambuf_iterator<char>(input)), {});
    const auto capture = std::as_bytes(std::span(characters));
    auto result = ready();
    p::FrameReader framing(liveLimits().frame);
    std::size_t offset = 0;

    while (offset < capture.size()) {
        auto framed = framing.feed(capture.subspan(offset));
        REQUIRE(framed.frame);
        offset += framed.accepted;

        if (framed.frame->type == 2 || framed.frame->type == 15) {
            const auto begin = static_cast<std::size_t>(framed.frame->startOffset);
            const auto size = static_cast<std::size_t>(framed.frame->endOffset - framed.frame->startOffset);
            const auto bytes = capture.subspan(begin, size);
            result.insert(result.end(), bytes.begin(), bytes.end());
        }
    }

    return result;
}

/** Run a finite bounded owner/peer loop until a caller predicate is satisfied.
 * @tparam Predicate Nonretained test-only completion predicate.
 * @param run Runtime owned on this thread.
 * @param peer Owned peer, or null for retirement without peer work.
 * @param done Test-only observation predicate, called after each result.
 * @param events Optional finite event collection used by ownership tests.
 */
template <class Predicate>
void pump(t::TcpRuntime &run, Peer *peer, Predicate done, std::vector<p::LiveEvent> *events = nullptr,
          DnsPeer *dnsPeer = nullptr) {
    const auto deadline = Clock::now() + 5s;

    while (!done() && Clock::now() < deadline) {
        if (peer) {
            peer->step();
        }

        if (dnsPeer) {
            dnsPeer->step();
        }

        auto work = runtimeTurn();
        const auto dns = run.dnsProgressWorkRequirement();
        work.dnsCalls = (std::max)(work.dnsCalls, dns.dnsCalls);
        work.dnsPackets = (std::max)(work.dnsPackets, dns.dnsPackets);
        work.dnsBytes = (std::max)(work.dnsBytes, dns.dnsBytes);
        work.dnsUnits = (std::max)(work.dnsUnits, dns.dnsUnits);
        auto protocolWork = liveTurn();

        for (unsigned step = 0; step < 32 && !done(); ++step) {
            auto result = run.poll(work, protocolWork);
            REQUIRE(result);

            if (result->event && events) {
                REQUIRE(events->size() < 64);
                events->push_back(std::move(*result->event));
            }
        }

        std::this_thread::sleep_for(1ms);
    }

    INFO("state=", static_cast<int>(run.state()), " pending=", run.resources().pendingOperations,
         " terminal-stage=", run.terminal() ? run.terminal()->primary.stage : "none");
    REQUIRE(done());
}

/** Request close and verify cooperative retirement under finite progress credits.
 * @param run Owned runtime on its recorded thread.
 */
void retire(t::TcpRuntime &run) {
    REQUIRE(run.requestClose());
    pump(run, nullptr, [&] {
        return run.terminal().has_value();
    });
    CHECK(run.resources().pendingOperations == 0);
    auto work = runtimeTurn();
    auto protocolWork = liveTurn();

    for (unsigned observation = 0; observation < 8; ++observation) {
        auto result = run.poll(work, protocolWork);
        REQUIRE(result);
        CHECK(result->status == t::TcpPollStatus::CLOSED);
    }
}

} // namespace

TEST_CASE("bootstrap readiness rejection preserves foreign SDK ownership and leaks no reference") {
#ifdef _WIN32
    REQUIRE(ares_library_initialized() == ARES_ENOTINITIALIZED);
#endif
    REQUIRE(ares_library_init(ARES_LIB_INIT_ALL) == ARES_SUCCESS);
    {
        // This reference is deliberately foreign to the bootstrap factory and is always retired by its owner.
        struct ForeignSdkReference {
            ~ForeignSdkReference() {
                ares_library_cleanup();
            }
        } foreign;

        std::size_t attempts = 0;
        const auto unavailable = +[](void *argument) noexcept {
            ++*static_cast<std::size_t *>(argument);
#ifdef _WIN32
            SetLastError(ERROR_ACCESS_DENIED);
#else
            errno = EACCES;
#endif

            return false;
        };
        auto rejected = td::BootstrapFactory::create(unavailable, &attempts);
        REQUIRE_FALSE(rejected);
        CHECK(attempts == 1);
        CHECK(rejected.error().error == t::TcpError::BOOTSTRAP_FAILURE);
        CHECK(rejected.error().stage == "bootstrap-entropy");
        CHECK(rejected.error().nativeCode == 0);
        CHECK(ares_library_initialized() == ARES_SUCCESS);
        {
            auto real = t::RuntimeBootstrap::create();
            REQUIRE(real);
            CHECK(ares_library_initialized() == ARES_SUCCESS);
        }
        CHECK(ares_library_initialized() == ARES_SUCCESS);
    }
#ifdef _WIN32
    CHECK(ares_library_initialized() == ARES_ENOTINITIALIZED);
#else
    // The pinned Unix getter always returns SUCCESS; it cannot observe the global reference count.
    CHECK(ares_library_initialized() == ARES_SUCCESS);
#endif
}

TEST_CASE("runtime preparation failures preserve schema and composer without activation") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto config = options(1);

    for (const auto contextBytes : {std::size_t{1}, std::size_t{16}, std::size_t{64}}) {
        auto source = configuration();
        config.limits.contextBytes = contextBytes;
        auto work = liveTurn();
        auto result = t::TcpRuntime::create(*bootstrap, std::move(source.schema), std::move(source.writer),
                                            liveLimits(), config, work);
        REQUIRE_FALSE(result);
        CHECK(result.error().error == t::TcpError::ALLOCATION_FAILURE);
        CHECK_FALSE(source.schema.records().empty());
        CHECK(source.writer.matchesSchema(source.schema));
    }

    auto source = configuration();
    auto security = liveLimits();
    security.credentials = p::LiveCredentialPolicy::VERIFIED_TLS_BASIC;
    auto work = liveTurn();
    auto rejected = t::TcpRuntime::create(*bootstrap, std::move(source.schema), std::move(source.writer), security,
                                          options(1), work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == t::TcpError::INVALID_CONFIGURATION);
    CHECK(source.writer.matchesSchema(source.schema));

    auto emptyWork = p::LiveWork{};
    auto noWork = t::TcpRuntime::create(*bootstrap, std::move(source.schema), std::move(source.writer), liveLimits(),
                                        options(1), emptyWork);
    REQUIRE_FALSE(noWork);
    CHECK(noWork.error().error == t::TcpError::WORK_LIMIT);
    CHECK(source.writer.matchesSchema(source.schema));
}

TEST_CASE("monotonic scheduling rejects overflow and preserves explicit synthetic time boundaries") {
    const auto minimum = Clock::time_point::min();
    const auto maximum = Clock::time_point::max();
    CHECK_FALSE(td::deadlineAfter(maximum, 1ms));
    CHECK_FALSE(td::deadlineAfter(Clock::time_point{}, 0ms));
    CHECK_FALSE(td::deadlineAfter(Clock::time_point{}, -1ms));
    CHECK(td::deadlineAfter(maximum - 1ms, 1ms) == maximum);
    CHECK(td::deadlineAfter(minimum, 1ms) == minimum + 1ms);
    const auto earlier = td::deadlineAfter(Clock::time_point{}, 10ms);
    const auto later = td::deadlineAfter(Clock::time_point{}, 11ms);
    REQUIRE(earlier);
    REQUIRE(later);
    CHECK(*earlier < *later);
}

TEST_CASE("a real standard Quote traverses byte fragments and retained owners survive close") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    Peer peer;
    peer.script = quoteScript();
    peer.endInput = true;
    auto source = configuration();
    auto mapping = q::PreparedQuote::prepare(source.schema, 0, {128, 256, 32, 64, 64, 128, 16384, 1000000});
    REQUIRE(mapping);
    auto work = liveTurn();
    auto created = t::TcpRuntime::create(*bootstrap, std::move(source.schema), std::move(source.writer), liveLimits(),
                                         options(peer.port), work);
    REQUIRE(created);
    std::vector<p::LiveEvent> events;
    REQUIRE(created->start());
    pump(
        *created, &peer,
        [&] {
            return created->terminal().has_value();
        },
        &events);
    REQUIRE(created->terminal()->primary.error == t::TcpError::NONE);
    CHECK(created->terminal()->peerEnded);
    CHECK(created->resources().pendingOperations == 0);
    const auto retainedBytes = created->resources().sessionBytes;
    CHECK(retainedBytes > 0);
    std::optional<q::Quote> quote;

    for (const auto &event : events) {
        for (const auto &record : event.records()) {
            std::vector<c::FieldValue> values;

            for (const auto &field : record.typedValues) {
                values.push_back({field.type, field.raw});
            }

            auto mapped = mapping->map(record.symbol, {record.context, values}, 128);
            REQUIRE(mapped);
            quote.emplace(std::move(*mapped));
        }
    }

    REQUIRE(quote);
    CHECK(quote->symbol == u"IBM");
    CHECK(quote->bidTimeMs == 1000);
    CHECK(quote->askTimeMs == 1000);
    CHECK(quote->timeMs == 1000);
    CHECK(created->resources().sessionBytes == retainedBytes);
    events.clear();
    CHECK(created->resources().sessionBytes < retainedBytes);
}

TEST_CASE("owned output survives repeated one-byte partial writes and full-slot rejection is atomic") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    Peer peer;
    auto config = options(peer.port);
    config.limits.writeBytes = 1;
    auto run = runtime(*bootstrap, config);
    auto work = liveTurn();
    const auto expectedBytes = std::vector<std::byte>{std::byte{2}, std::byte{0}, std::byte{0}};
    REQUIRE(run.composeHeartbeat({}, work));
    const auto occupiedBytes = run.resources().sessionBytes;
    auto rejected = run.composeHeartbeat({}, work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == t::TcpError::RESOURCE_LIMIT);
    CHECK(run.resources().sessionBytes == occupiedBytes);
    REQUIRE(run.start());
    pump(run, &peer, [&] {
        return peer.received.size() == expectedBytes.size();
    });
    CHECK(peer.received == expectedBytes);
    retire(run);
    CHECK(run.terminal()->reportedWriteBytes == expectedBytes.size());
    CHECK(run.terminal()->pendingOutputBytes == 0);
    CHECK(run.terminal()->aborted);
}

TEST_CASE("pending input cancellation drains every handler and each fresh run stays independent") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (unsigned iteration = 0; iteration < 24; ++iteration) {
        Peer peer;
        auto run = runtime(*bootstrap, options(peer.port));
        REQUIRE(run.start());
        pump(run, &peer, [&] {
            return run.state() == t::TcpState::CONNECTED && run.resources().pendingOperations == 1;
        });
        CHECK_FALSE(run.terminal());
        retire(run);
        INFO("primary-stage=", run.terminal()->primary.stage, " native=", run.terminal()->primary.nativeCode);
#ifdef _WIN32
        // Owned close can complete a pending read with ambiguous ERROR_CONNECTION_ABORTED instead of 995.
        const auto failure = run.terminal()->primary;
        CHECK((failure.error == t::TcpError::NONE ||
               (failure.error == t::TcpError::READ_FAILURE && failure.nativeCode == ERROR_CONNECTION_ABORTED &&
                failure.stage == "socket-read")));
#else
        CHECK(run.terminal()->primary.error == t::TcpError::NONE);
#endif
        CHECK(run.terminal()->aborted);
        auto work = liveTurn();
        auto rejected = run.composeHeartbeat({}, work);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == t::TcpError::INVALID_STATE);
    }
}

TEST_CASE("affinity rejection does not mutate run state and explicit close is allocation independent") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    Peer peer;
    auto run = runtime(*bootstrap, options(peer.port));
    std::optional<t::TcpDiagnostic> wrong;
    std::thread other([&] {
        auto result = run.start();
        REQUIRE_FALSE(result);
        wrong = result.error();
    });
    other.join();
    REQUIRE(wrong);
    CHECK(wrong->error == t::TcpError::WRONG_THREAD);
    CHECK(run.state() == t::TcpState::CONFIGURED);
    REQUIRE(run.requestClose());
    CHECK_FALSE(run.terminal());
    t::TcpWork noCredits{};
    auto work = liveTurn();
    auto closed = run.poll(noCredits, work);
    REQUIRE(closed);
    CHECK(closed->status == t::TcpPollStatus::CLOSED);
    CHECK(run.terminal()->aborted);
}

TEST_CASE("handler arena exhaustion becomes a typed terminal failure without an orphan operation") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    Peer peer;
    auto config = options(peer.port);
    config.limits.handlerBytes = 1;
    auto run = runtime(*bootstrap, config);
    REQUIRE(run.start());
    pump(run, &peer, [&] {
        return run.terminal().has_value();
    });
    CHECK(run.terminal()->primary.error == t::TcpError::ALLOCATION_FAILURE);
    CHECK(run.resources().pendingOperations == 0);
}

TEST_CASE("socket byte work yields without consuming the peer prefix and fresh turn resumes") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    Peer peer;
    peer.script = ready();
    peer.chunk = peer.script.size();
    auto run = runtime(*bootstrap, options(peer.port));
    REQUIRE(run.start());
    pump(run, &peer, [&] {
        return run.state() == t::TcpState::CONNECTED;
    });
    auto work = runtimeTurn();
    work.socketBytes = 0;
    auto sessionWork = liveTurn();
    bool yielded = false;

    for (unsigned step = 0; step < 16; ++step) {
        peer.step();
        auto result = run.poll(work, sessionWork);
        REQUIRE(result);
        CHECK_FALSE(result->event);
        yielded |= result->status == t::TcpPollStatus::YIELD;
    }

    CHECK(yielded);
    std::vector<p::LiveEvent> events;
    pump(
        run, &peer,
        [&] {
            return !events.empty();
        },
        &events);
    CHECK(events.front().kind() == p::LiveEventKind::PROTOCOL);
    retire(run);
}

TEST_CASE("idle read deadline is a failure and incomplete input never becomes clean EOF") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    SUBCASE("idle peer") {
        Peer peer;
        auto config = options(peer.port);
        config.readTimeout = 15ms;
        auto run = runtime(*bootstrap, config);
        REQUIRE(run.start());
        pump(run, &peer, [&] {
            return run.terminal().has_value();
        });
        CHECK(run.terminal()->primary.error == t::TcpError::DEADLINE);
        CHECK_FALSE(run.terminal()->aborted);
    }

    SUBCASE("incomplete envelope") {
        Peer peer;
        peer.script = {std::byte{7}, std::byte{1}, std::byte{'D'}};
        peer.endInput = true;
        auto run = runtime(*bootstrap, options(peer.port));
        REQUIRE(run.start());
        pump(run, &peer, [&] {
            return run.terminal().has_value();
        });
        CHECK(run.terminal()->primary.error == t::TcpError::PROTOCOL_FAILURE);
        CHECK_FALSE(run.terminal()->peerEnded);
        CHECK(run.terminal()->discardedInputBytes == peer.script.size());
    }
}

TEST_CASE("explicit DNS resolves serial families with bounded CNAME selection and copied address results") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto mode : {DnsMode::ADDRESS, DnsMode::CNAME, DnsMode::DUPLICATES, DnsMode::TCP}) {
        INFO("mode=", static_cast<int>(mode));
        Peer peer;
        peer.script = ready();
        DnsPeer dns(mode);
        auto config = options(peer.port);
        config.endpoint = "Feed.Invalid";
        config.dnsServers = std::span(&dns.server, 1);
        config.dnsPort = dns.port;
        config.dnsTimeout = 2s;
        config.limits.residentBytes = 128 * 1024 * 1024;

        if (mode == DnsMode::DUPLICATES) {
            config.limits.addresses = 1;
        }

        auto run = runtime(*bootstrap, config);
        const auto requirement = run.dnsProgressWorkRequirement();
        CHECK(requirement.dnsUnits > 0);
        CHECK(requirement.socketBytes == 0);
        CHECK(run.resources().dnsEnvelopeBytes == 80 * 1024 * 1024);
        REQUIRE(run.start());
        std::vector<p::LiveEvent> events;
        pump(
            run, &peer,
            [&] {
                return !events.empty() || run.terminal().has_value();
            },
            &events, &dns);
        INFO("terminal stage=", run.terminal() ? run.terminal()->primary.stage : "active");
        REQUIRE_FALSE(run.terminal());
        REQUIRE(events.size() == 1);
        CHECK(events.front().kind() == p::LiveEventKind::PROTOCOL);
        CHECK(run.resources().dnsEnvelopeBytes == 0);
        REQUIRE(dns.types.size() == (mode == DnsMode::TCP ? 4 : 2));
        CHECK(dns.types.front() == 1);
        CHECK(dns.types.back() == 28);

        for (const auto &name : dns.names) {
            CHECK(name == "Feed.Invalid");
        }

        retire(run);
        CHECK(run.resources().totalBytes <= config.limits.residentBytes);
        CHECK(run.resources().peakBytes <= config.limits.residentBytes);
    }
}

TEST_CASE("silent DNS cancellation and deadlines retire callbacks and the complete channel allowance") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (unsigned iteration = 0; iteration < 8; ++iteration) {
        DnsPeer dns(DnsMode::HOLD);
        auto config = options(1);
        config.endpoint = "hold.invalid";
        config.dnsServers = std::span(&dns.server, 1);
        config.dnsPort = dns.port;
        config.limits.residentBytes = 128 * 1024 * 1024;
        auto run = runtime(*bootstrap, config);
        REQUIRE(run.start());
        pump(
            run, nullptr,
            [&] {
                return !dns.types.empty();
            },
            nullptr, &dns);
        CHECK(run.state() == t::TcpState::RESOLVING);
        CHECK(run.resources().pendingOperations == 1);
        retire(run);
        CHECK(run.terminal()->primary.error == t::TcpError::NONE);
        CHECK(run.resources().dnsEnvelopeBytes == 0);
        const auto observed = dns.types.size();

        for (unsigned step = 0; step < 8; ++step) {
            dns.step();
        }

        CHECK(dns.types.size() == observed);
    }

    DnsPeer dns(DnsMode::HOLD);
    auto config = options(1);
    config.endpoint = "hold.invalid";
    config.dnsServers = std::span(&dns.server, 1);
    config.dnsPort = dns.port;
    config.dnsTimeout = 25ms;
    config.limits.residentBytes = 128 * 1024 * 1024;
    auto run = runtime(*bootstrap, config);
    REQUIRE(run.start());
    pump(
        run, nullptr,
        [&] {
            return run.terminal().has_value();
        },
        nullptr, &dns);
    CHECK(run.terminal()->primary.error == t::TcpError::DEADLINE);
    CHECK_FALSE(run.terminal()->aborted);
    CHECK(run.resources().pendingOperations == 0);
    CHECK(run.resources().dnsEnvelopeBytes == 0);
}

TEST_CASE("DNS ignores unrelated addresses and rejects unsupported aliases loops and excess results") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto mode :
         {DnsMode::UNRELATED, DnsMode::CNAME_ONLY, DnsMode::LOOP, DnsMode::ESCAPED, DnsMode::EXCESS}) {
        INFO("mode=", static_cast<int>(mode));
        DnsPeer dns(mode);
        auto config = options(1);
        config.endpoint = "feed.invalid";
        config.dnsServers = std::span(&dns.server, 1);
        config.dnsPort = dns.port;
        config.limits.residentBytes = 128 * 1024 * 1024;
        auto run = runtime(*bootstrap, config);
        REQUIRE(run.start());
        pump(
            run, nullptr,
            [&] {
                return run.terminal().has_value();
            },
            nullptr, &dns);
        const auto expected = mode == DnsMode::EXCESS    ? t::TcpError::RESOURCE_LIMIT
                              : mode == DnsMode::ESCAPED ? t::TcpError::UNSUPPORTED_DNS_NAME
                                                         : t::TcpError::DNS_FAILURE;
        CHECK(run.terminal()->primary.error == expected);
        CHECK(run.resources().pendingOperations == 0);
        CHECK(run.resources().dnsEnvelopeBytes == 0);

        for (const auto &name : dns.names) {
            CHECK(name == "feed.invalid");
        }
    }
}

TEST_CASE("injected UDP would-block resumes through the bridge hint absent from the SDK snapshot") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    DnsPeer dns(DnsMode::ADDRESS);
    auto config = options(1);
    config.endpoint = "hold.invalid";
    config.dnsServers = std::span(&dns.server, 1);
    config.dnsPort = dns.port;

    /** Instance-local finite send seam; the SDK/global allocators and socket functions remain unchanged. */
    struct SendState {
        bool block = true;   ///< Hold every initial-entry UDP attempt until the caller explicitly resumes.
        unsigned calls = 0;  ///< Bridge attempts, including injected non-native would-block results.
        unsigned native = 0; ///< Actual successful/failed native send attempts after resumption.
    } state;

    const auto attempt = [](ares_socket_t socket, const void *buffer, std::size_t length, int flags,
                            const sockaddr *address, ares_socklen_t addressLength,
                            void *context) noexcept -> ares_ssize_t {
        auto &current = *static_cast<SendState *>(context);
        ++current.calls;

        if (current.block) {
#ifdef _WIN32
            WSASetLastError(WSAEWOULDBLOCK);
#else
            errno = EWOULDBLOCK;
#endif

            return -1;
        }

        ++current.native;

        return sendto(socket, static_cast<const char *>(buffer), static_cast<int>(length), flags, address,
                      addressLength);
    };
    td::DnsResolver resolver(attempt, &state);
    REQUIRE(resolver.initialize(config));
    auto work = resolver.workRequirement();
    CHECK(resolver.progress(work) == t::TcpPollStatus::PROGRESS);
    CHECK(resolver.pending());
    CHECK(state.calls > 0);
    CHECK(state.native == 0);
    dns.step();
    CHECK(dns.types.empty());
    state.block = false;
    const auto deadline = Clock::now() + 2s;

    while (!resolver.finished() && !resolver.failure() && Clock::now() < deadline) {
        dns.step();
        work = resolver.workRequirement();
        (void)resolver.progress(work);
        std::this_thread::sleep_for(1ms);
    }

    INFO("stage=", resolver.failure() ? resolver.failure()->stage : "none");
    REQUIRE_FALSE(resolver.failure());
    REQUIRE(resolver.finished());
    REQUIRE(resolver.addresses().size() == 1);
    CHECK(resolver.addresses()[0].bytes[0] == 127);
    CHECK(resolver.addresses()[0].bytes[3] == 1);
    CHECK(state.native >= 2);
    CHECK(dns.types == std::vector<std::uint16_t>{1, 28});
    resolver.close();
    CHECK_FALSE(resolver.pending());
    CHECK(resolver.envelopeBytes() == 0);
}

TEST_CASE("a genuinely stalled short-buffer write retains its owner through abort or deadline") {
    constexpr std::size_t PAYLOAD_BYTES = 32 * 1024 * 1024;
    constexpr std::size_t OUTPUT_CAPACITY = 64 * 1024 * 1024;
    constexpr std::size_t WRITE_CHUNK_BYTES = 64 * 1024;
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const bool timeout : {false, true}) {
        INFO("timeout=", timeout);
        Peer peer(1024);
        peer.read = false;
        auto config = options(peer.port);
        config.socketSendBufferBytes = 4096;
        config.writeTimeout = 150ms;
        config.limits.writeBytes = WRITE_CHUNK_BYTES;
        config.limits.residentBytes = 256 * 1024 * 1024;
        auto bounds = liveLimits();
        bounds.frame = {OUTPUT_CAPACITY, OUTPUT_CAPACITY + 9};
        bounds.outputBytes = OUTPUT_CAPACITY;
        bounds.residentBytes = 192 * 1024 * 1024;
        bounds.protocol.text = {OUTPUT_CAPACITY, OUTPUT_CAPACITY};
        bounds.protocol.textUnits = OUTPUT_CAPACITY;
        auto source = configuration();
        auto work = liveTurn();
        work.bytes = work.visits = work.textUnits = 1024 * 1024 * 1024;
        auto result =
            t::TcpRuntime::create(*bootstrap, std::move(source.schema), std::move(source.writer), bounds, config, work);
        REQUIRE(result);
        auto &run = *result;
        const std::u16string payload(PAYLOAD_BYTES, u'x');
        const p::LivePropertyView properties[]{{u"payload", payload}};
        REQUIRE(run.composeProtocol({properties, {}, {}}, work));
        const auto retained = run.resources().sessionBytes;
        REQUIRE(run.start());
        pump(run, &peer, [&] {
            return run.terminal().has_value() ||
                   (run.state() == t::TcpState::CONNECTED && run.resources().pendingOperations == 2);
        });
        INFO("terminal-stage=", run.terminal() ? run.terminal()->primary.stage : "none");
        REQUIRE_FALSE(run.terminal());
        const auto observation = Clock::now() + 25ms;

        while (Clock::now() < observation) {
            peer.step();
            auto credits = runtimeTurn();
            auto protocolWork = liveTurn();

            for (unsigned step = 0; step < 32; ++step) {
                REQUIRE(run.poll(credits, protocolWork));
            }

            std::this_thread::sleep_for(1ms);
        }

        CHECK_FALSE(run.terminal());
        REQUIRE(run.resources().pendingOperations == 2);
        CHECK(run.resources().sessionBytes == retained);
        CHECK(peer.received.empty());
        std::cout
            << "genuinely-pending-write=1 observation-ms=25 send-buffer-request=4096 receive-buffer-request=1024\n";

        if (timeout) {
            pump(run, &peer, [&] {
                return run.terminal().has_value();
            });
            CHECK(run.terminal()->primary.error == t::TcpError::DEADLINE);
        } else {
            retire(run);
            CHECK(run.terminal()->aborted);
        }

        CHECK(run.terminal()->pendingOutputBytes > 0);
        CHECK(run.terminal()->reportedWriteBytes < payload.size());
#ifdef _WIN32
        CHECK(run.terminal()->writeAcceptanceUnknown);
#else
        CHECK_FALSE(run.terminal()->writeAcceptanceUnknown);
#endif
        CHECK(run.resources().pendingOperations == 0);
        CHECK(run.resources().sessionBytes < retained);
    }
}

TEST_CASE("a genuinely pending native connect retires without a resolver or reactor fallback thread") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    BacklogPeer peer;
    peer.saturate();
    auto config = options(peer.port);
    config.connectTimeout = 2s;
    auto run = runtime(*bootstrap, config);
    REQUIRE(run.start());
    pump(run, nullptr, [&] {
        return run.state() == t::TcpState::CONNECTING && run.resources().pendingOperations == 1;
    });
    const auto observation = Clock::now() + 25ms;

    while (Clock::now() < observation) {
        auto work = runtimeTurn();
        auto protocolWork = liveTurn();

        for (unsigned step = 0; step < 8; ++step) {
            REQUIRE(run.poll(work, protocolWork));
        }

        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(run.state() == t::TcpState::CONNECTING);
    REQUIRE(run.resources().pendingOperations == 1);
    std::cout << "genuinely-pending-connect=1 observation-ms=25 owned-pressure-sockets=512\n";
    retire(run);
    CHECK(run.terminal()->aborted);
    CHECK(run.terminal()->primary.error == t::TcpError::NONE);
    config.connectTimeout = 25ms;
    auto timed = runtime(*bootstrap, config);
    REQUIRE(timed.start());
    pump(timed, nullptr, [&] {
        return timed.terminal().has_value();
    });
    CHECK(timed.terminal()->primary.error == t::TcpError::DEADLINE);
    CHECK_FALSE(timed.terminal()->aborted);
    CHECK(timed.resources().pendingOperations == 0);
}

TEST_CASE("DNS and retained output overlap remains in historical peak after close") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    DnsPeer dns(DnsMode::HOLD);
    auto config = options(1);
    config.endpoint = "hold.invalid";
    config.dnsServers = std::span(&dns.server, 1);
    config.dnsPort = dns.port;
    config.limits.residentBytes = 128 * 1024 * 1024;
    auto bounds = liveLimits();
    bounds.outputBytes = 256 * 1024;
    auto source = configuration();
    auto work = liveTurn();
    auto preparation = t::TcpRuntime::dnsPreparationWorkRequirement(1);
    REQUIRE(preparation);
    work.bytes += preparation->bytes;
    work.visits += preparation->visits;
    auto created =
        t::TcpRuntime::create(*bootstrap, std::move(source.schema), std::move(source.writer), bounds, config, work);
    REQUIRE(created);
    auto &run = *created;
    const auto initial = run.resources();
    REQUIRE(run.composeHeartbeat({}, work));
    const auto composed = run.resources();
    CHECK(composed.peakBytes > initial.peakBytes);
    CHECK(composed.dnsEnvelopeBytes != 0);
    retire(run);
    const auto retired = run.resources();
    CHECK(retired.dnsEnvelopeBytes == 0);
    CHECK(retired.totalBytes < composed.totalBytes);
    CHECK(retired.peakBytes >= composed.peakBytes);
    CHECK(retired.peakBytes <= config.limits.residentBytes);
    CHECK(run.terminal()->reportedWriteBytes == 0);
    CHECK(run.terminal()->pendingOutputBytes == 3);
    CHECK_FALSE(run.terminal()->writeAcceptanceUnknown);
}

TEST_CASE("DNS interrupted-connect retry has a fixed entry cap despite excess caller credits") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    DnsPeer dns(DnsMode::HOLD);
    auto config = options(1);
    config.endpoint = "hold.invalid";
    config.dnsServers = std::span(&dns.server, 1);
    config.dnsPort = dns.port;
    std::size_t attempts = 0;
    const auto interrupted = [](ares_socket_t, const sockaddr *, ares_socklen_t, void *context) noexcept {
        ++*static_cast<std::size_t *>(context);
#ifdef _WIN32
        WSASetLastError(WSAEINTR);
#else
        errno = EINTR;
#endif

        return -1;
    };
    td::DnsResolver resolver(nullptr, nullptr, interrupted, &attempts);
    REQUIRE(resolver.initialize(config));
    const auto required = resolver.workRequirement();
    auto work = required;
    work.dnsCalls = 1000000;
    const auto initial = work.dnsCalls;
    CHECK(resolver.progress(work) == t::TcpPollStatus::PROGRESS);
    CHECK(attempts > 0);
    CHECK(attempts <= required.dnsCalls);
    CHECK(initial - work.dnsCalls == required.dnsCalls);
    resolver.close();
    CHECK_FALSE(resolver.pending());
    CHECK(resolver.envelopeBytes() == 0);
}

TEST_CASE("whole admitted records and snapshots retain only their pure epoch after runtime destruction") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    std::optional<p::LiveEvent> retained;
    p::LiveSnapshot snapshot;

    {
        Peer peer;
        peer.script = quoteScript();
        peer.chunk = peer.script.size();
        auto run = runtime(*bootstrap, options(peer.port));
        REQUIRE(run.start());
        std::vector<p::LiveEvent> events;
        pump(
            run, &peer,
            [&] {
                return std::ranges::any_of(events, [](const p::LiveEvent &event) {
                    return !event.records().empty();
                });
            },
            &events);

        for (auto &event : events) {
            if (!event.records().empty()) {
                snapshot = event.snapshot();
                retained.emplace(std::move(event));

                break;
            }
        }

        REQUIRE(retained);
        retire(run);
        CHECK(run.resources().sessionBytes > 0);
    }

    REQUIRE(retained);
    REQUIRE(retained->records().size() == 1);
    CHECK(retained->records()[0].symbol == u"IBM");
    CHECK(retained->snapshot().identity().matches(snapshot.identity()));
    CHECK_FALSE(snapshot.descriptions().empty());
}

#ifdef _WIN32
TEST_CASE("a dispatched non-cancellation write failure survives abort before output accounting") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    Peer peer;
    auto run = runtime(*bootstrap, options(peer.port));
    REQUIRE(run.start());
    pump(run, &peer, [&] {
        return run.state() == t::TcpState::CONNECTED;
    });
    peer.step();
    REQUIRE(peer.connection != NO_SOCKET);
    linger reset{1, 0};
    REQUIRE(setsockopt(peer.connection, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char *>(&reset), sizeof(reset)) ==
            0);
    closeSocket(peer.connection);
    peer.connection = NO_SOCKET;
    std::this_thread::sleep_for(25ms);
    auto protocolWork = liveTurn();
    REQUIRE(run.composeHeartbeat({}, protocolWork));
    auto work = runtimeTurn();
    auto submitted = run.poll(work, protocolWork);
    REQUIRE(submitted);
    REQUIRE(run.resources().pendingOperations == 1);
    const auto deadline = Clock::now() + 1s;

    while (run.resources().pendingOperations != 0 && Clock::now() < deadline) {
        work = runtimeTurn();
        REQUIRE(run.poll(work, protocolWork));
    }

    REQUIRE(run.resources().pendingOperations == 0);
    REQUIRE_FALSE(run.terminal());
    REQUIRE(run.requestClose());
    t::TcpWork noCredits{};
    auto retired = run.poll(noCredits, protocolWork);
    REQUIRE(retired);
    REQUIRE(run.terminal());
    CHECK(run.terminal()->aborted);
    CHECK(run.terminal()->primary.error == t::TcpError::WRITE_FAILURE);
    CHECK(run.terminal()->writeAcceptanceUnknown);
    CHECK(run.terminal()->pendingOutputBytes > 0);
}
#endif

namespace {
/** Complete one manually owned native handler without starting socket I/O.
 * @param core Real runtime owner with no submitted operations.
 * @param reading Exact input/output completion domain.
 * @param error Actual category-tagged native result.
 */
void completePauseProbe(td::RuntimeCore &core, bool reading, asio::error_code error) {
    core.pending = 1;

    if (reading)
        core.readPending = true;
    else
        core.writePending = true;

    core.completed(reading, error, 0);
    CHECK(core.pending == 0);
    CHECK(core.callbackDepth == 0);
}
} // namespace

TEST_CASE("actual plain pause orders owned cancellation after cutoff without erasing other evidence") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (unsigned scenario = 0; scenario != 8; ++scenario) {
        CAPTURE(scenario);
        auto lease = td::BootstrapLease::acquire(*bootstrap);
        REQUIRE(lease);
        auto *state = lease->state();
        td::RuntimeCore core(*state, std::move(*lease), options(1));
        CHECK_FALSE(core.pauseCancellation);
#ifdef _WIN32
        const asio::error_code cancelled(ERROR_CONNECTION_ABORTED, asio::error::get_system_category());
#else
        const asio::error_code cancelled = asio::error::connection_aborted;
#endif
        const asio::error_code reset = asio::error::connection_reset;

        if (scenario == 0) {
            completePauseProbe(core, true, cancelled);
            CHECK(core.outcome.primary.error == t::TcpError::READ_FAILURE);
            REQUIRE(core.requestPause());
            CHECK_FALSE(core.pauseCancellation);
            CHECK(core.outcome.primary.nativeCode == cancelled.value());
        } else if (scenario == 1) {
            REQUIRE(core.requestPause());
            CHECK(core.closeRequested);
            CHECK(core.closeApplied);
            CHECK(core.pauseCancellation);
            completePauseProbe(core, true, cancelled);
#ifdef _WIN32
            CHECK(core.outcome.primary.error == t::TcpError::NONE);
#else
            CHECK(core.outcome.primary.error == t::TcpError::READ_FAILURE);
#endif
        } else if (scenario == 2) {
            core.recordFailure({t::TcpError::PROTOCOL_FAILURE, "prior-protocol"});
            core.recordFailure({t::TcpError::WRITE_FAILURE, "prior-secondary", 17});
            REQUIRE(core.requestPause());
            completePauseProbe(core, true, cancelled);
            CHECK(core.outcome.primary.error == t::TcpError::PROTOCOL_FAILURE);
            CHECK(core.outcome.primary.stage == "prior-protocol");
            CHECK(core.outcome.secondary.stage == "prior-secondary");
            CHECK(core.outcome.secondary.nativeCode == 17);
        } else if (scenario == 3) {
            REQUIRE(core.requestClose());
            CHECK_FALSE(core.pauseCancellation);
            completePauseProbe(core, true, cancelled);
            CHECK(core.outcome.primary.error == t::TcpError::READ_FAILURE);
            CHECK(core.outcome.primary.nativeCode == cancelled.value());
        } else if (scenario == 4) {
            REQUIRE(core.requestPause());
            completePauseProbe(core, true, reset);
            CHECK(core.outcome.primary.error == t::TcpError::READ_FAILURE);
            CHECK(core.outcome.primary.nativeCode == reset.value());
        } else if (scenario == 5) {
            REQUIRE(core.requestPause());
            completePauseProbe(core, false, cancelled);
            CHECK(core.outcome.primary.error == t::TcpError::WRITE_FAILURE);
            CHECK(core.outcome.primary.nativeCode == cancelled.value());
        } else if (scenario == 6) {
            REQUIRE(core.requestPause());
            // WSAECONNABORTED is distinct from the owned ERROR_CONNECTION_ABORTED system result.
            completePauseProbe(core, true, asio::error::connection_aborted);
            CHECK(core.outcome.primary.error == t::TcpError::READ_FAILURE);
        } else {
            std::optional<t::TcpDiagnostic> rejection;
            std::thread wrong([&] {
                const auto result = core.requestPause();

                if (!result)
                    rejection = result.error();
            });
            wrong.join();
            REQUIRE(rejection);
            CHECK(rejection->error == t::TcpError::WRONG_THREAD);
            CHECK_FALSE(core.pauseCancellation);
            CHECK_FALSE(core.closeRequested);
        }
    }
}
