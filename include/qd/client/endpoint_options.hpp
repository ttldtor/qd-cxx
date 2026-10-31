// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/client/stream_dispatcher.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::client {
/** Configuration-only rejection; no diagnostic contains caller text or credentials. */
enum class EndpointError {
    INVALID_CONFIGURATION, ///< Invalid shape, units, policy or contradictory settings.
    RESOURCE_LIMIT,        ///< Schema/size arithmetic or declared finite capacity is insufficient.
    ALLOCATION_FAILURE,    ///< Owned snapshot allocation failed without changing caller inputs.
    UNSUPPORTED_PROFILE    ///< Structurally valid handoff cannot execute in the current plain STREAM driver.
};

/** Detached static error, independent of snapshot and application lifetimes. */
struct EndpointDiagnostic {
    EndpointError error = EndpointError::INVALID_CONFIGURATION; ///< Typed rejection.
    std::string_view stage = "none";                            ///< Static library-owned label, never caller data.
};
/** Requested transport policy; TLS is not evidence of a verified connection. */
enum class EndpointSecurity {
    CREDENTIAL_FREE_PLAIN, ///< Plain TCP, with no credential provider or TLS settings.
    TLS                    ///< Explicit verified-TLS policy handoff; current STREAM driver rejects it.
};
/** Selected execution model; no runtime or SDK is constructed by configuration. */
enum class EndpointRuntime {
    CALLER_DRIVEN_TCP ///< Serialized caller-driven TCP model.
};
/** Provider result without secret-bearing text. */
enum class CredentialError {
    UNAVAILABLE, ///< No credential is currently available.
    REJECTED     ///< Application declined this request.
};

/** Caller-owned Basic-token source for a future verified TLS adapter.
 * CONFIG never invokes this provider. The future adapter invokes only after actual TLS verification,
 * on its serialized owner, with a fresh bounded buffer per authentication epoch. No callback duration
 * bound is possible. Synchronize mutable state when sharing providers between endpoints. Provider
 * state/captures/resources and destructor/callback work are outside configuration-owned bounds.
 */
class CredentialProvider {
    public:
    /** Destroy on the thread releasing the last shared owner; the application must permit this. */
    virtual ~CredentialProvider() = default;
    /** Supply a complete Basic token, including the ASCII scheme prefix, without retaining the span.
     * The future adapter validates the returned length/prefix and translates exceptions before use.
     * @param destination Borrowed bounded UTF-16 output, valid only during this call; never retain it.
     * @return Written units in 1..destination.size(), or a static rejection. No secure-erasure promise.
     */
    [[nodiscard]] virtual std::expected<std::size_t, CredentialError> basicToken(std::span<char16_t> destination) = 0;
};

/** Owned TLS handoff values; no trust parsing, callback invocation or TLS execution occurs here. */
struct EndpointTls {
    std::string referenceIdentity; ///< Explicit 1..254-byte ASCII numeric IP/A-label DNS identity, no URI/wildcard.
    std::string trustPem; ///< Explicit 1..512 KiB PEM bytes; cryptographic/anchor validation belongs to the adapter.
    std::optional<std::chrono::milliseconds> handshakeTimeout; ///< Required positive TLS deadline, at most INT_MAX ms.
    std::optional<std::chrono::milliseconds> authenticationTimeout; ///< Required only with provider, in 1..INT_MAX ms.
    std::optional<std::chrono::milliseconds>
        closeTimeout;                      ///< Required positive TLS-close deadline, at most INT_MAX ms.
    std::optional<std::size_t> tokenUnits; ///< Required only with provider; bounded UTF-16 token capacity, at least 7.
    std::shared_ptr<CredentialProvider>
        credentials; ///< Optional shared external state; last release may run user code.
};

/** Finite future retry policy; current STREAM driver supports exactly one attempt. */
struct EndpointRetry {
    std::uint32_t maximumAttempts = 1;  ///< Positive finite attempt count, including the initial attempt.
    std::chrono::milliseconds delay{0}; ///< Zero only for one attempt; otherwise positive milliseconds <=INT_MAX.
};

