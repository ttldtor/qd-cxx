// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: bounded resolver snapshots without native discovery or network I/O.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "system_dns.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <string>

namespace api = org::ttldtor::qdcxx::api;

TEST_CASE("resolver snapshot preserves order and coalesces duplicates") {
    const auto result =
        api::detail::parseSystemDnsServers("search example.test\n nameserver\t127.0.0.53 # local\r\n"
                                           "nameserver 192.0.2.1; comment\nnameserver 127.0.0.53\n"
                                           "nameserver 192.0.2.2\nnameserver 192.0.2.3\nnameserver 192.0.2.4\n");
    REQUIRE(result);
    REQUIRE(result->size() == 4);
    CHECK(result->at(0).address == std::array<std::uint8_t, 4>{127, 0, 0, 53});
    CHECK(result->at(1).address == std::array<std::uint8_t, 4>{192, 0, 2, 1});
    CHECK(result->at(3).address == std::array<std::uint8_t, 4>{192, 0, 2, 3});
}

TEST_CASE("resolver snapshot rejects unsupported or unusable addresses without fallback") {
    for (const auto text :
         {"", "nameserver ::1\n", "nameserver 0.0.0.0\n", "nameserver 256.1.2.3\n", "nameserver 1.2.3\n",
          "nameserver 1.2.3.4.5\n", "nameserver +1.2.3.4\n", "nameserver 1.2.3.4:53\n", "nameserverX 192.0.2.1\n"}) {
        const auto result = api::detail::parseSystemDnsServers(text);
        REQUIRE_FALSE(result);
        CHECK(result.error().code == api::ErrorCode::DNS_CONFIGURATION);
    }
}

TEST_CASE("resolver snapshot skips invalid entries and accepts final unterminated line") {
    const auto result =
        api::detail::parseSystemDnsServers("# nameserver 192.0.2.2\nnameserver ::1\nnameserver 300.0.0.1\n"
                                           "nameserver\t 192.0.2.1");
    REQUIRE(result);
    REQUIRE(result->size() == 1);
    CHECK(result->front().address == std::array<std::uint8_t, 4>{192, 0, 2, 1});
}

TEST_CASE("resolver snapshot validates complete input before accepting a prefix") {
    std::string text = "nameserver 192.0.2.1\n";
    text.push_back('\0');
    CHECK_FALSE(api::detail::parseSystemDnsServers(text));
    text.assign(64 * 1024 + 1, ' ');
    CHECK_FALSE(api::detail::parseSystemDnsServers(text));
    text.assign(64 * 1024, ' ');
    const std::string prefix = "nameserver 192.0.2.1\n";
    text.replace(0, prefix.size(), prefix);
    CHECK(api::detail::parseSystemDnsServers(text));
}
