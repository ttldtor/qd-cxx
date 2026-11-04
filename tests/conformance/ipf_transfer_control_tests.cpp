// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/ipf/instrument_profile_url_reader.hpp"

#include "ipf_http_peer.hpp"
#include "raw_byte_owner.hpp"
#include "transfer_control.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <thread>

namespace t = org::ttldtor::qdcxx::transport;
namespace td = t::detail;
namespace i = org::ttldtor::qdcxx::ipf;
namespace h = org::ttldtor::qdcxx::test;
using namespace std::chrono_literals;

TEST_CASE("production transfer stop predicate cuts off real blocked raw input and drains on creator") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const i::IpfTransferOptions options;
    const std::string reply = "HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\nretained-prefix";
    h::IpfHttpPeer peer({reply}, 4096, true);
    t::TcpOptions native{"127.0.0.1",
                         peer.port(),
                         {},
                         options.dnsPort,
                         options.connectTimeout,
                         options.connectTimeout,
                         options.readTimeout,
                         options.writeTimeout,
                         options.pollInterval,
                         options.nativeLimits};
    auto copy = options.byteWork;
    auto work = options.nativeWork;
    auto made = td::RawByteOwner::create(*bootstrap, native, {4096, options.limits.requestBytes}, copy);
    REQUIRE(made);
    auto &owner = **made;
    REQUIRE(owner.start());
    const std::string request = "GET /profiles.ipf HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    REQUIRE(owner.write(std::as_bytes(std::span(request.data(), request.size())), copy));
    std::stop_source stop;
    const auto token = stop.get_token();
    std::jthread requester([&] {
        const auto deadline = std::chrono::steady_clock::now() + 3s;

        while (peer.sentBytes() < reply.size() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }

        stop.request_stop();
    });
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    std::size_t turns = 0;

    while (!i::detail::transferCancelled(true, false, token) && std::chrono::steady_clock::now() < deadline) {
        REQUIRE(owner.poll(work, copy));
        ++turns;
        std::this_thread::sleep_for(1ms);
    }

    requester.join();
    CHECK(token.stop_requested());
    CHECK(peer.sentBytes() == reply.size());
    CHECK(turns > 0);
    REQUIRE(owner.requestClose());

    while (owner.core().state() != t::TcpState::CLOSED && std::chrono::steady_clock::now() < deadline) {
        REQUIRE(owner.poll(work, copy));
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(owner.core().state() == t::TcpState::CLOSED);
    CHECK(owner.core().resources().pendingOperations == 0);
    CHECK(owner.core().terminal()->aborted);
    CHECK(owner.core().outcome.primary.error == t::TcpError::NONE);
    CHECK(owner.core().terminal().has_value());
}
