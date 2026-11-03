// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/client/subscription_receiver.hpp"
#include "qd/transport/tcp_recovery.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::examples::quote_subscription {
inline constexpr std::size_t MEBIBYTE = 1024 * 1024; ///< Byte unit for independent logical storage ceilings.
/** Per-channel logical SDK reservation from spec/transport/DNS-RESOURCES.md; not measured process RSS. */
inline constexpr std::size_t DNS_ENVELOPE_BYTES = 80 * MEBIBYTE;
/** Conservative complete DNS progress charge from spec/transport/DNS-RESOURCES.md.
 * Logical byte/node/primitive work units, not CPU instructions, time or requested storage.
 * Recovery owns the native runtime, so this example explicitly selects its documented pinned profile.
 */
inline constexpr std::uint64_t DNS_PROGRESS_UNITS = 351280142190671838ULL;
inline constexpr std::size_t MAXIMUM_SYMBOL_UNITS = 128; ///< Single exact selector capacity.
/** Explicit canonical Exact profile shared by queue, receiver and recovery's writer factory. */
inline constexpr auto SELECTORS = core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1;
inline constexpr auto DEFAULT_DURATION = std::chrono::milliseconds{5000};   ///< Default admission interval.
inline constexpr auto MAXIMUM_DURATION = std::chrono::milliseconds{600000}; ///< Maximum admission interval.
inline constexpr auto DEFAULT_DRAIN = std::chrono::milliseconds{1000};      ///< Default absolute local drain allowance.
inline constexpr auto MAXIMUM_DRAIN = std::chrono::milliseconds{60000};     ///< Largest accepted local drain allowance.
inline constexpr std::size_t MAXIMUM_RECORDS = 1000000;                     ///< Largest explicit printed-row limit.
inline constexpr std::size_t MAXIMUM_OWNER_TURNS = 1000000; ///< Finite caller-loop guard, including shutdown.
inline constexpr std::size_t MAXIMUM_REJECTED_POLLS = 2;    ///< Consecutive same-profile refusals before failure exit.
inline constexpr std::uint32_t MAXIMUM_ATTEMPTS = 3; ///< Finite lifetime attempt count, including the first connection.
inline constexpr auto RETRY_DELAY = std::chrono::milliseconds{250}; ///< Fixed delay between eligible attempts.
inline constexpr unsigned char FIRST_PRINTABLE_ASCII = 0x20;        ///< First accepted exact CLI symbol byte.
inline constexpr unsigned char LAST_PRINTABLE_ASCII = 0x7e;         ///< Last accepted exact CLI symbol byte.
/** Bounded full-shape preparation for the unchanged eight-field default Quote. */
inline constexpr events::MappingLimits MAPPING_LIMITS{128, 256, 16, 64, 128, 128, 16384, 100000};

/** Supply actual independent queue/collector/desired work for one caller-selected owner turn.
 * @return Finite credits, used unchanged during normal operation and local drain.
 */
inline mux::QueueWork queueWork() {
    return {100000,
            100000000,
            100000000,
            100000000,
            100000000,
            10000,
            10000,
            {100000, 100000000, 100000000, 100000000},
            {100000, 100000000, 100000000},
            {100000, 100000000, 100000000, 100000000},
            {100000, 100000000, 100000000, 100000000},
            {100000, 100000000, 100000000, 100000000}};
}

/** Reserve one application registration without treating callback capture memory as queue storage.
 * @return Explicit existing queue maxima for detached records, commands and completion slots.
 */
inline mux::ClientLimits clientLimits() {
    return {32, 512 * 1024, 8, 128 * 1024, 4, 128, MAXIMUM_SYMBOL_UNITS, 1024, 4096, 8192, 4, 8, 32, 4096};
}

/** Create the actual STREAM queue with the explicitly selected canonical Exact policy.
 * @param schema Complete default Quote schema and exact immutable provenance.
 * @param settings Explicit controlled endpoint limits, borrowed only for construction.
 * @param work Caller-owned initialization and eventual detached-retirement credits.
 * @return Existing queue owner or its original typed refusal, before native activation.
 */
inline std::expected<mux::MuxQueue, mux::QueueFailure>
createQueue(const core::LocalSchema &schema, const client::EndpointSettings &settings, mux::QueueWork &work) {
    const auto &desired = settings.budgets.desired;
    const mux::MuxLimits interests{desired.schemaRecords,  1,
                                   desired.entries,        desired.entries,
                                   desired.symbolUnits,    desired.totalSymbolUnits,
                                   desired.commandEntries, 4 * MEBIBYTE};
    const mux::QueueLimits limits{1, 128, 16, 4, 16, 32, 4 * MEBIBYTE, 8, 128 * 1024, 4, 8, 2, 16 * MEBIBYTE};

    return mux::MuxQueue::createStream(schema, interests, desired, settings.budgets.collector, limits, work, SELECTORS);
}

