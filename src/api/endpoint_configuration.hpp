// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/api/endpoint_configuration.hpp"

namespace org::ttldtor::qdcxx::api::detail {
/** Owning validated plain route; spelling preserves the existing first-address identity rule. */
struct EndpointAddress {
    std::string host;     ///< SDK-neutral numeric IP or ASCII/A-label DNS host, without brackets.
    std::uint16_t port;   ///< Nonzero complete decimal port.
    std::string spelling; ///< Exact admitted route spelling, without construction-only properties.
};

/** @param options Immutable local policy. @throws Error before publication for invalid policy. */
void validateEndpointOptions(const EndpointOptions &options);
/** @param address Complete plain host:port or [IPv6]:port; construction properties are unsupported here.
 * @return Independently owning validated route; no DNS/native work occurs.
 */
[[nodiscard]] EndpointAddress parseEndpointAddress(std::string_view address);
} // namespace org::ttldtor::qdcxx::api::detail
