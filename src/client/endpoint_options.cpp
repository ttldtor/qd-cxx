// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/client/endpoint_options.hpp"
#include "endpoint_host.hpp"

#include <algorithm>
#include <charconv>
#include <climits>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::client {
namespace {
constexpr std::size_t MEBIBYTE = 1024 * 1024;         ///< Byte unit, independent of UTF-16 storage units.
constexpr std::size_t MAXIMUM_PEM_BYTES = 512 * 1024; ///< ADR TLS trust-input ceiling, before SDK parsing.

/** Check milliseconds before conversion, without observing a mutable clock.
 * @param value Explicit duration.
 * @return True for positive representable selected-runtime millisecond durations.
 */
bool durationValid(std::chrono::milliseconds value) noexcept {
    return value.count() > 0 && value.count() <= INT_MAX &&
           value <= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::duration::max());
}

/** Check a requested product without overflow or allocating backing.
 * @param count Element/owner count.
 * @param bytes Per-element requested bytes or UTF-16 units.
 * @return True when the product fits size_t.
 */
bool productFits(std::size_t count, std::size_t bytes) noexcept {
    return count == 0 || bytes <= std::numeric_limits<std::size_t>::max() / count;
}

/** Check public capacity arithmetic; existing factories retain ABI-dependent feasibility authority.
 * @param b Independent finite budgets.
 * @return True if required envelope relationships and requested products/sums are representable.
 */
bool budgetsValid(const EndpointBudgets &b) noexcept {
    const auto &tcp = b.tcp;
    const auto &session = b.session;

    if (tcp.inputBytes == 0 || tcp.inputBytes > INT_MAX || tcp.writeBytes == 0 || tcp.writeBytes > INT_MAX ||
        tcp.addresses == 0 || tcp.addresses > 64 || tcp.cnameDepth == 0 || tcp.cnameDepth > 16 ||
        tcp.contextBytes == 0 || tcp.handlerBytes == 0 || tcp.residentBytes < session.residentBytes ||
        session.frame.maximumBodyBytes > INT_MAX ||
        session.frame.maximumBodyBytes > std::numeric_limits<std::size_t>::max() - 9 ||
        session.frame.maximumPendingBytes < session.frame.maximumBodyBytes + 9 || b.writer.frameBytes > INT_MAX ||
        b.writer.operationBytes < b.writer.frameBytes || session.outputBytes > b.writer.operationBytes ||
        session.credentials != protocol::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN ||
        (session.fieldTime != protocol::FieldTimePolicy::DISABLED &&
         session.fieldTime != protocol::FieldTimePolicy::READ) ||
        (session.events != protocol::DataEventPolicy::ZERO_FLAGS &&
         session.events != protocol::DataEventPolicy::BINARY_CATALOG_V1)) {
        return false;
    }

    const std::size_t sizes[]{tcp.inputBytes,        tcp.contextBytes,       tcp.handlerBytes,
                              session.residentBytes, b.client.residentBytes, b.collector.residentBytes,
                              b.writer.residentBytes};
    std::size_t total = 0;

    for (auto size : sizes) {
        if (size > std::numeric_limits<std::size_t>::max() - total) {
            return false;
        }

        total += size;
    }

    return productFits(b.schema.totalNameUnits, sizeof(char16_t)) &&
           productFits(b.desired.peakSymbolUnits, sizeof(char16_t)) &&
           productFits(b.client.commandSlots, b.client.commandSymbolUnits) &&
           productFits(b.client.commandSymbolUnits, sizeof(char16_t)) &&
           productFits(b.client.commandSlots, b.client.commandSymbolUnits * sizeof(char16_t)) &&
           productFits(b.client.batchOwners, b.client.batchArenaBytes) &&
           productFits(session.owners, session.eventBytes) && productFits(session.owners, session.snapshotBytes);
}

/** Validate only local TLS handoff shape; no certificate parsing or user-provider call.
 * @param tls Explicit owned-to-be values.
 * @param budgets Selected output/text capacities.
 * @return True for structurally consistent explicit inputs, without a cryptographic success claim.
 */
bool tlsValid(const EndpointTls &tls, const EndpointBudgets &budgets) noexcept {
    if (!detail::validEndpointHost(tls.referenceIdentity) || tls.trustPem.empty() ||
        tls.trustPem.size() > MAXIMUM_PEM_BYTES || !tls.handshakeTimeout || !durationValid(*tls.handshakeTimeout) ||
        !tls.closeTimeout || !durationValid(*tls.closeTimeout)) {
        return false;
    }

    if (!tls.credentials) {
        return !tls.authenticationTimeout && !tls.tokenUnits;
    }

    return tls.authenticationTimeout && durationValid(*tls.authenticationTimeout) && tls.tokenUnits &&
           *tls.tokenUnits >= 7 && *tls.tokenUnits <= budgets.session.protocol.text.codeUnits &&
           *tls.tokenUnits <= budgets.session.outputBytes / sizeof(char16_t) &&
           productFits(*tls.tokenUnits, sizeof(char16_t));
}

