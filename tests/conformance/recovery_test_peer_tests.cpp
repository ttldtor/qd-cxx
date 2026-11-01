// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner for the byte-only native loopback fixture; no production protocol implementation.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "recovery_test_peer.hpp"

#include <array>
#include <thread>

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <cerrno>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

namespace {
namespace t = org::ttldtor::qdcxx::test;
using namespace std::chrono_literals;

/** Wait for a synchronized fixture predicate with an independent finite test deadline.
 * @tparam Predicate Nullary callable returning whether the observation is ready.
 * @param predicate Observation to poll on the test thread.
 * @return Whether the predicate became true within two seconds.
 */
template <class Predicate> bool waitFor(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return true;
}

/** Socket ownership begins at creation, including while a containing constructor unwinds. */
struct ClientSocket {
#ifdef _WIN32
    SOCKET value{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)}; ///< Sole owned native socket.
#else
    int value{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)}; ///< Sole owned native socket.
#endif
    ClientSocket() = default;
    ClientSocket(const ClientSocket &) = delete;
    ClientSocket &operator=(const ClientSocket &) = delete;

    /** Close an acquired socket even if a containing Client failed construction. */
    ~ClientSocket() {
#ifdef _WIN32

        if (value != INVALID_SOCKET) {
            closesocket(value);
        }

#else

        if (value >= 0) {
            close(value);
        }

#endif
    }
};

/** Independent native client, using only finite socket I/O timeouts. */
struct Client {
    ClientSocket socket; ///< Owns the socket before any constructor-body assertion can throw.

    /** Connect to the fixture's ephemeral loopback listener.
     * @param port Already bound native listener port.
     */
    explicit Client(std::uint16_t port) {
#ifdef _WIN32
        REQUIRE(socket.value != INVALID_SOCKET);
        const DWORD timeout = 1000;
#else
        REQUIRE(socket.value >= 0);
        const timeval timeout{1, 0};
#endif
        REQUIRE(setsockopt(socket.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout),
                           sizeof(timeout)) == 0);
        REQUIRE(setsockopt(socket.value, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout),
                           sizeof(timeout)) == 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        REQUIRE(connect(socket.value, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0);
    }

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    /** Send one deterministic fixture octet.
     * @param value Expected application byte.
     */
    void sendByte(char value) {
        REQUIRE(send(socket.value, &value, 1, 0) == 1);
    }

    /** Read one fixture octet under the finite receive timeout.
     * @return Received byte, checked by the caller.
     */
    char readByte() {
        char value{};
        REQUIRE(recv(socket.value, &value, 1, 0) == 1);

        return value;
    }
};

/** Build one-session configuration.
 * @param script Owned exchange script.
 * @return Valid finite helper configuration.
 */
t::TcpPeerConfig one(t::TcpPeerScript script = {}) {
    t::TcpPeerConfig config;
    config.sessions.push_back(std::move(script));

    return config;
}
} // namespace

TEST_CASE("native byte fixture exchanges three sessions on one ephemeral port") {
    t::TcpPeerScript script;
    script.initialBytes = {std::byte{0x11}};
    script.expectedClientBytes = {std::byte{0x22}};
    script.replyBytes = {std::byte{0x33}};
    script.waitForControllerClose = true;
    auto config = one(script);
    config.sessions.push_back(script);
    config.sessions.push_back(script);
    t::OwnedTcpPeer peer(std::move(config));
    REQUIRE(peer.port() != 0);

    for (std::size_t index = 0; index < 3; ++index) {
        Client client(peer.port());
        CHECK(client.readByte() == 0x11);
        client.sendByte(0x22);
        CHECK(client.readByte() == 0x33);
        REQUIRE(waitFor([&] {
            return peer.snapshot().sessions[index].replyBytesWritten == 1;
        }));
        const auto snapshot = peer.snapshot();
        CHECK(snapshot.sessions[index].accepted);
        CHECK(snapshot.sessions[index].matched);
        CHECK_FALSE(snapshot.sessions[index].completed);
        CHECK(snapshot.sessions[index].received == script.expectedClientBytes);
        CHECK(snapshot.sessions[index].initialBytesWritten == 1);
        CHECK(snapshot.sessions[index].replyBytesWritten == 1);
        peer.allowClose(index);
        REQUIRE(waitFor([&] {
            return peer.snapshot().sessions[index].completed;
        }));
    }

    REQUIRE(waitFor([&] {
        return peer.snapshot().finished;
    }));
    CHECK(peer.snapshot().error == t::TcpPeerError::NONE);
    peer.stop();
    peer.stop();
}

