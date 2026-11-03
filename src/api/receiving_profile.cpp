// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "receiving_profile.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
constexpr std::size_t MEBIBYTE = 1024 * 1024;                         ///< Independent logical byte unit.
constexpr core::SchemaLimits SCHEMA_LIMITS{16, 128, 256, 128, 16384}; ///< Complete declarations and aliases.
constexpr std::size_t FRAME_RECORDS = 64;                             ///< Whole admitted DATA frame record ceiling.
constexpr std::size_t CALLBACK_RECORDS = 4;                           ///< Whole callback projection ceiling.
constexpr std::size_t FIELD_TEXT_UNITS = 8192;            ///< Aggregate text copied from one complete record.
constexpr std::size_t FIELD_BYTE_UNITS = 16384;           ///< Aggregate opaque payload copied from one record.
constexpr std::size_t RECORD_ARENA_BYTES = 65536;         ///< Complete detached per-record arena request ceiling.
constexpr std::size_t QUEUED_RECORDS = 128;               ///< Independent detached-record slots per registration.
constexpr std::size_t RECORD_BYTES = 16 * MEBIBYTE;       ///< Per-registration detached backing ceiling.
constexpr std::size_t BATCH_BYTES = MEBIBYTE;             ///< Per-registration composite backing ceiling.
constexpr std::size_t CONTROL_OPERATIONS = 100000;        ///< Qualified finite one-client owner work seed.
constexpr std::size_t CONTROL_VISITS = 100000000;         ///< Qualified finite metadata/equality work seed.
constexpr std::size_t CONTROL_BYTES = 256 * MEBIBYTE;     ///< Qualified logical copy/disposal seed.
constexpr std::size_t DNS_ENVELOPE_BYTES = 80 * MEBIBYTE; ///< Separate documented DNS SDK envelope.
constexpr std::uint64_t DNS_PROGRESS_UNITS = 351280142190671838ULL; ///< Complete documented DNS logical charge.
constexpr std::size_t WRITER_CONFIGURATION_BYTES = 4 * MEBIBYTE;    ///< Retained complete writer reservation.
constexpr auto SELECTORS = core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1; ///< Exact ASCII selector domain.

/** Reject before activating any native state.
 * @param code Detached high-level category.
 * @param stage Static library-owned checkpoint.
 */
[[noreturn]] void refuse(ErrorCode code, std::string_view stage) {
    throw Error({code, stage});
}

/** Add finite quantities without wrapping.
 * @param left Original amount.
 * @param right Additional amount in the same units.
 * @return Exact sum or detached resource refusal.
 */
std::size_t add(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        refuse(ErrorCode::RESOURCE_LIMIT, "profile-arithmetic");
    }

    return left + right;
}

/** Multiply finite capacities without wrapping.
 * @param count Number of independent reservations.
 * @param amount One reservation's amount.
 * @return Exact product or detached resource refusal.
 */
std::size_t multiply(std::size_t count, std::size_t amount) {
    if (amount && count > std::numeric_limits<std::size_t>::max() / amount) {
        refuse(ErrorCode::RESOURCE_LIMIT, "profile-arithmetic");
    }

    return count * amount;
}

/** Select an immutable caller registry or prepare compatible defaults without native work.
 * @param registry Optional explicit registry, borrowed during selection.
 * @return Owning immutable snapshot or detached registry refusal.
 */
EventRegistry selectRegistry(const EventRegistry *registry) {
    if (registry) {
        if (!registry->valid()) {
            refuse(ErrorCode::INVALID_ARGUMENT, "profile-registry");
        }

        return *registry;
    }

    EventRegistryWork work;
    auto result = EventRegistry::builtIn({}, work);

    if (!result) {
        const auto allocation = std::visit(
            [](auto error) {
                using Error = decltype(error);

                return error == Error::ALLOCATION_FAILURE;
            },
            result.error().cause);
        refuse(allocation ? ErrorCode::ALLOCATION_FAILURE : ErrorCode::UNSUPPORTED_PROFILE, "profile-registry");
    }

    return std::move(*result);
}

/** Copy every complete field using its first explicit wire alias.
 * @param schema Complete selected declarations, including unregistered generic records and extra fields.
 * @return Complete one-to-one outgoing description blueprint.
 */
