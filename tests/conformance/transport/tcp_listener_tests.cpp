// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/transport/tcp_listener.hpp"

#include "qd/protocol/frame_reader.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <utility>

namespace {
namespace t = org::ttldtor::qdcxx::transport;
namespace p = org::ttldtor::qdcxx::protocol;
using asio::ip::tcp;
using namespace std::chrono_literals;
using std::byte;

template <class Attempt> void eventually(Attempt attempt) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;

    while (!attempt()) {
        REQUIRE(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(1ms);
    }
}

struct Peer {
    asio::io_context context;
    tcp::socket socket{context};

    explicit Peer(std::uint16_t port) {
        asio::error_code error;
        socket.connect({asio::ip::address_v4::loopback(), port}, error);
        REQUIRE_FALSE(error);
        socket.non_blocking(true, error);
        REQUIRE_FALSE(error);
    }

    void send(std::span<const byte> bytes) {
        eventually([&] {
            asio::error_code error;
            const auto count = socket.write_some(asio::buffer(bytes.data(), bytes.size()), error);
            REQUIRE((!error || error == asio::error::would_block || error == asio::error::try_again));
            bytes = bytes.subspan(count);

            return bytes.empty();
        });
    }

    void endInput() {
        asio::error_code error;
        socket.shutdown(tcp::socket::shutdown_send, error);
        REQUIRE_FALSE(error);
    }

    void receive(std::span<byte> bytes) {
        eventually([&] {
            asio::error_code error;
            const auto count = socket.read_some(asio::buffer(bytes.data(), bytes.size()), error);
            REQUIRE((!error || error == asio::error::would_block || error == asio::error::try_again));
            bytes = bytes.subspan(count);

            return bytes.empty();
        });
    }

    void checkClosed() {
        eventually([&] {
            asio::error_code error;
            byte unused{};
            const auto count = socket.read_some(asio::buffer(&unused, 1), error);
            CHECK(count == 0);
            REQUIRE((error == asio::error::eof || error == asio::error::connection_reset ||
                     error == asio::error::would_block || error == asio::error::try_again));

            return error == asio::error::eof || error == asio::error::connection_reset;
        });
    }
};

t::TcpConnectionId accept(t::TcpListener &listener) {
    std::optional<t::TcpConnectionId> connection;
    eventually([&] {
        const auto result = listener.accept();
        REQUIRE(result);
        connection = *result;

        return connection.has_value();
    });

    return *connection;
}

t::TcpIoResult read(t::TcpListener &listener, t::TcpConnectionId connection, std::span<byte> bytes) {
    t::TcpIoResult observation{};
    eventually([&] {
        const auto result = listener.read(connection, bytes);
        REQUIRE(result);
        observation = *result;

        return observation.status != t::TcpIoStatus::WOULD_BLOCK;
    });

    return observation;
}

void send(t::TcpListener &listener, t::TcpConnectionId connection, std::span<const byte> bytes, std::size_t cap) {
    eventually([&] {
        const auto result = listener.write(connection, bytes);
        REQUIRE(result);
        REQUIRE(result->bytes <= cap);

        if (result->status == t::TcpIoStatus::WOULD_BLOCK) {
            REQUIRE(result->bytes == 0);

            return false;
        }

        REQUIRE(result->status == t::TcpIoStatus::PROGRESS);
        REQUIRE(result->bytes > 0);
        bytes = bytes.subspan(result->bytes);

        return bytes.empty();
    });
}

} // namespace

TEST_CASE("accepted TCP streams keep independent partial QTP input and exact write suffixes") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto listener = t::TcpListener::create(*bootstrap, {.maxConnections = 2, .maxReadBytes = 1, .maxWriteBytes = 2});
    REQUIRE(listener);
    REQUIRE(listener->port() != 0);
    const auto emptyAccept = listener->accept();
    REQUIRE(emptyAccept);
    CHECK_FALSE(emptyAccept->has_value());

    Peer first(listener->port());
    const auto firstId = accept(*listener);
    Peer second(listener->port());
    const auto secondId = accept(*listener);
    CHECK(firstId != secondId);
    CHECK(listener->activeConnections() == 2);

    std::array<byte, 8> input{};
    const auto emptyRead = listener->read(firstId, input);
    REQUIRE(emptyRead);
    CHECK(emptyRead->status == t::TcpIoStatus::WOULD_BLOCK);
    CHECK(emptyRead->bytes == 0);

    constexpr std::array firstMessage{byte{3}, byte{1}, byte{11}, byte{12}};
    constexpr std::array secondMessage{byte{3}, byte{1}, byte{21}, byte{22}};
    p::FrameReader firstFramer({3, 12});
    p::FrameReader secondFramer({3, 12});
    first.send(std::span(firstMessage).first<1>());
    const auto firstPrefix = read(*listener, firstId, input);
    REQUIRE(firstPrefix.bytes == 1);
    CHECK(firstFramer.feed(std::span(input).first(firstPrefix.bytes)).status == p::FrameStatus::NEED_INPUT);

    second.send(secondMessage);

    for (std::size_t index = 0; index < secondMessage.size(); ++index) {
        const auto prefix = read(*listener, secondId, input);
        REQUIRE(prefix.bytes == 1);
        const auto frame = secondFramer.feed(std::span(input).first(prefix.bytes));

        if (index + 1 < secondMessage.size()) {
            CHECK(frame.status == p::FrameStatus::NEED_INPUT);
        } else {
            REQUIRE(frame.frame);
            CHECK(frame.frame->type == 1);
            CHECK(std::ranges::equal(frame.frame->payload, std::span(secondMessage).last<2>()));
        }
    }

    CHECK(firstFramer.pending().size() == 1);

    first.endInput();
    CHECK(read(*listener, firstId, input).status == t::TcpIoStatus::PEER_EOF);
    CHECK(firstFramer.finish() == p::FrameStatus::FAILED);
    CHECK(firstFramer.error() == p::FrameError::TRUNCATED_INPUT);
    CHECK(secondFramer.finish() == p::FrameStatus::FINISHED);
    const auto stickyEof = listener->read(firstId, input);
    REQUIRE(stickyEof);
    CHECK(stickyEof->status == t::TcpIoStatus::PEER_EOF);

    send(*listener, firstId, secondMessage, 2);
    send(*listener, secondId, firstMessage, 2);
    std::array<byte, firstMessage.size()> firstReceived{}, secondReceived{};
    first.receive(firstReceived);
    second.receive(secondReceived);
    CHECK(firstReceived == secondMessage);
    CHECK(secondReceived == firstMessage);

    REQUIRE(listener->shutdown());
    CHECK(listener->activeConnections() == 0);
    CHECK(listener->port() == 0);
    REQUIRE(listener->shutdown());
    first.checkClosed();
    second.checkClosed();
}