/** State the bounded recovery-copy/replay profile for this one-schema, one-symbol application.
 * @return Finite independent schema/blueprint/endpoint reservations and STREAM replay banks.
 */
inline transport::RecoveryOptions recoveryOptions() {
    transport::RecoveryOptions result;
    result.selectors = SELECTORS;
    result.limits.schemaCopies = {1024, 4096, 4096, 65536};
    result.limits.blueprintCopy = {1024, 4096, 65536};
    result.limits.adoptedBlueprint = {1024, 4096, 65536};
    result.limits.endpointCopyBytes = 2 * MEBIBYTE;
    result.limits.writerConfigurationBytes = 4 * MEBIBYTE;
    result.limits.replay[static_cast<std::size_t>(transport::RecoveryFamily::STREAM)] = {32, 4096};

    return result;
}

/** Fund complete recovery preparation, including the existing prepaid retirement path.
 * @return Explicit independent recovery/writer/queue coordinates, never borrowed from a future turn.
 */
inline transport::RecoveryPreparationWork preparationWork() {
    return {{10000, 100000000, 100000000, 100000000},
            {10000, 100000000, 100000000, 100000000},
            {queueWork(), queueWork(), queueWork()}};
}

/** Fund the explicit named-route storage and work profile before native activation.
 * @param settings Mutable construction settings, changed once before EndpointOptions snapshots them.
 * @return Existing DNS inspection refusal, or success after checked addition of separate DNS storage and work.
 * @throws std::length_error If a reservation or explicitly inspected credit sum cannot be represented.
 */
inline std::expected<void, transport::TcpDiagnostic> configureDnsProfile(client::EndpointSettings &settings) {
    const auto required = transport::TcpRuntime::dnsPreparationWorkRequirement(1);

    if (!required) {
        return std::unexpected(required.error());
    }

    auto &residentBytes = settings.budgets.tcp.residentBytes;

    if (DNS_ENVELOPE_BYTES > std::numeric_limits<std::size_t>::max() - residentBytes) {
        throw std::length_error("DNS storage reservation exceeds the example profile");
    }

    auto &work = settings.budgets.protocolTurn;
    const std::array additions{
        std::pair{&work.operations, required->operations}, std::pair{&work.visits, required->visits},
        std::pair{&work.textUnits, required->textUnits}, std::pair{&work.bytes, required->bytes}};

    for (const auto &[remaining, amount] : additions) {
        if (amount > std::numeric_limits<std::size_t>::max() - *remaining) {
            throw std::length_error("DNS preparation work exceeds the example profile");
        }

        *remaining += amount;
    }

    // Keep the existing adapter/session reservation independent of the documented DNS SDK envelope.
    residentBytes += DNS_ENVELOPE_BYTES;

    auto &runtime = settings.budgets.runtimeTurn;
    runtime.dnsCalls = 10000;
    runtime.dnsPackets = 10000;
    runtime.dnsBytes = 10000000;
    runtime.dnsUnits = DNS_PROGRESS_UNITS;

    return {};
}

/** Supply one actual serialized recovery turn without a separate enlarged shutdown profile.
 * @param endpoint Immutable endpoint whose controlled native/session settings apply to this run.
 * @return Mutable credits selected explicitly by the application for this turn.
 */
inline transport::RecoveryTurnWork recoveryTurn(const client::EndpointOptions &endpoint) {
    const auto &runtime = endpoint.settings().budgets.runtimeTurn;

    return {{10000, 100000000, 100000000, 100000000},
            {10000, 100000000, 100000000, 100000000},
            {runtime.steps, runtime.handlers, runtime.socketBytes, runtime.dnsCalls, runtime.dnsPackets,
             runtime.dnsBytes, runtime.dnsUnits},
            endpoint.settings().budgets.protocolTurn,
            {queueWork(), queueWork(), queueWork()}};
}

/** Fund the optional owned event projection explicitly inside one application callback.
 * @return Finite complete-shape, selection, symbol-copy and logical-byte credits.
 */
inline client::ReceiverProjectionWork projectionWork() {
    return {10000, 1024, 64 * 1024};
}
} // namespace org::ttldtor::qdcxx::examples::quote_subscription