std::vector<protocol::OutgoingRecord> prepareBlueprint(const core::LocalSchema &schema) {
    std::vector<protocol::OutgoingRecord> result;
    result.reserve(schema.records().size());

    for (const auto &record : schema.records()) {
        protocol::OutgoingRecord outgoing{record.id, record.id, record.name, {}};
        outgoing.fields.reserve(record.fields.size());

        for (std::size_t field = 0; field < record.fields.size(); ++field) {
            const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
                return entry.fieldId == field;
            });

            if (lookup == record.lookups.end()) {
                refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-wire-alias");
            }

            outgoing.fields.push_back(
                {lookup->name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
        }

        result.push_back(std::move(outgoing));
    }

    return result;
}

/** Create an explicit finite multi-registration work seed from the existing qualified one-client profile.
 * Normal turns and drain use this same seed, with no retry-time top-up.
 * @param scale Simultaneous registration/selector reservation multiplier.
 * @return Independent finite queue/component work coordinates.
 */
mux::QueueWork queueWork(std::size_t scale) {
    const auto operations = multiply(scale, CONTROL_OPERATIONS);
    const auto visits = multiply(scale, CONTROL_VISITS);
    const auto bytes = multiply(scale, CONTROL_BYTES);

    return {operations,
            visits,
            visits,
            visits,
            bytes,
            multiply(scale, 10000),
            multiply(scale, 10000),
            {operations, visits, visits, bytes},
            {operations, visits, visits},
            {operations, visits, visits, bytes},
            {operations, visits, visits, bytes},
            {operations, visits, visits, bytes}};
}

/** Reserve actual complete schema/blueprint copies and three replay banks.
 * @param profile Complete preparation, modified before publication.
 */
void prepareCopies(ReceivingProfile &profile) {
    transport::RecoverySchemaCapacity one;
    one.records = profile.schema.records().size();

    for (const auto &record : profile.schema.records()) {
        one.fields = add(one.fields, record.fields.size());
        one.lookups = add(one.lookups, record.lookups.size());
        one.nameUnits = add(one.nameUnits, add(record.name.size(), 8));

        for (const auto &lookup : record.lookups) {
            one.nameUnits = add(one.nameUnits, add(lookup.name.size(), 8));
        }
    }

    auto &limits = profile.recoveryOptions.limits;
    limits.schemaCopies = {multiply(3, one.records), multiply(3, one.fields), multiply(3, one.lookups),
                           multiply(3, one.nameUnits)};
    limits.blueprintCopy.records = profile.blueprint.size();
    limits.adoptedBlueprint.records = profile.blueprint.capacity();

    for (const auto &record : profile.blueprint) {
        limits.blueprintCopy.fields = add(limits.blueprintCopy.fields, record.fields.size());
        limits.adoptedBlueprint.fields = add(limits.adoptedBlueprint.fields, record.fields.capacity());
        limits.blueprintCopy.nameUnits = add(limits.blueprintCopy.nameUnits, add(record.wireName.size(), 8));
        limits.adoptedBlueprint.nameUnits = add(limits.adoptedBlueprint.nameUnits, add(record.wireName.capacity(), 1));

        for (const auto &field : record.fields) {
            limits.blueprintCopy.nameUnits = add(limits.blueprintCopy.nameUnits, add(field.wireName.size(), 8));
            limits.adoptedBlueprint.nameUnits =
                add(limits.adoptedBlueprint.nameUnits, add(field.wireName.capacity(), 1));
        }
    }

    const auto &host = profile.endpoint.settings().host;
    limits.endpointCopyBytes = multiply(2, std::max(add(host.capacity(), 1), add(host.size(), 16)));
    limits.writerConfigurationBytes = WRITER_CONFIGURATION_BYTES;
    const auto family = static_cast<std::size_t>(profile.family == core::SubscriptionFamily::TICKER
                                                     ? transport::RecoveryFamily::TICKER
                                                     : transport::RecoveryFamily::STREAM);
    const auto &desired = profile.endpoint.settings().budgets.desired;
    // Recovery inspects the actual collector owner maxima, including each independent TICKER view.
    // The queue requires every view's desired scope to cover its aggregate interest reservation.
    const auto owners = profile.family == core::SubscriptionFamily::TICKER ? profile.tickerLimits.viewSlots : 1;
    limits.replay[family] = {multiply(owners, desired.entries), multiply(owners, desired.totalSymbolUnits)};
}

/** Prepare each existing mapping and exact whole-callback projection coordinates.
 * @param profile Complete preparation, modified before publication.
 * @param registry Immutable validated routes; complete schema identity is preserved by profile.schema.
 * @param symbolUnits Maximum owned exact symbol size.
 */
