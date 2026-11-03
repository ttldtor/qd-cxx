// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "system_dns.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <limits>
#include <new>
#include <optional>

#ifdef _WIN32
#    include <winsock2.h>

// IP Helper declarations require the Windows socket types above.
#    include <iphlpapi.h>
#endif

namespace org::ttldtor::qdcxx::api::detail {
namespace {
constexpr std::size_t CONFIGURATION_BYTES = 64 * 1024; ///< Finite system-configuration payload bound.
constexpr std::size_t MAXIMUM_SERVERS = 4;             ///< Existing transport DNS profile maximum.
constexpr unsigned OCTET_MAXIMUM = 255;                ///< IPv4 wire octet domain.

/** Parse exactly four ordinary decimal IPv4 octets.
 * @param text Complete address without a port, zone, whitespace or trailing suffix.
 * @return Usable nonzero resolver, or no value; no native resolver is called.
 */
std::optional<transport::DnsServer> address(std::string_view text) {
    transport::DnsServer server{};

    for (std::size_t index = 0; index < server.address.size(); ++index) {
        const auto dot = text.find('.');
        const auto token = text.substr(0, dot);
        unsigned value = 0;
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);

        if (token.empty() || result.ec != std::errc{} || result.ptr != token.data() + token.size() ||
            value > OCTET_MAXIMUM || ((index + 1 == server.address.size()) != (dot == text.npos))) {
            return std::nullopt;
        }

        server.address[index] = static_cast<std::uint8_t>(value);

        if (dot != text.npos) {
            text.remove_prefix(dot + 1);
        }
    }

    if (std::ranges::all_of(server.address, [](auto value) {
            return value == 0;
        })) {
        return std::nullopt;
    }

    return server;
}

/** Preserve platform order while coalescing repeated numeric servers.
 * @param result Owned finite output vector.
 * @param text Candidate numeric IPv4 text.
 */
void append(std::vector<transport::DnsServer> &result, std::string_view text) {
    const auto parsed = address(text);

    if (parsed && result.size() < MAXIMUM_SERVERS && std::ranges::none_of(result, [&](const auto &server) {
            return server.address == parsed->address;
        })) {
        result.push_back(*parsed);
    }
}

/** Reject an empty snapshot without an implicit public or system resolver fallback.
 * @param result Owned deduplicated snapshot.
 * @return Nonempty snapshot or static unsupported/configuration failure.
 */
std::expected<std::vector<transport::DnsServer>, Diagnostic> finish(std::vector<transport::DnsServer> result) {
    if (result.empty()) {
        return std::unexpected(Diagnostic{ErrorCode::DNS_CONFIGURATION, "dns-no-ipv4-server"});
    }

    return result;
}
} // namespace

std::expected<std::vector<transport::DnsServer>, Diagnostic> parseSystemDnsServers(std::string_view text) {
    if (text.size() > CONFIGURATION_BYTES || text.find('\0') != text.npos) {
        return std::unexpected(Diagnostic{ErrorCode::DNS_CONFIGURATION, "dns-configuration-shape"});
    }

    try {
        std::vector<transport::DnsServer> result;
        result.reserve(MAXIMUM_SERVERS);

        while (!text.empty() && result.size() < MAXIMUM_SERVERS) {
            const auto end = text.find('\n');
            auto line = text.substr(0, end);
            text.remove_prefix(end == text.npos ? text.size() : end + 1);
            const auto first = line.find_first_not_of(" \t\r");

            if (first == line.npos) {
                continue;
            }

            line.remove_prefix(first);
            constexpr std::string_view KEY = "nameserver";

            if (!line.starts_with(KEY) || line.size() <= KEY.size() ||
                (line[KEY.size()] != ' ' && line[KEY.size()] != '\t')) {
                continue;
            }

            line.remove_prefix(KEY.size());
            const auto value = line.find_first_not_of(" \t");

            if (value == line.npos) {
                continue;
            }

            line.remove_prefix(value);
            append(result, line.substr(0, line.find_first_of(" \t\r#;")));
        }

        return finish(std::move(result));
    } catch (const std::bad_alloc &) {
        return std::unexpected(Diagnostic{ErrorCode::ALLOCATION_FAILURE, "dns-snapshot-storage"});
    }
}

std::expected<std::vector<transport::DnsServer>, Diagnostic> discoverSystemDnsServers() {
    try {
#ifdef _WIN32
        alignas(FIXED_INFO) std::array<std::byte, CONFIGURATION_BYTES> storage{};
        ULONG size = static_cast<ULONG>(storage.size());
        auto *info = reinterpret_cast<FIXED_INFO *>(storage.data());
        const auto code = GetNetworkParams(info, &size);

        if (code != ERROR_SUCCESS || size < sizeof(FIXED_INFO) || size > storage.size()) {
            return std::unexpected(Diagnostic{ErrorCode::DNS_CONFIGURATION, "dns-system-snapshot", 0, code});
        }

        constexpr std::size_t MAXIMUM_NODES = 64; ///< Finite traversal even for an inconsistent native list.
        std::array<const IP_ADDR_STRING *, MAXIMUM_NODES> visited{};
        std::size_t count = 0;
        std::vector<transport::DnsServer> result;
        result.reserve(MAXIMUM_SERVERS);
        const auto begin = reinterpret_cast<std::uintptr_t>(storage.data());
        const auto end = begin + storage.size();

        for (auto *node = &info->DnsServerList; node && result.size() < MAXIMUM_SERVERS; node = node->Next) {
            const auto position = reinterpret_cast<std::uintptr_t>(node);

            if (count == visited.size() || position < begin || position > end - sizeof(IP_ADDR_STRING) ||
                position % alignof(IP_ADDR_STRING) != 0 ||
                std::find(visited.begin(), visited.begin() + static_cast<std::ptrdiff_t>(count), node) !=
                    visited.begin() + static_cast<std::ptrdiff_t>(count)) {
                return std::unexpected(Diagnostic{ErrorCode::DNS_CONFIGURATION, "dns-system-list"});
            }

            visited[count++] = node;
            const auto &characters = node->IpAddress.String;
            const auto zero = std::find(std::begin(characters), std::end(characters), '\0');

            if (zero != std::end(characters)) {
                append(result, {characters, static_cast<std::size_t>(zero - std::begin(characters))});
            }
        }

        return finish(std::move(result));
#else
        std::ifstream input("/etc/resolv.conf", std::ios::binary);

        if (!input) {
            return std::unexpected(Diagnostic{ErrorCode::DNS_CONFIGURATION, "dns-system-open"});
        }

        std::string text(CONFIGURATION_BYTES + 1, '\0');
        input.read(text.data(), static_cast<std::streamsize>(text.size()));

        if (input.bad()) {
            return std::unexpected(Diagnostic{ErrorCode::DNS_CONFIGURATION, "dns-system-read"});
        }

        text.resize(static_cast<std::size_t>(input.gcount()));

        return parseSystemDnsServers(text);
#endif
    } catch (const std::bad_alloc &) {
        return std::unexpected(Diagnostic{ErrorCode::ALLOCATION_FAILURE, "dns-snapshot-storage"});
    }
}
} // namespace org::ttldtor::qdcxx::api::detail
