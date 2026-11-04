// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <string_view>

namespace org::ttldtor::qdcxx::client::detail {
inline constexpr std::size_t MAXIMUM_HOST_BYTES = 254; ///< Complete bounded ASCII presentation.

/** Inspect one bounded IPv4 presentation without SDK/bootstrap work.
 * @param text Borrowed ASCII candidate.
 * @return True for four complete decimal octets in0..255.
 */
inline bool ipv4(std::string_view text) noexcept {
    unsigned parts = 0;

    while (!text.empty()) {
        const auto dot = text.find('.');
        const auto part = text.substr(0, dot);
        unsigned value = 0;
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), value);

        if (part.empty() || part.size() > 3 || (part.size() > 1 && part.front() == '0') || parsed.ec != std::errc{} ||
            parsed.ptr != part.data() + part.size() || value > 255) {
            return false;
        }

        ++parts;

        if (dot == std::string_view::npos) {
            return parts == 4;
        }

        text.remove_prefix(dot + 1);
    }

    return false;
}

/** Validate IPv6 groups, including an optional final IPv4 suffix, without native address APIs.
 * @param text Borrowed candidate, excluding brackets and zones.
 * @return True for eight groups or exactly one shortening double colon.
 */
inline bool ipv6(std::string_view text) noexcept {
    const auto compressed = text.find("::");

    if (text.empty() || (text.front() == ':' && !text.starts_with("::")) ||
        (text.back() == ':' && !text.ends_with("::")) ||
        (compressed == std::string_view::npos && (text.front() == ':' || text.back() == ':')) ||
        (compressed != std::string_view::npos && text.find("::", compressed + 2) != std::string_view::npos)) {
        return false;
    }

    std::size_t groups = 0;

    for (std::size_t begin = 0; begin < text.size();) {
        if (text[begin] == ':') {
            ++begin;
            continue;
        }

        const auto colon = text.find(':', begin);
        const auto part = text.substr(begin, colon == std::string_view::npos ? text.size() - begin : colon - begin);

        if (part.find('.') != std::string_view::npos) {
            if (colon != std::string_view::npos || !ipv4(part)) {
                return false;
            }

            groups += 2;
        } else {
            if (part.size() > 4 || !std::ranges::all_of(part, [](unsigned char unit) {
                    return (unit >= '0' && unit <= '9') || (unit >= 'a' && unit <= 'f') || (unit >= 'A' && unit <= 'F');
                })) {
                return false;
            }

            ++groups;
        }

        begin = colon == std::string_view::npos ? text.size() : colon + 1;
    }

    return compressed == std::string_view::npos ? groups == 8
                                                : groups < 8 && text.find(":::") == std::string_view::npos;
}

/** Inspect numeric endpoint eligibility without touching sockets or process SDKs.
 * @param host Bounded explicit presentation.
 * @return True for a supported numeric IP literal.
 */
inline bool numericEndpointHost(std::string_view host) noexcept {
    return host.find(':') == std::string_view::npos ? ipv4(host) : ipv6(host);
}

/** Check host/reference-identity length before any copy; never perform DNS or URI/property parsing.
 * @param host Borrowed presentation.
 * @return True for a numeric IP or complete bounded ASCII/A-label DNS presentation.
 */
inline bool validEndpointHost(std::string_view host) noexcept {
    if (host.empty() || host.size() > MAXIMUM_HOST_BYTES) {
        return false;
    }

    if (host.find(':') != std::string_view::npos) {
        return ipv6(host);
    }

    if (std::ranges::all_of(host, [](unsigned char unit) {
            return (unit >= '0' && unit <= '9') || unit == '.';
        })) {
        return ipv4(host);
    }

    if (host.back() == '.') {
        host.remove_suffix(1);
    }

    while (!host.empty()) {
        const auto dot = host.find('.');
        const auto label = host.substr(0, dot);

        if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-' ||
            !std::ranges::all_of(label, [](unsigned char unit) {
                return (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') || (unit >= '0' && unit <= '9') ||
                       unit == '-';
            })) {
            return false;
        }

        if (dot == std::string_view::npos) {
            return true;
        }

        host.remove_prefix(dot + 1);
    }

    return false;
}

} // namespace org::ttldtor::qdcxx::client::detail
