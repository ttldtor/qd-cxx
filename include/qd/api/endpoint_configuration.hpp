// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::api {
/** Borrowed construction input; neither key nor value is retained by the resolver. */
struct EndpointProperty {
    std::string_view key;   ///< Exact case-sensitive endpoint key; address connector aliases have their own rules.
    std::string_view value; ///< Explicit value, including empty; there is no environment or file lookup.
};

/** Exact source of a resolved local value, never evidence of server negotiation. */
enum class EndpointPropertyOrigin {
    BASE_OPTIONS,        ///< Typed fallback options, whether supplied by the caller or default constructed.
    DEFAULT_PROPERTIES,  ///< Explicit caller-provided lower-priority property layer.
    EXPLICIT_PROPERTIES, ///< Explicit caller-provided overriding property layer.
    ADDRESS_PROPERTIES,  ///< Supported trailing address properties, resolved during construction only.
    RUNTIME_UPDATE       ///< Current requested-aggregation intent changed through the explicit endpoint setter.
};

/** Finite complete construction envelope, separate from transport/runtime grants. */
struct EndpointConfigurationLimits {
    std::size_t properties = 64;    ///< Combined entries across both explicit layers and address blocks.
    std::size_t keyBytes = 96;      ///< Maximum borrowed key length before inspection/copy.
    std::size_t valueBytes = 4096;  ///< Maximum borrowed property value or address bytes.
    std::size_t totalBytes = 16384; ///< Combined inspected key/value/address bytes, with checked addition.
};

/** Owning local policy observation. Credentials are unsupported and never enter this value. */
struct EffectiveEndpointProperty {
    std::string key;            ///< Canonical supported key, without arbitrary rejected input.
    std::string requestedValue; ///< Selected local value before consumer-specific normalization.
    std::string effectiveValue; ///< Applied local policy or metadata value; never a server report.
    EndpointPropertyOrigin
        origin; ///< Selected construction layer or explicit requested-aggregation update; no ambient origin.
};

/** Independent configuration/accepted-request observation; no native state or runtime is retained. */
struct EndpointConfigurationSnapshot {
    std::vector<EffectiveEndpointProperty> properties; ///< Bounded owning policy values; no secret-bearing keys.
    std::optional<std::string> configuredAddress;      ///< Validated plain route, without trailing property text.
    EndpointPropertyOrigin addressOrigin = EndpointPropertyOrigin::BASE_OPTIONS;
    ///< Source of configuredAddress, if present. An explicit later connect argument may select another first route.
    std::optional<std::string> acceptedAddress;     ///< First accepted local connect spelling; not DNS/peer evidence.
    bool activationRequested = false;               ///< Whether a local connect intent is currently admitted.
    std::uint64_t connectionRevision = 0;           ///< Local accepted control revision, not a remote acknowledgement.
    std::uint64_t requestedAggregationRevision = 0; ///< Local coalesced request revision; never a peer ACK.
};

/** Immutable instance-local address/policy snapshot. Resolution performs no files, DNS, SDK or user callbacks.
 * Sources are typed fallback, explicit defaults, explicit overrides, then supported address properties.
 * Last duplicate wins within a layer. Unknown keys, credential input and unsupported connectors reject.
 * Read/copy distinct retaining handles concurrently; synchronize replacement/destruction of the same handle.
 * A moved-from handle may only be reassigned or destroyed. No secure-erasure guarantee is made for caller input.
 */
class EndpointConfiguration final {
    struct State;
    std::shared_ptr<const State> state_;
    explicit EndpointConfiguration(std::shared_ptr<const State> state) noexcept;

    public:
    /** Resolve complete bounded explicit input before any endpoint slot or native activation.
     * @param fallback Typed local policy used before applying property layers; not inferred to be an explicit override.
     * @param defaults Caller-provided lower-priority values, borrowed only for this call.
     * @param properties Caller-provided overrides, borrowed only for this call.
     * @param limits Complete finite input envelope; every input entry is bounded, including shadowed entries.
     * @return Immutable owning configuration. Error uses only static labels; no rejected key/value/address is retained.
     */
    [[nodiscard]] static EndpointConfiguration create(const EndpointOptions &fallback = {},
                                                      std::span<const EndpointProperty> defaults = {},
                                                      std::span<const EndpointProperty> properties = {},
                                                      EndpointConfigurationLimits limits = {});
    /** @return Borrowed immutable native policy, valid while a retaining non-moved-from handle remains alive. */
    [[nodiscard]] const EndpointOptions &options() const noexcept;
    /** @return Owning bounded local values; acceptedAddress is absent until inspected through an endpoint. */
    [[nodiscard]] EndpointConfigurationSnapshot snapshot() const;
};
} // namespace org::ttldtor::qdcxx::api
