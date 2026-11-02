// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "configuration.hpp"

#include "qd/core/subscription.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::connect {

/** Explicit build availability supplied by the executable, without ambient feature discovery. */
struct CompiledRecoveryFeatures {
    bool recovery = false; ///< Existing recovery library is available to this executable.
    bool tls = false;      ///< Existing qualified TLS recovery library is available.
    bool json = false;     ///< Existing native binary JSON schema loader is available.
};

/** Owned application arguments for the explicitly selected recovery route.
 * No field opens a file, loads a schema, resolves a host or acquires native/provider state.
 * Timeouts are effective values, resolved after parsing regardless of argument order.
 */
struct RecoveryConfiguration {
    std::string host;       ///< Required numeric IP or bounded ASCII/A-label DNS presentation, without URI/zone.
    std::uint16_t port = 0; ///< Required remote port in 1..65535.
    core::SubscriptionFamily family = core::SubscriptionFamily::STREAM; ///< One selected delivery contract.
    SchemaProfile schema = SchemaProfile::QUOTE; ///< Complete built-in default when schemaJson is absent.
    std::optional<std::string> schemaJson;       ///< Explicit bounded native-binary JSON path; contents remain unread.
    std::int32_t recordId = 0; ///< Selected local record ID; an external schema requires explicit input.
    OutputProfile output = OutputProfile::GENERIC; ///< Generic values or an explicitly prepared existing mapper.
    std::vector<std::u16string> symbols;        ///< One to sixteen distinct printable ASCII selectors, copied exactly.
    std::optional<std::int64_t> historyFromRaw; ///< Required HISTORY lower bound; raw signed identity, not Unix time.
    core::SubscriptionSelectorPolicy selectors =
        core::SubscriptionSelectorPolicy::EXACT_ONLY; ///< Immutable opt-in policy; All remains unavailable.
    std::optional<std::string> dnsServer;     ///< Explicit numeric IPv4 resolver; no discovery or resolver calls.
    std::uint16_t dnsPort = 53;               ///< Explicit UDP/TCP resolver port in 1..65535.
    std::uint32_t attempts = 1;               ///< Finite lifetime attempt limit, including the first attempt.
    std::chrono::milliseconds retryDelay{0};  ///< Zero for one attempt; otherwise positive, default 250 ms.
    std::chrono::milliseconds duration{5000}; ///< Admission interval in 1..600000 ms, including negotiation.
    std::optional<std::chrono::milliseconds> removeAfter; ///< Remove the initial keys once; strictly before duration.
    std::optional<std::size_t> maximumRecords;            ///< Optional delivered-record target in 1..1000000.
    std::chrono::milliseconds drain{1000};          ///< Local admitted-prefix retirement allowance in 1..60000 ms.
    std::chrono::milliseconds connectTimeout{1000}; ///< Explicit finite connect deadline, at most INT_MAX ms.
    std::chrono::milliseconds readTimeout{5000};    ///< Explicit finite read inactivity; default admission duration.
    std::chrono::milliseconds writeTimeout{5000};   ///< Explicit finite write inactivity; default admission duration.
    std::chrono::milliseconds pollInterval{1};      ///< Explicit finite owner cadence, at most INT_MAX ms.
    std::chrono::milliseconds dnsTimeout{1000};     ///< Explicit finite DNS deadline; default connect timeout.
    client::EndpointSecurity security = client::EndpointSecurity::CREDENTIAL_FREE_PLAIN; ///< Plain or verified TLS.
    std::optional<std::string> tlsTrustPem;  ///< Required TLS trust-file path, never read by argument parsing.
    std::optional<std::string> tlsReference; ///< Required TLS reference identity, independent of route/DNS aliases.
    std::chrono::milliseconds tlsHandshake{1000};         ///< Finite TLS handshake deadline, at most INT_MAX ms.
    std::chrono::milliseconds tlsClose{1000};             ///< Finite TLS close deadline, at most INT_MAX ms.
    std::optional<std::string> authBasicTokenFile;        ///< Optional TLS-only bounded secret-file path, never echoed.
    std::optional<std::chrono::milliseconds> authTimeout; ///< Present only with token input; default 1000 ms.
};

/** Parse only the explicit recovery command-line grammar, without working subsystem side effects.
 * Values and symbols are copied into the result; no caller string_view escapes. Unknown, duplicate,
 * contradictory and unavailable profiles fail with static text that does not include caller input.
 * Complete schemas, mappings, trust contents, token contents and runtime budgets require later
 * validation before activation. Standalone help and legacy-mode dispatch belong to main.
 * @param arguments Native option/value pairs after the executable, including mandatory --mode recovery.
 * @param features Explicit compiled availability; no environment, SDK or global settings are consulted.
 * @return Complete owned configuration or a static argument/feature/allocation refusal, without partial publication.
 */
[[nodiscard]] std::expected<RecoveryConfiguration, std::string_view>
parseRecoveryArguments(std::span<const std::string_view> arguments, CompiledRecoveryFeatures features);

} // namespace org::ttldtor::qdcxx::connect