TEST_CASE("native fixture cancels blocked accept read and controller gate") {
    {
        t::OwnedTcpPeer peer(one());
        using NativeSocket = decltype(ClientSocket::value);
        NativeSocket observed{};
        enum class InjectedFailure { AFTER_ACQUISITION };

        /** Exercise the same owning member while its containing constructor fails. */
        struct PartialClient {
            ClientSocket socket; ///< Acquired before the explicit constructor failure.

            /** Retain the numeric handle for an independent native closed-handle observation.
             * @param observed Receives the handle before unwinding closes it.
             */
            explicit PartialClient(NativeSocket &observed) {
                observed = socket.value;
                throw InjectedFailure::AFTER_ACQUISITION;
            }
        };

        CHECK_THROWS_AS(PartialClient{observed}, InjectedFailure);
        int socketType{};
#ifdef _WIN32
        REQUIRE(observed != INVALID_SOCKET);
        int size = sizeof(socketType);
        CHECK(getsockopt(observed, SOL_SOCKET, SO_TYPE, reinterpret_cast<char *>(&socketType), &size) == SOCKET_ERROR);
        CHECK(WSAGetLastError() == WSAENOTSOCK);
#else
        REQUIRE(observed >= 0);
        socklen_t size = sizeof(socketType);
        CHECK(getsockopt(observed, SOL_SOCKET, SO_TYPE, &socketType, &size) == -1);
        CHECK(errno == EBADF);
#endif
    }

    for (unsigned iteration = 0; iteration < 10; ++iteration) {
        {
            t::OwnedTcpPeer peer(one());
            REQUIRE(peer.port() != 0);
            const auto started = std::chrono::steady_clock::now();
            peer.stop();
            CHECK(std::chrono::steady_clock::now() - started < 1s);
            CHECK(peer.snapshot().finished);
            CHECK(peer.snapshot().error == t::TcpPeerError::NONE);
        }

        for (const bool gate : {false, true}) {
            t::TcpPeerScript script;
            script.clientBytesToRead = gate ? 0 : 1;
            script.waitForControllerClose = gate;
            t::OwnedTcpPeer peer(one(script));
            Client client(peer.port());
            REQUIRE(waitFor([&] {
                return peer.snapshot().sessions[0].accepted;
            }));
            const auto started = std::chrono::steady_clock::now();
            peer.stop();
            CHECK(std::chrono::steady_clock::now() - started < 1s);
            CHECK(peer.snapshot().finished);
            CHECK(peer.snapshot().error == t::TcpPeerError::NONE);
            CHECK_FALSE(peer.snapshot().sessions[0].completed);
        }
    }
}

TEST_CASE("native fixture reports exact mismatch premature EOF and deadline") {
    {
        t::TcpPeerScript script;
        script.expectedClientBytes = {std::byte{0x22}};
        t::OwnedTcpPeer peer(one(script));
        Client client(peer.port());
        client.sendByte(0x44);
        REQUIRE(waitFor([&] {
            return peer.snapshot().finished;
        }));
        CHECK(peer.snapshot().error == t::TcpPeerError::MISMATCH);
        CHECK_FALSE(peer.snapshot().sessions[0].matched);
    }
    {
        t::TcpPeerScript script;
        script.clientBytesToRead = 1;
        t::OwnedTcpPeer peer(one(script));
        {
            Client client(peer.port());
        }
        REQUIRE(waitFor([&] {
            return peer.snapshot().finished;
        }));
        CHECK(peer.snapshot().error == t::TcpPeerError::PEER_CLOSED);
    }

    for (const bool connectClient : {false, true}) {
        t::TcpPeerScript script;
        script.clientBytesToRead = 1;
        auto config = one(script);
        config.lifetime = 100ms;
        t::OwnedTcpPeer peer(std::move(config));
        std::unique_ptr<Client> client;

        if (connectClient) {
            client = std::make_unique<Client>(peer.port());
        }

        REQUIRE(waitFor([&] {
            return peer.snapshot().finished;
        }));
        CHECK(peer.snapshot().error == t::TcpPeerError::TIMEOUT);
    }
}

TEST_CASE("native fixture rejects invalid finite budgets before binding") {
    std::array<t::TcpPeerConfig, 6> cases;
    cases[0] = {};
    cases[1] = one();
    cases[1].sessions.resize(4);
    cases[2] = one();
    cases[2].lifetime = 0ms;
    cases[3] = one();
    cases[3].lifetime = std::chrono::milliseconds::max();
    cases[4] = one();
    cases[4].sessions[0].clientBytesToRead = 5;
    cases[4].maximumBytesPerSession = 4;
    cases[5] = one();
    cases[5].sessions[0].clientBytesToRead = 1;
    cases[5].sessions[0].expectedClientBytes.resize(2);

    for (auto &config : cases) {
        t::OwnedTcpPeer peer(std::move(config));
        CHECK(peer.port() == 0);
        CHECK(peer.snapshot().finished);
        CHECK(peer.snapshot().error == t::TcpPeerError::INVALID_CONFIG);
    }
}