/** Check the selected driver's sufficient indivisible retirement/full-quantum work profile.
 * These restrictions are not generic component minima; partial-progress/handoff snapshots remain valid.
 * @param settings Immutable explicit limits and work maxima.
 * @param schemaRecords Actual captured context cardinality, not a record identity.
 * @return True when checked public-capacity bounds cover whole abort/disposal operations.
 */
bool driverWorkSupported(const EndpointSettings &settings, std::size_t schemaRecords) noexcept {
    const auto &b = settings.budgets;
    const auto &w = b.dispatchTurn;
    std::size_t retirementRecords = 0;

    for (const auto count : {b.collector.frames, b.collector.records, schemaRecords, b.desired.entries}) {
        if (count > std::numeric_limits<std::size_t>::max() - retirementRecords) {
            return false;
        }

        retirementRecords += count;
    }

    if (b.collector.frames > std::numeric_limits<std::size_t>::max() - b.collector.records ||
        !productFits(b.collector.frames, b.session.residentBytes)) {
        return false;
    }

    return w.operations >= 1 && w.commands >= 1 &&
           w.records >= std::max({std::size_t{1}, b.client.commandEntries, retirementRecords}) &&
           w.bytes >= std::max(b.collector.residentBytes, b.session.residentBytes) && w.collector.operations >= 1 &&
           w.collector.visits >= b.collector.records + b.collector.frames &&
           w.collector.bytes >= b.collector.frames * b.session.residentBytes && b.runtimeTurn.steps >= 1 &&
           b.runtimeTurn.handlers >= 1 && b.runtimeTurn.socketBytes >= std::max(b.tcp.inputBytes, b.tcp.writeBytes);
}
} // namespace

/** Complete immutable configuration allocation; contains no native runtime or protocol lease. */
struct EndpointOptions::State {
    core::LocalSchema schema;  ///< Independently copied declarations with original provenance.
    EndpointSettings settings; ///< Owned values and finite shared external provider handle.
};

EndpointOptions::EndpointOptions(std::shared_ptr<const State> state) noexcept : state_(std::move(state)) {
}

std::expected<EndpointOptions, EndpointDiagnostic> EndpointOptions::create(const core::LocalSchema &schema,
                                                                           const EndpointSettings &settings) {
    const auto invalid = [](std::string_view stage) {
        return std::unexpected(EndpointDiagnostic{EndpointError::INVALID_CONFIGURATION, stage});
    };

    if (settings.runtime != EndpointRuntime::CALLER_DRIVEN_TCP ||
        (settings.security != EndpointSecurity::CREDENTIAL_FREE_PLAIN && settings.security != EndpointSecurity::TLS) ||
        (settings.security == EndpointSecurity::CREDENTIAL_FREE_PLAIN && settings.tls) ||
        (settings.security == EndpointSecurity::TLS && (!settings.tls || !tlsValid(*settings.tls, settings.budgets)))) {
        return invalid("endpoint-security");
    }

    if (!detail::validEndpointHost(settings.host) || settings.port == 0) {
        return invalid("endpoint-address");
    }

    const auto &t = settings.timeouts;
    const auto &r = settings.retry;

    if (!durationValid(t.dns) || !durationValid(t.connect) || !durationValid(t.read) || !durationValid(t.write) ||
        !durationValid(t.poll) || r.maximumAttempts == 0 ||
        (r.maximumAttempts == 1 ? r.delay.count() != 0 : !durationValid(r.delay)) ||
        (r.maximumAttempts > 1 &&
         r.delay.count() > std::numeric_limits<std::chrono::milliseconds::rep>::max() / (r.maximumAttempts - 1))) {
        return invalid("endpoint-timeout");
    }

    if (!budgetsValid(settings.budgets)) {
        return std::unexpected(EndpointDiagnostic{EndpointError::RESOURCE_LIMIT, "endpoint-budgets"});
    }

    try {
        auto copied = schema.boundedCopy(settings.budgets.schema);

        if (!copied) {
            return std::unexpected(EndpointDiagnostic{copied.error() == core::SchemaError::ALLOCATION_FAILURE
                                                          ? EndpointError::ALLOCATION_FAILURE
                                                          : EndpointError::RESOURCE_LIMIT,
                                                      "endpoint-schema"});
        }

        return EndpointOptions(std::make_shared<const State>(State{std::move(*copied), settings}));
    } catch (const std::bad_alloc &) {
        return std::unexpected(EndpointDiagnostic{EndpointError::ALLOCATION_FAILURE, "endpoint-copy"});
    } catch (const std::length_error &) {
        return std::unexpected(EndpointDiagnostic{EndpointError::RESOURCE_LIMIT, "endpoint-copy"});
    }
}