/** Runtime-neutral requested TCP capacities; explicitly mapped to transport values, never layout-cast. */
struct EndpointTcpLimits {
    std::size_t inputBytes{};    ///< Owned read capacity, in 1..INT_MAX bytes.
    std::size_t writeBytes{};    ///< Maximum native write size, in 1..INT_MAX bytes.
    std::size_t addresses{};     ///< Address capacity, in 1..64.
    std::size_t cnameDepth{};    ///< CNAME depth ceiling, in 1..16; no DNS activation in the current driver.
    std::size_t contextBytes{};  ///< Positive fixed execution-context arena bytes.
    std::size_t handlerBytes{};  ///< Positive fixed operation arena bytes.
    std::size_t residentBytes{}; ///< Conservative total run allowance, including session capacity; not process RSS.
};

/** Independent positive operation/cadence durations, all in milliseconds and at most INT_MAX. */
struct EndpointTimeouts {
    std::chrono::milliseconds dns{};     ///< Whole DNS operation handoff deadline; current driver is numeric-only.
    std::chrono::milliseconds connect{}; ///< Whole serial address-connect deadline.
    std::chrono::milliseconds read{};    ///< Connected input inactivity deadline.
    std::chrono::milliseconds write{};   ///< Pending-output inactivity deadline.
    std::chrono::milliseconds poll{};    ///< Caller-owned progress cadence, not an I/O wake notifier.
};

/** Runtime-neutral credits for one caller turn; no field is an automatic lifetime quota. */
struct EndpointRuntimeWork {
    std::size_t steps{};       ///< Public progress attempts.
    std::size_t handlers{};    ///< Internal handler dispatches.
    std::size_t socketBytes{}; ///< Conservative native read/write buffer bytes.
    std::size_t dnsCalls{};    ///< DNS socket callback attempts; zero for the current numeric-only driver.
    std::size_t dnsPackets{};  ///< Successful DNS receive operations.
    std::size_t dnsBytes{};    ///< DNS receive/send bytes, preserving datagrams.
    std::uint64_t dnsUnits{};  ///< Conservative DNS parser/copy work units.
};

/** Independent finite component budgets and application-turn credits, read from the owned snapshot.
 * Existing component types keep their documented units and zero-capacity semantics. ABI-derived
 * allocation feasibility remains the existing factories' responsibility before network activation.
 */
struct EndpointBudgets {
    core::SchemaLimits schema{};                 ///< Complete configuration-schema logical copy limits.
    protocol::LayoutLimits layout{};             ///< Complete explicit outgoing-layout limits.
    protocol::SubscriptionWriterLimits writer{}; ///< Whole-output and retained composer capacities.
    protocol::LiveLimits session{};              ///< Pure-session capacities; credential eligibility must remain plain.
    core::SubscriptionLimits desired{};          ///< Complete desired-state capacities.
    collectors::StreamLimits collector{};        ///< Local whole-frame/record/resident capacities.
    ClientLimits client{};                       ///< Command/detached-batch capacities, separate from lower owners.
    EndpointTcpLimits tcp{};                     ///< Runtime-neutral socket/SDK/aggregate ceilings.
    protocol::SubscriptionWriterWork writerPreparation{}; ///< One writer-preparation allowance.
    SubmitWork submitTurn{};                              ///< One application command-submission allowance.
    protocol::LiveWork protocolTurn{};                    ///< One application protocol preparation/progress allowance.
    DispatchWork dispatchTurn{};       ///< One owner turn, also used during applicable stop/drain paths.
    EndpointRuntimeWork runtimeTurn{}; ///< One application native-progress allowance.
};

/** Mutable caller construction input, copied once by EndpointOptions::create; no ambient sources. */
struct EndpointSettings {
    std::string host;       ///< Explicit bounded numeric IP/A-label DNS host, copied without URI/property resolution.
    std::uint16_t port = 0; ///< Required numeric port in 1..65535.
    EndpointRuntime runtime = EndpointRuntime::CALLER_DRIVEN_TCP; ///< Selected model, no SDK type or owner retained.
    EndpointSecurity security = EndpointSecurity::CREDENTIAL_FREE_PLAIN; ///< Requested policy, not connection status.
    std::optional<EndpointTls> tls; ///< Required for TLS, forbidden for plain; handoff-only in current driver.
    EndpointRetry retry{};          ///< One-attempt initial policy; multiple attempts are handoff-only.
    EndpointTimeouts timeouts{};    ///< Explicit finite millisecond deadlines/cadence.
    EndpointBudgets budgets{};      ///< Explicit per-component limits/turn credits, never global defaults.
};

