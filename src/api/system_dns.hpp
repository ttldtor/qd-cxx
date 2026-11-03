// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

#include <expected>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::api::detail {
/** Read one bounded system IPv4 resolver snapshot at the explicit high-level owning boundary.
 * Called by ApiRuntime construction only when no explicit DNS servers were supplied.
 * @return Up to four distinct servers in platform order, or static/numeric configuration failure.
 */
[[nodiscard]] std::expected<std::vector<transport::DnsServer>, Diagnostic> discoverSystemDnsServers();

/** Parse a bounded resolv.conf snapshot without I/O or ambient state.
 * @param text Complete configuration, at most 64 KiB; IPv6-only servers are outside this profile.
 * @return First four distinct usable IPv4 nameservers or a static failure; no fallback resolver is invented.
 */
[[nodiscard]] std::expected<std::vector<transport::DnsServer>, Diagnostic> parseSystemDnsServers(std::string_view text);
} // namespace org::ttldtor::qdcxx::api::detail
