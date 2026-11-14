// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "fixture_peer.hpp"

#include "fixture.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace fixture = q::test::ondemand;

/** Decode the checked-in synthetic response.
 * @param hex Fixture bytes encoded as lowercase hexadecimal.
 * @return Complete HTTP response owned by the test peer.
 */
std::vector<std::byte> response(std::string_view hex) {
    std::vector<std::byte> body;
    const auto digit = [](char value) {
        return value <= '9' ? value - '0' : value - 'a' + 10;
    };

    for (std::size_t i = 0; i < hex.size(); i += 2) {
        body.push_back(static_cast<std::byte>(16 * digit(hex[i]) + digit(hex[i + 1])));
    }

    const auto header = std::string("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: ") +
                        std::to_string(body.size()) + "\r\n\r\n";
    const auto headerBytes = std::as_bytes(std::span(header));
    std::vector<std::byte> result(headerBytes.begin(), headerBytes.end());
    result.insert(result.end(), body.begin(), body.end());

    return result;
}
} // namespace

std::unique_ptr<org::ttldtor::qdcxx::test::OwnedTcpPeer> startFixturePeer() {
    // The request contains a variable-width ephemeral loopback port.
    constexpr std::string_view HEADER =
        "POST /MarketDataReplay HTTP/1.1\r\nHost: 127.0.0.1:0\r\n"
        "Content-Type: application/octet-stream\r\nAccept: application/octet-stream\r\n"
        "X-OnDemand-StartTime: 2024-02-12T14:30:05.000Z\r\nContent-Length: 131\r\nConnection: close\r\n\r\n";

    q::test::TcpPeerConfig config;
    config.sessions.resize(1);
    config.sessions.front().clientBytesToRead = HEADER.size() + fixture::REQUEST.size() / 2;
    config.sessions.front().replyBytes = response(fixture::RESPONSE_2000MS);
    config.sessions.front().waitForClientClose = true;

    return std::make_unique<q::test::OwnedTcpPeer>(std::move(config));
}