void prepareMappings(ReceivingProfile &profile, const EventRegistry &registry, std::size_t symbolUnits) {
    const events::EventValueLimits values{symbolUnits, FIELD_TEXT_UNITS, add(symbolUnits, 2 * FIELD_TEXT_UNITS)};
    profile.bindings.reserve(registry.registrations().size());

    if (profile.bindings.capacity() > SCHEMA_LIMITS.records) {
        refuse(ErrorCode::RESOURCE_LIMIT, "profile-mapping-storage");
    }

    for (std::size_t kind = 0; kind < profile.eventPlans.size(); ++kind) {
        const auto quote = kind == 0;
        const auto type = quote ? EventType::QUOTE : EventType::PROFILE;
        auto &plan = profile.eventPlans[kind];
        plan.firstBinding = profile.bindings.size();
        plan.work.visits = 16;

        for (const auto &registration : registry.registrations()) {
            if (registration.type != type) {
                continue;
            }

            const auto mapping = registration.mappingLimits;
            const auto preparation =
                add(add(add(mapping.fields, mapping.lookups), add(mapping.selections, mapping.keys)),
                    add(add(mapping.typeAlternatives, mapping.nameUnits),
                        add(mapping.totalNameUnits, mapping.comparisonWork)));
            const auto policy = registration.eventTimePolicy == EventTimePolicy::FIELD_METADATA
                                    ? client::ReceiverEventTimePolicy::FIELD_METADATA
                                    : client::ReceiverEventTimePolicy::UNSPECIFIED;
            profile.bindings.push_back({registration.localRecordId,
                                        quote ? client::PreparedEventKind::QUOTE : client::PreparedEventKind::PROFILE,
                                        mapping, values, policy});
            const auto fields =
                profile.schema.records()[static_cast<std::size_t>(registration.localRecordId)].fields.size();
            const auto units = quote ? values.symbolUnits : values.totalUnits;
            const auto strings = quote ? std::size_t{1} : std::size_t{3};
            const auto bytes =
                add(sizeof(client::SubscriptionProjection), multiply(add(units, strings), sizeof(char16_t)));
            const client::ReceiverProjectionWork projection{
                multiply(CALLBACK_RECORDS, add(fields, add(mapping.selections, 3))), multiply(CALLBACK_RECORDS, units),
                multiply(CALLBACK_RECORDS, bytes)};
            plan.bindingCount = add(plan.bindingCount, 1);
            plan.limits.bindings = plan.bindingCount;
            plan.limits.shapeFields = add(plan.limits.shapeFields, fields);
            plan.limits.selections = add(plan.limits.selections, mapping.selections);
            plan.work.preparationWork = add(plan.work.preparationWork, preparation);
            plan.work.visits = add(plan.work.visits, add(plan.bindingCount, 1));
            // Any callback can contain this route; summing bounds covers heterogeneous complete batches.
            plan.projectionWork.visits = add(plan.projectionWork.visits, projection.visits);
            plan.projectionWork.textUnits = add(plan.projectionWork.textUnits, projection.textUnits);
            plan.projectionWork.bytes = add(plan.projectionWork.bytes, projection.bytes);
        }

        // Each projection scans preceding routes even when its matching record is last in the plan.
        plan.projectionWork.visits = add(plan.projectionWork.visits, multiply(CALLBACK_RECORDS, plan.bindingCount));
    }

    auto &all = profile.allEvents;
    all.firstBinding = 0;
    all.bindingCount = profile.bindings.size();
    all.limits.bindings = all.bindingCount;
    all.work.visits = 16;

    for (std::size_t index = 0; index < all.bindingCount; ++index) {
        // One validation and one construction visit, plus every preceding-route duplicate inspection.
        all.work.visits = add(all.work.visits, add(index, 2));
    }

    for (const auto &plan : profile.eventPlans) {
        all.limits.shapeFields = add(all.limits.shapeFields, plan.limits.shapeFields);
        all.limits.selections = add(all.limits.selections, plan.limits.selections);
        all.work.preparationWork = add(all.work.preparationWork, plan.work.preparationWork);
        all.projectionWork.visits = add(all.projectionWork.visits, plan.projectionWork.visits);
        all.projectionWork.textUnits = add(all.projectionWork.textUnits, plan.projectionWork.textUnits);
        all.projectionWork.bytes = add(all.projectionWork.bytes, plan.projectionWork.bytes);
    }
}
} // namespace

ReceivingProfile::ReceivingProfile(core::LocalSchema schema, client::EndpointOptions endpoint)
    : schema(std::move(schema)), endpoint(std::move(endpoint)) {
}