/** Immutable owning endpoint/schema snapshot without runtime/process ownership.
 * Distinct handles may be copied/read/released on different threads. Synchronize replacement or
 * destruction of the same handle object. Provider state/destructor work remains caller-owned and
 * excluded from configuration bounds. Moved-from handles may only be reassigned or destroyed.
 * No secret formatter, settings dump, file/environment lookup or credential callback is provided.
 */
class EndpointOptions {
    struct State;
    std::shared_ptr<const State> state_; ///< Immutable copied values/schema plus finite external provider handle.
    /** Adopt an already complete immutable snapshot.
     * @param state Owned complete state, published only after all validation/copies succeed.
     */
    explicit EndpointOptions(std::shared_ptr<const State> state) noexcept;

    public:
    /** Retain immutable state without allocation.
     * @param other Non-moved-from handle, externally protected during copying.
     */
    EndpointOptions(const EndpointOptions &other) noexcept = default;
    /** Transfer state; source may only be reassigned/destroyed.
     * @param other Source handle, made moved from.
     */
    EndpointOptions(EndpointOptions &&other) noexcept = default;
    /** Replace this handle, expiring its previous views if no other owner retains them.
     * @param other Retained immutable source.
     * @return This handle.
     */
    EndpointOptions &operator=(const EndpointOptions &other) noexcept = default;
    /** Transfer state, releasing previous ownership on this thread.
     * @param other Source handle, made moved from.
     * @return This handle.
     */
    EndpointOptions &operator=(EndpointOptions &&other) noexcept = default;
    /** Release state; the last external provider release may run its destructor on this thread. */
    ~EndpointOptions() = default;
    /** Validate locally and copy immutable values/schema before any driver-owned I/O.
     * TLS validation checks presentation/bounds only, never cryptographic trust or completed verification.
     * Caller inputs remain unchanged on failure. Allocation exceptions are translated into typed errors.
     * @param schema Immutable validated selection; copied within settings.budgets.schema, preserving provenance.
     * @param settings Explicit caller input, borrowed unchanged during this call only.
     * @return Complete owning snapshot or a static typed configuration/resource/allocation rejection.
     */
    [[nodiscard]] static std::expected<EndpointOptions, EndpointDiagnostic> create(const core::LocalSchema &schema,
                                                                                   const EndpointSettings &settings);
    /** Inspect immutable effective inputs; requires a non-moved-from handle.
     * @return Borrowed values valid until this retaining handle is replaced/destroyed; provider state is external.
     */
    [[nodiscard]] const EndpointSettings &settings() const noexcept;
    /** Inspect the complete explicit schema; requires a non-moved-from handle.
     * @return Borrowed immutable copy with original provenance, valid until retaining handle replacement/destruction.
     */
    [[nodiscard]] const core::LocalSchema &schema() const noexcept;
    /** Reject handoff-only policies before this controlled driver touches bootstrap or activates I/O.
     * Requires a non-moved-from handle. Caller bootstrap/other endpoints may already exist independently.
     * The controlled driver requires sufficient whole retirement credits and full read/write quantum credits.
     * The write quantum floor is a sufficient driver profile, not a universal TCP minimum; generic
     * snapshots can represent smaller partial-progress budgets. No credit is increased during shutdown.
     * @return Success for the selected numeric plain STREAM work profile, otherwise static unsupported error.
     */
    [[nodiscard]] std::expected<void, EndpointDiagnostic> requirePlainStream() const noexcept;
};

/** Obtain the existing controlled-example numeric budgets without selecting an endpoint/schema.
 * This explicit helper is not a Java/global/default-source policy. TLS has no implicit bounds.
 * @param duration Admission interval in 1..600000 ms; also supplies read/write inactivity defaults.
 * @return Finite values with empty host/zero port, or a static invalid-duration rejection.
 */
[[nodiscard]] std::expected<EndpointSettings, EndpointDiagnostic>
controlledStreamSettings(std::chrono::milliseconds duration = std::chrono::milliseconds{5000});
} // namespace org::ttldtor::qdcxx::client