TEST_CASE("capacity preserves backlog and retired or alien identities cannot access a reused slot") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto listener = t::TcpListener::create(*bootstrap, {.maxConnections = 1});
    auto alien = t::TcpListener::create(*bootstrap, {.maxConnections = 1});
    REQUIRE(listener);
    REQUIRE(alien);
    Peer first(listener->port());
    const auto old = accept(*listener);
    Peer alienPeer(alien->port());
    const auto alienId = accept(*alien);
    std::array<byte, 1> bytes{};
    const auto wrongListener = listener->write(alienId, bytes);
    REQUIRE_FALSE(wrongListener);
    CHECK(wrongListener.error().error == t::TcpListenerError::INVALID_STATE);

    Peer waiting(listener->port());
    const auto full = listener->accept();
    REQUIRE_FALSE(full);
    CHECK(full.error().error == t::TcpListenerError::RESOURCE_LIMIT);
    CHECK(listener->activeConnections() == 1);
    REQUIRE(listener->retire(old));
    first.checkClosed();

    const auto next = accept(*listener);
    CHECK(next != old);
    const auto stale = listener->read(old, bytes);
    REQUIRE_FALSE(stale);
    CHECK(stale.error().error == t::TcpListenerError::INVALID_STATE);
    const auto invalidInput = listener->write(next, {});
    REQUIRE_FALSE(invalidInput);
    CHECK(invalidInput.error().error == t::TcpListenerError::INVALID_ARGUMENT);

    auto moved = std::move(*listener);
    const auto movedAccept = listener->accept();
    REQUIRE_FALSE(movedAccept);
    CHECK(movedAccept.error().error == t::TcpListenerError::INVALID_STATE);
    CHECK(moved.activeConnections() == 1);

    std::optional<t::TcpListenerDiagnostic> foreignError;
    std::jthread foreign([&] {
        const auto foreignClose = moved.shutdown();

        if (!foreignClose) {
            foreignError = foreignClose.error();
        }
    });
    foreign.join();
    REQUIRE(foreignError);
    CHECK(foreignError->error == t::TcpListenerError::WRONG_THREAD);

    bytes[0] = byte{42};
    send(moved, next, bytes, 1);
    bytes[0] = byte{0};
    waiting.receive(bytes);
    CHECK(bytes[0] == byte{42});
    REQUIRE(moved.shutdown());
    waiting.checkClosed();
    CHECK(alien->activeConnections() == 1);
    REQUIRE(alien->retire(alienId));
    alienPeer.checkClosed();
    const auto stopped = moved.accept();
    REQUIRE_FALSE(stopped);
    CHECK(stopped.error().error == t::TcpListenerError::INVALID_STATE);
}

TEST_CASE("listener validates limits and reports bind failure without disturbing the current owner") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto options :
         {t::TcpListenerOptions{.address = "localhost"}, t::TcpListenerOptions{.maxConnections = 0},
          t::TcpListenerOptions{.maxConnections = t::TcpListenerOptions::MAX_CONNECTIONS + 1},
          t::TcpListenerOptions{.maxReadBytes = 0}, t::TcpListenerOptions{.maxWriteBytes = 0},
          t::TcpListenerOptions{.backlog = 0}}) {
        const auto invalid = t::TcpListener::create(*bootstrap, options);
        REQUIRE_FALSE(invalid);
        CHECK(invalid.error().error == t::TcpListenerError::INVALID_CONFIGURATION);
    }

    auto listener = t::TcpListener::create(*bootstrap);
    REQUIRE(listener);
    const auto collision = t::TcpListener::create(*bootstrap, {.port = listener->port()});
    REQUIRE_FALSE(collision);
    CHECK(collision.error().error == t::TcpListenerError::BIND_FAILURE);
    CHECK(collision.error().nativeCode != 0);

    Peer peer(listener->port());
    const auto connection = accept(*listener);
    CHECK(listener->activeConnections() == 1);
    {
        auto retiring = std::move(*listener);
        CHECK(retiring.activeConnections() == 1);
    }
    peer.checkClosed();
    const auto retired = listener->retire(connection);
    REQUIRE_FALSE(retired);
    CHECK(retired.error().error == t::TcpListenerError::INVALID_STATE);
}