ReceivingProfile prepareReceivingProfile(const RuntimeOptions &runtime, const EndpointOptions &options,
                                         const std::string &host, std::uint16_t port) {
    auto selected = selectRegistry(options.eventRegistry ? &*options.eventRegistry : nullptr);
    auto copied = selected.schema().boundedCopy(SCHEMA_LIMITS);

    if (!copied) {
        refuse(copied.error() == core::SchemaError::ALLOCATION_FAILURE ? ErrorCode::ALLOCATION_FAILURE
                                                                       : ErrorCode::UNSUPPORTED_PROFILE,
               "profile-schema");
    }

    auto schema = std::move(*copied);
    std::array<std::size_t, 2> routes{};
    std::size_t maximumFields = 16;

    for (const auto &registration : selected.registrations()) {
        const auto kind = registration.type == EventType::QUOTE ? std::size_t{0} : std::size_t{1};
        routes[kind] = add(routes[kind], 1);
    }

    for (const auto &record : schema.records()) {
        maximumFields = std::max(maximumFields, record.fields.size());
    }

    // A mixed subscription may be created after connect; reserve the complete immutable registered selection.
    const auto maximumRoutes = std::max(std::size_t{1}, add(routes[0], routes[1]));
    auto configured = client::controlledStreamSettings();

    if (!configured) {
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-settings");
    }

    auto settings = std::move(*configured);
    settings.host = host;
    settings.port = port;
    settings.timeouts = {options.dnsTimeout, options.connectTimeout, options.readTimeout, options.writeTimeout,
                         options.pollInterval};
    settings.retry = {options.maximumAttempts,
                      options.maximumAttempts == 1 ? std::chrono::milliseconds{0} : options.retryDelay};
    const auto clients = runtime.maximumSubscriptionsPerEndpoint;
    const auto subscriptionEntries = multiply(runtime.maximumSymbolsPerSubscription, maximumRoutes);
    const auto entries = multiply(clients, subscriptionEntries);
    const auto symbolUnits = multiply(entries, runtime.maximumSymbolUnits);
    const auto scale = multiply(clients, add(subscriptionEntries, 15) / 16);
    auto &budgets = settings.budgets;
    budgets.session.events = protocol::DataEventPolicy::BINARY_CATALOG_V1;
    budgets.session.fieldTime = protocol::FieldTimePolicy::READ;
    budgets.session.data.processedRecords = budgets.session.data.emittedRecords = FRAME_RECORDS;
    budgets.session.data.wireFields = budgets.session.data.emittedValues = multiply(FRAME_RECORDS, maximumFields);
    budgets.session.data.symbol.codeUnits = budgets.session.data.symbol.stateUnits = runtime.maximumSymbolUnits;
    budgets.session.data.ownedFieldUnits = FIELD_TEXT_UNITS;
    budgets.session.data.ownedPayloadBytes = FIELD_BYTE_UNITS;
    budgets.desired = {SCHEMA_LIMITS.records,    entries, runtime.maximumSymbolUnits, symbolUnits, multiply(2, entries),
                       multiply(2, symbolUnits), entries};
    budgets.writer.operationEntries = std::max(budgets.writer.operationEntries, entries);

    // A complete named-route preparation and progress allowance is independent of queue reservations.
    const auto dns = transport::TcpRuntime::dnsPreparationWorkRequirement(runtime.dnsServers.size());

    if (!dns) {
        refuse(ErrorCode::DNS_CONFIGURATION, "profile-dns");
    }

    budgets.tcp.residentBytes = add(budgets.tcp.residentBytes, DNS_ENVELOPE_BYTES);
    budgets.protocolTurn.operations = add(budgets.protocolTurn.operations, dns->operations);
    budgets.protocolTurn.visits = add(budgets.protocolTurn.visits, dns->visits);
    budgets.protocolTurn.textUnits = add(budgets.protocolTurn.textUnits, dns->textUnits);
    budgets.protocolTurn.bytes = add(budgets.protocolTurn.bytes, dns->bytes);
    budgets.runtimeTurn.dnsCalls = budgets.runtimeTurn.dnsPackets = 10000;
    budgets.runtimeTurn.dnsBytes = 10000000;
    budgets.runtimeTurn.dnsUnits = DNS_PROGRESS_UNITS;

    auto endpoint = client::EndpointOptions::create(schema, settings);

    if (!endpoint) {
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-endpoint");
    }

    ReceivingProfile result(std::move(schema), std::move(*endpoint));
    result.family = options.contract == DeliveryContract::TICKER ? core::SubscriptionFamily::TICKER
                                                                 : core::SubscriptionFamily::STREAM;
    result.recoveryOptions.selectors = SELECTORS;
    result.recoveryOptions.dns.serverCount = runtime.dnsServers.size();
    result.recoveryOptions.dns.port = runtime.dnsPort;
    std::ranges::copy(runtime.dnsServers, result.recoveryOptions.dns.servers.begin());
    result.interests = {SCHEMA_LIMITS.records,      clients,     entries, entries,
                        runtime.maximumSymbolUnits, symbolUnits, entries, multiply(scale, 4 * MEBIBYTE)};
    result.clientLimits = {QUEUED_RECORDS,
                           RECORD_BYTES,
                           8,
                           BATCH_BYTES,
                           CALLBACK_RECORDS,
                           SCHEMA_LIMITS.fields,
                           runtime.maximumSymbolUnits,
                           FIELD_TEXT_UNITS,
                           FIELD_BYTE_UNITS,
                           RECORD_ARENA_BYTES,
                           4,
                           8,
                           subscriptionEntries,
                           multiply(subscriptionEntries, runtime.maximumSymbolUnits)};
    result.queueLimits = {clients,
                          FRAME_RECORDS,
                          16,
                          CALLBACK_RECORDS,
                          16,
                          multiply(clients, QUEUED_RECORDS),
                          multiply(clients, RECORD_BYTES),
                          multiply(clients, 8),
                          multiply(clients, BATCH_BYTES),
                          multiply(clients, 4),
                          multiply(clients, 8),
                          multiply(clients, 2),
                          multiply(scale, 64 * MEBIBYTE)};
    // One retained frame per exact cached key, plus an incoming candidate, covers fragmented updates.
    // Twice the event arena also covers its fixed owner/control backing in the qualified STL profiles.
    const auto tickerFrames = add(entries, 1);
    const auto tickerBytes = multiply(tickerFrames, multiply(2, budgets.session.eventBytes));
    result.tickerLimits = {
        clients, entries, tickerFrames, tickerBytes, add(tickerBytes, multiply(scale, 8 * MEBIBYTE)), budgets.desired};
    result.blueprint = prepareBlueprint(result.schema);
    prepareCopies(result);
    prepareMappings(result, selected, runtime.maximumSymbolUnits);
    result.queueWork = queueWork(scale);
    result.submitWork = {16, multiply(scale, CONTROL_VISITS), multiply(scale, CONTROL_VISITS),
                         multiply(scale, CONTROL_BYTES)};
    const transport::RecoveryWork recovery{10000, multiply(scale, CONTROL_VISITS), multiply(scale, CONTROL_VISITS),
                                           multiply(scale, CONTROL_BYTES)};
    const protocol::SubscriptionWriterWork writer{10000, multiply(scale, CONTROL_VISITS),
                                                  multiply(scale, CONTROL_VISITS), multiply(scale, CONTROL_BYTES)};
    result.preparationWork = {recovery, writer, {result.queueWork, result.queueWork, result.queueWork}};
    const auto &native = result.endpoint.settings().budgets.runtimeTurn;
    result.turnWork = {recovery,
                       writer,
                       {native.steps, native.handlers, native.socketBytes, native.dnsCalls, native.dnsPackets,
                        native.dnsBytes, native.dnsUnits},
                       result.endpoint.settings().budgets.protocolTurn,
                       {result.queueWork, result.queueWork, result.queueWork}};

    return result;
}

const ReceivingEventPlan &receivingEventPlan(const ReceivingProfile &profile, EventSelection selection) {
    switch (selection) {
    case EventSelection::QUOTE:
        return profile.eventPlans[0];
    case EventSelection::PROFILE:
        return profile.eventPlans[1];
    case EventSelection::BOTH:
        return profile.allEvents;
    default:
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "subscription-event-type");
    }
}

std::expected<mux::MuxQueue, mux::QueueFailure> createReceivingQueue(const ReceivingProfile &profile,
                                                                     mux::QueueWork &work) {
    if (profile.family == core::SubscriptionFamily::TICKER) {
        return mux::MuxQueue::createTicker(profile.schema, profile.interests, profile.tickerLimits,
                                           {false, 100, SELECTORS}, profile.queueLimits, work);
    }

    return mux::MuxQueue::createStream(profile.schema, profile.interests, profile.endpoint.settings().budgets.desired,
                                       profile.endpoint.settings().budgets.collector, profile.queueLimits, work,
                                       SELECTORS);
}
} // namespace org::ttldtor::qdcxx::api::detail