const EndpointSettings &EndpointOptions::settings() const noexcept {
    return state_->settings;
}

const core::LocalSchema &EndpointOptions::schema() const noexcept {
    return state_->schema;
}

std::expected<void, EndpointDiagnostic> EndpointOptions::requirePlainStream() const noexcept {
    const auto &s = settings();

    if (s.security != EndpointSecurity::CREDENTIAL_FREE_PLAIN || s.retry.maximumAttempts != 1 ||
        !detail::numericEndpointHost(s.host) || s.budgets.session.fieldTime != protocol::FieldTimePolicy::DISABLED ||
        s.budgets.session.events != protocol::DataEventPolicy::ZERO_FLAGS || s.budgets.session.historySnapshot) {
        return std::unexpected(EndpointDiagnostic{EndpointError::UNSUPPORTED_PROFILE, "endpoint-plain-stream"});
    }

    if (!driverWorkSupported(s, schema().records().size())) {
        return std::unexpected(EndpointDiagnostic{EndpointError::UNSUPPORTED_PROFILE, "endpoint-driver-work"});
    }

    return {};
}

std::expected<EndpointSettings, EndpointDiagnostic> controlledStreamSettings(std::chrono::milliseconds duration) {
    if (duration.count() <= 0 || duration.count() > 600000) {
        return std::unexpected(EndpointDiagnostic{EndpointError::INVALID_CONFIGURATION, "endpoint-duration"});
    }

    EndpointSettings s;
    s.timeouts = {std::chrono::milliseconds(1000), std::chrono::milliseconds(1000), duration, duration,
                  std::chrono::milliseconds(1)};
    auto &b = s.budgets;
    b.schema = {16, 128, 256, 128, 16384};
    b.layout = {b.schema, 16, 128, 128, 16384};
    b.writer = {8192, 8201, 512, 128, {8192, 1024}, 65536, 65536, 128};
    b.session.frame = {8192, 8201};
    b.session.protocol = {128, 64, {8192, 1024}, 16384};
    b.session.descriptions = {8192, 32, 32, 256, {8192, 1024}, 8192};
    b.session.context = {b.schema, 32, {512, 1024, 16384}};
    b.session.data = {8192, 64,   512,   64,    512,   8192, {8192, 128, 128}, {8192, 1024}, 8192, 8192, 8192,
                      8192, 8192, 16384, 16384, 16384, 64};
    b.session.residentBytes = 4 * MEBIBYTE;
    b.session.owners = 128;
    b.session.snapshotBytes = b.session.scratchBytes = b.session.eventBytes = 65536;
    b.session.outputBytes = 8192;
    b.session.credentials = protocol::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    b.session.fieldTime = protocol::FieldTimePolicy::DISABLED;
    b.session.events = protocol::DataEventPolicy::ZERO_FLAGS;
    b.session.historySnapshot = false;
    b.desired = {16, 32, 128, 2048, 64, 4096, 32};
    b.collector = {16, 128, 2 * MEBIBYTE, 4 * MEBIBYTE};
    b.client = {4, 8, 16, 2048, 1, 128, 128, 8192, 8192, 65536, 4, MEBIBYTE, 2 * MEBIBYTE};
    b.tcp = {8192, 8192, 16, 4, 65536, 8192, 8 * MEBIBYTE};
    b.writerPreparation = {10000, 1000000, 1000000, 1000000};
    b.submitTurn = {16, 1000000, 1000000};
    b.protocolTurn = {10000, 10000000, 10000000, 100 * MEBIBYTE};
    b.dispatchTurn = {10000,
                      10000,
                      10000,
                      1000000,
                      1000000,
                      1000000,
                      100 * MEBIBYTE,
                      {10000, 1000000, 1000000},
                      {10000, 1000000, 1000000, 100 * MEBIBYTE}};
    b.runtimeTurn = {128, 128, 10000000, 0, 0, 0, 0};

    return s;
}
} // namespace org::ttldtor::qdcxx::client
