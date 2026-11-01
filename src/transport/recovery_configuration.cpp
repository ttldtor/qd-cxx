// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_configuration.hpp"

#include "recovery_copy_profile.hpp"

#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::transport::detail {
namespace {

constexpr std::size_t REPLAY_BANKS = 3;               ///< Latest desired, emitted and candidate coexist.
constexpr std::size_t RUNTIME_SCHEMAS = 3;            ///< Endpoint, composer and session schema copies coexist.
constexpr std::size_t SETTINGS_COPIES = 2;            ///< Temporary settings and the candidate immutable State.
constexpr std::size_t SETTINGS_STATE_MOVES = 1;       ///< State transfer may copy fixed SSO character buffers.
constexpr std::size_t ENDPOINT_SCHEMA_MOVES = 2;      ///< Bounded copy into temporary State, then allocated State.
constexpr std::size_t CONFIGURATION_FIXED_PASSES = 2; ///< Candidate aggregate construction and expected-value move.
constexpr std::size_t ENDPOINT_BUDGET_SCALARS = 7;    ///< Existing budgetsValid iterates seven fixed byte ceilings.
constexpr std::size_t ENDPOINT_FIXED_OWNERS = 4;      ///< Two settings objects, immutable State and result owner.
constexpr std::size_t SCHEMA_METADATA_PASSES = 5;     ///< Validation, copy, measurement, postcheck and disposal.
constexpr std::size_t HOST_VALIDATION_PASSES = 16;    ///< Colon/compression, segmented address/DNS and edge checks.

/** Local capacity/work refusal, with allocator exception bookkeeping outside logical backing. */
struct Refusal {
    RecoveryDiagnostic diagnostic; ///< Detached static evidence.
};

/** Reject before any input ownership transfer.
 * @param error Recovery category.
 * @param stage Static checkpoint.
 */
[[noreturn]] void refuse(RecoveryError error, std::string_view stage) {
    throw Refusal{{error, stage}};
}

/** Sum capacities without wrapping.
 * @param current Current total.
 * @param amount Additional capacity in identical units.
 * @return Representable total.
 */
std::size_t sum(std::size_t current, std::size_t amount) {
    if (amount > std::numeric_limits<std::size_t>::max() - current) {
        refuse(RecoveryError::RESOURCE_LIMIT, "recovery-capacity-arithmetic");
    }

    return current + amount;
}

/** Multiply finite simultaneous scopes without wrapping.
 * @param amount Per-owner capacity.
 * @param count Owner count or actual element width.
 * @return Representable product.
 */
std::size_t product(std::size_t amount, std::size_t count) {
    if (count != 0 && amount > std::numeric_limits<std::size_t>::max() / count) {
        refuse(RecoveryError::RESOURCE_LIMIT, "recovery-capacity-arithmetic");
    }

    return amount * count;
}

/** Charge an actual new-owner work coordinate.
 * @param amount Required work.
 * @param remaining Caller remainder, changed only for admitted charges.
 */
void debit(std::size_t amount, std::size_t &remaining) {
    if (amount > remaining) {
        refuse(RecoveryError::WORK_LIMIT, "recovery-preparation-work");
    }

    remaining -= amount;
}

/** Inspect one fixed source position without moving its owner.
 * @param queues Actual caller-owned optional sources.
 * @param index Fixed family position0..2.
 * @return Existing optional slot.
 */
std::optional<mux::MuxQueue> &queueAt(RecoveryQueues &queues, std::size_t index) noexcept {
    if (index == 0) {
        return queues.ticker;
    }

    if (index == 1) {
        return queues.stream;
    }

    return queues.history;
}

/** Obtain the exact source family for a fixed slot.
 * @param index Fixed family position0..2.
 * @return Required source family.
 */
core::SubscriptionFamily sourceFamily(std::size_t index) noexcept {
    constexpr std::array families{core::SubscriptionFamily::TICKER, core::SubscriptionFamily::STREAM,
                                  core::SubscriptionFamily::HISTORY};

    return families[index];
}

/** Add future queue reservations and validate source-growth replay sufficiency before allocation.
 * @param endpoint Original immutable provenance.
 * @param queues Caller sources, unchanged by inspection.
 * @param options Selected finite policy.
 * @param work Actual queue and new-owner inspection credits.
 * @param capacity In-progress finite vector.
 * @return Enabled family bits.
 */
std::uint8_t inspectQueues(const client::EndpointOptions &endpoint, RecoveryQueues &queues,
                           const RecoveryOptions &options, RecoveryPreparationWork &work, RecoveryCapacity &capacity) {
    std::uint8_t families = 0;

    for (std::size_t family = 0; family != capacity.queues.size(); ++family) {
        auto &queue = queueAt(queues, family);
        debit(1, work.recovery.visits);

        if (!queue) {
            continue;
        }

        const auto binding =
            queue->validateBinding(endpoint.schema(), sourceFamily(family), options.selectors, work.queues[family]);

        if (!binding) {
            auto diagnostic = RecoveryDiagnostic{binding.error().error == mux::QueueError::WORK_LIMIT
                                                     ? RecoveryError::WORK_LIMIT
                                                     : RecoveryError::INVALID_CONFIGURATION,
                                                 "recovery-queue-binding"};
            diagnostic.queue = binding.error();
            throw Refusal{diagnostic};
        }

        families |= static_cast<std::uint8_t>(1U << family);
        const auto limit = queue->capacity();
        capacity.queues[family] = limit;
        capacity.queueBytes =
            sum(capacity.queueBytes, sum(limit.queueBytes, sum(limit.muxBytes, limit.collectorBytes)));
        const auto neededEntries = product(limit.desiredOwners, limit.desiredPerOwner.entries);
        const auto neededUnits = product(limit.desiredOwners, limit.desiredPerOwner.totalSymbolUnits);
        // Preserve separate source peak/count dimensions and reject even unused overflowing derived maxima.
        (void)product(limit.desiredOwners, limit.desiredPerOwner.schemaRecords);
        (void)product(limit.desiredOwners, limit.desiredPerOwner.peakEntries);
        (void)product(limit.desiredOwners, limit.desiredPerOwner.peakSymbolUnits);
        const auto replay = options.limits.replay[family];

        if (neededEntries > replay.entries || neededUnits > replay.symbolUnits) {
            refuse(RecoveryError::RESOURCE_LIMIT, "recovery-replay-capacity");
        }

        const auto entries = product(REPLAY_BANKS, replay.entries);
        const auto units = product(REPLAY_BANKS, replay.symbolUnits);
        capacity.replayEntries = sum(capacity.replayEntries, entries);
        capacity.replaySymbolUnits = sum(capacity.replaySymbolUnits, units);
        capacity.replayBytes = sum(capacity.replayBytes, sum(product(entries, sizeof(core::SubscriptionRequest)),
                                                             product(units, sizeof(char16_t))));
    }

    return families;
}

/** Obtain a one-schema tuple from the already admitted runtime peak.
 * @param peak Exactly three equal immutable source copies, checked by the copy scanner.
 * @return One-copy inspection allowance.
 */
RecoverySchemaCapacity oneSchema(RecoverySchemaCapacity peak) noexcept {
    return {peak.records / RUNTIME_SCHEMAS, peak.fields / RUNTIME_SCHEMAS, peak.lookups / RUNTIME_SCHEMAS,
            peak.nameUnits / RUNTIME_SCHEMAS};
}

/** Independent precharge and retained disposal vectors; neither is a resident-capacity claim. */
struct PreparationCharge {
    RecoveryWork total;    ///< All remaining new configuration work, including prepaid rollback/final disposal.
    RecoveryWork disposal; ///< Persistent new-owner disposal subset, copied into the resulting owner.
};

/** Derive all remaining new-owner work before the first settings/schema/array copy.
 * Existing first-writer preparation remains exclusively charged by its actual writer work object.
 * @param endpoint Immutable source supplying bounded address lengths and optional string count.
 * @param capacity Already admitted simultaneous copy/adoption/replay vector.
 * @return Checked conservative backing/metadata work and its persistent disposal subset.
 */
PreparationCharge preparationCharge(const client::EndpointOptions &endpoint, const RecoveryCapacity &capacity) {
    const auto schema = oneSchema(capacity.schemaCopies);
    const auto schemaNodes = sum(schema.records, sum(schema.fields, schema.lookups));
    auto schemaBytes = sum(sizeof(core::LocalSchema), product(schema.records, sizeof(core::LocalRecord)));
    schemaBytes = sum(schemaBytes, product(schema.fields, sizeof(core::LocalField)));
    schemaBytes = sum(schemaBytes, product(schema.lookups, sizeof(core::FieldLookup)));
    schemaBytes = sum(schemaBytes, product(schema.nameUnits, sizeof(char16_t)));

    const auto &settings = endpoint.settings();
    const auto strings = settings.tls ? std::size_t{3} : std::size_t{1};
    const auto singleSettingsText = capacity.endpointCopyBytes / SETTINGS_COPIES;
    auto addressUnits = settings.host.size();

    if (settings.tls) {
        addressUnits = sum(addressUnits, settings.tls->referenceIdentity.size());
    }

    // validHost: colon dispatch; bounded compression searches; disjoint segment scans/conversion;
    // optional final triple-colon search and edge tests. Sixteen full-unit passes dominate all routes.
    const auto validationText = product(HOST_VALIDATION_PASSES, addressUnits);
    const auto blueprintNodes = sum(capacity.adoptedBlueprint.records, capacity.adoptedBlueprint.fields);
    auto blueprintBytes = product(capacity.adoptedBlueprint.records, sizeof(protocol::OutgoingRecord));
    blueprintBytes = sum(blueprintBytes, product(capacity.adoptedBlueprint.fields, sizeof(protocol::OutgoingField)));
    blueprintBytes = sum(blueprintBytes, product(capacity.adoptedBlueprint.nameUnits, sizeof(char16_t)));

    PreparationCharge result{};
    result.disposal.operations = 1;
    result.disposal.visits = sum(1, sum(schemaNodes, sum(strings, sum(blueprintNodes, capacity.replayEntries))));
    result.disposal.textUnits = capacity.replaySymbolUnits;
    result.disposal.bytes = sum(schemaBytes, sum(singleSettingsText, sum(blueprintBytes, capacity.replayBytes)));

    result.total.operations = 2; // One preparation phase and the one prepaid persistent disposal phase.
    const auto settingsPasses = sum(2 * SETTINGS_COPIES, sum(SETTINGS_STATE_MOVES, 1));
    result.total.visits = sum(product(SCHEMA_METADATA_PASSES, schemaNodes), product(settingsPasses, strings));
    result.total.visits = sum(result.total.visits, sum(ENDPOINT_BUDGET_SCALARS, ENDPOINT_FIXED_OWNERS));
    result.total.visits = sum(result.total.visits, sum(blueprintNodes, product(2, capacity.replayEntries)));
    const auto settingsCopyText = product(sum(SETTINGS_COPIES, SETTINGS_STATE_MOVES), singleSettingsText);
    result.total.textUnits = sum(schema.nameUnits, sum(settingsCopyText, validationText));
    result.total.textUnits = sum(result.total.textUnits, product(2, capacity.replaySymbolUnits));
    // Copy/initialize plus possible rollback/final release. SSO is conservatively counted twice:
    // once inside public metadata and once in the independently declared text-capacity coordinate.
    result.total.bytes =
        sum(product(2, schemaBytes), product(sum(2 * SETTINGS_COPIES, SETTINGS_STATE_MOVES), singleSettingsText));
    result.total.bytes =
        sum(result.total.bytes, product(sum(SETTINGS_COPIES, SETTINGS_STATE_MOVES), sizeof(client::EndpointSettings)));
    result.total.bytes = sum(result.total.bytes, product(ENDPOINT_SCHEMA_MOVES, sizeof(core::LocalSchema)));
    result.total.bytes = sum(result.total.bytes, product(CONFIGURATION_FIXED_PASSES, sizeof(RecoveryConfiguration)));
    result.total.bytes = sum(result.total.bytes, sum(blueprintBytes, product(2, capacity.replayBytes)));

    return result;
}

/** Precharge every new coordinate before construction; successful earlier charges remain spent.
 * @param charge Checked complete preparation/rollback/final-disposal allowance.
 * @param work Actual caller remainder, never refilled.
 */
void precharge(RecoveryWork charge, RecoveryWork &work) {
    debit(charge.operations, work.operations);
    debit(charge.visits, work.visits);
    debit(charge.textUnits, work.textUnits);
    debit(charge.bytes, work.bytes);
}

/** Inspect actual copied string capacities after their source-bound precharge.
 * @param settings Complete owned snapshot's values.
 * @param allowance One settings copy's admitted string capacity.
 * @return True when actual settings backing fits the selected profile.
 */
bool settingsCopyFits(const client::EndpointSettings &settings, std::size_t allowance) {
    auto actual = sum(settings.host.capacity(), 1);

    if (settings.tls) {
        actual = sum(actual, sum(settings.tls->referenceIdentity.capacity(), 1));
        actual = sum(actual, sum(settings.tls->trustPem.capacity(), 1));
    }

    return actual <= allowance;
}

/** Allocate each enabled replay owner after all simultaneous requests were admitted.
 * @param queues Unchanged caller slots selecting enabled families.
 * @param options Required fixed per-bank capacities.
 * @return Complete banks or an allocation exception, with caller inputs still unchanged.
 */
std::array<RecoveryReplay, 3> allocateReplay(RecoveryQueues &queues, const RecoveryOptions &options) {
    std::array<RecoveryReplay, 3> result{};

    for (std::size_t family = 0; family != result.size(); ++family) {
        if (!queueAt(queues, family)) {
            continue;
        }

        auto &replay = result[family];
        replay.limits = options.limits.replay[family];

        for (auto &bank : replay.banks) {
            if (replay.limits.entries != 0) {
                bank.entries = std::make_unique<core::SubscriptionRequest[]>(replay.limits.entries);
            }

            if (replay.limits.symbolUnits != 0) {
                bank.symbols = std::make_unique<char16_t[]>(replay.limits.symbolUnits);
            }
        }
    }

    return result;
}

} // namespace

