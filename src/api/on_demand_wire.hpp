// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

#include <array>
#include <span>

namespace org::ttldtor::qdcxx::api::detail {

inline constexpr std::size_t ON_DEMAND_MAXIMUM_SYMBOL = 16;
inline constexpr std::size_t ON_DEMAND_MAXIMUM_REQUEST = 512;
inline constexpr std::size_t ON_DEMAND_MAXIMUM_HEADER = 2048;
inline constexpr std::size_t ON_DEMAND_MAXIMUM_BODY = 8192;
inline constexpr std::size_t ON_DEMAND_MAXIMUM_EVENTS = 64;
inline constexpr auto ON_DEMAND_NETWORK_DEADLINE = std::chrono::seconds{5};
inline constexpr auto ON_DEMAND_MAXIMUM_WINDOW = std::chrono::milliseconds{60000};

struct OnDemandRequest {
    std::u16string symbol;
    std::chrono::sys_time<std::chrono::milliseconds> from;
    std::uint16_t port;
};

struct OnDemandWireRequest {
    std::array<std::byte, ON_DEMAND_MAXIMUM_REQUEST> bytes{};
    std::size_t size = 0;
};

struct OnDemandRecord {
    std::int64_t time;
    std::array<std::int32_t, 8> fields;
};

struct OnDemandResponse {
    std::array<OnDemandRecord, ON_DEMAND_MAXIMUM_EVENTS> records{};
    std::size_t size = 0;
};

bool validOnDemandRequest(const OnDemandRequest &request) noexcept;

std::expected<OnDemandWireRequest, Diagnostic> encodeOnDemandRequest(const OnDemandRequest &request);

std::expected<std::optional<std::span<const std::byte>>, Diagnostic> readOnDemandHttp(std::span<const std::byte> bytes);

std::expected<OnDemandResponse, Diagnostic> decodeOnDemandResponse(std::span<const std::byte> bytes,
                                                                   const OnDemandRequest &request);

} // namespace org::ttldtor::qdcxx::api::detail