std::expected<RecoveryConfiguration, RecoveryDiagnostic>
RecoveryConfiguration::prepare(const client::EndpointOptions &endpoint,
                               const std::vector<protocol::OutgoingRecord> &blueprint, RecoveryQueues &queues,
                               const RecoveryOptions &options, client::EndpointSecurity security,
                               RecoveryPreparationWork &work) {
    auto inspected = inspectRecoveryCopies(endpoint, blueprint, options.limits, work.recovery);

    if (!inspected) {
        return std::unexpected(inspected.error());
    }

    try {
        if (endpoint.settings().security != security || options.dns.serverCount > options.dns.servers.size()) {
            refuse(RecoveryError::INVALID_CONFIGURATION, "recovery-configuration");
        }

        auto capacity = *inspected;
        const auto families = inspectQueues(endpoint, queues, options, work, capacity);
        const auto retry = endpoint.settings().retry;
        const RecoveryConfig scheduler{retry.maximumAttempts, retry.delay, options.heartbeatInterval,
                                       options.protocolInactivity, families};
        const auto policy = RecoveryState::create(scheduler, &capacity);

        if (!policy) {
            refuse(RecoveryError::INVALID_CONFIGURATION, "recovery-liveness");
        }

        const auto charge = preparationCharge(endpoint, capacity);
        precharge(charge.total, work.recovery);

        std::optional<client::EndpointOptions> owned;
        {
            auto settings = endpoint.settings();
            settings.retry = {1, std::chrono::milliseconds{0}};
            auto created = client::EndpointOptions::create(endpoint.schema(), settings);

            if (!created) {
                const auto error = created.error().error;
                auto diagnostic = RecoveryDiagnostic{error == client::EndpointError::ALLOCATION_FAILURE
                                                         ? RecoveryError::ALLOCATION_FAILURE
                                                         : RecoveryError::RESOURCE_LIMIT,
                                                     "recovery-endpoint-copy"};
                diagnostic.endpoint = created.error();
                throw Refusal{diagnostic};
            }

            owned.emplace(std::move(*created));
        }

        if (!recoverySchemaFits(owned->schema(), oneSchema(capacity.schemaCopies)) ||
            !settingsCopyFits(owned->settings(), capacity.endpointCopyBytes / SETTINGS_COPIES)) {
            refuse(RecoveryError::UNSUPPORTED_PROFILE, "recovery-schema-copy-profile");
        }

        auto writer =
            protocol::SubscriptionWriter::create(owned->schema(), blueprint, owned->settings().budgets.layout,
                                                 owned->settings().budgets.writer, work.writer, options.selectors);

        if (!writer) {
            auto error = RecoveryError::INVALID_CONFIGURATION;

            if (writer.error() == protocol::SubscriptionWriterError::WORK_LIMIT) {
                error = RecoveryError::WORK_LIMIT;
            } else if (writer.error() == protocol::SubscriptionWriterError::RESOURCE_LIMIT) {
                error = RecoveryError::RESOURCE_LIMIT;
            } else if (writer.error() == protocol::SubscriptionWriterError::ALLOCATION_FAILURE) {
                error = RecoveryError::ALLOCATION_FAILURE;
            }

            auto diagnostic = RecoveryDiagnostic{error, "recovery-first-writer"};
            diagnostic.writer = writer.error();

            return std::unexpected(diagnostic);
        }

        capacity.writerConfigurationBytes = writer->storageBytes();

        if (capacity.writerConfigurationBytes > options.limits.writerConfigurationBytes) {
            refuse(RecoveryError::RESOURCE_LIMIT, "recovery-writer-adoption");
        }

        auto replay = allocateReplay(queues, options);
        static_assert(std::is_nothrow_move_constructible_v<RecoveryQueues>);
        static_assert(std::is_nothrow_move_constructible_v<RecoveryConfiguration>);

        // The wrapper must still allocate its stable Core before calling the separate noexcept input commit.
        return RecoveryConfiguration{std::move(*owned),
                                     {},
                                     {},
                                     std::move(replay),
                                     std::optional<protocol::SubscriptionWriter>{std::move(*writer)},
                                     options,
                                     capacity,
                                     scheduler,
                                     charge.disposal};
    } catch (const Refusal &failure) {
        return std::unexpected(failure.diagnostic);
    } catch (const std::bad_alloc &) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::ALLOCATION_FAILURE, "recovery-preparation-storage"});
    } catch (const std::length_error &) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::RESOURCE_LIMIT, "recovery-preparation-length"});
    }
}

bool RecoveryConfiguration::adopt(std::vector<protocol::OutgoingRecord> &&blueprintInput,
                                  RecoveryQueues &&queueInput) noexcept {
    if (adopted) {
        return false;
    }

    static_assert(std::is_nothrow_move_assignable_v<RecoveryQueues>);
    static_assert(std::is_nothrow_move_assignable_v<std::vector<protocol::OutgoingRecord>>);
    blueprint = std::move(blueprintInput);
    queues = std::move(queueInput);
    adopted = true;

    return true;
}

std::optional<mux::MuxQueue> &RecoveryConfiguration::queue(std::size_t family) noexcept {
    return queueAt(queues, family);
}

} // namespace org::ttldtor::qdcxx::transport::detail
